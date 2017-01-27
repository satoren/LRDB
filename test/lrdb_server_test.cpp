
#include <iostream>
#include <thread>

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
    rid_ = 0;
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
  int rid_;

  lrdb::json::value sync_request(
      const std::string& method,
      const lrdb::json::value& param = lrdb::json::value()) {
    int rid = rid_++;
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
  void luaDofile(lua_State* L, const char* file) {
    int err = luaL_dofile(L, file);
    const char* errorstring = 0;
    if (err) {
      errorstring = lua_tostring(L, -1);
    }
    ASSERT_STREQ(0, errorstring);
  }
};
}

TEST_F(DebugServerTest, ConnectTest1) {
  const char* TEST_LUA_SCRIPT = "../test/lua/test1.lua";

  std::thread client([&] {

    lrdb::json::object break_point;
    break_point["file"] = lrdb::json::value(TEST_LUA_SCRIPT);
    break_point["line"] = lrdb::json::value(5.);

    lrdb::json::value res =
        sync_request("add_breakpoint", lrdb::json::value(break_point));
    ASSERT_TRUE(res.evaluate_as_boolean());
    res = sync_request("get_breakpoints");
    ASSERT_TRUE(res.evaluate_as_boolean());

    res = sync_request("get_stacktrace");
    ASSERT_TRUE(res.evaluate_as_boolean());

    res = sync_request("continue");
    ASSERT_TRUE(res.evaluate_as_boolean());
    wait_for_paused();
    res = sync_request("continue");
    ASSERT_TRUE(res.evaluate_as_boolean());
  });

  luaDofile(L, TEST_LUA_SCRIPT);
  server.exit();

  client.join();
}
TEST_F(DebugServerTest, ConnectTest2) {
  const char* TEST_LUA_SCRIPT = "../test/lua/test1.lua";

  std::thread client([&] {

    lrdb::json::object break_point;
    break_point["file"] = lrdb::json::value(TEST_LUA_SCRIPT);
    break_point["line"] = lrdb::json::value(5.);

    lrdb::json::value res =
        sync_request("add_breakpoint", lrdb::json::value(break_point));
    ASSERT_TRUE(res.evaluate_as_boolean());
    res = sync_request("get_breakpoints");
    ASSERT_TRUE(res.evaluate_as_boolean());
    // std::cout << res.serialize() << std::endl;

    res = sync_request("continue");
    ASSERT_TRUE(res.evaluate_as_boolean());
    wait_for_paused();

    res = sync_request("get_local_variable");
    ASSERT_TRUE(res.evaluate_as_boolean());
    //		std::cout << res.serialize() << std::endl;
    res = sync_request("get_upvalues");
    ASSERT_TRUE(res.evaluate_as_boolean());
    //		std::cout << res.serialize() << std::endl;
    lrdb::json::object stack_var_req_param_obj;
    stack_var_req_param_obj["stack_no"] = lrdb::json::value(0.);
    lrdb::json::value stack_var_req_param =
        lrdb::json::value(stack_var_req_param_obj);
    res = sync_request("get_local_variable", stack_var_req_param);
    ASSERT_TRUE(res.evaluate_as_boolean());
    //		std::cout << res.serialize() << std::endl;
    res = sync_request("get_upvalues", stack_var_req_param);
    ASSERT_TRUE(res.evaluate_as_boolean());
    //		std::cout << res.serialize() << std::endl;

    sync_request("continue");
  });

  luaDofile(L, TEST_LUA_SCRIPT);
  server.exit();

  client.join();
}

TEST_F(DebugServerTest, ConnectTest4) {
  const char* TEST_LUA_SCRIPT = "../test/lua/test1.lua";

  std::thread client([&] {

    lrdb::json::object break_point;
    break_point["file"] = lrdb::json::value(TEST_LUA_SCRIPT);
    break_point["line"] = lrdb::json::value(5.);

    lrdb::json::value res =
        sync_request("add_breakpoint", lrdb::json::value(break_point));
    ASSERT_TRUE(res.evaluate_as_boolean());
    res = sync_request("get_breakpoints");
    ASSERT_TRUE(res.evaluate_as_boolean());

    res = sync_request("get_stacktrace");
    ASSERT_TRUE(res.evaluate_as_boolean());

    res = sync_request("continue");
    ASSERT_TRUE(res.evaluate_as_boolean());
    wait_for_paused();
    res = sync_request("continue");
    ASSERT_TRUE(res.evaluate_as_boolean());
  });

  luaDofile(L, TEST_LUA_SCRIPT);
  server.exit();

  client.join();
}

TEST_F(DebugServerTest, ConnectTest5) {
  const char* TEST_LUA_SCRIPT = "../test/lua/eval_test2.lua";

  std::thread client([&] {

    lrdb::json::object break_point;
    break_point["file"] = lrdb::json::value(TEST_LUA_SCRIPT);
    break_point["line"] = lrdb::json::value(2.);

    lrdb::json::value res =
        sync_request("add_breakpoint", lrdb::json::value(break_point));
    ASSERT_TRUE(res.evaluate_as_boolean());
    res = sync_request("get_breakpoints");
    ASSERT_TRUE(res.evaluate_as_boolean());
    // std::cout << res.serialize() << std::endl;

    res = sync_request("continue");
    ASSERT_TRUE(res.evaluate_as_boolean());
    wait_for_paused();

    res = sync_request("get_local_variable");
    ASSERT_TRUE(res.evaluate_as_boolean());
    //		std::cout << res.serialize() << std::endl;
    res = sync_request("get_upvalues");
    ASSERT_TRUE(res.evaluate_as_boolean());
    //		std::cout << res.serialize() << std::endl;
    lrdb::json::object stack_var_req_param_obj;
    stack_var_req_param_obj["stack_no"] = lrdb::json::value(0.);
    lrdb::json::value stack_var_req_param =
        lrdb::json::value(stack_var_req_param_obj);
    res = sync_request("get_local_variable", stack_var_req_param);
    ASSERT_TRUE(res.evaluate_as_boolean());
    //		std::cout << res.serialize() << std::endl;
    res = sync_request("get_upvalues", stack_var_req_param);
    ASSERT_TRUE(res.evaluate_as_boolean());
    // std::cout << res.serialize() << std::endl;

    res = sync_request("continue");
    ASSERT_TRUE(res.evaluate_as_boolean());
  });

  luaDofile(L, TEST_LUA_SCRIPT);
  server.exit();

  client.join();
}

int main(int argc, char* argv[]) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}