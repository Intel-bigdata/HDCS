// Copyright [2016] <Intel>
#ifndef THREAD_POOL_HPP_
#define THREAD_POOL_HPP_

#include <atomic>
#include <thread>
#include <mutex>
#include <vector>
#include <list>
#include <functional>
#include <condition_variable>

class ThreadPool {
 public:
    ThreadPool(size_t thd_cnt):jobs_left(0), bailout(false),
                               finished(false), ThreadCount(thd_cnt) {
        for ( unsigned i = 0; i < ThreadCount; ++i )
            threads.push_back(std::move(std::thread( [this,i]{ this->Task(); } )));
    }
    ~ThreadPool() {
        JoinAll();
    }

    void schedule(std::function<void(void)> job) {
        std::lock_guard<std::mutex> guard(queue_mutex);
        queue.emplace_back(job);
        ++jobs_left;
        job_available_var.notify_one();
    }

    size_t Size() const {
        return ThreadCount;
    }

    void JoinAll(bool WaitForAll = true) {
        if ( !finished ) {
            if( WaitForAll ) {
                WaitAll();
            }

            // note that we're done, and wake up any thread that's
            // waiting for a new job
            bailout = true;
            job_available_var.notify_all();

            for( auto &x : threads )
                if( x.joinable() )
                    x.join();
            finished = true;
        }
    }

    void WaitAll() {
        if ( jobs_left > 0 ) {
            std::unique_lock<std::mutex> lk( wait_mutex );
            wait_var.wait( lk, [this]{ return this->jobs_left == 0; } );
            lk.unlock();
        }
    }

    void wait() {
        if ( jobs_left > 0 ) {
            std::unique_lock<std::mutex> lk( wait_mutex );
            wait_var.wait( lk, [this]{ return this->jobs_left == 0; } );
            lk.unlock();
        }
    }

 private:
    int ThreadCount;
    std::vector<std::thread> threads;
    std::list<std::function<void(void)>> queue;

    std::atomic_int         jobs_left;
    std::atomic_bool        bailout;
    std::atomic_bool        finished;
    std::condition_variable job_available_var;
    std::condition_variable wait_var;
    std::mutex              wait_mutex;
    std::mutex              queue_mutex;

    /**
     *  Take the next job in the queue and run it.
     *  Notify the main thread that a job has completed.
     */
    void Task() {
        while ( !bailout ) {
            next_job()();
            --jobs_left;
            wait_var.notify_one();
        }
    }

    /**
     *  Get the next job; pop the first item in the queue, 
     *  otherwise wait for a signal from the main thread.
     */
    std::function<void(void)> next_job() {
        std::function<void(void)> res;
        std::unique_lock<std::mutex> job_lock( queue_mutex );

        // Wait for a job if we don't have any.
        job_available_var.wait(job_lock, [this]() ->bool { return queue.size() || bailout; });
        
        // Get job from the queue
        if ( !bailout ) {
            res = queue.front();
            queue.pop_front();
        }
        else { // If we're bailing out, 'inject' a job into the queue to keep jobs_left accurate.
            res = []{};
            ++jobs_left;
        }
        return res;
    }
};

#endif  //  THREAD_POOL_HPP_
