
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
      : server(21115), client_stream(), pause_(0) {}

  virtual ~DebugServerTest() {}

  virtual void SetUp() {
    L = luaL_newstate();
    luaL_openlibs(L);
    server.reset(L);
	client_stream= asio::ip::tcp::iostream("localhost", "21115");
	rid_ = 0;
  }

  virtual void TearDown() {
    server.reset();
    lua_close(L);
    L = 0;
	client_stream.clear();
	client_stream.close();
  }
  lua_State* L;
  lrdb::server server;
  asio::ip::tcp::iostream client_stream;

  std::function<void(const picojson::value&)> notify_handler;

  bool pause_;
  int rid_;

  picojson::value sync_request( const std::string& method,
      const picojson::value& param = picojson::value()) {
	  int rid = rid_++;
    client_stream << lrdb::message::request::serialize(rid, method, param)
                  << std::endl;

    if (client_stream.bad()) {
      picojson::value error_res;
      return error_res;
    }

    while (true) {
      std::string line;
      std::getline(client_stream, line, '\n');
      if (client_stream.bad()) {
        picojson::value error_res;
        return error_res;
      }
      picojson::value v;
      std::string err = picojson::parse(v, line);
      if (!err.empty()) {
        picojson::value error_res;
        return error_res;
      }
      const picojson::value& resid = lrdb::message::get_id(v);
      if (resid.is<double>() && resid.get<double>() == rid) {
        return v;
      } else {
        notify(v);
      }
    }
    picojson::value error_res;
    return error_res;
  }

  void notify(const picojson::value& v) {
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
      picojson::value v;
      std::string err = picojson::parse(v, line);
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

    picojson::object break_point;
    break_point["file"] = picojson::value(TEST_LUA_SCRIPT);
    break_point["line"] = picojson::value(5.);

    picojson::value res =
        sync_request( "add_breakpoint", picojson::value(break_point));
    res = sync_request( "get_breakpoints");

    res = sync_request( "get_stacktrace");

    sync_request( "continue");
    wait_for_paused();
    sync_request( "continue");
  });

  luaDofile(L, TEST_LUA_SCRIPT);
  server.exit();

  client.join();
}
TEST_F(DebugServerTest, ConnectTest2) {
  const char* TEST_LUA_SCRIPT = "../test/lua/test1.lua";

  std::thread client([&] {

    picojson::object break_point;
    break_point["file"] = picojson::value(TEST_LUA_SCRIPT);
    break_point["line"] = picojson::value(5.);

    picojson::value res =
        sync_request( "add_breakpoint", picojson::value(break_point));
    res = sync_request( "get_breakpoints");
    // std::cout << res.serialize() << std::endl;

    sync_request( "continue");
    wait_for_paused();

    res = sync_request( "get_local_variable");
    //		std::cout << res.serialize() << std::endl;
    res = sync_request( "get_upvalues");
    //		std::cout << res.serialize() << std::endl;
    picojson::object stack_var_req_param_obj;
    stack_var_req_param_obj["stack_no"] = picojson::value(0.);
    picojson::value stack_var_req_param =
        picojson::value(stack_var_req_param_obj);
    res = sync_request( "get_local_variable", stack_var_req_param);
    //		std::cout << res.serialize() << std::endl;
    res = sync_request( "get_upvalues", stack_var_req_param);
    //		std::cout << res.serialize() << std::endl;

    sync_request( "continue");
  });

  luaDofile(L, TEST_LUA_SCRIPT);
  server.exit();

  client.join();
}

TEST_F(DebugServerTest, ConnectTest4) {
  const char* TEST_LUA_SCRIPT = "../test/lua/test1.lua";

  std::thread client([&] {

    picojson::object break_point;
    break_point["file"] = picojson::value(TEST_LUA_SCRIPT);
    break_point["line"] = picojson::value(5.);

    picojson::value res =
        sync_request( "add_breakpoint", picojson::value(break_point));
    res = sync_request( "get_breakpoints");

    res = sync_request( "get_stacktrace");

    sync_request( "continue");
    wait_for_paused();
    sync_request( "continue");
  });

  luaDofile(L, TEST_LUA_SCRIPT);
  server.exit();

  client.join();
}


TEST_F(DebugServerTest, ConnectTest5) {
	const char* TEST_LUA_SCRIPT = "../test/lua/eval_test2.lua";

	std::thread client([&] {

		picojson::object break_point;
		break_point["file"] = picojson::value(TEST_LUA_SCRIPT);
		break_point["line"] = picojson::value(2.);

		picojson::value res =
			sync_request( "add_breakpoint", picojson::value(break_point));
		res = sync_request( "get_breakpoints");
		// std::cout << res.serialize() << std::endl;

		sync_request( "continue");
		wait_for_paused();

		res = sync_request( "get_local_variable");
		//		std::cout << res.serialize() << std::endl;
		res = sync_request( "get_upvalues");
		//		std::cout << res.serialize() << std::endl;
		picojson::object stack_var_req_param_obj;
		stack_var_req_param_obj["stack_no"] = picojson::value(0.);
		picojson::value stack_var_req_param =
			picojson::value(stack_var_req_param_obj);
		res = sync_request( "get_local_variable", stack_var_req_param);
		//		std::cout << res.serialize() << std::endl;
		res = sync_request( "get_upvalues", stack_var_req_param);
				std::cout << res.serialize() << std::endl;

		sync_request( "continue");
	});

	luaDofile(L, TEST_LUA_SCRIPT);
	server.exit();

	client.join();
}

int main(int argc, char* argv[]) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}