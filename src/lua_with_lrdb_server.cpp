#ifdef EMSCRIPTEN
#include <emscripten.h>
#endif
#include <iostream>

#include "lrdb/command_stream_stdstream.hpp"
#include "lrdb/server.hpp"

template <typename DebugServer>
int exec(const char* program, DebugServer& debug_server) {
  lua_State* L = luaL_newstate();
  luaL_openlibs(L);

  debug_server.reset(L);

  bool ret = luaL_dofile(L, program);
  if (ret != 0) {
    std::cerr << lua_tostring(L, -1);  // output error
  }

  debug_server.reset();

  lua_close(L);
  L = 0;
  return ret ? 1 : 0;
}

int main(int argc, char* argv[]) {
  int port = 0;
  const char* program = 0;

  // parse args
  for (int i = 1; i < argc; ++i) {
    if (argv[i][0] == '-') {
      if (i + 1 < argc) {
        if (strcmp(argv[i], "-p") || strcmp(argv[i], "--port")) {
          port = atoi(argv[i + 1]);
          ++i;
        }
      } else {
        return 1;  // invalid argument
      }
    } else {
      program = argv[i];
    }
  }
  if (!program) {
    return 1;
  }
  std::cout << LUA_COPYRIGHT << std::endl;
#ifdef EMSCRIPTEN
  EM_ASM_(
      {
        var path = require('path');
        var program_path = Pointer_stringify($0);
        var program_full_path = path.resolve(program_path);
        var token = path.parse(program_full_path);
        var cwd = path.relative(token.root, process.cwd());
        FS.mkdir('root');
        FS.mount(NODEFS, {root : token.root}, 'root');
        FS.chdir('root/' + cwd);
      },
      program);
#endif
  if (port == 0)  // if no port use std::cin and std::cout
  {
    lrdb::basic_server<lrdb::command_stream_stdstream> debug_server(std::cin,
                                                                    std::cout);
    return exec(program, debug_server);
  } else {
    lrdb::server debug_server(port);
    return exec(program, debug_server);
  }
}