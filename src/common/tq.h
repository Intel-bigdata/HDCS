// Copyright [2017] <Intel>
#ifndef WORK_QUEUE_HPP_
#define WORK_QUEUE_HPP_

#include <thread>
#include <mutex>
#include <vector>
#include <deque>
#include <list>
#include <future>
#include <functional>
#include <condition_variable>

namespace hdcs {
class TWorkQueue {
  typedef std::function<void(void)> fp_t;

  bool stop;
  std::deque<fp_t> job_queue;
  std::vector<std::thread> threads;
  std::mutex q_lock;
  std::condition_variable cond;

  void add_worker() {
    std::thread t([this]() {
      while(1) {
        std::function<void()> task;
        {
          std::unique_lock<std::mutex> lock(q_lock);
          cond.wait(lock, [this]{ return this->stop || !this->job_queue.empty(); });

          if(this->stop && this->job_queue.empty()) {
            return;
          }

          task = std::move(job_queue.front());
          job_queue.pop_front();
        }
        task();
      }
    });
    threads.push_back(std::move(t));
  }

 public:
  TWorkQueue(size_t thd_cnt = 1) : stop(false) {
    for ( unsigned i = 0; i < thd_cnt; ++i )
      add_worker();
    }
  ~TWorkQueue() {
    {
      std::unique_lock<std::mutex> lock(q_lock);
      stop = true;
    }
    cond.notify_all();
    for (auto &t : threads)
      t.join();
  }

  template<class F, class... Args>
  auto add_task(F&& f, Args&&... args)
    -> std::future<typename std::result_of<F(Args...)>::type> {
    using return_type = typename std::result_of<F(Args...)>::type;

    auto task = std::make_shared< std::packaged_task<return_type()> >(
        std::bind(std::forward<F>(f), std::forward<Args>(args)...)
      );

    std::future<return_type> ret = task->get_future();
    {
      std::unique_lock<std::mutex> lock(q_lock);

      if(stop) {
          throw std::runtime_error("add_task on stopped WorkQueue");
      }

      job_queue.push_back([task](){ (*task)(); });
    }

    cond.notify_one();

    return ret;
  }



};
}  //  namespace dslab

#endif  //  WORK_QUEUE_HPP_
