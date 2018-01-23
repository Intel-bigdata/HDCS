#ifndef THREADWRAPPER_H
#define THREADWRAPPER_H

#include <assert.h>

#include <thread>
#include <mutex>
#include <condition_variable>
#include <iostream>

class ThreadWrapper {
  public:
    ThreadWrapper() : done(false) {}
    virtual ~ThreadWrapper() {
      printf("destroy thread.\n"); 
    }
    void join() {
      if (thread.joinable()) {
        thread.join(); 
      } else {
        std::unique_lock<std::mutex> l(join_mutex); 
        join_event.wait(l, [=] { return done; });
      }
    }
    void start(bool background_thread = false) {
      thread = std::thread(&ThreadWrapper::thread_body, this); 
      t_handler = thread.native_handle();
      if (background_thread) {
        thread.detach(); 
      }
    }
    void set_affinity(int cpu) {
      cpu_set_t cpuset; 
      CPU_ZERO(&cpuset);
      CPU_SET(cpu, &cpuset);
      int res = pthread_setaffinity_np(thread.native_handle(), sizeof(cpu_set_t), &cpuset);
      if (res) {
        abort(); 
      }
    }
    void thread_body() {
      try {
        entry(); 
      } catch (ThreadAbortException&) {
        abort(); 
      } catch (std::exception& ex) {
        ExceptionCaught(ex); 
      } catch (...) {
        UnknownExceptionCaught(); 
      }
    }
    void stop() {
      pthread_cancel(t_handler);
      std::cout << "finished." << std::endl;
    }
  private:
    class ThreadAbortException : std::exception {};

  protected:
    virtual void entry() = 0;
    virtual void abort() = 0;
    virtual void ExceptionCaught(std::exception& exception) {}
    virtual void UnknownExceptionCaught() {}
  private:
    std::thread thread;
    pthread_t t_handler;
    std::mutex join_mutex;
    std::condition_variable join_event;
    bool done;
};

#endif
