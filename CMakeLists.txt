set(CMAKE_LEGACY_CYGWIN_WIN32 0)
cmake_minimum_required(VERSION 2.6)
project(LRDB)

include(cmake/ReduceDependencies.cmake)

include(cmake/FindLua.cmake)

include_directories(SYSTEM ${LUA_INCLUDE_DIRS})
link_directories(${LUA_LIBRARY_DIRS})

file(GLOB HEADER_FILE RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}  include/*.hpp)

include_directories("${CMAKE_CURRENT_SOURCE_DIR}/third_party/asio/asio/include")

include_directories("${CMAKE_CURRENT_SOURCE_DIR}/third_party/picojson")


if(ADDITIONAL_INCLUDE_PATH)
  include_directories(SYSTEM ${ADDITIONAL_INCLUDE_PATH})
endif()

include_directories("${CMAKE_CURRENT_SOURCE_DIR}/include")

if(NOT MSVC)#-Wall nonsense on MSVC
  add_definitions(-Wall -W)
  add_definitions(-pedantic)
  add_definitions(-Wno-variadic-macros)
  add_definitions ("-Wno-unused-local-typedefs")
  add_definitions ("-Wno-unknown-warning-option")
  add_definitions("-std=c++11")
endif(NOT MSVC)


if(${CMAKE_CXX_COMPILER_ID} STREQUAL "GNU")
  add_definitions ("-fno-strict-overflow")
endif()

add_executable(lua_with_lrdb_server src/lua_with_lrdb_server.cpp ${HEADER_FILE})
target_link_libraries(lua_with_lrdb_server ${LUA_LIBRARIES})

set(CMAKE_INSTALL_PREFIX ${CMAKE_CURRENT_SOURCE_DIR})
if(WIN32)
install(TARGETS lua_with_lrdb_server
        RUNTIME DESTINATION vscode_extension/out/bin/windows)
elseif(APPLE)
install(TARGETS lua_with_lrdb_server
        RUNTIME DESTINATION vscode_extension/out/bin/macos)
elseif(UNIX)
target_link_libraries(lua_with_lrdb_server -lpthread)

install(TARGETS lua_with_lrdb_server
        RUNTIME DESTINATION vscode_extension/out/bin/linux)
else(WIN32)
message(FATAL_ERROR "Unsupported operation system")
endif(WIN32)

enable_testing()

if(COVERAGE)
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -O0 -Wall -Woverloaded-virtual -Wwrite-strings -fprofile-arcs -ftest-coverage -coverage -fno-inline -fno-inline-small-functions -fno-default-inline")
SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -O0 -Wall -fprofile-arcs -ftest-coverage -coverage -fno-inline -fno-inline-small-functions -fno-default-inline")
SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fprofile-arcs -ftest-coverage -coverage -fno-inline -fno-inline-small-functions -fno-default-inline")
endif(COVERAGE)

include_directories("${CMAKE_CURRENT_SOURCE_DIR}/third_party/googletest" "${CMAKE_CURRENT_SOURCE_DIR}/third_party/googletest/include")
set(GTEST_SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/third_party/googletest/src/gtest-all.cc)

add_executable(lua_debugger_test test/lua_debugger_test.cpp ${HEADER_FILE} ${GTEST_SOURCE})
target_link_libraries(lua_debugger_test ${LUA_LIBRARIES})

add_executable(lrdb_server_test test/lrdb_server_test.cpp test/lrdb_server_test1.cpp ${HEADER_FILE} ${GTEST_SOURCE})
target_link_libraries(lrdb_server_test ${LUA_LIBRARIES})

if(WIN32)

elseif(APPLE)

elseif(UNIX)
target_link_libraries(lua_debugger_test -lpthread)
target_link_libraries(lrdb_server_test -lpthread)
else(WIN32)
message(FATAL_ERROR "Unsupported operation system")
endif(WIN32)


add_test(
  NAME lua_debugger_test
  COMMAND $<TARGET_FILE:lua_debugger_test>
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/test)

add_test(
  NAME lrdb_server_test
  COMMAND $<TARGET_FILE:lrdb_server_test>
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/test)
