#ifndef IO_SERVICE_POOL
#define IO_SERVICE_POOL

#include <boost/asio.hpp>
#include <vector>
#include <thread>
#include <memory>

namespace rbc{
class io_service_pool {
public:
io_service_pool(std::size_t pool_size, int _threads_num_of_one_ios)
    :next_io_service_(0), threads_num_of_one_ios(_threads_num_of_one_ios)
{
    if (pool_size == 0)
        throw std::runtime_error("io_service_pool size is 0");
    for (std::size_t i = 0; i < pool_size; ++i)
    {
        io_service_ptr io_service(new boost::asio::io_service);
        work_ptr work(new boost::asio::io_service::work(*io_service));
        io_services_.push_back(io_service);
        work_.push_back(work);
    }
}

void run()
{
    for (std::size_t i = 0; i < io_services_.size(); ++i)
    {
        std::vector<std::shared_ptr<std::thread>> temp_vec;
        for (std::size_t j = 0; j < threads_num_of_one_ios; ++j)
        {
            std::shared_ptr<std::thread> thread(new std::thread(
               [i, this]() {
               io_services_[i]->run();
            }));
            temp_vec.push_back(thread); 
        }
        threads_pool_.push_back(temp_vec);
    }
}

void stop()
{
    for (std::size_t i = 0; i < work_.size(); ++i){
        io_services_[i]->stop();
        //work_[i].reset();
        for(std::size_t j=0; j < threads_num_of_one_ios; j++){
           threads_pool_[i][j]->join();
        }
    }
}

boost::asio::io_service& get_io_service()
{
    boost::asio::io_service& io_service = *io_services_[next_io_service_];
    ++next_io_service_;
    if (next_io_service_ == io_services_.size())
        next_io_service_ = 0;
    return io_service;
}

boost::asio::io_service& get_io_service(size_t index) {
    index = index % io_services_.size();
    return *io_services_[index];
}

private:
    typedef std::shared_ptr<boost::asio::io_service> io_service_ptr;
    typedef std::shared_ptr<boost::asio::io_service::work> work_ptr;
    std::vector<io_service_ptr> io_services_;
    std::vector<work_ptr> work_;
    std::size_t next_io_service_;
    int threads_num_of_one_ios;
    std::vector<std::vector<std::shared_ptr<std::thread>>> threads_pool_;
};

}



#endif
