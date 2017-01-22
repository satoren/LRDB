#pragma once

#include <memory>
#include <vector>

#include "debug_command.hpp"
#include "debugger.hpp"
#include "message.hpp"

#ifdef EMSCRIPTEN
#include "server_socket_emscripten.hpp"
#else
#include "server_socket.hpp"
#endif
namespace lrdb {

#define LRDB_SERVER_VERSION "0.0.1"

class server {
 public:
  server(uint16_t port = 21110)
      : wait_for_connect_(true), server_socket_(port) {
    debugger_.set_pause_handler([&](debugger& debugger) {
      send_pause_status();
	  if (wait_for_connect_) {
		  debugger.pause();
		  server_socket_.wait_for_connection();
	  }
      while (debugger.paused() && server_socket_.is_open()) {
        server_socket_.run_one();
      }
      send_message(message::notify::serialize("running"));
    });

    debugger_.set_tick_handler([&](debugger&) {
      server_socket_.poll();
    });

    debugger_.step_in();
    server_socket_.on_connection = [=]() { connected_done(); };
    server_socket_.on_data = [=](const std::string& data) {
      execute_message(data);
    };
    server_socket_.on_close = [=]() {
		debugger_.unpause();
	};
  }

  ~server() { exit(); }

  void reset(lua_State* L = 0) {
    debugger_.reset(L);
    if (!L) {
      exit();
    }
  }

  void exit() {
    send_message(message::notify::serialize("exit"));
    server_socket_.close();
  }

 private:
  void send_pause_status() {
    picojson::object pauseparam;
    pauseparam["reason"] = picojson::value(debugger_.pause_reason());
    send_message(
        message::notify::serialize("paused", picojson::value(pauseparam)));
  }
  void connected_done() { wait_for_connect_ = false; }

  void send_message(const std::string& message) {
    server_socket_.send_message(message + "\r\n");
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
  server_socket server_socket_;
};
}