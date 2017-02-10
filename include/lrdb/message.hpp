#pragma once

#include "picojson.h"
#include "lrdb/optional.hpp"

namespace lrdb {
namespace json {
using namespace ::picojson;
}

namespace message {

struct request_message {
  request_message() {}
  request_message(std::string id, std::string method,
                  json::value params = json::value())
      : id(std::move(id)),
        method(std::move(method)),
        params(std::move(params)) {}
  request_message(int id, std::string method,
                  json::value params = json::value())
      : id(double(id)), method(std::move(method)), params(std::move(params)) {}
  json::value id;
  std::string method;
  json::value params;
};

struct response_error {
  int code;
  std::string message;
  json::value data;

  response_error(int code, std::string message)
      : code(code), message(std::move(message)) {}

  enum error_code {
    ParseError = -32700,
    InvalidRequest = -32600,
    MethodNotFound = -32601,
    InvalidParams = -32602,
    InternalError = -32603,
    serverErrorStart = -32099,
    serverErrorEnd = -32000,
    ServerNotInitialized,
    UnknownErrorCode = -32001
  };
};

struct response_message {
  response_message() {}
  response_message(std::string id, json::value result = json::value())
      : id(std::move(id)), result(std::move(result)) {}
  response_message(int id, json::value result = json::value())
      : id(double(id)), result(std::move(result)) {}
  json::value id;
  json::value result;
  optional<response_error> error;
};
struct notify_message {
  notify_message(std::string method, json::value params = json::value())
      : method(std::move(method)), params(std::move(params)) {}
  std::string method;
  json::value params;
};

inline bool is_notify(const json::value& msg) {
  return msg.is<json::object>() && !msg.contains("id");
}
inline bool is_request(const json::value& msg) {
  return msg.is<json::object>() && msg.contains("method") &&
         msg.get("method").is<std::string>();
}
inline bool is_response(const json::value& msg) {
  return msg.is<json::object>() && !msg.contains("method") &&
         msg.contains("id");
}

inline bool parse(const json::value& message, request_message& request) {
  if (!is_request(message)) {
    return false;
  }
  request.id = message.get("id");
  request.method = message.get("method").get<std::string>();
  if (message.contains("param")) {
    request.params = message.get("param");
  } else {
    request.params = message.get("params");
  }
  return true;
}

inline bool parse(const json::value& message, notify_message& notify) {
  if (!is_notify(message)) {
    return false;
  }
  notify.method = message.get("method").get<std::string>();
  if (message.contains("param")) {
    notify.params = message.get("param");
  } else {
    notify.params = message.get("params");
  }
  return true;
}

inline bool parse(const json::value& message, response_message& response) {
  if (!is_response(message)) {
    return false;
  }
  response.id = message.get("id");
  response.result = message.get("result");
  return true;
}

inline std::string serialize(const request_message& msg) {
  json::object obj;
  obj["jsonrpc"] = json::value("2.0");

  obj["method"] = json::value(msg.method);
  if (!msg.params.is<json::null>()) {
    obj["params"] = msg.params;
  }
  obj["id"] = msg.id;
  return json::value(obj).serialize();
}

inline std::string serialize(const response_message& msg) {
  json::object obj;
  obj["jsonrpc"] = json::value("2.0");

  obj["result"] = msg.result;

  if (msg.error) {
    json::object error = {{"code", json::value(double(msg.error->code))},
                          {"message", json::value(msg.error->message)},
                          {"data", json::value(msg.error->data)}};
    obj["error"] = json::value(error);
  }

  obj["id"] = msg.id;
  return json::value(obj).serialize();
}

inline std::string serialize(const notify_message& msg) {
  json::object obj;
  obj["jsonrpc"] = json::value("2.0");

  obj["method"] = json::value(msg.method);
  if (!msg.params.is<json::null>()) {
    obj["params"] = msg.params;
  }
  return json::value(obj).serialize();
}

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
  if (!msg.is<json::object>() || !msg.contains("params")) {
    return null;
  }
  return msg.get<json::object>().at("params");
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
    obj["params"] = param;
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
    obj["params"] = param;
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
using message::request_message;
using message::response_message;
using message::notify_message;
using message::response_error;
}
