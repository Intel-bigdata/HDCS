#ifndef WORKQUEUE_H
#define WORKQUEUE_H

#include <queue>
#include <mutex>
//#include <std/thread.hpp>
#include <condition_variable>
#include <iostream>
#include <chrono>

namespace hdcs {
template <typename T> class WorkQueue{
public:
    typedef T queue_type;
    std::queue<queue_type> _queue;
    std::mutex _queue_lock;
    std::mutex cond_lock;
    //std::mutex::scoped_lock scope_cond_lock;
    std::condition_variable m_cond;
    std::unique_lock<std::mutex> unique_lock;

    //WorkQueue():scope_cond_lock(cond_lock){}
    WorkQueue():unique_lock(cond_lock){}

    void enqueue( queue_type _work ){
        std::lock_guard<std::mutex> guard(_queue_lock);
        this->_queue.push( _work );
        m_cond.notify_all();
    }

    queue_type dequeue(){
        //m_cond.wait(unique_lock, [&]{return !empty();});
        if(m_cond.wait_for(unique_lock, std::chrono::milliseconds(50), [&]{return !empty();})){
            std::lock_guard<std::mutex> guard(_queue_lock);
            queue_type data = this->_queue.front();
            this->_queue.pop();
            return data;

        }else{
            return nullptr;
        }
    }

    bool empty(){
        std::lock_guard<std::mutex> guard(_queue_lock);
        return this->_queue.empty();
    }

    ssize_t size(){
        std::lock_guard<std::mutex> guard(_queue_lock);
        return this->_queue.size();
    }

    void wake_all(){
        m_cond.notify_all();
    }
};
}
#endif
