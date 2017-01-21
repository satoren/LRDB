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

// one to one server socket
class server_socket {
 public:
  server_socket(uint16_t port = 21110)
      : endpoint_(asio::ip::tcp::v4(), port),
        acceptor_(io_service_, endpoint_),
        socket_(io_service_) {
    acceptor_.async_accept(socket_, [&](const asio::error_code& ec) {
      if (!ec) {
        connected_done();
      } else {
        if (on_error) {
          on_error(ec.message());
        }
      }
    });
  }

  ~server_socket() {
    close();
    acceptor_.close();
  }

  void close() {
    socket_.close();
    if (on_close) {
      on_close();
    }
  }

  std::function<void(const std::string& data)> on_data;
  std::function<void()> on_connection;
  std::function<void()> on_close;
  std::function<void(const std::string&)> on_error;

  bool is_open() const { return socket_.is_open(); }
  void poll() { io_service_.poll(); }
  void run_one() { io_service_.run_one(); }
  void wait_for_connection() {
    while (!is_open()) {
      io_service_.run_one();
    }
  }

  // sync
  bool send_message(const std::string& message) {
    asio::error_code ec;
    asio::write(socket_, asio::buffer(message), ec);
    if (ec) {
      if (on_error) {
        on_error(ec.message());
      }
      socket_.close();
      return true;
    }
    return false;
  }

 private:
  void connected_done() {
    if (on_connection) {
      on_connection();
    }
    start_receive_commands();
  }
  void start_receive_commands() {
    asio::async_read_until(socket_, read_buffer_, "\n",
                           [&](const asio::error_code& ec, std::size_t) {
                             if (!ec) {
                               std::istream is(&read_buffer_);
                               std::string command;
                               std::getline(is, command);
                               if (on_data) {
                                 on_data(command);
                               }
                               start_receive_commands();
                             } else {
                               if (on_error) {
                                 on_error(ec.message());
                               }
                             }
                           });
  }

  asio::io_service io_service_;
  asio::ip::tcp::endpoint endpoint_;
  asio::ip::tcp::acceptor acceptor_;
  asio::ip::tcp::socket socket_;
  asio::streambuf read_buffer_;
};
}