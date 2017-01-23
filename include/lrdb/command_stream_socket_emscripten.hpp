#pragma once

#include <emscripten.h>
#include <unistd.h>
#include <cstdint>

extern "C" void lrdb_server_socket_on_connect(void* ptr);
extern "C" void lrdb_server_socket_on_close(void* ptr);
extern "C" void lrdb_server_socket_on_error(void* ptr, const char* msg);
extern "C" void lrdb_server_socket_on_data(void* ptr, const char* data);

namespace lrdb {
// one to one server socket
class command_stream_socket {
 public:
  command_stream_socket(uint16_t port = 21110) {
    EM_ASM_(
        {
          var net = require('net');
          if (!Module.lrdb_server_table) {
            Module.lrdb_server_table = {}
          }
          var this_ptr = $0;
          var port = $1;
          Module.lrdb_server_table[this_ptr] = {};
          var server = Module.lrdb_server_table[this_ptr];
          server.close = function() {
            if (server.server) {
              server.server.close();
            }
            if (server.conn) {
              server.conn.end();
              server.conn = null;
            }
          };
          server.is_connect = function() {
            if (server.conn) {
              return true;
            }
            return false;
          };

          server.server = net.createServer(function(conn) {
            if (server.conn) {
              server.conn.end()
            }
            server.conn = conn;
            var result = Module.ccall(
                'lrdb_server_socket_on_connect',  // name of C function
                'void',                           // return type
                ['number'],                       // argument types
                [this_ptr]);

            conn.on('data', function(data) {

              var result = Module.ccall(
                  'lrdb_server_socket_on_data',  // name of C function
                  'void',                        // return type
                  [ 'number', "string" ],        // argument types
                  [ this_ptr, data.toString() ]);
            });
            conn.on('error', function(){});
            conn.on('close', function() {
              server.conn = null;
              var result = Module.ccall(
                  'lrdb_server_socket_on_close',  // name of C function
                  'void',                         // return type
                  ['number'],                     // argument types
                  [this_ptr]);
            });
          });
          server.server.listen(port);
        },
        this, port);
  }
  ~command_stream_socket() { close(); }
  void close() {
    EM_ASM_(
        {
          var server = Module.lrdb_server_table[$0];
          if (server) {
            server.close();
          }
        },
        this);
  }

  std::function<void(const std::string& data)> on_data;
  std::function<void()> on_connection;
  std::function<void()> on_close;
  std::function<void(const std::string&)> on_error;

  bool is_open() const {
    int r = EM_ASM_INT(
        {
          var server = Module.lrdb_server_table[$0];
          if (server) {
            return server.is_connect();
          }
          return 0;
        },
        this);
    return r;
  }
  void poll() { emscripten_sleep(1); }
  void run_one() { emscripten_sleep(100); }
  void wait_for_connection() {
    while (!is_open()) {
      emscripten_sleep(100);
    }
  }

  bool send_message(const std::string& m) {
    const std::string message = m + "\r\n";
    return EM_ASM_INT(
        {
          var server = Module.lrdb_server_table[$0];
          if (server && server.conn) {
            var data = Pointer_stringify($1);
            server.conn.write(data);
            return 1;
          }
          return 0;
        },
        this, message.c_str());
  }

  // private
  void on_con_() {
    if (on_connection) {
      on_connection();
    }
  }
  void on_clo_() {
    if (on_close) {
      on_close();
    }
  }
  void on_err_(const char* msg) {
    if (on_error) {
      on_error(msg);
    }
  }
  void on_dat_(const char* data) {
    if (on_data) {
      on_data(data);
    }
  }

 private:
};
}

extern "C" void lrdb_server_socket_on_connect(void* ptr) {
  lrdb::server_socket* this_ = reinterpret_cast<lrdb::server_socket*>(ptr);
  this_->on_con_();
}
extern "C" void lrdb_server_socket_on_close(void* ptr) {
  lrdb::server_socket* this_ = reinterpret_cast<lrdb::server_socket*>(ptr);
  this_->on_clo_();
}
extern "C" void lrdb_server_socket_on_error(void* ptr, const char* msg) {
  lrdb::server_socket* this_ = reinterpret_cast<lrdb::server_socket*>(ptr);
  this_->on_err_(msg);
}
extern "C" void lrdb_server_socket_on_data(void* ptr, const char* data) {
  lrdb::server_socket* this_ = reinterpret_cast<lrdb::server_socket*>(ptr);
  this_->on_dat_(data);
}
