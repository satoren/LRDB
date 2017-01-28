#pragma once

#include "picojson.h"

namespace lrdb {
namespace json {
using namespace ::picojson;
}

namespace message {
inline const std::string& get_method(const json::value& msg) {
  static std::string null;
  if (!msg.is<json::object>() || !msg.contains("method")) {
    return null;
  }
  const json::value& m = msg.get<json::object>().at("method");
  if (!m.is<std::string>()) {
    return null;
  }
  return m.get<std::string>();
}
inline const json::value& get_param(const json::value& msg) {
  static json::value null;
  if (!msg.is<json::object>() || !msg.contains("param")) {
    return null;
  }
  return msg.get<json::object>().at("param");
}
inline const json::value& get_id(const json::value& msg) {
  static json::value null;
  if (!msg.is<json::object>() || !msg.contains("id")) {
    return null;
  }
  return msg.get<json::object>().at("id");
}
namespace request {
inline std::string serialize(const json::value& id, const std::string& medhod,
                             const json::value& param = json::value()) {
  json::object obj;
  obj["method"] = json::value(medhod);
  if (!param.is<json::null>()) {
    obj["param"] = param;
  }
  obj["id"] = id;
  return json::value(obj).serialize();
}
inline std::string serialize(const json::value& id, const std::string& medhod,
                             const std::string& param) {
  return serialize(id, medhod, json::value(param));
}
inline std::string serialize(double id, const std::string& medhod,
                             const std::string& param) {
  return serialize(json::value(id), medhod, json::value(param));
}
inline std::string serialize(double id, const std::string& medhod,
                             const json::value& param = json::value()) {
  return serialize(json::value(id), medhod, json::value(param));
}
inline std::string serialize(const std::string& id, const std::string& medhod,
                             const std::string& param) {
  return serialize(json::value(id), medhod, json::value(param));
}
inline std::string serialize(const std::string& id, const std::string& medhod,
                             const json::value& param = json::value()) {
  return serialize(json::value(id), medhod, json::value(param));
}
}
namespace notify {
inline std::string serialize(const std::string& medhod,
                             const json::value& param = json::value()) {
  json::object obj;
  obj["method"] = json::value(medhod);
  if (!param.is<json::null>()) {
    obj["param"] = param;
  }
  return json::value(obj).serialize();
}
inline std::string serialize(const std::string& medhod,
                             const std::string& param) {
  return serialize(medhod, json::value(param));
}
}
namespace responce {
inline std::string serialize(const json::value& id,
                             const json::value& result = json::value(),
                             bool error = false) {
  json::object obj;
  if (error) {
    obj["error"] = result;
  } else {
    obj["result"] = result;
  }
  obj["id"] = id;
  return json::value(obj).serialize();
}
inline std::string serialize(const json::value& id, const std::string& result,
                             bool error = false) {
  return serialize(id, json::value(result), error);
}
}
}
}
