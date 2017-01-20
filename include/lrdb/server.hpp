#pragma once

#include <memory>
#include <vector>

#ifdef LRDB_USE_BOOST_ASIO
#include <boost/asio.hpp>
#else
#define ASIO_STANDALONE
#include <asio.hpp>
#endif

#include "debug_command.hpp"
#include "debugger.hpp"
#include "message.hpp"

namespace lrdb {
#ifdef LRDB_USE_BOOST_ASIO
namespace asio {
using namespace boost;
}
#endif

#define LRDB_SERVER_VERSION "0.0.1"

class server {
 public:
  server(uint16_t port = 21110)
      : wait_for_connect_(true),
        endpoint_(asio::ip::tcp::v4(), port),
        acceptor_(io_service_, endpoint_),
        socket_(io_service_) {
    debugger_.set_pause_handler([&](debugger& debugger) {
      send_pause_status();
      while (debugger.paused() && socket_.is_open()) {
        io_service_.run_one();
      }
      send_message(message::notify::serialize("running"));
    });

    debugger_.set_tick_handler([&](debugger&) {
      io_service_.poll();
      while (wait_for_connect_ && !socket_.is_open()) {
        io_service_.run_one();
      }
    });

    debugger_.step_in();

    acceptor_.async_accept(socket_, [&](const asio::error_code& ec) {
      if (!ec) {
        connected_done();
      }
    });
  }

  ~server() {
    exit();
    acceptor_.close();
  }

  void reset(lua_State* L = 0) {
    debugger_.reset(L);
    if (!L) {
      exit();
    }
  }

  void exit() {
    send_message(message::notify::serialize("exit"));
    socket_.close();
  }

 private:
  void send_pause_status() {
    picojson::object pauseparam;
    pauseparam["reason"] = picojson::value(debugger_.pause_reason());
    send_message(
        message::notify::serialize("paused", picojson::value(pauseparam)));
  }
  void connected_done() {
    wait_for_connect_ = false;
    start_receive_commands();
  }
  void start_receive_commands() {
    asio::async_read_until(socket_, read_buffer_, "\n",
                           [&](const asio::error_code& ec, std::size_t) {
                             if (!ec) {
                               std::istream is(&read_buffer_);
                               std::string command;
                               std::getline(is, command);
                               execute_message(command);
                               start_receive_commands();
                             }
                           });
  }

  void send_message(const std::string& message) {
    asio::error_code ec;
    asio::write(socket_, asio::buffer(message + "\r\n"), ec);
    if (ec) {
      socket_.close();
      return;
    }
  }
  void execute_message(const std::string& message) {
    picojson::value req;
    std::string err = picojson::parse(req, message);
    if (err.empty()) {
      execute_request(req);
    }
  }

  void execute_request(picojson::value& req) {
    const std::string& method = message::get_method(req);
    const picojson::value& param = message::get_param(req);
    const picojson::value& reqid = message::get_id(req);

    try {
      picojson::value result;

#define DEBUG_COMMAND_TABLE(NAME)                    \
  if (method == #NAME) {                             \
    result = command::exec_##NAME(debugger_, param); \
  } else

      DEBUG_COMMAND_TABLE(step)
      DEBUG_COMMAND_TABLE(step_in)
      DEBUG_COMMAND_TABLE(continue)
      DEBUG_COMMAND_TABLE(pause)
      DEBUG_COMMAND_TABLE(add_breakpoint)
      DEBUG_COMMAND_TABLE(get_breakpoints)
      DEBUG_COMMAND_TABLE(clear_breakpoints)
      DEBUG_COMMAND_TABLE(get_stacktrace)
      DEBUG_COMMAND_TABLE(get_local_variable)
      DEBUG_COMMAND_TABLE(get_upvalues)
      DEBUG_COMMAND_TABLE(eval)
      DEBUG_COMMAND_TABLE(get_global)
      if (true) {
        throw std::runtime_error(method + " is not supported");
      }

#undef DEBUG_COMMAND_TABLE

      if (!reqid.is<picojson::null>()) {
        send_message(message::responce::serialize(reqid, result));
      }
    } catch (std::exception& e) {
      if (!reqid.is<picojson::null>()) {
        send_message(
            message::responce::serialize(reqid, std::string(e.what()), true));
      }
    }
  }

  bool wait_for_connect_;
  asio::io_service io_service_;
  debugger debugger_;

  asio::ip::tcp::endpoint endpoint_;
  asio::ip::tcp::acceptor acceptor_;
  asio::ip::tcp::socket socket_;
  asio::streambuf read_buffer_;
};
}