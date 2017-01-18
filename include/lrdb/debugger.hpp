#pragma once
#include <cstdio>
#include <functional>
#include <string>
#include <vector>

#include "picojson.h"
extern "C" {
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
}

namespace lrdb {

namespace utility {
inline picojson::value to_json(lua_State* L, int index, int max_recursive = 1) {
  index = lua_absindex(L, index);
  int type = lua_type(L, index);
  switch (type) {
    case LUA_TNIL:
      return picojson::value();
    case LUA_TBOOLEAN:
      return picojson::value(bool(lua_toboolean(L, index) != 0));
    case LUA_TNUMBER:
      // todo integer or double
      {
        double n = lua_tonumber(L, index);
        if (std::isnan(n)) {
          return picojson::value("NaN");
        }
        if (std::isinf(n)) {
          return picojson::value("Infinity");
        } else {
          return picojson::value(n);
        }
      }
    case LUA_TSTRING:
      return picojson::value(lua_tostring(L, index));
    case LUA_TTABLE: {
      if (max_recursive <= 0) {
        return picojson::value(picojson::object());
      }
      int array_size = lua_rawlen(L, index);
      if (array_size > 0) {
        picojson::array a;
        lua_pushnil(L);
        while (lua_next(L, index) != 0) {
          int prev = lua_gettop(L);
          if (lua_type(L, -2) == LUA_TNUMBER) {
            a.push_back(to_json(L, -1, max_recursive - 1));
          }
          lua_pop(L, 1);  // pop value
        }
        return picojson::value(a);
      } else {
        picojson::object obj;
        lua_pushnil(L);
        while (lua_next(L, index) != 0) {
          if (lua_type(L, -2) == LUA_TSTRING) {
            const char* key = lua_tostring(L, -2);
            picojson::value& b = obj[key];

            b = to_json(L, -1, max_recursive - 1);
          }
          lua_pop(L, 1);  // pop value
        }
        return picojson::value(obj);
      }
    }
    case LUA_TUSERDATA: {
      const char* str = lua_tostring(L, index);
      if (str) {
        return picojson::value(str);
      }
    }
    case LUA_TLIGHTUSERDATA:
    case LUA_TTHREAD:
    case LUA_TFUNCTION: {
      char buffer[128] = {};
      snprintf(buffer, sizeof(buffer) - 1, "%s(%d)", lua_typename(L, type),
               reinterpret_cast<uintptr_t>(lua_topointer(L, index)));
      return picojson::value(buffer);
    }
  }
  return picojson::value();
}
inline void push_json(lua_State* L, const picojson::value& v) {
  if (v.is<picojson::null>()) {
    lua_pushnil(L);
  } else if (v.is<bool>()) {
    lua_pushboolean(L, v.get<bool>());
  } else if (v.is<double>()) {
    lua_pushnumber(L, v.get<double>());
  } else if (v.is<std::string>()) {
    const std::string& str = v.get<std::string>();
    lua_pushlstring(L, str.c_str(), str.size());
  } else if (v.is<picojson::object>()) {
    const picojson::object& obj = v.get<picojson::object>();
    lua_createtable(L, 0, obj.size());
    for (picojson::object::const_iterator itr = obj.begin(); itr != obj.end();
         ++itr) {
      push_json(L, itr->second);
      lua_setfield(L, -2, itr->first.c_str());
    }
  } else if (v.is<picojson::array>()) {
    const picojson::array& array = v.get<picojson::array>();
    lua_createtable(L, array.size(), 0);
    for (size_t index = 0; index < array.size(); ++index) {
      push_json(L, array[index]);
      lua_seti(L, -2, index + 1);
    }
  }
}
}

struct breakpoint_info {
  breakpoint_info() : line(-1), enabled(false), break_count(0) {}
  std::string file;
  std::string func;
  int line;
  std::string condition;  // todo
  size_t break_count;
  bool enabled;
};

class debug_info {
 public:
  typedef std::vector<std::pair<std::string, picojson::value> > local_vars_type;
  debug_info() : state_(0), debug_(0) {}
  debug_info(const debug_info& other)
      : state_(other.state_),
        debug_(other.debug_),
        got_debug_(other.got_debug_) {}
  debug_info& operator=(const debug_info& other) {
    state_ = other.state_;
    debug_ = other.debug_;
    got_debug_ = other.got_debug_;
  }
  void assign(lua_State* L, lua_Debug* debug, const char* got_type = "") {
    state_ = L;
    debug_ = debug;
    got_debug_.clear();
    got_debug_.append(got_type);
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
  const char* name() {
    if (!get_info("n") || !debug_->name) {
      return "";
    }
    return debug_->name;
  }
  const char* namewhat() {
    if (!get_info("n") || !debug_->namewhat) {
      return "";
    }
    return debug_->namewhat;
  }
  const char* what() {
    if (!get_info("S") || !debug_->what) {
      return "";
    }
    return debug_->what;
  }
  const char* source() {
    if (!get_info("S") || !debug_->source) {
      return "";
    }
    return debug_->source;
  }
  int currentline() {
    if (!get_info("l")) {
      return -1;
    }
    return debug_->currentline;
  }
  int linedefined() {
    if (!get_info("S")) {
      return -1;
    }
    return debug_->linedefined;
  }
  int lastlinedefined() {
    if (!get_info("S")) {
      return -1;
    }
    return debug_->lastlinedefined;
  }
  int number_of_upvalues() {
    if (!get_info("u")) {
      return -1;
    }
    return debug_->nups;
  }
  int number_of_parameters() {
    if (!get_info("u")) {
      return -1;
    }
    return debug_->nparams;
  }
  bool is_variadic_arg() {
    if (!get_info("u")) {
      return false;
    }
    return debug_->isvararg != 0;
  }
  bool is_tailcall() {
    if (!get_info("t")) {
      return false;
    }
    return debug_->istailcall != 0;
  }
  const char* short_src() {
    if (!get_info("S")) {
      return "";
    }
    return debug_->short_src;
  }

  std::vector<picojson::value> eval(const char* script, bool global = true,
                                    bool upvalue = true, bool local = true,
                                    int object_depth = 1) {
    int stack_start = lua_gettop(state_);
    luaL_loadstring(state_, script);
    create_eval_env(global, upvalue, local);
#if LUA_VERSION_NUM >= 502
    lua_setupvalue(state_, -2, 1);
#else
    lua_setfenv(state_, -2);
#endif
    lua_pcall(state_, 0, LUA_MULTRET, 0);
    std::vector<picojson::value> ret;
    int ret_end = lua_gettop(state_);
    for (int retindex = stack_start + 1; retindex <= ret_end; ++retindex) {
      ret.push_back(utility::to_json(state_, retindex, object_depth));
    }
    lua_settop(state_, stack_start);
    return ret;
  }
  local_vars_type get_local_vars(int object_depth = 0) {
    local_vars_type localvars;
    int varno = 1;
    while (const char* varname = lua_getlocal(state_, debug_, varno++)) {
      if (varname[0] != '(') {
        localvars.push_back(std::pair<std::string, picojson::value>(
            varname, utility::to_json(state_, -1, object_depth)));
      }
      lua_pop(state_, 1);
    }
    if (is_variadic_arg()) {
      picojson::array va;
      int varno = -1;
      while (const char* varname = lua_getlocal(state_, debug_, varno--)) {
        va.push_back(utility::to_json(state_, -1));
        lua_pop(state_, 1);
      }
      localvars.push_back(std::pair<std::string, picojson::value>(
          "(*vararg)", picojson::value(va)));
    }
    return localvars;
  }
  bool set_local_var(const char* name, const picojson::value& v) {
    local_vars_type vars = get_local_vars();
    for (size_t index = 0; index < vars.size(); ++index) {
      if (vars[index].first == name) {
        return set_local_var(index, v);
      }
    }
    return false;  // local variable name not found
  }
  bool set_local_var(int local_var_index, const picojson::value& v) {
    utility::push_json(state_, v);
    return lua_setlocal(state_, debug_, local_var_index + 1) != 0;
  }

  local_vars_type get_upvalues(int object_depth = 0) {
    local_vars_type localvars;

    lua_getinfo(state_, "f", debug_);  // push current running function
    int upvno = 1;
    while (const char* varname = lua_getupvalue(state_, -1, upvno++)) {
      localvars.push_back(std::pair<std::string, picojson::value>(
          varname, utility::to_json(state_, -1, object_depth)));
    }
    lua_pop(state_, 1);  // pop current running function
    return localvars;
  }
  bool set_upvalue(const char* name, const picojson::value& v) {
    local_vars_type vars = get_upvalues();
    for (size_t index = 0; index < vars.size(); ++index) {
      if (vars[index].first == name) {
        return set_upvalue(index, v);
      }
    }
    return false;  // local variable name not found
  }
  bool set_upvalue(int local_var_index, const picojson::value& v) {
    lua_getinfo(state_, "f", debug_);  // push current running function
    int target_functin_index = lua_gettop(state_);
    utility::push_json(state_, v);
    bool ret =
        lua_setupvalue(state_, target_functin_index, local_var_index + 1) != 0;
    lua_pop(state_, 1);  // pop current running function
    return ret;
  }
  bool is_available() { return state_ && debug_; }

 private:
  void create_eval_env(bool global = true, bool upvalue = true,
                       bool local = true) {
    int nup = number_of_upvalues();
    lua_createtable(state_, 0, 0);

    // copy global
    if (global) {
      int envtable = lua_gettop(state_);
      lua_pushglobaltable(state_);
      int globaltable = lua_gettop(state_);
      lua_pushnil(state_);
      while (lua_next(state_, globaltable) != 0) {
        lua_pushvalue(state_, -2);  // push key
        lua_rotate(state_, -2, 1);
        lua_rawset(state_, envtable);
      }
      lua_pop(state_, 1);  // pop global table
    }
    if (upvalue) {
      lua_getinfo(state_, "f", debug_);  // push current running function
      int upvno = 1;
      while (const char* varname = lua_getupvalue(state_, -1, upvno++)) {
        lua_setfield(state_, -3, varname);
      }
      lua_pop(state_, 1);  // pop current running function
    }

    if (local) {
      int varno = 0;
      while (const char* varname = lua_getlocal(state_, debug_, ++varno)) {
        lua_setfield(state_, -2, varname);
      }
      // va arg
      if (is_variadic_arg()) {
        varno = 0;
        lua_createtable(state_, 0, 0);
        while (const char* varname = lua_getlocal(state_, debug_, --varno)) {
          lua_seti(state_, -2, -varno);
        }
        if (varno < -1) {
          lua_setfield(state_, -2, "(*vararg)");
        } else {
          lua_pop(state_, 1);
        }
      }
    }
    return;
  }

  friend class debugger;
  friend class stack_info;

  lua_State* state_;
  lua_Debug* debug_;
  std::string got_debug_;
};
class stack_info : private debug_info {
 public:
  stack_info(lua_State* L, int level) {
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
  }
  bool is_available() { return valid_ && debug_info::is_available(); }
  ~stack_info() { debug_ = 0; }
  using debug_info::assign;
  using debug_info::is_available_info;
  using debug_info::get_info;
  using debug_info::name;
  using debug_info::namewhat;
  using debug_info::what;
  using debug_info::source;
  using debug_info::currentline;
  using debug_info::linedefined;
  using debug_info::lastlinedefined;
  using debug_info::number_of_upvalues;
  using debug_info::number_of_parameters;
  using debug_info::is_variadic_arg;
  using debug_info::is_tailcall;
  using debug_info::short_src;
  using debug_info::eval;
  using debug_info::get_local_vars;
  using debug_info::set_local_var;
  using debug_info::get_upvalues;
  using debug_info::set_upvalue;

 private:
  lua_Debug debug_var_;
  bool valid_;
};

class debugger {
 public:
  typedef std::vector<breakpoint_info> line_breakpoint_type;
  typedef std::function<void(debugger& debugger)> pause_handler_type;
  typedef std::function<void(debugger& debugger)> tick_handler_type;

  debugger() : state_(0), pause_(false), step_type_(STEP_NONE) {}
  debugger(lua_State* L) : state_(0), pause_(false), step_type_(STEP_NONE) {
    reset(L);
  }
  ~debugger() { reset(); }

  void add_breakpoint(const std::string& file, int line, bool enabled = true) {
    breakpoint_info info;
    info.file = file;
    info.line = line;
    info.enabled = enabled;
    line_breakpoints_.push_back(info);
  }

  const line_breakpoint_type& line_breakpoints() const {
    return line_breakpoints_;
  }

  void error_break(bool enable) { error_break_ = enable; }
  void set_tick_handler(tick_handler_type handler) { tick_handler_ = handler; }
  void set_pause_handler(pause_handler_type handler) {
    pause_handler_ = handler;
  }

  debug_info& current_debug_info() { return current_debug_info_; }

  // get breaked break point
  breakpoint_info* current_breakpoint() { return current_breakpoint_; }

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
  void pause() { pause_ = true; }
  void unpause() {
    pause_ = false;
    step_type_ = STEP_NONE;
  }
  bool paused() { return pause_; }

  const char* pause_reason() {
    if (current_breakpoint_) {
      return "breakpoint";
    } else if (step_type_ == STEP_OVER) {
      return "step";
    } else if (step_type_ == STEP_IN) {
      return "step_in";
    } else if (step_type_ == STEP_OUT) {
      return "step_out";
    }
    return "exception";
  }

  void step() { step_over(); }
  void step_over() {
    step_type_ = STEP_OVER;
    step_callstack_size_ = get_call_stack().size();
    pause_ = false;
  }
  void step_in() {
    step_type_ = STEP_IN;
    step_callstack_size_ = get_call_stack().size();
    pause_ = false;
  }
  void step_out() {
    step_type_ = STEP_OUT;
    step_callstack_size_ = get_call_stack().size();
    pause_ = false;
  }
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

 private:
  void sethook() {
    lua_pushlightuserdata(state_, &hook_function);
    lua_pushlightuserdata(state_, this);
    lua_rawset(state_, LUA_REGISTRYINDEX);
    lua_sethook(state_, &hook_function,
                LUA_MASKCALL | LUA_MASKRET | LUA_MASKLINE, 0);
  }
  void unsethook() {
    if (state_) {
      lua_sethook(state_, 0, 0, 0);
      lua_pushlightuserdata(state_, &hook_function);
      lua_pushnil(state_);
      lua_rawset(state_, LUA_REGISTRYINDEX);
      state_ = 0;
    }
  }
  debugger(const debugger&);             //=delete;
  debugger& operator=(const debugger&);  //=delete;

  breakpoint_info* search_breakpoints(debug_info& debuginfo) {
    bool getinfo = false;
    int currentline = debuginfo.currentline();
    for (line_breakpoint_type::iterator it = line_breakpoints_.begin();
         it != line_breakpoints_.end(); ++it) {
      if (currentline == it->line) {
        if (it->file == (debuginfo.source() + 1)) {
          return &(*it);
        }
      }
    }
    return 0;
  }

  void hookline() {
    current_breakpoint_ = search_breakpoints(current_debug_info_);
    if (current_breakpoint_ && current_breakpoint_->enabled) {
      current_breakpoint_->break_count++;
      pause_ = true;
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
        if (step_callstack_size_ == callstack.size()) {
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
    }
  }
  void hook(lua_State* L, lua_Debug* ar) {
    current_debug_info_.assign(L, ar);
    current_breakpoint_ = 0;
    pause_ = false;
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
    }
  }
  static void hook_function(lua_State* L, lua_Debug* ar) {
    lua_pushlightuserdata(L, &hook_function);
    lua_rawget(L, LUA_REGISTRYINDEX);

    debugger* self = static_cast<debugger*>(lua_touserdata(L, -1));
    lua_pop(L, 1);
    self->hook(L, ar);
  }

  enum step_type {
    STEP_NONE,
    STEP_OVER,
    STEP_IN,
    STEP_OUT,
  };

  lua_State* state_;
  bool pause_;
  bool error_break_;
  step_type step_type_;
  size_t step_callstack_size_;
  debug_info current_debug_info_;
  line_breakpoint_type line_breakpoints_;
  breakpoint_info* current_breakpoint_;
  pause_handler_type pause_handler_;
  tick_handler_type tick_handler_;
};
}