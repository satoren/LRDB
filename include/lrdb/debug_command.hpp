#pragma once

#include "debugger.hpp"
#include "message.hpp"

namespace lrdb {

namespace command {
inline picojson::value exec_step(debugger& debugger, const picojson::value&) {
  debugger.step();
  return picojson::value();
}
inline picojson::value exec_step_in(debugger& debugger,
                                    const picojson::value&) {
  debugger.step_in();
  return picojson::value();
}

inline picojson::value exec_step_out(debugger& debugger,
                                     const picojson::value&) {
  debugger.step_out();
  return picojson::value();
}
inline picojson::value exec_continue(debugger& debugger,
                                     const picojson::value&) {
  debugger.unpause();
  return picojson::value();
}
inline picojson::value exec_pause(debugger& debugger, const picojson::value&) {
  debugger.pause();
  return picojson::value();
}
inline picojson::value exec_add_breakpoint(debugger& debugger,
                                           const picojson::value& param) {
  bool has_source = param.get("file").is<std::string>();
  bool has_condition = param.get("condition").is<std::string>();
  bool has_hit_condition = param.get("hit_condition").is<std::string>();
  bool has_line = param.get("line").is<double>();
  if (has_source && has_line) {
    std::string source =
        param.get<picojson::object>().at("file").get<std::string>();
    int line = static_cast<int>(
        param.get<picojson::object>().at("line").get<double>());

    std::string condition;
    std::string hit_condition;
    if (has_condition) {
      condition =
          param.get<picojson::object>().at("condition").get<std::string>();
    }
    if (has_hit_condition) {
      hit_condition =
          param.get<picojson::object>().at("hit_condition").get<std::string>();
    }
    debugger.add_breakpoint(source, line, condition);
    return picojson::value(true);
  }
  return picojson::value();
}
inline picojson::value exec_clear_breakpoints(debugger& debugger,
                                              const picojson::value& param) {
  bool has_source = param.get("file").is<std::string>();
  bool has_line = param.get("line").is<double>();
  if (!has_source) {
    debugger.clear_breakpoints();
    return picojson::value(true);
  }
  std::string source =
      param.get<picojson::object>().at("file").get<std::string>();
  if (!has_line) {
    debugger.clear_breakpoints(source);
  } else {
    int line = static_cast<int>(
        param.get<picojson::object>().at("line").get<double>());
    debugger.clear_breakpoints(source, line);
  }
  return picojson::value(true);
}
inline picojson::value exec_get_breakpoints(debugger& debugger,
                                            const picojson::value&) {
  const debugger::line_breakpoint_type& breakpoints =
      debugger.line_breakpoints();

  picojson::array res;
  for (const auto& b : breakpoints) {
    picojson::object br;
    br["file"] = picojson::value(b.file);
    if (!b.func.empty()) {
      br["func"] = picojson::value(b.func);
    }
    br["line"] = picojson::value(double(b.line));
    if (!b.condition.empty()) {
      br["condition"] = picojson::value(b.condition);
    }
    br["hit_count"] = picojson::value(double(b.hit_count));
    res.push_back(picojson::value(br));
  }

  return picojson::value(res);
}
inline picojson::value exec_get_stacktrace(debugger& debugger,
                                           const picojson::value&) {
  auto callstack = debugger.get_call_stack();
  picojson::array res;
  for (auto& s : callstack) {
    picojson::object data;
    if (s.source()) {
      data["file"] = picojson::value(s.source());
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
    data["func"] = picojson::value(name);
    data["line"] = picojson::value(double(s.currentline()));
    data["id"] = picojson::value(s.short_src());
    res.push_back(picojson::value(data));
  }

  return picojson::value(res);
}
inline picojson::value exec_get_local_variable(debugger& debugger,
                                               const picojson::value& param) {
  if (!param.is<picojson::object>()) {
    return picojson::value();
  }
  bool has_stackno = param.get("stack_no").is<double>();
  if (has_stackno) {
    int stack_no = static_cast<int>(
        param.get<picojson::object>().at("stack_no").get<double>());
    auto callstack = debugger.get_call_stack();
    if (int(callstack.size()) > stack_no) {
      auto localvar = callstack[stack_no].get_local_vars();
      picojson::object obj;
      for (auto& var : localvar) {
        obj[var.first] = var.second;
      }
      return picojson::value(obj);
    }
  }
  return picojson::value();
}
inline picojson::value exec_get_upvalues(debugger& debugger,
                                         const picojson::value& param) {
  if (!param.is<picojson::object>()) {
    return picojson::value();
  }
  bool has_stackno = param.get("stack_no").is<double>();
  if (has_stackno) {
    int stack_no = static_cast<int>(
        param.get<picojson::object>().at("stack_no").get<double>());
    auto callstack = debugger.get_call_stack();
    if (int(callstack.size()) > stack_no) {
      auto localvar = callstack[stack_no].get_upvalues();
      picojson::object obj;
      for (auto& var : localvar) {
        obj[var.first] = var.second;
      }
      return picojson::value(obj);
    }
  }
  return picojson::value();
}
inline picojson::value exec_eval(debugger& debugger,
                                 const picojson::value& param) {
  bool has_chunk = param.get("chunk").is<std::string>();
  bool has_stackno = param.get("stack_no").is<double>();

  bool use_global =
      !param.get("global").is<bool>() || param.get("global").get<bool>();
  bool use_upvalue =
      !param.get("upvalue").is<bool>() || param.get("upvalue").get<bool>();
  bool use_local =
      !param.get("local").is<bool>() || param.get("local").get<bool>();

  if (has_chunk && has_stackno) {
    std::string chunk =
        param.get<picojson::object>().at("chunk").get<std::string>();
    int stack_no = static_cast<int>(
        param.get<picojson::object>().at("stack_no").get<double>());
    auto callstack = debugger.get_call_stack();
    if (int(callstack.size()) > stack_no) {
      return picojson::value(callstack[stack_no].eval(chunk.c_str(), use_global,
                                                      use_upvalue, use_local));
    }
  }
  return picojson::value();
}
}
}