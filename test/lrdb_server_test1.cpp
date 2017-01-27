
#include <iostream>

#include "lrdb/client.hpp"
#include "lrdb/message.hpp"
#include "lrdb/server.hpp"

#include "gtest/gtest.h"

namespace {

#ifdef LRDB_USE_BOOST_ASIO
using namespace boost::asio;
#endif
class DebugServerTest : public ::testing::Test {
 protected:
  DebugServerTest()
      : server(21115), client_stream("localhost", "21115"), pause_(0) {}

  virtual ~DebugServerTest() {}

  virtual void SetUp() {
    L = luaL_newstate();
    luaL_openlibs(L);
    server.reset(L);
  }

  virtual void TearDown() {
    server.reset();
    lua_close(L);
    L = 0;
  }
  lua_State* L;
  lrdb::server server;
  asio::ip::tcp::iostream client_stream;

  std::function<void(const lrdb::json::value&)> notify_handler;

  bool pause_;

  lrdb::json::value sync_request(
      int rid, const std::string& method,
      const lrdb::json::value& param = lrdb::json::value()) {
    client_stream << lrdb::message::request::serialize(rid, method, param)
                  << std::endl;

    if (client_stream.bad()) {
      lrdb::json::value error_res;
      return error_res;
    }

    while (true) {
      std::string line;
      std::getline(client_stream, line, '\n');
      if (client_stream.bad()) {
        lrdb::json::value error_res;
        return error_res;
      }
      lrdb::json::value v;
      std::string err = lrdb::json::parse(v, line);
      if (!err.empty()) {
        lrdb::json::value error_res;
        return error_res;
      }
      const lrdb::json::value& resid = lrdb::message::get_id(v);
      if (resid.is<double>() && resid.get<double>() == rid) {
        return v;
      } else {
        notify(v);
      }
    }
    lrdb::json::value error_res;
    return error_res;
  }

  void notify(const lrdb::json::value& v) {
    if (notify_handler) {
      notify_handler(v);
    }
    std::string method = lrdb::message::get_method(v);
    if (method == "running") {
      pause_ = false;
    } else if (method == "paused") {
      pause_ = true;
    }
  }

  void wait_for_paused() {
    while (pause_) {
      std::string line;
      std::getline(client_stream, line, '\n');
      if (client_stream.bad()) {
        return;
      }
      lrdb::json::value v;
      std::string err = lrdb::json::parse(v, line);
      if (!err.empty()) {
        return;
      }

      notify(v);
    }
  }
};
}
