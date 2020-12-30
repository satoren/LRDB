#include <emscripten.h>
#include <emscripten/bind.h>
#include <emscripten/val.h>
#include <unistd.h>

#include <cstdint>
#include <cstdlib>
#include <iostream>

#include "lrdb/basic_server.hpp"

class command_stream {
 public:
  command_stream() {}
  ~command_stream() {}
  void close() {}

  std::function<void(const std::string& data)> on_data;
  std::function<void()> on_connection;
  std::function<void()> on_close;
  std::function<void(const std::string&)> on_error;

  bool is_open() const { return true; }
  void poll() { emscripten_sleep(0); }
  void run_one() { emscripten_sleep(1); }
  void wait_for_connection() {}

  bool send_message(const std::string& m) {
    if (send_message_) {
      return send_message_(m);
    }
    return false;
  }
  std::function<bool(const std::string&)> send_message_;
};

typedef lrdb::basic_server<command_stream> server;

class debuggable_lua {
 public:
  debuggable_lua(emscripten::val notify) {
    luaL_openlibs(L);
    debug_server.reset(L);

    debug_server.command_stream().send_message_ =
        [=](const std::string& message) {
          notify(message);
          return true;
        };
  }
  ~debuggable_lua() { lua_close(L); }
  void do_file(const std::string& path, const emscripten::val& args,
               const emscripten::val& callback) {
    int ret = luaL_loadfile(L, path.c_str());
    if (ret != 0) {
      std::cerr << lua_tostring(L, -1);  // output error
      callback(false);
    } else {
      callback(exec(args));
    }
  }
  void do_string(const std::string& script, const emscripten::val& args,
                 const emscripten::val& callback) {
    int ret = luaL_loadstring(L, script.c_str());
    if (ret != 0) {
      std::cerr << lua_tostring(L, -1);  // output error
      callback(false);
    } else {
      callback(exec(args));
    }
  }
  void exit() { debug_server.exit(); }
  void debug_command(const std::string& jsondata) {
    const auto on_data = debug_server.command_stream().on_data;
    if (on_data) {
      on_data(jsondata);
    }
  }
  void connected() {
    const auto on_connection = debug_server.command_stream().on_connection;
    if (on_connection) {
      on_connection();
    }
  }
  void close() {
    const auto on_close = debug_server.command_stream().on_close;
    if (on_close) {
      on_close();
    }
  }
  void error(const std::string& err) {
    const auto on_error = debug_server.command_stream().on_error;
    if (on_error) {
      on_error(err);
    }
  }

 private:
  server debug_server;
  lua_State* L = luaL_newstate();
  bool executed = false;

  bool exec(const emscripten::val& args) {
    if (executed) {
      std::cerr << "already executed." << std::endl;
      return false;
    }
    executed = true;
    const auto length = args["length"].as<int>();

    for (int i = 0; i < length; ++i) {
      lua_pushstring(L, args[i].as<std::string>().c_str());
    }
    const auto ret = lua_pcall(L, length, LUA_MULTRET, 0);
    if (ret != 0) {
      std::cerr << lua_tostring(L, -1);  // output error
    }
    executed = false;
    return ret == 0;
  }
};
EMSCRIPTEN_BINDINGS(DebuggableLua) {
  emscripten::class_<debuggable_lua>("Lua")
      .constructor<emscripten::val>()
      .function("do_string", &debuggable_lua::do_string)
      .function("do_file", &debuggable_lua::do_file)
      .function("debug_command", &debuggable_lua::debug_command)
      .function("connected", &debuggable_lua::connected)
      .function("close", &debuggable_lua::close)
      .function("exit", &debuggable_lua::exit)
      .function("error", &debuggable_lua::error);
}
