#ifndef ACCEPTOR
#define ACCEPTOR
#include "common/networking_common.h"
#include "asio_common/asio_acceptor.h"
#include "rdma_common/rdma_acceptor.h"

namespace hdcs{
namespace networking{
class Acceptor{
private:

    std::shared_ptr<asio_acceptor> asio_acceptor_ptr;
    std::shared_ptr<rdma_acceptor> rdma_acceptor_ptr;
public:

    Acceptor( std::string ip_address, std::string port_num, SessionSet& _set, int s_num, int thd_num )
        //: asio_acceptor_ptr(new asio_acceptor(ip_address, port_num, _set ,s_num,thd_num))
        : rdma_acceptor_ptr(new rdma_acceptor(ip_address, port_num, _set))
    {
        //std::cout<<"acceptor contruction success..."<<std::endl;
    }

    ~Acceptor(){
        stop();
    }

    void stop(){
        //asio_acceptor_ptr->stop();
    }

    void start(ProcessMsg _process_msg){
        std::cout<<"acceptor-->start..."<<std::endl;
        //asio_acceptor_ptr->start( _process_msg );
        rdma_acceptor_ptr->start(_process_msg);
        //std::cout<<"Acceptor: start success..."<<std::endl;
    }

    void sync_run(){
        //asio_acceptor_ptr->sync_run();
        rdma_acceptor_ptr->sync_run();
    }

    void async_run(){
        //asio_acceptor_ptr->async_run();
        rdma_acceptor_ptr->async_run();
    }
}; // 
} //
}
#endif
