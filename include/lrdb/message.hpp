#pragma once

#include "picojson.h"

namespace lrdb {

namespace message {
inline const std::string& get_method(const picojson::value& msg) {
  static std::string null;
  if (!msg.is<picojson::object>() || !msg.contains("method")) {
    return null;
  }
  const picojson::value& m = msg.get<picojson::object>().at("method");
  if (!m.is<std::string>()) {
    return null;
  }
  return m.get<std::string>();
}
inline const picojson::value& get_param(const picojson::value& msg) {
  static picojson::value null;
  if (!msg.is<picojson::object>() || !msg.contains("param")) {
    return null;
  }
  return msg.get<picojson::object>().at("param");
}
inline const picojson::value& get_id(const picojson::value& msg) {
  static picojson::value null;
  if (!msg.is<picojson::object>() || !msg.contains("id")) {
    return null;
  }
  return msg.get<picojson::object>().at("id");
}
namespace request {
inline std::string serialize(const picojson::value& id,
                             const std::string& medhod,
                             const picojson::value& param = picojson::value()) {
  picojson::object obj;
  obj["method"] = picojson::value(medhod);
  if (!param.is<picojson::null>()) {
    obj["param"] = param;
  }
  obj["id"] = id;
  return picojson::value(obj).serialize();
}
inline std::string serialize(const picojson::value& id,
                             const std::string& medhod,
                             const std::string& param) {
  return serialize(id, medhod, picojson::value(param));
}
inline std::string serialize(double id, const std::string& medhod,
                             const std::string& param) {
  return serialize(picojson::value(id), medhod, picojson::value(param));
}
inline std::string serialize(double id, const std::string& medhod,
                             const picojson::value& param = picojson::value()) {
  return serialize(picojson::value(id), medhod, picojson::value(param));
}
inline std::string serialize(const std::string& id, const std::string& medhod,
                             const std::string& param) {
  return serialize(picojson::value(id), medhod, picojson::value(param));
}
inline std::string serialize(const std::string& id, const std::string& medhod,
                             const picojson::value& param = picojson::value()) {
  return serialize(picojson::value(id), medhod, picojson::value(param));
}
}
namespace notify {
inline std::string serialize(const std::string& medhod,
                             const picojson::value& param = picojson::value()) {
  picojson::object obj;
  obj["method"] = picojson::value(medhod);
  if (!param.is<picojson::null>()) {
    obj["param"] = param;
  }
  return picojson::value(obj).serialize();
}
inline std::string serialize(const std::string& medhod,
                             const std::string& param) {
  return serialize(medhod, picojson::value(param));
}
}
namespace responce {
inline std::string serialize(const picojson::value& id,
                             const picojson::value& result = picojson::value(),
                             bool error = false) {
  picojson::object obj;
  if (error) {
    obj["error"] = result;
  } else {
    obj["result"] = result;
  }
  obj["id"] = id;
  return picojson::value(obj).serialize();
}
inline std::string serialize(const picojson::value& id,
                             const std::string& result, bool error = false) {
  return serialize(id, picojson::value(result), error);
}
}
}
}