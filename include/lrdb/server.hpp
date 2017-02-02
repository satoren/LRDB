#pragma once

#if __cplusplus >= 201103L || (defined(_MSC_VER) && _MSC_VER >= 1800)
#include <memory>
#include <utility>
#include <vector>

#include "debugger.hpp"
#include "message.hpp"
#include "server/debug_command.hpp"

#ifdef EMSCRIPTEN
#include "server/command_stream_socket_emscripten.hpp"
#else
#include "server/command_stream_socket.hpp"
#endif

namespace lrdb {

#define LRDB_SERVER_VERSION "0.1.9"

/// @brief Debug Server Class
/// template type is messaging communication customization point
/// require members
///  void close();			/// connection close
///  bool is_open() const; /// connection is opened
///  void poll();          /// polling event data. Require non blocking
///  void run_one();		/// run event data. Blocking until run one
///  message.
///  void wait_for_connection(); //Blocking until connection.
///  bool send_message(const std::string& message); /// send message to
///  communication opponent
///  //callback functions. Must that call inside poll or run_one
///  std::function<void(const std::string& data)> on_data;///callback for
///  receiving data.
///  std::function<void()> on_connection;
///  std::function<void()> on_close;
///  std::function<void(const std::string&)> on_error;
template <typename StreamType>
class basic_server {
 public:
  /// @brief constructor
  /// @param arg Forward to StreamType constructor
  template <typename... StreamArgs>
  basic_server(StreamArgs&&... arg)
      : wait_for_connect_(true),
        command_stream_(std::forward<StreamArgs>(arg)...) {
    init();
  }

  ~basic_server() { exit(); }

  /// @brief attach (or detach) for debug target
  /// @param lua_State*  debug target
  void reset(lua_State* L = 0) {
    debugger_.reset(L);
    if (!L) {
      exit();
    }
  }

  /// @brief Exit debug server
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
    json::object pauseparam;
    pauseparam["reason"] = json::value(debugger_.pause_reason());
    send_message(message::notify::serialize("paused", json::value(pauseparam)));
  }
  void connected_done() { wait_for_connect_ = false; }

  void send_message(const std::string& message) {
    command_stream_.send_message(message);
  }
  void execute_message(const std::string& message) {
    json::value req;
    std::string err = json::parse(req, message);
    if (err.empty()) {
      execute_request(req);
    }
  }

  void execute_request(json::value& req) {
    const std::string& method = message::get_method(req);
    const json::value& param = message::get_param(req);
    const json::value& reqid = message::get_id(req);

    typedef json::value (*exec_cmd_fn)(debugger & debugger,
                                       const json::value& param, bool& error);
    static const std::map<std::string, exec_cmd_fn> cmd_map = {
#define LRDB_DEBUG_COMMAND_TABLE(NAME) \
  { #NAME, &command::exec_##NAME }
        LRDB_DEBUG_COMMAND_TABLE(step),
        LRDB_DEBUG_COMMAND_TABLE(step_in),
        LRDB_DEBUG_COMMAND_TABLE(step_out),
        LRDB_DEBUG_COMMAND_TABLE(continue),
        LRDB_DEBUG_COMMAND_TABLE(pause),
        LRDB_DEBUG_COMMAND_TABLE(add_breakpoint),
        LRDB_DEBUG_COMMAND_TABLE(get_breakpoints),
        LRDB_DEBUG_COMMAND_TABLE(clear_breakpoints),
        LRDB_DEBUG_COMMAND_TABLE(get_stacktrace),
        LRDB_DEBUG_COMMAND_TABLE(get_local_variable),
        LRDB_DEBUG_COMMAND_TABLE(get_upvalues),
        LRDB_DEBUG_COMMAND_TABLE(eval),
        LRDB_DEBUG_COMMAND_TABLE(get_global),
#undef LRDB_DEBUG_COMMAND_TABLE
    };

    auto match = cmd_map.find(method);
    if (match != cmd_map.end()) {
      bool error = false;
      json::value result = match->second(debugger_, param, error);

      if (!reqid.is<json::null>() || error) {
        send_message(message::responce::serialize(reqid, result, error));
      }
    } else {
      send_message(message::responce::serialize(
          reqid, method + " is not supported", true));
    }
  }

  bool wait_for_connect_;
  debugger debugger_;
  StreamType command_stream_;
};
typedef basic_server<command_stream_socket> server;
}

#else
#error Needs at least a C++11 compiler
#endif