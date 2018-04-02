#ifndef HDCS_TIMER_H
#define HDCS_TIMER_H

#include <pthread.h>
#include <unordered_map>
#include <map>
#include <list>
#include <time.h>
#include <sys/time.h>
#include <cmath>
#include <mutex>
#include <thread>
#include <condition_variable>
#include "AioCompletion.h"

namespace hdcs {

typedef struct timespec utime_t;
inline uint64_t to_nanoseconds(utime_t &ts) {
    return ts.tv_sec * (uint64_t)1000000000L + ts.tv_nsec;
}

typedef AioCompletion Context;
typedef std::multimap<uint64_t, Context*> scheduled_map_t;
typedef std::unordered_map<Context*, scheduled_map_t::iterator> event_lookup_map_t;

class SafeTimer { 
public:
  SafeTimer() :
    go(true) {
      process_thread = new std::thread(std::bind(&SafeTimer::process, this));
  }
  ~SafeTimer() {
    go = false;
    cond.notify_all();
    process_thread->join();
    delete process_thread;
  }

  bool add_event_after(uint64_t nanoseconds_delta, Context *callback) {
    utime_t when;
    uint64_t when_l;
    clock_gettime(CLOCK_REALTIME, &when);
    when_l = to_nanoseconds(when) + nanoseconds_delta;
    return add_event_at(when_l, callback);
  }

  /* Cancel an event.
   * Call with the event_lock LOCKED
   *
   * Returns true if the callback was cancelled.
   * Returns false if you never addded the callback in the first place.
   */
  bool cancel_event(Context *callback, bool delete_event = true){
    std::lock_guard<std::mutex> guard(map_lock);
     event_lookup_map_t::iterator event_it = events.find(callback);
    if (event_it == events.end()) {
      return false;
    }
  
    if (delete_event) {
      delete (Context*)event_it->first;
    }
  
    schedule.erase(event_it->second);
    events.erase(event_it);
    return true;
  }

private:

  std::mutex lock;
  std::mutex map_lock;
  std::condition_variable cond;
  std::thread* process_thread;

  bool go;
  utime_t now;
  uint64_t now_l;
  
  scheduled_map_t schedule;
  event_lookup_map_t events;

  void process() {
    std::unique_lock<std::mutex> unique_lock(lock);
    while (go) {
      clock_gettime(CLOCK_REALTIME, &now);
      now_l = to_nanoseconds(now);
      std::list<Context*> event_list;
      map_lock.lock();
      for (scheduled_map_t::iterator p = schedule.begin(); p != schedule.end();) {
        scheduled_map_t::iterator tmp = p++;
        if (tmp->first <= now_l) {
          Context *callback = (Context*)tmp->second;
          event_list.emplace_back(callback);
          event_lookup_map_t::iterator event_it = events.find(callback);
          if (event_it != events.end()) {
            schedule.erase(event_it->second);
            events.erase(event_it);
          }
        }
      }

      for (auto &cur_event : event_list) {
        cur_event->complete(-2);
      }
  
      if (schedule.empty()) {
        map_lock.unlock();
        cond.wait(unique_lock);
      } else {
        uint64_t first_future = schedule.begin()->first;
        int64_t wait_period = first_future - now_l;
        assert(wait_period > 0);
        map_lock.unlock();
        cond.wait_for(unique_lock, std::chrono::nanoseconds(wait_period));
      }
    }
  }

  /* Schedule an event in the future
   * Call with the event_lock LOCKED */
  bool add_event_at(uint64_t when, Context *callback) {
    std::lock_guard<std::mutex> guard(map_lock);
    scheduled_map_t::iterator i = schedule.insert({when, callback});
  
    std::pair<event_lookup_map_t::iterator, bool> rval(events.insert({callback, i}));
  
    /* insert event faild*/
    assert(rval.second);
  
    /* If the event we have just inserted comes before everything else, we need to
     * adjust our timeout. */
    if (i == schedule.begin()) {
      cond.notify_all();
    }
    return true;
  }

  /* Cancel all events.
   * Call with the event_lock LOCKED
   *
   * When this function returns, all events have been cancelled, and there are no
   * more in progress.
   */
  void cancel_all_events() {
    std::lock_guard<std::mutex> guard(map_lock);
    while (!events.empty()) {
      auto p = events.begin();
      delete (Context*)p->first;
      schedule.erase(p->second);
      events.erase(p);
    }
  }
};

}// hdcs
#endif
