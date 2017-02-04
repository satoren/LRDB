#pragma once

#include "lrdb/debugger.hpp"
#include "lrdb/message.hpp"

namespace lrdb {

namespace command {
inline json::value exec_step(debugger& debugger, const json::value&, bool&) {
  debugger.step();
  return json::value();
}
inline json::value exec_step_in(debugger& debugger, const json::value&, bool&) {
  debugger.step_in();
  return json::value();
}

inline json::value exec_step_out(debugger& debugger, const json::value&,
                                 bool&) {
  debugger.step_out();
  return json::value();
}
inline json::value exec_continue(debugger& debugger, const json::value&,
                                 bool&) {
  debugger.unpause();
  return json::value();
}
inline json::value exec_pause(debugger& debugger, const json::value&, bool&) {
  debugger.pause();
  return json::value();
}
inline json::value exec_add_breakpoint(debugger& debugger,
                                       const json::value& param, bool& error) {
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
      condition = param.get<json::object>().at("condition").get<std::string>();
    }
    if (has_hit_condition) {
      hit_condition =
          param.get<json::object>().at("hit_condition").get<std::string>();
    }
    debugger.add_breakpoint(source, line, condition, hit_condition);
    return json::value(true);
  }
  error = true;
  return json::value();
}
inline json::value exec_clear_breakpoints(debugger& debugger,
                                          const json::value& param, bool&) {
  bool has_source = param.get("file").is<std::string>();
  bool has_line = param.get("line").is<double>();
  if (!has_source) {
    debugger.clear_breakpoints();
    return json::value(true);
  }
  std::string source = param.get<json::object>().at("file").get<std::string>();
  if (!has_line) {
    debugger.clear_breakpoints(source);
  } else {
    int line =
        static_cast<int>(param.get<json::object>().at("line").get<double>());
    debugger.clear_breakpoints(source, line);
  }
  return json::value(true);
}
inline json::value exec_get_breakpoints(debugger& debugger, const json::value&,
                                        bool&) {
  const debugger::line_breakpoint_type& breakpoints =
      debugger.line_breakpoints();

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

  return json::value(res);
}
inline json::value exec_get_stacktrace(debugger& debugger, const json::value&,
                                       bool&) {
  auto callstack = debugger.get_call_stack();
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

  return json::value(res);
}
inline json::value exec_get_local_variable(debugger& debugger,
                                           const json::value& param,
                                           bool& error) {
  if (!param.is<json::object>()) {
    return json::value();
  }
  bool has_stackno = param.get("stack_no").is<double>();
  int depth = param.get("depth").is<double>()
                  ? static_cast<int>(param.get("depth").get<double>())
                  : 0;
  if (has_stackno) {
    int stack_no = static_cast<int>(param.get("stack_no").get<double>());
    auto callstack = debugger.get_call_stack();
    if (int(callstack.size()) > stack_no) {
      auto localvar = callstack[stack_no].get_local_vars(depth);
      json::object obj;
      for (auto& var : localvar) {
        obj[var.first] = var.second;
      }
      return json::value(obj);
    }
  }
  error = true;
  return json::value("invalid arguments");
}
inline json::value exec_get_upvalues(debugger& debugger,
                                     const json::value& param, bool& error) {
  if (!param.is<json::object>()) {
    return json::value();
  }
  bool has_stackno = param.get("stack_no").is<double>();
  int depth = param.get("depth").is<double>()
                  ? static_cast<int>(param.get("depth").get<double>())
                  : 0;
  if (has_stackno) {
    int stack_no = static_cast<int>(
        param.get<json::object>().at("stack_no").get<double>());
    auto callstack = debugger.get_call_stack();
    if (int(callstack.size()) > stack_no) {
      auto localvar = callstack[stack_no].get_upvalues(depth);
      json::object obj;
      for (auto& var : localvar) {
        obj[var.first] = var.second;
      }
      return json::value(obj);
    }
  }
  error = true;
  return json::value("invalid arguments");
}
inline json::value exec_eval(debugger& debugger, const json::value& param,
                             bool& error) {
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
    auto callstack = debugger.get_call_stack();
    if (int(callstack.size()) > stack_no) {
      return json::value(callstack[stack_no].eval(
          chunk.c_str(), use_global, use_upvalue, use_local, depth));
    }
  }
  error = true;
  return json::value("invalid arguments");
}
inline json::value exec_get_global(debugger& debugger, const json::value& param,
                                   bool&) {
  int depth = param.get("depth").is<double>()
                  ? static_cast<int>(param.get("depth").get<double>())
                  : 0;
  return debugger.get_global_table(depth);
}
}
}
