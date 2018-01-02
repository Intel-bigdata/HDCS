#ifndef HDCS_COMMON_HEARTBEAT_H
#define HDCS_COMMON_HEARTBEAT_H
#include <stdexcept>
#include <chrono>             // std::chrono::seconds
#include <mutex>              // std::mutex, std::unique_lock
#include <condition_variable> // std::condition_variable, std::cv_status
#include "common/AioCompletionImp.h"

namespace hdcs {

typedef uint8_t HeartBeatStat;
#define HEARTBEAT_STAT_OK      0XE0
#define HEARTBEAT_STAT_TIMEOUT 0XE1

struct HeartBeatOpts {
  uint64_t timeout_nanoseconds;
  uint64_t heartbeat_interval_nanoseconds;
  HeartBeatOpts(uint64_t timeout_nanoseconds, uint64_t heartbeat_interval_nanoseconds):
    timeout_nanoseconds(timeout_nanoseconds),
    heartbeat_interval_nanoseconds(heartbeat_interval_nanoseconds) {}
};

typedef uint8_t HEARTBEAT_MSG_TYPE;
#define HDCS_HEARTBEAT_PING  0XE2
#define HDCS_HEARTBEAT_REPLY 0XE3
class HDCS_HEARTBEAT_MSG {
public:
  HDCS_HEARTBEAT_MSG (HEARTBEAT_MSG_TYPE type) {
    data_.type = type;
  }

  ~HDCS_HEARTBEAT_MSG() {
  }

  char* data() {
    return (char*)&data_;
  }

  uint64_t size() {
    return sizeof(HDCS_HEARTBEAT_MSG_T);
  }

private:
  struct HDCS_HEARTBEAT_MSG_T {
    HEARTBEAT_MSG_TYPE type;
  } data_;
};

class HeartBeat {

private:

  HeartBeatOpts *hb_opts;
  std::shared_ptr<SafeTimer> event_timer;
  networking::Connection* conn;
  HeartBeatStat stat;
  AioCompletion* timeout_event;
  std::shared_ptr<AioCompletion> error_handler;
  std::string hb_msg;
  //std::mutex stat_mutex;

public:

  HeartBeat(std::string addr, std::string port,
            std::shared_ptr<AioCompletion> error_handler,
            HeartBeatOpts *hb_opts,
            std::shared_ptr<SafeTimer> event_timer):
            hb_opts(hb_opts),
            event_timer(event_timer),
            stat(HEARTBEAT_STAT_OK),
            timeout_event(nullptr),
            error_handler(error_handler) {
    conn = new networking::Connection([&](void* p, std::string s){request_handler(p, s);}, 1, 1);
    conn->connect(addr, port);
    heartbeat_ping();
  }

  ~HeartBeat() {
    event_timer->cancel_event(timeout_event);
    conn->close();
  }

  void request_handler(void* args, std::string reply_data) {
    //check msg
    // remove timeout then at a new timer event
    if (timeout_event) {
      event_timer->cancel_event(timeout_event);
    }
    heartbeat_ping();
  }

  void heartbeat_ping() {
    timeout_event = new AioCompletionImp([&](ssize_t r){
      // send out HeartBeat Msg
      HDCS_HEARTBEAT_MSG msg_content(HDCS_HEARTBEAT_PING);
      conn->aio_communicate(std::move(std::string(msg_content.data(), msg_content.size())));
      //printf("Heartbeat Msg sent out.\n");
    });
    event_timer->add_event_after(hb_opts->heartbeat_interval_nanoseconds, timeout_event);
      // add a timeout check event
      timeout_event = new AioCompletionImp([&](ssize_t r){
        //when this event happens, means HeartBeat timeout
        stat = HEARTBEAT_STAT_TIMEOUT;
        //printf("Heartbeat Msg no reply, timeout.\n");
        //notify others
        error_handler->complete(0);
      });
      event_timer->add_event_after(hb_opts->timeout_nanoseconds + hb_opts->heartbeat_interval_nanoseconds, timeout_event);
  }
};
}
#endif
