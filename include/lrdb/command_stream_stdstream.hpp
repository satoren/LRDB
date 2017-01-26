#include <condition_variable>
#include <deque>
#include <mutex>
#include <string>
#include <thread>

// experimental implementation

#define LRDB_IOSTREAM_PREFIX "lrdb_stream_message:"
namespace lrdb {

class command_stream_stdstream {
 public:
  command_stream_stdstream(std::istream& in, std::ostream& out)
      : end_(false), istream_(in), ostream_(out) {
    thread_ = std::thread([&] { read_thread(); });
  }
  ~command_stream_stdstream() { close(); }

  void close() {
    {
      std::unique_lock<std::mutex> lk(mutex_);
      end_ = true;
      cond_.notify_all();
    }
    ostream_ << 1;
    if (thread_.joinable()) {
      thread_.join();
    }
  }
  std::function<void(const std::string& data)> on_data;
  std::function<void()> on_connection;
  std::function<void()> on_close;
  std::function<void(const std::string&)> on_error;

  bool is_open() const { return true; }
  void poll() {
    std::string msg = pop_message();
    if (!msg.empty()) {
      on_data(msg);
    }
  }
  void run_one() {
    std::string msg = wait_message();
    if (!msg.empty()) {
      on_data(msg);
    }
  }
  void wait_for_connection() {}

  // sync
  bool send_message(const std::string& message) {
    ostream_ << (LRDB_IOSTREAM_PREFIX + message + "\r\n");
    return true;
  }

 private:
  std::string pop_message() {
    std::unique_lock<std::mutex> lk(mutex_);
    if (command_buffer_.empty()) {
      return "";
    }

    std::string message = std::move(command_buffer_.front());
    command_buffer_.pop_front();
    return message;
  }
  std::string wait_message() {
    std::unique_lock<std::mutex> lk(mutex_);
    while (command_buffer_.empty() && !end_) {
      cond_.wait(lk);
    }
    if (command_buffer_.empty()) {
      return "";
    }
    std::string message = std::move(command_buffer_.front());
    command_buffer_.pop_front();
    return message;
  }
  void push_message(std::string message) {
    std::unique_lock<std::mutex> lk(mutex_);
    command_buffer_.push_back(std::move(message));
    cond_.notify_one();
  }

  void read_thread() {
    std::unique_lock<std::mutex> lk(mutex_);
    std::string msg;
    while (!end_) {
      mutex_.unlock();
      std::getline(istream_, msg);
      if (msg.find(LRDB_IOSTREAM_PREFIX) == 0) {
        push_message(msg.substr(sizeof(LRDB_IOSTREAM_PREFIX)));
      }
      mutex_.lock();
    }
  }

  bool end_;
  std::istream& istream_;
  std::ostream& ostream_;
  std::deque<std::string> command_buffer_;
  std::mutex mutex_;
  std::condition_variable cond_;
  std::thread thread_;
};
}
