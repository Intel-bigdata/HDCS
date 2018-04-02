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
  AioCompletionImp(std::function<void(ssize_t)>&& Callback,
                   int shared_count = 1,
                   bool delete_when_complete = true) :
                   defined(true),
                   func_w_arg(false),
                   Callback(Callback),
                   data(nullptr),
                   shared_count(shared_count),
                   delete_when_complete(delete_when_complete) {}
  AioCompletionImp(std::function<void(ssize_t, void*)>&& Callback,
                   int shared_count = 1,
                   bool delete_when_complete = true) :
                   defined(true),
                   func_w_arg(true),
                   Callback_arg(Callback),
                   data(nullptr),
                   shared_count(shared_count),
                   delete_when_complete(delete_when_complete) {}
  ~AioCompletionImp(){
    if (data != nullptr) {
      free(data);
    }
  }
  void complete(ssize_t r) {
    if (defined) {
      if (r == -2) {
        //when this complete is triggered by timeout
        if (func_w_arg) Callback_arg(r, (void*)data);
        else Callback(r);
        return;
      }
      if (shared_count == -1) {
        if (func_w_arg) Callback_arg(r, (void*)data);
        else Callback(r);
      }else if (--shared_count == 0) {
        if (func_w_arg) Callback_arg(r, (void*)data);
        else Callback(r);
        cond.notify_all();
        if (delete_when_complete) delete this;
      }
    } else {
      if (delete_when_complete) delete this;
    }
  }
  ssize_t get_return_value() {};
  void release() {};
  void wait_for_complete() {
   std::unique_lock<std::mutex> l(cond_lock);
   cond.wait(l, [&]{return shared_count == 0;});
  };
  void set_reserved_ptr(void* ptr) {
    data = (char*)ptr;
  }
  std::function<void(ssize_t)> Callback;
  std::function<void(ssize_t, void*)> Callback_arg;
  char* data;
  bool defined;
  bool func_w_arg;
  std::atomic<int> shared_count;
  std::mutex cond_lock;
  std::condition_variable cond;
  bool delete_when_complete;
};

}// hdcs
#endif
