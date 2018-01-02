#ifndef HDCS_AIOCOMPLETIONIMP_H
#define HDCS_AIOCOMPLETIONIMP_H

#include "AioCompletion.h"
#include <atomic>
#include <functional>
#include <mutex>
#include <condition_variable>
#include <chrono>

namespace hdcs{

class AioCompletionImp : public AioCompletion {
public:
  AioCompletionImp() : defined(false), Callback(nullptr), data(nullptr), shared_count(1), delete_when_complete(true) {
  }
  AioCompletionImp(std::function<void(ssize_t)>&& Callback, int shared_count = 1, bool delete_when_complete = true) : defined(true), Callback(Callback), data(nullptr), shared_count(shared_count), delete_when_complete(delete_when_complete) {}
  ~AioCompletionImp(){
    if (data != nullptr) {
      free(data);
    }
  }
  void complete(ssize_t r) {
    if (defined) {
      if (shared_count == -1) {
        Callback(r);
      }
      if (--shared_count == 0) {
        Callback(r);
        cond.notify_all();
        if (delete_when_complete) delete this;
      }
    } else {
      if (delete_when_complete) delete this;
    }
  }
  ssize_t get_return_value() {};
  void wait_for_complete() {
    if (shared_count > 0) {
      std::unique_lock<std::mutex> l(cond_lock);
      while( shared_count > 0 ) {
        cond.wait_for(l, std::chrono::milliseconds(50));
      }
    }
  };
  void set_reserved_ptr(void* ptr) {
    data = (char*)ptr;
  }
  std::function<void(ssize_t)> Callback;
  char* data;
  bool defined;
  std::atomic<int> shared_count;
  std::mutex cond_lock;
  std::condition_variable cond;
  bool delete_when_complete;
};

}// hdcs
#endif
