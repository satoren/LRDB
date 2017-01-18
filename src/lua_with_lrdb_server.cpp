#include "lrdb/server.hpp"

int main(int argc, char* argv[]) {
  int port = 21110;  // default port
  const char* program = 0;

  // parse args
  for (int i = 1; i < argc; ++i) {
    if (argv[i][0] == '-') {
      if (i + 1 < argc) {
        if (strcmp(argv[i], "-p") || strcmp(argv[i], "--port")) {
          port = atoi(argv[i + 1]);
        }
      } else {
        return 1;  // invalid argument
      }
    } else {
      program = argv[i];
    }
  }

  lrdb::server debug_server(port);

  lua_State* L = luaL_newstate();
  luaL_openlibs(L);

  debug_server.reset(L);

  bool ret = luaL_dofile(L, program);

  debug_server.reset();

  lua_close(L);
  L = 0;
  return ret ? 1 : 0;
}