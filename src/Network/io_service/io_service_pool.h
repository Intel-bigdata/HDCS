#ifndef IO_SERVICE_POOL_H_
#define IO_SERVICE_POOL_H_

#include <vector>

#include "thread_group_impl.h"
#include "../common/wait_event.h"

namespace hdcs {
namespace networking {

class io_service_pool
{
public:
    io_service_pool(std::size_t pool_size, int pool_thread_num )
         : _next_service(0)
         , is_sync_model(true)
    {
        assert(pool_size > 0);
        for (size_t i = 0; i < pool_size; ++i)
        {
            ThreadGroupImplPtr service(new ThreadGroupImpl(pool_thread_num));
            _pool.push_back(service);
        }
    }   

    ~io_service_pool()
    {}

    bool sync_run()
    {
        is_sync_model = true;
        run();
        wait_event.Wait();
        return true;
    }

    bool async_run()
    {
        is_sync_model = false;
        return run();
    }

    void stop()
    {
        size_t pool_size = _pool.size();
        for (size_t i = 0; i < pool_size; ++i)
        {
            _pool[i]->stop();
            // Because ThreadGroupImplPtr is shared_ptr,
            // if in here call reset operation, destruction still call destruction
            // , so lead to double free.
        }
        if(is_sync_model)
        {
            wait_event.Signal();
        }
    }

    boost::asio::io_service& get_io_service()
    {
        IOService& io_service = _pool[_next_service]->io_service();
        ++_next_service;
        if (_next_service == _pool.size())
        {
            _next_service = 0;
        }
        return io_service;
    }

private:

    bool run()
    {
         size_t pool_size = _pool.size();
        for (size_t i = 0; i < pool_size; ++i)
        {
            if (!_pool[i]->start())
            {
                std::cout<<"io_service_pool::Start(): start thread group failed"<<std::endl;
                // TODO error handling ...dehao
                assert(0);
            }
        }
        return true;
    }

private:

    std::vector<ThreadGroupImplPtr> _pool;
    size_t _next_service;
    bool is_sync_model;
    WaitEvent wait_event;
};

} 
} 

#endif 
