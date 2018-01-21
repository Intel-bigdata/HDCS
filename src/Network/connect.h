#ifndef CONNECT
#define CONNECT
#include "asio_common/asio_connect.h"
#include "rdma_common/rdma_connect.h"
namespace hdcs{
namespace networking{

class Connect{
public:
    // TODO rmda and tcp use the same thread pool 
    Connect(int _ios_num, int _thd_num_of_one_ios)
       : asio_connect(NULL)
       , rdma_connect(NULL)
    {
        asio_connect.reset(new AsioConnect(_ios_num, _thd_num_of_one_ios));
        rdma_connect.reset( new RDMAConnect()); 
    }

    ~Connect(){
        close();
    }

    int async_connect( std::string ip_address, std::string port ){
        // TODO
        return 1;
    }

    void close(){
        asio_connect->close();
    }

    SessionPtr sync_connect(std::string ip_address, std::string port, ProcessMsgClient _process_msg){
       // use config file to selct tcp/rdma 
       // tcp
       if(false){
           return asio_connect->sync_connect(ip_address, port, _process_msg);
       }else{
           std::cout<<"connect: sync_connect"<<std::endl;
           return rdma_connect->connect(ip_address, port, _process_msg);
       }
    }

    //just for rdma
    void set_hdcs_arg(void* arg){
        rdma_connect->set_hdcs_arg(arg);
    }
private:
    std::shared_ptr<AsioConnect> asio_connect;
    std::shared_ptr<RDMAConnect> rdma_connect;
};
}
}
#endif
