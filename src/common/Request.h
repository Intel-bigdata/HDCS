// Copyright [2017] <Intel>
#ifndef HDCS_REQUEST_H
#define HDCS_REQUEST_H

#include "common/AioCompletion.h"
#include <mutex>
#include <atomic>
#include "common/Log.h"

namespace hdcs {

typedef uint8_t IO_TYPE;
#define IO_TYPE_READ    0xF0
#define IO_TYPE_WRITE   0xF1
#define IO_TYPE_FLUSH   0xF2
#define IO_TYPE_DISCARD 0xF3
#define IO_TYPE_DEMOTE_CACHE 0xF4
#define IO_TYPE_PROMOTE 0xF5

  class Request {
  public:
    IO_TYPE io_type;
    char* data;
    uint64_t offset;
    uint64_t length;
    AioCompletion* comp;
    uint32_t pending_requests;
    ssize_t status;
    std::mutex req_mutex;
    Request(IO_TYPE io_type, char* data, uint64_t offset,
            uint64_t length, void* arg) :
            io_type(io_type), data(data), offset(offset),
            length(length), comp((AioCompletion*)arg),
            pending_requests(0), status(0) {
    }
    ~Request() {
    }
    void add_request() {
      req_mutex.lock();
      pending_requests++;
      req_mutex.unlock();
    }
    void complete(int r) {
      bool completed = false;
      req_mutex.lock();
      --pending_requests;
      status += r;
      if (pending_requests == 0) {
        completed = true;
      }
      req_mutex.unlock();
      if (completed) {
        comp->complete(status);
      }
    }
  };
}// hdcs
#endif
