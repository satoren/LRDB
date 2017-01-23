#pragma once

#include <memory>
#include <vector>
#include <utility>

#include "debug_command.hpp"
#include "debugger.hpp"
#include "message.hpp"

#ifdef EMSCRIPTEN
#include "command_stream_socket_emscripten.hpp"
#else
#include "command_stream_socket.hpp"
#endif

namespace lrdb {

#define LRDB_SERVER_VERSION "0.0.1"

template <typename StreamType>
class basic_server {
 public:

 template<typename... StreamArgs>
  basic_server(StreamArgs&&... arg)
      : wait_for_connect_(true), command_stream_(std::forward<StreamArgs>(arg)...) {
    init();
  }

  ~basic_server() { exit(); }

  void reset(lua_State* L = 0) {
    debugger_.reset(L);
    if (!L) {
      exit();
    }
  }

  void exit() {
    send_message(message::notify::serialize("exit"));
    command_stream_.close();
  }

 private:
  void init() {
    debugger_.set_pause_handler([&](debugger& debugger) {
      send_pause_status();
      if (wait_for_connect_) {
        debugger.pause();
        command_stream_.wait_for_connection();
      }
      while (debugger.paused() && command_stream_.is_open()) {
        command_stream_.run_one();
      }
      send_message(message::notify::serialize("running"));
    });

    debugger_.set_tick_handler([&](debugger&) { command_stream_.poll(); });

    debugger_.step_in();
    command_stream_.on_connection = [=]() { connected_done(); };
    command_stream_.on_data = [=](const std::string& data) {
      execute_message(data);
    };
    command_stream_.on_close = [=]() { debugger_.unpause(); };
  }
  void send_pause_status() {
    picojson::object pauseparam;
    pauseparam["reason"] = picojson::value(debugger_.pause_reason());
    send_message(
        message::notify::serialize("paused", picojson::value(pauseparam)));
  }
  void connected_done() { wait_for_connect_ = false; }

  void send_message(const std::string& message) {
    command_stream_.send_message(message);
  }
  void execute_message(const std::string& message) {
    picojson::value req;
    std::string err = picojson::parse(req, message);
    if (err.empty()) {
      execute_request(req);
    }
  }

  void execute_request(picojson::value& req) {
    const std::string& method = message::get_method(req);
    const picojson::value& param = message::get_param(req);
    const picojson::value& reqid = message::get_id(req);

    picojson::value result;

#define DEBUG_COMMAND_TABLE(NAME)                    \
  if (method == #NAME) {                             \
    result = command::exec_##NAME(debugger_, param); \
  } else

    DEBUG_COMMAND_TABLE(step)
    DEBUG_COMMAND_TABLE(step_in)
    DEBUG_COMMAND_TABLE(continue)
    DEBUG_COMMAND_TABLE(pause)
    DEBUG_COMMAND_TABLE(add_breakpoint)
    DEBUG_COMMAND_TABLE(get_breakpoints)
    DEBUG_COMMAND_TABLE(clear_breakpoints)
    DEBUG_COMMAND_TABLE(get_stacktrace)
    DEBUG_COMMAND_TABLE(get_local_variable)
    DEBUG_COMMAND_TABLE(get_upvalues)
    DEBUG_COMMAND_TABLE(eval)
    DEBUG_COMMAND_TABLE(get_global)
    if (true) {
      send_message(message::responce::serialize(
          reqid, method + " is not supported", true));
      return;
    }

#undef DEBUG_COMMAND_TABLE

    if (!reqid.is<picojson::null>()) {
      send_message(message::responce::serialize(reqid, result));
    }
  }

  bool wait_for_connect_;
  debugger debugger_;
  StreamType command_stream_;
};
typedef basic_server<command_stream_socket> server;
}