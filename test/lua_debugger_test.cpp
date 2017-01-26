
#include <iostream>

#include "kaguya.hpp"
#include "lrdb/debugger.hpp"

#include "gtest/gtest.h"

using picojson::object;
using picojson::array;
namespace {

class DebuggerTest : public ::testing::Test {
 protected:
  DebuggerTest() {}

  virtual ~DebuggerTest() {}

  virtual void SetUp() {
    L = luaL_newstate();
    luaL_openlibs(L);
    debugger.reset(L);
  }

  virtual void TearDown() {
    debugger.reset();
    lua_close(L);
    L = 0;
  }
  lua_State* L;
  lrdb::debugger debugger;
};
void luaDofile(lua_State* L, const char* file) {
  int err = luaL_dofile(L, file);
  const char* errorstring = 0;
  if (err) {
    errorstring = lua_tostring(L, -1);
  }
  ASSERT_STREQ(0, errorstring);
}
}
TEST_F(DebuggerTest, BreakPointTest1) {
  const char* TEST_LUA_SCRIPT = "../test/lua/test1.lua";

  debugger.add_breakpoint(TEST_LUA_SCRIPT, 3);

  bool breaked = false;
  debugger.set_pause_handler([&](lrdb::debugger& debugger) {
    auto* breakpoint = debugger.current_breakpoint();
    ASSERT_TRUE(breakpoint);
    ASSERT_EQ(TEST_LUA_SCRIPT, breakpoint->file);
    ASSERT_EQ(3, breakpoint->line);
    ASSERT_EQ(1U, breakpoint->hit_count);

    ASSERT_EQ(3, debugger.current_debug_info().currentline());

    auto callstack = debugger.get_call_stack();
    ASSERT_TRUE(callstack.size() > 0);

    breaked = true;
  });

  luaDofile(L, TEST_LUA_SCRIPT);

  ASSERT_TRUE(breaked);
}
TEST_F(DebuggerTest, BreakPointTestCoroutine) {
  const char* TEST_LUA_SCRIPT = "../test/lua/break_coroutine_test1.lua";

  debugger.add_breakpoint(TEST_LUA_SCRIPT, 3);

  std::vector<int> break_line_numbers;
  debugger.set_pause_handler([&](lrdb::debugger& debugger) {
    break_line_numbers.push_back(debugger.current_debug_info().currentline());
    auto* breakpoint = debugger.current_breakpoint();
    ASSERT_TRUE(breakpoint);
    ASSERT_EQ(TEST_LUA_SCRIPT, breakpoint->file);
    ASSERT_EQ(3, breakpoint->line);
    ASSERT_EQ(1U, breakpoint->hit_count);

    ASSERT_EQ(3, debugger.current_debug_info().currentline());
    ASSERT_EQ(2, debugger.current_debug_info().linedefined());
    ASSERT_EQ(7, debugger.current_debug_info().lastlinedefined());

    auto callstack = debugger.get_call_stack();
    ASSERT_TRUE(callstack.size() > 0);
  });

  luaDofile(L, TEST_LUA_SCRIPT);
  std::vector<int> require_line_number = {3};
  ASSERT_EQ(require_line_number, break_line_numbers);
}
TEST_F(DebuggerTest, StepInTestCoroutine) {
  const char* TEST_LUA_SCRIPT = "../test/lua/break_coroutine_test1.lua";
  std::vector<int> break_line_numbers;
  debugger.step_in();
  debugger.set_pause_handler([&](lrdb::debugger& debugger) {
    break_line_numbers.push_back(debugger.current_debug_info().currentline());
    auto* breakpoint = debugger.current_breakpoint();
    ASSERT_TRUE(!breakpoint);
    debugger.step_in();
  });

  luaDofile(L, TEST_LUA_SCRIPT);
  std::vector<int> require_line_number = {1, 7, 2, 9, 10, 3, 4, 5, 6, 7};
  ASSERT_EQ(require_line_number, break_line_numbers);
}
TEST_F(DebuggerTest, StepOverTest) {
  const char* TEST_LUA_SCRIPT = "../test/lua/step_over_test1.lua";
  debugger.step_in();

  std::vector<int> break_line_numbers;

  debugger.set_pause_handler([&](lrdb::debugger& debugger) {
    break_line_numbers.push_back(debugger.current_debug_info().currentline());
    auto* breakpoint = debugger.current_breakpoint();
    ASSERT_TRUE(!breakpoint);
    debugger.step_over();
  });

  luaDofile(L, TEST_LUA_SCRIPT);
  std::vector<int> require_line_number = {1, 7, 2, 9};

  //workaround for luajit
  if (require_line_number.size() < break_line_numbers.size())
  {
	  break_line_numbers.pop_back();
  }

  ASSERT_EQ(require_line_number, break_line_numbers);
}
TEST_F(DebuggerTest, StepInTest) {
  const char* TEST_LUA_SCRIPT = "../test/lua/step_in_test1.lua";
  std::vector<int> break_line_numbers;
  debugger.step_in();
  debugger.set_pause_handler([&](lrdb::debugger& debugger) {
    break_line_numbers.push_back(debugger.current_debug_info().currentline());
    auto* breakpoint = debugger.current_breakpoint();
    ASSERT_TRUE(!breakpoint);
    debugger.step_in();
  });

  luaDofile(L, TEST_LUA_SCRIPT);
  std::vector<int> require_line_number = {1, 7, 2, 9, 3, 4, 5, 6, 7};

  //workaround for luajit
  if (require_line_number.size() < break_line_numbers.size())
  {
	  break_line_numbers.pop_back();
  }
  ASSERT_EQ(require_line_number, break_line_numbers);
}
TEST_F(DebuggerTest, StepOutTest) {
  const char* TEST_LUA_SCRIPT = "../test/lua/step_out_test1.lua";
  debugger.add_breakpoint(TEST_LUA_SCRIPT, 3);

  std::vector<int> break_line_numbers;
  debugger.set_pause_handler([&](lrdb::debugger& debugger) {
    break_line_numbers.push_back(debugger.current_debug_info().currentline());
    debugger.step_out();
  });

  luaDofile(L, TEST_LUA_SCRIPT);
  std::vector<int> require_line_number = {3, 11};
  ASSERT_EQ(require_line_number, break_line_numbers);
}
TEST_F(DebuggerTest, TickTest1) {
  const char* TEST_LUA_SCRIPT = "../test/lua/test1.lua";

  int tick_count = 0;
  debugger.set_tick_handler([&](lrdb::debugger&) { tick_count++; });

  luaDofile(L, TEST_LUA_SCRIPT);

  ASSERT_TRUE(tick_count > 0);
}

TEST_F(DebuggerTest, EvalTest1) {
  const char* TEST_LUA_SCRIPT = "../test/lua/eval_test1.lua";

  debugger.add_breakpoint(TEST_LUA_SCRIPT, 4);

  debugger.set_pause_handler([&](lrdb::debugger& debugger) {
    std::vector<picojson::value> ret = debugger.current_debug_info().eval(
        "return arg,value,local_value,local_value3");

    ASSERT_EQ(4U, ret.size());

    ASSERT_TRUE(ret[0].is<double>());
    ASSERT_EQ(4, ret[0].get<double>());
    ASSERT_TRUE(ret[1].is<double>());
    ASSERT_EQ(1, ret[1].get<double>());
    ASSERT_TRUE(ret[2].is<double>());
    ASSERT_EQ(2, ret[2].get<double>());
    ASSERT_TRUE(ret[3].is<picojson::null>());
  });

  luaDofile(L, TEST_LUA_SCRIPT);
}

TEST_F(DebuggerTest, EvalTest2) {
  const char* TEST_LUA_SCRIPT = "../test/lua/eval_test2.lua";

  debugger.add_breakpoint(TEST_LUA_SCRIPT, 2);

  debugger.set_pause_handler([&](lrdb::debugger& debugger) {
    std::vector<picojson::value> ret =
        debugger.current_debug_info().eval("return _G", true, false, false, 1);

    ASSERT_EQ(1U, ret.size());
    ASSERT_TRUE(ret[0].is<object>());
    ASSERT_LT(0U, ret[0].get<object>().size());

    std::vector<picojson::value> ret2 = debugger.current_debug_info().eval(
        "return _ENV._G", false, true, false, 1);
    ASSERT_EQ(ret, ret2);
    ASSERT_TRUE(ret[0].is<object>());
    ASSERT_LT(0U, ret[0].get<object>().size());

    ret = debugger.current_debug_info().eval("return _ENV['envvar']", false,
                                             false, true, 1);
    ASSERT_EQ(1U, ret.size());
    ASSERT_TRUE(ret[0].is<double>());
    ASSERT_EQ(5456, ret[0].get<double>());
  });

  luaDofile(L, TEST_LUA_SCRIPT);
}

TEST_F(DebuggerTest, GetLocalTest1) {
  const char* TEST_LUA_SCRIPT = "../test/lua/get_local_var_test1.lua";

  debugger.add_breakpoint(TEST_LUA_SCRIPT, 5);

  debugger.set_pause_handler([&](lrdb::debugger& debugger) {

    lrdb::debug_info::local_vars_type localvars =
        debugger.current_debug_info().get_local_vars();
    ASSERT_LE(2U, localvars.size());
    ASSERT_EQ("local_value1", localvars[0].first);
    ASSERT_EQ(1, localvars[0].second.get<double>());

    bool ret = debugger.current_debug_info().set_local_var(
        "local_value1", picojson::value("ab"));

    ASSERT_TRUE(ret);
    localvars = debugger.current_debug_info().get_local_vars();

    ASSERT_LE(2U, localvars.size());
    ASSERT_EQ("local_value1", localvars[0].first);
    ASSERT_EQ("ab", localvars[0].second.get<std::string>());

    std::vector<picojson::value> eret =
        debugger.current_debug_info().eval("return local_value1");

    ASSERT_EQ(1U, eret.size());
    ASSERT_TRUE(eret[0].is<std::string>());
    ASSERT_EQ("ab", eret[0].get<std::string>());

    eret = debugger.current_debug_info().eval("return _G");
    ASSERT_EQ(1U, eret.size());
    ASSERT_TRUE(eret[0].is<picojson::object>());

  });

  luaDofile(L, TEST_LUA_SCRIPT);
}
TEST_F(DebuggerTest, GetVaArgTest1) {
  const char* TEST_LUA_SCRIPT = "../test/lua/vaarg_test1.lua";

  debugger.add_breakpoint(TEST_LUA_SCRIPT, 5);

  debugger.set_pause_handler([&](lrdb::debugger& debugger) {

    lrdb::debug_info::local_vars_type localvars =
        debugger.current_debug_info().get_local_vars(1);

#if LUA_VERSION_NUM >= 502
	ASSERT_LE(4U, localvars.size());
#else
    ASSERT_LE(3U, localvars.size());
#endif
    std::map<std::string, picojson::value> varmap;
    for (auto& v : localvars) {
      varmap[v.first] = v.second;
    }
    ASSERT_LE(1, varmap.count("v1"));
    ASSERT_LE(1, varmap.count("local_value1"));
    ASSERT_LE(1, varmap.count("local_value2"));
#if LUA_VERSION_NUM >= 502
    ASSERT_LE(1, varmap.count("(*vararg)"));
#endif

    ASSERT_EQ(2, varmap["v1"].get<double>());
    ASSERT_EQ(1, varmap["local_value1"].get<double>());
    ASSERT_EQ("abc", varmap["local_value2"].get<std::string>());
#if LUA_VERSION_NUM >= 502
    ASSERT_TRUE(varmap["(*vararg)"].is<array>());

    std::vector<picojson::value> eret = debugger.current_debug_info().eval(
        "return table.unpack(_ENV[\"(*vararg)\"])");
    ASSERT_EQ(2U, eret.size());
    ASSERT_EQ(1, eret[0].get<double>());
    ASSERT_EQ(3, eret[1].get<double>());
    bool vaarg = debugger.current_debug_info().is_variadic_arg();
    ASSERT_TRUE(vaarg);
#endif
  });

  luaDofile(L, TEST_LUA_SCRIPT);
}

TEST_F(DebuggerTest, ConditionBreakPointTest) {
  const char* TEST_LUA_SCRIPT = "../test/lua/loop_test.lua";

  debugger.add_breakpoint(TEST_LUA_SCRIPT, 11, "i==4");

  std::vector<int> break_line_numbers;
  debugger.set_pause_handler([&](lrdb::debugger& debugger) {
    break_line_numbers.push_back(debugger.current_debug_info().currentline());
    auto* breakpoint = debugger.current_breakpoint();
    ASSERT_TRUE(breakpoint);
    ASSERT_EQ(TEST_LUA_SCRIPT, breakpoint->file);
    ASSERT_EQ(11, breakpoint->line);
    ASSERT_EQ(1, breakpoint->hit_count);
    debugger.unpause();
  });

  luaDofile(L, TEST_LUA_SCRIPT);

  std::vector<int> require_line_number = {11};
  ASSERT_EQ(require_line_number, break_line_numbers);
}

TEST_F(DebuggerTest, HitConditionBreakPointTest) {
  const char* TEST_LUA_SCRIPT = "../test/lua/loop_test.lua";

  debugger.add_breakpoint(TEST_LUA_SCRIPT, 11, "", "10");

  std::vector<int> break_line_numbers;
  debugger.set_pause_handler([&](lrdb::debugger& debugger) {
    break_line_numbers.push_back(debugger.current_debug_info().currentline());
    auto* breakpoint = debugger.current_breakpoint();
    ASSERT_TRUE(breakpoint);
    ASSERT_EQ(TEST_LUA_SCRIPT, breakpoint->file);
    ASSERT_EQ(11, breakpoint->line);
    ASSERT_EQ(10, breakpoint->hit_count);
    debugger.unpause();
  });

  luaDofile(L, TEST_LUA_SCRIPT);

  std::vector<int> require_line_number = {11};
  ASSERT_EQ(require_line_number, break_line_numbers);
}
TEST_F(DebuggerTest, HitConditionBreakPointTest2) {
  const char* TEST_LUA_SCRIPT = "../test/lua/loop_test.lua";

  debugger.add_breakpoint(TEST_LUA_SCRIPT, 11, "", "7");

  std::vector<int> break_line_numbers;
  debugger.set_pause_handler([&](lrdb::debugger& debugger) {
    break_line_numbers.push_back(debugger.current_debug_info().currentline());
    auto* breakpoint = debugger.current_breakpoint();
    ASSERT_TRUE(breakpoint);
    ASSERT_EQ(TEST_LUA_SCRIPT, breakpoint->file);
    ASSERT_EQ(11, breakpoint->line);
    debugger.unpause();
  });

  luaDofile(L, TEST_LUA_SCRIPT);

  std::vector<int> require_line_number = {11, 11, 11, 11};
  ASSERT_EQ(require_line_number, break_line_numbers);
}
TEST_F(DebuggerTest, HitConditionBreakPointTestGreaterThenOrEqual) {
	const char* TEST_LUA_SCRIPT = "../test/lua/loop_test.lua";

	debugger.add_breakpoint(TEST_LUA_SCRIPT, 11, "", ">=7");

	std::vector<int> break_line_numbers;
	debugger.set_pause_handler([&](lrdb::debugger& debugger) {
		break_line_numbers.push_back(debugger.current_debug_info().currentline());
		auto* breakpoint = debugger.current_breakpoint();
		ASSERT_TRUE(breakpoint);
		ASSERT_EQ(TEST_LUA_SCRIPT, breakpoint->file);
		ASSERT_EQ(11, breakpoint->line);
		debugger.unpause();
	});

	luaDofile(L, TEST_LUA_SCRIPT);

	std::vector<int> require_line_number = { 11, 11, 11, 11 };
	ASSERT_EQ(require_line_number, break_line_numbers);
}
TEST_F(DebuggerTest, HitConditionBreakPointTestGreaterThen) {
	const char* TEST_LUA_SCRIPT = "../test/lua/loop_test.lua";

	debugger.add_breakpoint(TEST_LUA_SCRIPT, 11, "", ">7");

	std::vector<int> break_line_numbers;
	debugger.set_pause_handler([&](lrdb::debugger& debugger) {
		break_line_numbers.push_back(debugger.current_debug_info().currentline());
		auto* breakpoint = debugger.current_breakpoint();
		ASSERT_TRUE(breakpoint);
		ASSERT_EQ(TEST_LUA_SCRIPT, breakpoint->file);
		ASSERT_EQ(11, breakpoint->line);
		debugger.unpause();
	});

	luaDofile(L, TEST_LUA_SCRIPT);

	std::vector<int> require_line_number = { 11, 11, 11 };
	ASSERT_EQ(require_line_number, break_line_numbers);
}
TEST_F(DebuggerTest, HitConditionBreakPointTestLessThenOrEqual) {
	const char* TEST_LUA_SCRIPT = "../test/lua/loop_test.lua";

	debugger.add_breakpoint(TEST_LUA_SCRIPT, 11, "", "<=7");

	std::vector<int> break_line_numbers;
	debugger.set_pause_handler([&](lrdb::debugger& debugger) {
		break_line_numbers.push_back(debugger.current_debug_info().currentline());
		auto* breakpoint = debugger.current_breakpoint();
		ASSERT_TRUE(breakpoint);
		ASSERT_EQ(TEST_LUA_SCRIPT, breakpoint->file);
		ASSERT_EQ(11, breakpoint->line);
		ASSERT_GE(7, breakpoint->hit_count);
		debugger.unpause();
	});

	luaDofile(L, TEST_LUA_SCRIPT);

	std::vector<int> require_line_number = { 11,11,11,11,11, 11, 11 };
	ASSERT_EQ(require_line_number, break_line_numbers);
}
TEST_F(DebuggerTest, HitConditionBreakPointTestLessThen) {
	const char* TEST_LUA_SCRIPT = "../test/lua/loop_test.lua";

	debugger.add_breakpoint(TEST_LUA_SCRIPT, 11, "", "<7");

	std::vector<int> break_line_numbers;
	debugger.set_pause_handler([&](lrdb::debugger& debugger) {
		break_line_numbers.push_back(debugger.current_debug_info().currentline());
		auto* breakpoint = debugger.current_breakpoint();
		ASSERT_TRUE(breakpoint);
		ASSERT_EQ(TEST_LUA_SCRIPT, breakpoint->file);
		ASSERT_EQ(11, breakpoint->line);
		ASSERT_GT(7, breakpoint->hit_count);
		debugger.unpause();
	});

	luaDofile(L, TEST_LUA_SCRIPT);

	std::vector<int> require_line_number = { 11,11,11,11, 11, 11 };
	ASSERT_EQ(require_line_number, break_line_numbers);
}
TEST_F(DebuggerTest, HitConditionBreakPointTestEqual) {
	const char* TEST_LUA_SCRIPT = "../test/lua/loop_test.lua";

	debugger.add_breakpoint(TEST_LUA_SCRIPT, 11, "", "%2");

	std::vector<int> break_line_numbers;
	debugger.set_pause_handler([&](lrdb::debugger& debugger) {
		break_line_numbers.push_back(debugger.current_debug_info().currentline());
		auto* breakpoint = debugger.current_breakpoint();
		ASSERT_TRUE(breakpoint);
		ASSERT_EQ(TEST_LUA_SCRIPT, breakpoint->file);
		ASSERT_EQ(11, breakpoint->line);
		ASSERT_EQ(1, breakpoint->hit_count % 2);
		debugger.unpause();
	});

	luaDofile(L, TEST_LUA_SCRIPT);

	std::vector<int> require_line_number = { 11,11,11, 11, 11 };
	ASSERT_EQ(require_line_number, break_line_numbers);
}


TEST_F(DebuggerTest, RemoveBreakPointTest1) {
  const char* TEST_LUA_SCRIPT = "../test/lua/loop_test.lua";

  debugger.add_breakpoint(TEST_LUA_SCRIPT, 11);

  std::vector<int> break_line_numbers;
  debugger.set_pause_handler([&](lrdb::debugger& debugger) {
    break_line_numbers.push_back(debugger.current_debug_info().currentline());
    auto* breakpoint = debugger.current_breakpoint();
    ASSERT_TRUE(breakpoint);
    ASSERT_EQ(TEST_LUA_SCRIPT, breakpoint->file);
    ASSERT_EQ(11, breakpoint->line);
    if (3 < breakpoint->hit_count) {
      debugger.clear_breakpoints(TEST_LUA_SCRIPT);
    }
    debugger.unpause();
  });

  luaDofile(L, TEST_LUA_SCRIPT);

  std::vector<int> require_line_number = {11, 11, 11, 11};
  ASSERT_EQ(require_line_number, break_line_numbers);
}

TEST_F(DebuggerTest, RemoveBreakPointTest2) {
  const char* TEST_LUA_SCRIPT = "../test/lua/loop_test.lua";

  debugger.add_breakpoint(TEST_LUA_SCRIPT, 11);

  std::vector<int> break_line_numbers;
  debugger.set_pause_handler([&](lrdb::debugger& debugger) {
    break_line_numbers.push_back(debugger.current_debug_info().currentline());
    auto* breakpoint = debugger.current_breakpoint();
    ASSERT_TRUE(breakpoint);
    ASSERT_EQ(TEST_LUA_SCRIPT, breakpoint->file);
    ASSERT_EQ(11, breakpoint->line);
    if (3 < breakpoint->hit_count) {
      debugger.clear_breakpoints(TEST_LUA_SCRIPT, 11);
    }
    debugger.unpause();
  });

  luaDofile(L, TEST_LUA_SCRIPT);

  std::vector<int> require_line_number = {11, 11, 11, 11};
  ASSERT_EQ(require_line_number, break_line_numbers);
}
TEST_F(DebuggerTest, RemoveBreakPointTest3) {
  const char* TEST_LUA_SCRIPT = "../test/lua/loop_test.lua";

  debugger.add_breakpoint(TEST_LUA_SCRIPT, 11);

  std::vector<int> break_line_numbers;
  debugger.set_pause_handler([&](lrdb::debugger& debugger) {
    break_line_numbers.push_back(debugger.current_debug_info().currentline());
    auto* breakpoint = debugger.current_breakpoint();
    ASSERT_TRUE(breakpoint);
    ASSERT_EQ(TEST_LUA_SCRIPT, breakpoint->file);
    ASSERT_EQ(11, breakpoint->line);
    if (3 < breakpoint->hit_count) {
      debugger.clear_breakpoints(TEST_LUA_SCRIPT);
    }
    debugger.unpause();
  });

  luaDofile(L, TEST_LUA_SCRIPT);

  std::vector<int> require_line_number = {11, 11, 11, 11};
  ASSERT_EQ(require_line_number, break_line_numbers);
}

TEST_F(DebuggerTest, GetEnvDataTest1) {
  const char* TEST_LUA_SCRIPT = "../test/lua/loop_test.lua";

  debugger.set_pause_handler([&](lrdb::debugger& debugger) {
    auto env = debugger.current_debug_info().eval("return envvar");
    ASSERT_EQ(1, env.size());
    ASSERT_TRUE(env[0].is<double>());
    ASSERT_EQ(2, env[0].get<double>());
    debugger.unpause();
  });

  debugger.step_in();
  debugger.add_breakpoint(TEST_LUA_SCRIPT, 6);

  kaguya::State state(L);

  kaguya::LuaTable envtable = state.newTable();
  envtable["envvar"] = 2;
  bool ret = state.dofile(TEST_LUA_SCRIPT, envtable);
  ASSERT_TRUE(ret);
}

TEST_F(DebuggerTest, GetGlobalTest) {
  const char* TEST_LUA_SCRIPT = "../test/lua/test1.lua";

  debugger.add_breakpoint(TEST_LUA_SCRIPT, 3);

  debugger.set_pause_handler([&](lrdb::debugger& debugger) {
    auto global = debugger.get_global_table();
    ASSERT_TRUE(global.is<picojson::object>());
    ASSERT_TRUE(global.get<picojson::object>().size());
  });

  luaDofile(L, TEST_LUA_SCRIPT);
}
int main(int argc, char* argv[]) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}