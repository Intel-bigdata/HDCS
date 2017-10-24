#ifndef HDCS_C_AIOREQUESTCOMPLETION_H
#define HDCS_C_AIOREQUESTCOMPLETION_H

#include "common/AioCompletion.h"
#include <condition_variable>
#include <chrono>
#include <mutex>
#include "common/Log.h"

namespace hdcs{

class C_AioRequestCompletion : public AioCompletion {
private:
    callback_t complete_cb;
    void *complete_arg;
    ssize_t ret;
    bool completed;
    std::condition_variable m_cond;
    std::mutex cond_lock;

public:
    C_AioRequestCompletion( void *cb_arg, callback_t cb ):complete_arg(cb_arg),complete_cb(cb){
        assert(complete_cb != NULL);
        assert(complete_arg != NULL);
        ret = -1;
        completed = false;
    }
    ~C_AioRequestCompletion(){
    }

    void complete(ssize_t r){
      ret = r;
      if (complete_cb) {
        complete_cb(ret, complete_arg);
      }
      completed = true;
      m_cond.notify_all();
    }

    ssize_t get_return_value() {
      return ret;
    }

    void wait_for_complete() {
      std::unique_lock<std::mutex> unique_lock(cond_lock);
      m_cond.wait_for(unique_lock, std::chrono::milliseconds(50), [&]{return completed;});
    }

    void set_reserved_ptr(void* ptr){}
};

}// hdcs
#endif
