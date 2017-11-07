#ifndef ACCEPTOR
#define ACCEPTOR
#include "./common/networking_common.h"
#include "asio_acceptor.h"
#include "./common/io_pool.h"

namespace hdcs{
namespace networking{
class Acceptor{
private:

    std::shared_ptr<asio_acceptor> asio_acceptor_impl_ptr;
    //std::shared_ptr<rdma_acceptor> rdma_acceptor_impl_ptr;
public:

    Acceptor( const short port_num, SessionSet& _set, int s_num, int thd_num ):
        asio_acceptor_impl_ptr(new asio_acceptor( port_num, _set ,s_num,thd_num)){
        if(false){
            //rdma_acceptor_impl_ptr.reset(new rdma_acceptor (port_num, _set));
        }
    }

    ~Acceptor(){
        close();
    }

    void close(){
        asio_acceptor_impl_ptr->close();
    }

    void start(ProcessMsg _process_msg){
        asio_acceptor_impl_ptr->start( _process_msg );
        if(false){
            //rdma_acceptor_impl_ptr->start(_process_msg);
        }
    }

    void run(){
        asio_acceptor_impl_ptr->run();
        if(false){
            //rdma_acceptor_impl_ptr->run();
        }       
    }
}; // acceptor
} //dslab
}
#endif
