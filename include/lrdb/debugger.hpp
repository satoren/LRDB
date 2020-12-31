#pragma once

#if __cplusplus >= 201103L || (defined(_MSC_VER) && _MSC_VER >= 1800)

#include <cstdio>
#include <functional>
#include <string>
#include <vector>

#include <cmath>

#include "picojson.h"
extern "C" {
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
}

namespace lrdb {
namespace json {
using namespace ::picojson;
}
#if LUA_VERSION_NUM < 502
inline int lua_absindex(lua_State* L, int idx) {
  return (idx > 0 || (idx <= LUA_REGISTRYINDEX)) ? idx
                                                 : lua_gettop(L) + 1 + idx;
}
inline size_t lua_rawlen(lua_State* L, int index) {
  return lua_objlen(L, index);
}
inline void lua_pushglobaltable(lua_State* L) {
  lua_pushvalue(L, LUA_GLOBALSINDEX);
}
inline void lua_rawgetp(lua_State* L, int index, void* p) {
  lua_pushlightuserdata(L, p);
  lua_rawget(L, LUA_REGISTRYINDEX);
}
#endif
namespace utility {

/// @brief Lua stack value convert to json
inline json::value to_json(lua_State* L, int index, int max_recursive = 1) {
  index = lua_absindex(L, index);
  int type = lua_type(L, index);
  switch (type) {
    case LUA_TNIL:
      return json::value();
    case LUA_TBOOLEAN:
      return json::value(bool(lua_toboolean(L, index) != 0));
    case LUA_TNUMBER:
      // todo integer or double
      {
        double n = lua_tonumber(L, index);
        if (std::isnan(n)) {
          return json::value("NaN");
        }
        if (std::isinf(n)) {
          return json::value("Infinity");
        } else {
          return json::value(n);
        }
      }
    case LUA_TSTRING:
      return json::value(lua_tostring(L, index));
    case LUA_TTABLE: {
      if (max_recursive <= 0) {
        char buffer[128] = {};
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4996)
#endif
        sprintf(buffer, "%p", lua_topointer(L, -1));
#ifdef _MSC_VER
#pragma warning(pop)
#endif
        json::object obj;

        int tt = luaL_getmetafield(L, index, "__name");
        const char* type =
            (tt == LUA_TSTRING) ? lua_tostring(L, -1) : luaL_typename(L, index);
        obj[type] = json::value(buffer);
        if (tt != LUA_TNIL) {
          lua_pop(L, 1); /* remove '__name' */
        }
        return json::value(obj);
      }
      int array_size = lua_rawlen(L, index);
      if (array_size > 0) {
        json::array a;
        lua_pushnil(L);
        while (lua_next(L, index) != 0) {
          if (lua_type(L, -2) == LUA_TNUMBER) {
            a.push_back(to_json(L, -1, max_recursive - 1));
          }
          lua_pop(L, 1);  // pop value
        }
        return json::value(a);
      } else {
        json::object obj;
        lua_pushnil(L);
        while (lua_next(L, index) != 0) {
          if (lua_type(L, -2) == LUA_TSTRING) {
            const char* key = lua_tostring(L, -2);
            json::value& b = obj[key];

            b = to_json(L, -1, max_recursive - 1);
          }
          lua_pop(L, 1);  // pop value
        }
        return json::value(obj);
      }
    }
    case LUA_TUSERDATA: {
      if (luaL_callmeta(L, index, "__tostring")) {
        json::value v = to_json(L, -1, max_recursive);  // return value to json
        lua_pop(L, 1);  // pop return value and metatable
        return v;
      }
      if (luaL_callmeta(L, index, "__totable")) {
        json::value v = to_json(L, -1, max_recursive);  // return value to json
        lua_pop(L, 1);  // pop return value and metatable
        return v;
      }
    }
    case LUA_TLIGHTUSERDATA:
    case LUA_TTHREAD:
    case LUA_TFUNCTION: {
      int tt = luaL_getmetafield(L, index, "__name");
      const char* type =
          (tt == LUA_TSTRING) ? lua_tostring(L, -1) : luaL_typename(L, index);
      char buffer[128] = {};
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4996)
#endif
      sprintf(buffer, "%s: %p", type, lua_topointer(L, index));
#ifdef _MSC_VER
#pragma warning(pop)
#endif
      if (tt != LUA_TNIL) {
        lua_pop(L, 1); /* remove '__name' */
      }
      return json::value(buffer);
    }
  }
  return json::value();
}
/// @brief push value to Lua stack from json
inline void push_json(lua_State* L, const json::value& v) {
  if (v.is<json::null>()) {
    lua_pushnil(L);
  } else if (v.is<bool>()) {
    lua_pushboolean(L, v.get<bool>());
  } else if (v.is<double>()) {
    lua_pushnumber(L, v.get<double>());
  } else if (v.is<std::string>()) {
    const std::string& str = v.get<std::string>();
    lua_pushlstring(L, str.c_str(), str.size());
  } else if (v.is<json::object>()) {
    const json::object& obj = v.get<json::object>();
    lua_createtable(L, 0, obj.size());
    for (json::object::const_iterator itr = obj.begin(); itr != obj.end();
         ++itr) {
      push_json(L, itr->second);
      lua_setfield(L, -2, itr->first.c_str());
    }
  } else if (v.is<json::array>()) {
    const json::array& array = v.get<json::array>();
    lua_createtable(L, array.size(), 0);
    for (size_t index = 0; index < array.size(); ++index) {
      push_json(L, array[index]);
      lua_rawseti(L, -2, index + 1);
    }
  }
}
}  // namespace utility

/// @brief line based break point type
struct breakpoint_info {
  breakpoint_info() : line(-1), hit_count(0) {}
  std::string file;           /// source file
  std::string func;           /// function name(currently unused)
  int line;                   /// line number
  std::string condition;      /// break point condition
  std::string hit_condition;  // expression that controls how many hits of the
                              // breakpoint are ignored
  size_t hit_count;           /// breakpoint hit counts
};

/// @brief debug data
/// this data is available per stack frame
class debug_info {
 public:
  typedef std::vector<std::pair<std::string, json::value> > local_vars_type;
  debug_info() : state_(0), debug_(0) {}
  debug_info(const debug_info& other)
      : state_(other.state_),
        debug_(other.debug_),
        got_debug_(other.got_debug_) {}
  debug_info& operator=(const debug_info& other) {
    state_ = other.state_;
    debug_ = other.debug_;
    got_debug_ = other.got_debug_;
    return *this;
  }
  void assign(lua_State* L, lua_Debug* debug, const char* got_type = 0) {
    state_ = L;
    debug_ = debug;
    got_debug_.clear();
    if (got_type) {
      got_debug_.append(got_type);
    }
    if (debug->event == LUA_HOOKLINE) {
      got_debug_.append("l");
    }
  }
  bool is_available_info(const char* type) const {
    return got_debug_.find(type) != std::string::npos;
  }
  bool get_info(const char* type) {
    if (!is_available()) {
      return 0;
    }
    if (is_available_info(type)) {
      return true;
    }
    return lua_getinfo(state_, type, debug_) != 0;
  }
  /// @breaf get name
  /// @link https://www.lua.org/manual/5.3/manual.html#4.9
  const char* name() {
    if (!get_info("n") || !debug_->name) {
      return "";
    }
    return debug_->name;
  }
  /// @link https://www.lua.org/manual/5.3/manual.html#4.9
  const char* namewhat() {
    if (!get_info("n") || !debug_->namewhat) {
      return "";
    }
    return debug_->namewhat;
  }
  /// @link https://www.lua.org/manual/5.3/manual.html#4.9
  const char* what() {
    if (!get_info("S") || !debug_->what) {
      return "";
    }
    return debug_->what;
  }
  /// @link https://www.lua.org/manual/5.3/manual.html#4.9
  const char* source() {
    if (!get_info("S") || !debug_->source) {
      return "";
    }
    return debug_->source;
  }
  /// @link https://www.lua.org/manual/5.3/manual.html#4.9
  int currentline() {
    if (!get_info("l")) {
      return -1;
    }
    return debug_->currentline;
  }
  /// @link https://www.lua.org/manual/5.3/manual.html#4.9
  int linedefined() {
    if (!get_info("S")) {
      return -1;
    }
    return debug_->linedefined;
  }
  /// @link https://www.lua.org/manual/5.3/manual.html#4.9
  int lastlinedefined() {
    if (!get_info("S")) {
      return -1;
    }
    return debug_->lastlinedefined;
  }
  /// @link https://www.lua.org/manual/5.3/manual.html#4.9
  int number_of_upvalues() {
    if (!get_info("u")) {
      return -1;
    }
    return debug_->nups;
  }
#if LUA_VERSION_NUM >= 502
  /// @link https://www.lua.org/manual/5.3/manual.html#4.9
  int number_of_parameters() {
    if (!get_info("u")) {
      return -1;
    }
    return debug_->nparams;
  }
  /// @link https://www.lua.org/manual/5.3/manual.html#4.9
  bool is_variadic_arg() {
    if (!get_info("u")) {
      return false;
    }
    return debug_->isvararg != 0;
  }
  /// @link https://www.lua.org/manual/5.3/manual.html#4.9
  bool is_tailcall() {
    if (!get_info("t")) {
      return false;
    }
    return debug_->istailcall != 0;
  }
#endif
  /// @link https://www.lua.org/manual/5.3/manual.html#4.9
  const char* short_src() {
    if (!get_info("S")) {
      return "";
    }
    return debug_->short_src;
  }
  /*
  std::vector<bool> valid_lines_on_function() {
    std::vector<bool> ret;
    lua_getinfo(state_, "fL", debug_);
    lua_pushnil(state_);
    while (lua_next(state_, -2) != 0) {
      int t = lua_type(state_, -1);
      ret.push_back(lua_toboolean(state_, -1));
      lua_pop(state_, 1);  // pop value
    }
    lua_pop(state_, 2);
    return ret;
  }*/

  /// @brief evaluate script
  /// e.g.
  /// auto ret = debuginfo.eval("return 4,6");
  /// for(auto r: ret){std::cout << r.get<double>() << ,;}//output "4,6,"
  /// @param script luascript string
  /// @param global execute environment include global
  /// @param upvalue execute environment include upvalues
  /// @param local execute environment include local variables
  /// @param object_depth depth of extract for table for return value
  /// @return array of name and value pair
  std::vector<json::value> eval(const char* script, bool global = true,
                                bool upvalue = true, bool local = true,
                                int object_depth = 1) {
    std::string error;
    std::vector<json::value> ret =
        eval(script, error, global, upvalue, local, object_depth);
    if (!error.empty()) {
      ret.push_back(json::value(error));
    }
    return ret;
  }

  std::vector<json::value> eval(const char* script, std::string& error,
                                bool global = true, bool upvalue = true,
                                bool local = true, int object_depth = 1) {
    int stack_start = lua_gettop(state_);
    int loadstat =
        luaL_loadstring(state_, (std::string("return ") + script).c_str());
    if (loadstat != 0) {
      lua_pop(state_, 1);
      loadstat = luaL_loadstring(state_, script);
    }
    if (!lua_isfunction(state_, -1)) {
      error = lua_tostring(state_, -1);
      return std::vector<json::value>();
    }

    create_eval_env(global, upvalue, local);
#if LUA_VERSION_NUM >= 502
    lua_setupvalue(state_, -2, 1);
#else
    lua_setfenv(state_, -2);
#endif
    int call_stat = lua_pcall(state_, 0, LUA_MULTRET, 0);
    if (call_stat != 0) {
      error = lua_tostring(state_, -1);
      return std::vector<json::value>();
    }
    std::vector<json::value> ret;
    int ret_end = lua_gettop(state_);
    for (int retindex = stack_start + 1; retindex <= ret_end; ++retindex) {
      ret.push_back(utility::to_json(state_, retindex, object_depth));
    }
    lua_settop(state_, stack_start);
    return ret;
  }
  /// @brief get local variables
  /// @param object_depth depth of extract for table for return value
  /// @return array of name and value pair
  local_vars_type get_local_vars(int object_depth = 0) {
    local_vars_type localvars;
    int varno = 1;
    while (const char* varname = lua_getlocal(state_, debug_, varno++)) {
      if (varname[0] != '(') {
        localvars.push_back(std::pair<std::string, json::value>(
            varname, utility::to_json(state_, -1, object_depth)));
      }
      lua_pop(state_, 1);
    }
#if LUA_VERSION_NUM >= 502
    if (is_variadic_arg()) {
      json::array va;
      int varno = -1;
      while (const char* varname = lua_getlocal(state_, debug_, varno--)) {
        (void)varname;  // unused
        va.push_back(utility::to_json(state_, -1));
        lua_pop(state_, 1);
      }
      localvars.push_back(
          std::pair<std::string, json::value>("(*vararg)", json::value(va)));
    }
#endif
    return localvars;
  }
  /// @brief set local variables
  /// @param name local variable name
  /// @param v assign value
  /// @return If set is successful, return true. Otherwise return false.
  bool set_local_var(const char* name, const json::value& v) {
    local_vars_type vars = get_local_vars();
    for (size_t index = 0; index < vars.size(); ++index) {
      if (vars[index].first == name) {
        return set_local_var(index, v);
      }
    }
    return false;  // local variable name not found
  }

  /// @brief set local variables
  /// @param local_var_index local variable index
  /// @param v assign value
  /// @return If set is successful, return true. Otherwise return false.
  bool set_local_var(int local_var_index, const json::value& v) {
    utility::push_json(state_, v);
    return lua_setlocal(state_, debug_, local_var_index + 1) != 0;
  }

  /// @brief get upvalues
  /// @param object_depth depth of extract for table for return value
  /// @return array of name and value pair
  local_vars_type get_upvalues(int object_depth = 0) {
    local_vars_type localvars;

    lua_getinfo(state_, "f", debug_);  // push current running function
    int upvno = 1;
    while (const char* varname = lua_getupvalue(state_, -1, upvno++)) {
      localvars.push_back(std::pair<std::string, json::value>(
          varname, utility::to_json(state_, -1, object_depth)));
      lua_pop(state_, 1);
    }
    lua_pop(state_, 1);  // pop current running function
    return localvars;
  }
  /// @brief set upvalue
  /// @param name upvalue name
  /// @param v assign value
  /// @return If set is successful, return true. Otherwise return false.
  bool set_upvalue(const char* name, const json::value& v) {
    local_vars_type vars = get_upvalues();
    for (size_t index = 0; index < vars.size(); ++index) {
      if (vars[index].first == name) {
        return set_upvalue(index, v);
      }
    }
    return false;  // local variable name not found
  }

  /// @brief set upvalue
  /// @param var_index upvalue index
  /// @param v assign value
  /// @return If set is successful, return true. Otherwise return false.
  bool set_upvalue(int var_index, const json::value& v) {
    lua_getinfo(state_, "f", debug_);  // push current running function
    int target_functin_index = lua_gettop(state_);
    utility::push_json(state_, v);
    bool ret = lua_setupvalue(state_, target_functin_index, var_index + 1) != 0;
    lua_pop(state_, 1);  // pop current running function
    return ret;
  }
  /// @brief data is available
  /// @return If data is available, return true. Otherwise return false.
  bool is_available() { return state_ && debug_; }

 private:
  void create_eval_env(bool global = true, bool upvalue = true,
                       bool local = true) {
    lua_createtable(state_, 0, 0);
    int envtable = lua_gettop(state_);
    lua_createtable(state_, 0, 0);  // create metatable for env
    int metatable = lua_gettop(state_);
    // use global
    if (global) {
      lua_pushglobaltable(state_);
      lua_setfield(state_, metatable, "__index");
    }

    // use upvalue
    if (upvalue) {
      lua_getinfo(state_, "f", debug_);  // push current running function

#if LUA_VERSION_NUM < 502
      lua_getfenv(state_, -1);
      lua_setfield(state_, metatable, "__index");
#endif
      int upvno = 1;
      while (const char* varname = lua_getupvalue(state_, -1, upvno++)) {
        if (strcmp(varname, "_ENV") == 0)  // override _ENV
        {
          lua_pushvalue(state_, -1);
          lua_setfield(state_, metatable, "__index");
        }
        lua_setfield(state_, envtable, varname);
      }
      lua_pop(state_, 1);  // pop current running function
    }
    // use local vars
    if (local) {
      int varno = 0;
      while (const char* varname = lua_getlocal(state_, debug_, ++varno)) {
        if (strcmp(varname, "_ENV") == 0)  // override _ENV
        {
          lua_pushvalue(state_, -1);
          lua_setfield(state_, metatable, "__index");
        }
        lua_setfield(state_, envtable, varname);
      }
#if LUA_VERSION_NUM >= 502
      // va arg
      if (is_variadic_arg()) {
        varno = 0;
        lua_createtable(state_, 0, 0);
        while (const char* varname = lua_getlocal(state_, debug_, --varno)) {
          (void)varname;  // unused
          lua_rawseti(state_, -2, -varno);
        }
        if (varno < -1) {
          lua_setfield(state_, envtable, "(*vararg)");
        } else {
          lua_pop(state_, 1);
        }
      }
#endif
    }
    lua_setmetatable(state_, envtable);
#if LUA_VERSION_NUM < 502
    lua_pushvalue(state_, envtable);
    lua_setfield(state_, envtable, "_ENV");
#endif
    return;
  }

  friend class debugger;
  friend class stack_info;

  lua_State* state_;
  lua_Debug* debug_;
  std::string got_debug_;
};

/// @brief stack frame infomation data
class stack_info : private debug_info {
 public:
  stack_info(lua_State* L, int level) {
    memset(&debug_var_, 0, sizeof(debug_var_));
    valid_ = lua_getstack(L, level, &debug_var_) != 0;
    if (valid_) {
      assign(L, &debug_var_);
    }
  }
  stack_info(const stack_info& other)
      : debug_info(other), debug_var_(other.debug_var_), valid_(other.valid_) {
    debug_ = &debug_var_;
  }
  stack_info& operator=(const stack_info& other) {
    debug_info::operator=(other);
    debug_var_ = other.debug_var_;
    valid_ = other.valid_;
    debug_ = &debug_var_;
    return *this;
  }
  bool is_available() { return valid_ && debug_info::is_available(); }
  ~stack_info() { debug_ = 0; }
  using debug_info::assign;
  using debug_info::currentline;
  using debug_info::get_info;
  using debug_info::is_available_info;
  using debug_info::lastlinedefined;
  using debug_info::linedefined;
  using debug_info::name;
  using debug_info::namewhat;
  using debug_info::number_of_upvalues;
  using debug_info::source;
  using debug_info::what;
#if LUA_VERSION_NUM >= 502
  using debug_info::is_tailcall;
  using debug_info::is_variadic_arg;
  using debug_info::number_of_parameters;
#endif
  using debug_info::eval;
  using debug_info::get_local_vars;
  using debug_info::get_upvalues;
  using debug_info::set_local_var;
  using debug_info::set_upvalue;
  using debug_info::short_src;

 private:
  lua_Debug debug_var_;
  bool valid_;
};

/// @brief Debugging interface class
class debugger {
 public:
  typedef std::vector<breakpoint_info> line_breakpoint_type;
  typedef std::function<void(debugger& debugger)> pause_handler_type;
  typedef std::function<void(debugger& debugger)> tick_handler_type;

  debugger() : state_(0), pause_(true), step_type_(STEP_ENTRY) {}
  debugger(lua_State* L) : state_(0), pause_(true), step_type_(STEP_ENTRY) {
    reset(L);
  }
  ~debugger() { reset(); }

  /// @brief add breakpoints
  /// @param file filename
  /// @param line line number
  /// @param condition
  /// @param hit_condition start <, <=, ==, >, >=, % , followed by value. If
  /// operator is omit, equal to >=
  /// e.g.
  /// ">5" break always after 5 hits
  /// "<5" break on the first 4 hits only
  void add_breakpoint(const std::string& file, int line,
                      const std::string& condition = "",
                      const std::string& hit_condition = "") {
    breakpoint_info info;
    info.file = file;
    if (!is_path_separator(file.at(0)) && working_dir) { // is relative path and working_dir is set
      info.file = std::string(working_dir) + "/" + info.file;
    }
    info.line = line;
    info.condition = condition;
    if (!hit_condition.empty()) {
      if (is_first_cond_operators(hit_condition)) {
        info.hit_condition = hit_condition;
      } else {
        info.hit_condition = ">=" + hit_condition;
      }
    }

    line_breakpoints_.push_back(info);
  }
  /// @brief clear breakpoints with filename and line number
  /// @param file source filename
  /// @param line If minus,ignore line number. default -1
  void clear_breakpoints(const std::string& file, int line = -1) {
    line_breakpoints_.erase(
        std::remove_if(line_breakpoints_.begin(), line_breakpoints_.end(),
                       [&](const breakpoint_info& b) {
                         return (line < 0 || b.line == line) &&
                                (b.file == file);
                       }),
        line_breakpoints_.end());
  }
  /// @brief clear breakpoints
  void clear_breakpoints() { line_breakpoints_.clear(); }

  /// @brief get line break points.
  /// @return array of breakpoints.
  const line_breakpoint_type& line_breakpoints() const {
    return line_breakpoints_;
  }

  //  void error_break(bool enable) { error_break_ = enable; }

  /// @brief set tick handler. callback at new line,function call and function
  /// return.
  void set_tick_handler(tick_handler_type handler) { tick_handler_ = handler; }

  /// @brief set pause handler. callback at paused by pause,step,breakpoint.
  /// If want continue pause,execute the loop so as not to return.
  ///  e.g. basic_server::init
  void set_pause_handler(pause_handler_type handler) {
    pause_handler_ = handler;
  }

  /// @brief get current debug info,i.e. executing stack frame top.
  debug_info& current_debug_info() { return current_debug_info_; }

  /// @brief get breakpoint
  breakpoint_info* current_breakpoint() { return current_breakpoint_; }

  /// @brief assign or unassign debug target
  void reset(lua_State* L = 0) {
    if (state_ != L) {
      if (state_) {
        unsethook();
      }
      state_ = L;
      if (state_) {
        sethook();
      }
    }
  }
  /// @brief pause
  void pause() { step_type_ = STEP_PAUSE; }
  /// @brief unpause(continue)
  void unpause() {
    pause_ = false;
    step_type_ = STEP_NONE;
  }
  /// @brief paused
  /// @return If paused, return true. Otherwise return false.
  bool paused() { return pause_; }

  /// @brief paused
  /// @return string for pause
  /// reason."breakpoint","step","step_in","step_out","exception"
  const char* pause_reason() {
    if (current_breakpoint_) {
      return "breakpoint";
    } else if (step_type_ == STEP_OVER) {
      return "step";
    } else if (step_type_ == STEP_IN) {
      return "step_in";
    } else if (step_type_ == STEP_OUT) {
      return "step_out";
    } else if (step_type_ == STEP_PAUSE) {
      return "pause";
    } else if (step_type_ == STEP_ENTRY) {
      return "entry";
    }

    return "exception";
  }

  /// @brief step. same for step_over
  void step() { step_over(); }

  /// @brief step_over
  void step_over() {
    step_type_ = STEP_OVER;
    step_callstack_size_ = get_call_stack().size();
    pause_ = false;
  }
  /// @brief step in
  void step_in() {
    step_type_ = STEP_IN;
    step_callstack_size_ = get_call_stack().size();
    pause_ = false;
  }
  /// @brief step out
  void step_out() {
    step_type_ = STEP_OUT;
    step_callstack_size_ = get_call_stack().size();
    pause_ = false;
  }
  /// @brief get call stack info
  /// @return array of call stack information
  std::vector<stack_info> get_call_stack() {
    std::vector<stack_info> ret;
    if (!current_debug_info_.state_) {
      return ret;
    }
    ret.push_back(stack_info(current_debug_info_.state_, 0));
    while (ret.back().is_available()) {
      ret.push_back(stack_info(current_debug_info_.state_, ret.size()));
    }
    ret.pop_back();
    return ret;
  }

  /// @brief get global table
  /// @param object_depth depth of extract for return value
  /// @return global table value
  json::value get_global_table(int object_depth = 1) {
    lua_pushglobaltable(state_);
    json::value v = utility::to_json(state_, -1, object_depth);
    lua_pop(state_, 1);  // pop global table
    return v;
  }

  /// @brief set wcurrent working directory for relative path resolution
  void set_working_dir(char* arg_working_dir) {
    working_dir = (char*) malloc(4096 * sizeof(char));
    strcpy(working_dir, arg_working_dir);
  }

  void path_to_absolute(char* path_absolute, const char* path) {
    if (path[0] == '@') {
      path++;
      path_absolute[0] = '@';
      path_absolute[1] = '\0';
    }
    if (!is_path_separator(path[0]) && working_dir) {  // is a relative path and working_dir is set
      strcat(path_absolute, working_dir);
      strcat(path_absolute, "/");
    }
    strcat(path_absolute, path);
  }

 private:
  void sethook() {
    lua_pushlightuserdata(state_, this_data_key());
    lua_pushlightuserdata(state_, this);
    lua_rawset(state_, LUA_REGISTRYINDEX);
    lua_sethook(state_, &hook_function,
                LUA_MASKCALL | LUA_MASKRET | LUA_MASKLINE, 0);
  }
  void unsethook() {
    if (state_) {
      lua_sethook(state_, 0, 0, 0);
      lua_pushlightuserdata(state_, this_data_key());
      lua_pushnil(state_);
      lua_rawset(state_, LUA_REGISTRYINDEX);
      state_ = 0;
    }
  }
  debugger(const debugger&);             //=delete;
  debugger& operator=(const debugger&);  //=delete;

  static bool is_path_separator(char c) { return c == '\\' || c == '/'; }
  static bool is_file_path_match(const char* path1, const char* path2) {
    while (true) {
      // skip './'
      if (*path1 == '.' && is_path_separator(*(path1 + 1))) {
        path1 += 2;
        continue;
      }
      if (*path2 == '.' && is_path_separator(*(path2 + 1))) {
        path2 += 2;
        continue;
      }

      if ((*path1 != *path2) &&
          (!is_path_separator(*path1) || !is_path_separator(*path2))) {
        return false;
      }
      if (*path1 == '\0') {
        return true;
      }
      path1++;
      path2++;
    }
  }

  breakpoint_info* search_breakpoints(debug_info& debuginfo) {
    if (line_breakpoints_.empty()) {
      return 0;
    }
    int currentline = debuginfo.currentline();
    for (line_breakpoint_type::iterator it = line_breakpoints_.begin();
         it != line_breakpoints_.end(); ++it) {
      if (currentline == it->line) {
        const char* source = debuginfo.source();
        if (!source) {
          continue;
        }
        // remove front @
        if (source[0] == '@') {
          source++;
        }
        else { // @ is prepended to all filenames. When there is no @, the line is from eval, therefore can not have a breakpoint
          return 0;
        }
        char source_absolute[4096] = {0};
        path_to_absolute(source_absolute, source);

        if (is_file_path_match(it->file.c_str(), source_absolute)) {
          return &(*it);
        }
      }
    }
    return 0;
  }
  bool breakpoint_cond(const breakpoint_info& breakpoint,
                       debug_info& debuginfo) {
    if (!breakpoint.condition.empty()) {
      json::array condret = debuginfo.eval(breakpoint.condition.c_str());
      return !condret.empty() && condret[0].evaluate_as_boolean();
    }
    return true;
  }
  static bool is_first_cond_operators(const std::string& cond) {
    const char* ops[] = {"<", "==", ">", "%"};  //,"<=" ,">="
    for (size_t i = 0; i < sizeof(ops) / sizeof(ops[0]); ++i) {
      if (cond.compare(0, strlen(ops[i]), ops[i]) == 0) {
        return true;
      }
    }
    return false;
  }

  bool breakpoint_hit_cond(const breakpoint_info& breakpoint,
                           debug_info& debuginfo) {
    if (!breakpoint.hit_condition.empty()) {
      json::array condret = debuginfo.eval(
          (std::to_string(breakpoint.hit_count) + breakpoint.hit_condition)
              .c_str());

      return condret.empty() || condret[0].evaluate_as_boolean();
    }
    return true;
  }
  void hookline() {
    current_breakpoint_ = search_breakpoints(current_debug_info_);
    if (current_breakpoint_ &&
        breakpoint_cond(*current_breakpoint_, current_debug_info_)) {
      current_breakpoint_->hit_count++;
      if (breakpoint_hit_cond(*current_breakpoint_, current_debug_info_)) {
        pause_ = true;
      }
    }
  }
  void hookcall() {}
  void hookret() {}

  void tick() {
    if (tick_handler_) {
      tick_handler_(*this);
    }
  }
  void check_code_step_pause() {
    if (step_type_ == STEP_NONE) {
      return;
    }

    std::vector<stack_info> callstack = get_call_stack();
    switch (step_type_) {
      case STEP_OVER:
        if (step_callstack_size_ >= callstack.size()) {
          pause_ = true;
        }
        break;
      case STEP_IN:
        pause_ = true;
        break;
      case STEP_OUT:
        if (step_callstack_size_ > callstack.size()) {
          pause_ = true;
        }
        break;
      case STEP_PAUSE:
        pause_ = true;
        break;
      case STEP_ENTRY:
      case STEP_NONE:
        break;
    }
  }
  void hook(lua_State* L, lua_Debug* ar) {
    current_debug_info_.assign(L, ar);
    current_breakpoint_ = 0;
    tick();

    if (!pause_ && ar->event == LUA_HOOKLINE) {
      check_code_step_pause();
    }

    if (ar->event == LUA_HOOKLINE) {
      hookline();
    } else if (ar->event == LUA_HOOKCALL) {
      hookcall();
    } else if (ar->event == LUA_HOOKRET) {
      hookret();
    }
    if (pause_ && pause_handler_) {
      step_callstack_size_ = 0;
      pause_handler_(*this);
      if (step_type_ == STEP_NONE) {
        pause_ = false;
      }
    }
  }
  static void* this_data_key() {
    static int key_data = 0;
    return &key_data;
  }
  static void hook_function(lua_State* L, lua_Debug* ar) {
    lua_rawgetp(L, LUA_REGISTRYINDEX, this_data_key());

    debugger* self = static_cast<debugger*>(lua_touserdata(L, -1));
    lua_pop(L, 1);
    self->hook(L, ar);
  }

  enum step_type {
    STEP_NONE,
    STEP_OVER,
    STEP_IN,
    STEP_OUT,
    STEP_PAUSE,
    STEP_ENTRY,
  };

  char *working_dir = NULL;

  lua_State* state_;
  bool pause_;
  //  bool error_break_;
  step_type step_type_;
  size_t step_callstack_size_;
  debug_info current_debug_info_;
  line_breakpoint_type line_breakpoints_;
  breakpoint_info* current_breakpoint_;
  pause_handler_type pause_handler_;
  tick_handler_type tick_handler_;
};
}  // namespace lrdb

#else
#error Needs at least a C++11 compiler
#endif