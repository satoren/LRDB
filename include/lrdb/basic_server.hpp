#pragma once

#if __cplusplus >= 201103L || (defined(_MSC_VER) && _MSC_VER >= 1800)
#include <memory>
#include <utility>
#include <vector>

#include "debugger.hpp"
#include "message.hpp"

namespace lrdb {

#define LRDB_SERVER_PROTOCOL_VERSION "2"

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
    send_notify(notify_message("exit"));
    command_stream_.close();
  }

 private:
  void init() {
    debugger_.set_pause_handler([&](debugger&) {
      send_pause_status();
      while (debugger_.paused() && command_stream_.is_open()) {
        command_stream_.run_one();
      }
      send_notify(notify_message("running"));
    });

    debugger_.set_tick_handler([&](debugger&) {
      if (wait_for_connect_) {
        command_stream_.wait_for_connection();
      }
      command_stream_.poll();
    });

    command_stream_.on_connection = [=]() { connected_done(); };
    command_stream_.on_data = [=](const std::string& data) {
      execute_message(data);
    };
    command_stream_.on_close = [=]() { debugger_.unpause(); };
  }
  void send_pause_status() {
    json::object pauseparam;
    pauseparam["reason"] = json::value(debugger_.pause_reason());
    send_notify(notify_message("paused", json::value(pauseparam)));
  }
  void connected_done() {
    wait_for_connect_ = false;
    json::object param;
    param["protocol_version"] = json::value(LRDB_SERVER_PROTOCOL_VERSION);

    json::object lua;
    lua["version"] = json::value(LUA_VERSION);
    lua["release"] = json::value(LUA_RELEASE);
    lua["copyright"] = json::value(LUA_COPYRIGHT);

    param["lua"] = json::value(lua);
    send_notify(notify_message("connected", json::value(param)));
  }

  bool send_message(const std::string& message) {
    return command_stream_.send_message(message);
  }
  void execute_message(const std::string& message) {
    json::value msg;
    std::string err = json::parse(msg, message);
    if (err.empty()) {
      if (message::is_request(msg)) {
        request_message request;
        message::parse(msg, request);
        execute_request(request);
      }
    }
  }

  bool send_notify(const notify_message& message) {
    return send_message(message::serialize(message));
  }
  bool send_response(response_message& message) {
    return send_message(message::serialize(message));
  }
  bool step_request(response_message& response, const json::value&) {
    debugger_.step();
    return send_response(response);
  }

  bool step_in_request(response_message& response, const json::value&) {
    debugger_.step_in();
    return send_response(response);
  }
  bool step_out_request(response_message& response, const json::value&) {
    debugger_.step_out();
    return send_response(response);
  }
  bool continue_request(response_message& response, const json::value&) {
    debugger_.unpause();
    return send_response(response);
  }
  bool pause_request(response_message& response, const json::value&) {
    debugger_.pause();
    return send_response(response);
  }
  bool add_breakpoint_request(response_message& response,
                              const json::value& param) {
    bool has_source = param.get("file").is<std::string>();
    bool has_condition = param.get("condition").is<std::string>();
    bool has_hit_condition = param.get("hit_condition").is<std::string>();
    bool has_line = param.get("line").is<double>();
    if (has_source && has_line) {
      std::string source =
          param.get<json::object>().at("file").get<std::string>();
      int line =
          static_cast<int>(param.get<json::object>().at("line").get<double>());

      std::string condition;
      std::string hit_condition;
      if (has_condition) {
        condition =
            param.get<json::object>().at("condition").get<std::string>();
      }
      if (has_hit_condition) {
        hit_condition =
            param.get<json::object>().at("hit_condition").get<std::string>();
      }
      debugger_.add_breakpoint(source, line, condition, hit_condition);

    } else {
      response.error =
          response_error(response_error::InvalidParams, "invalid params");
    }
    return send_response(response);
  }

  bool clear_breakpoints_request(response_message& response,
                                 const json::value& param) {
    bool has_source = param.get("file").is<std::string>();
    bool has_line = param.get("line").is<double>();
    if (!has_source) {
      debugger_.clear_breakpoints();
    } else {
      std::string source =
          param.get<json::object>().at("file").get<std::string>();
      if (!has_line) {
        debugger_.clear_breakpoints(source);
      } else {
        int line = static_cast<int>(
            param.get<json::object>().at("line").get<double>());
        debugger_.clear_breakpoints(source, line);
      }
    }

    return send_response(response);
  }

  bool get_breakpoints_request(response_message& response, const json::value&) {
    const debugger::line_breakpoint_type& breakpoints =
        debugger_.line_breakpoints();

    json::array res;
    for (const auto& b : breakpoints) {
      json::object br;
      br["file"] = json::value(b.file);
      if (!b.func.empty()) {
        br["func"] = json::value(b.func);
      }
      br["line"] = json::value(double(b.line));
      if (!b.condition.empty()) {
        br["condition"] = json::value(b.condition);
      }
      br["hit_count"] = json::value(double(b.hit_count));
      res.push_back(json::value(br));
    }

    response.result = json::value(res);

    return send_response(response);
  }

  bool get_stacktrace_request(response_message& response, const json::value&) {
    auto callstack = debugger_.get_call_stack();
    json::array res;
    for (auto& s : callstack) {
      json::object data;
      if (s.source()) {
        data["file"] = json::value(s.source());
      }
      const char* name = s.name();
      if (!name || name[0] == '\0') {
        name = s.name();
      }
      if (!name || name[0] == '\0') {
        name = s.namewhat();
      }
      if (!name || name[0] == '\0') {
        name = s.what();
      }
      if (!name || name[0] == '\0') {
        name = s.source();
      }
      data["func"] = json::value(name);
      data["line"] = json::value(double(s.currentline()));
      data["id"] = json::value(s.short_src());
      res.push_back(json::value(data));
    }
    response.result = json::value(res);

    return send_response(response);
  }

  bool get_local_variable_request(response_message& response,
                                  const json::value& param) {
    if (!param.is<json::object>()) {
      response.error =
          response_error(response_error::InvalidParams, "invalid params");

      return send_response(response);
    }
    bool has_stackno = param.get("stack_no").is<double>();
    int depth = param.get("depth").is<double>()
                    ? static_cast<int>(param.get("depth").get<double>())
                    : 1;
    if (has_stackno) {
      int stack_no = static_cast<int>(param.get("stack_no").get<double>());
      auto callstack = debugger_.get_call_stack();
      if (int(callstack.size()) > stack_no) {
        auto localvar = callstack[stack_no].get_local_vars(depth);
        json::object obj;
        for (auto& var : localvar) {
          obj[var.first] = var.second;
        }
        response.result = json::value(obj);
        return send_response(response);
      }
    }
    response.error =
        response_error(response_error::InvalidParams, "invalid params");

    return send_response(response);
  }

  bool get_upvalues_request(response_message& response,
                            const json::value& param) {
    if (!param.is<json::object>()) {
      response.error =
          response_error(response_error::InvalidParams, "invalid params");

      return send_response(response);
    }
    bool has_stackno = param.get("stack_no").is<double>();
    int depth = param.get("depth").is<double>()
                    ? static_cast<int>(param.get("depth").get<double>())
                    : 1;
    if (has_stackno) {
      int stack_no = static_cast<int>(
          param.get<json::object>().at("stack_no").get<double>());
      auto callstack = debugger_.get_call_stack();
      if (int(callstack.size()) > stack_no) {
        auto localvar = callstack[stack_no].get_upvalues(depth);
        json::object obj;
        for (auto& var : localvar) {
          obj[var.first] = var.second;
        }

        response.result = json::value(obj);

        return send_response(response);
      }
    }
    response.error =
        response_error(response_error::InvalidParams, "invalid params");

    return send_response(response);
  }
  bool eval_request(response_message& response, const json::value& param) {
    bool has_chunk = param.get("chunk").is<std::string>();
    bool has_stackno = param.get("stack_no").is<double>();

    bool use_global =
        !param.get("global").is<bool>() || param.get("global").get<bool>();
    bool use_upvalue =
        !param.get("upvalue").is<bool>() || param.get("upvalue").get<bool>();
    bool use_local =
        !param.get("local").is<bool>() || param.get("local").get<bool>();

    int depth = param.get("depth").is<double>()
                    ? static_cast<int>(param.get("depth").get<double>())
                    : 1;

    if (has_chunk && has_stackno) {
      std::string chunk =
          param.get<json::object>().at("chunk").get<std::string>();
      int stack_no = static_cast<int>(
          param.get<json::object>().at("stack_no").get<double>());
      auto callstack = debugger_.get_call_stack();
      if (int(callstack.size()) > stack_no) {
        std::string error;
        json::value ret = json::value(
            callstack[stack_no].eval(chunk.c_str(), error, use_global,
                                     use_upvalue, use_local, depth + 1));
        if (error.empty()) {
          response.result = ret;

          return send_response(response);
        } else {
          response.error = response_error(response_error::InvalidParams, error);

          return send_response(response);
        }
      }
    }
    response.error =
        response_error(response_error::InvalidParams, "invalid params");

    return send_response(response);
  }
  bool get_global_request(response_message& response,
                          const json::value& param) {
    int depth = param.get("depth").is<double>()
                    ? static_cast<int>(param.get("depth").get<double>())
                    : 1;
    response.result =
        debugger_.get_global_table(depth + 1);  //+ 1 is global table self

    return send_response(response);
  }

  void execute_request(const request_message& req) {
    typedef bool (basic_server::*exec_cmd_fn)(response_message & response,
                                              const json::value& param);

    static const std::map<std::string, exec_cmd_fn> cmd_map = {
#define LRDB_DEBUG_COMMAND_TABLE(NAME) {#NAME, &basic_server::NAME##_request}
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

    response_message response;
    response.id = req.id;
    auto match = cmd_map.find(req.method);
    if (match != cmd_map.end()) {
      (this->*(match->second))(response, req.params);
    } else {
      response.error = response_error(response_error::MethodNotFound,
                                      "method not found : " + req.method);
      send_response(response);
    }
  }

  bool wait_for_connect_;
  debugger debugger_;
  StreamType command_stream_;
};
}  // namespace lrdb

#else
#error Needs at least a C++11 compiler
#endif