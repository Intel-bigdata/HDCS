#ifndef RDMA_SESSION
#define RDMA_SESSION

#include "../common/Message.h"
#include "../common/session.h"

namespace hdcs{
namespace networking{

class rdma_session : public Session{
private:
    std::shared_ptr<RDMAConnection> rdma_connection;
    void* hdcs_arg;

public:
    rdma_session(RDMAConnection* conn){
        rdma_connection.reset(conn);
    } 

    ~rdma_session(){
    }

    // implement virtual class interface.
    void stop(){
        rdma_connection->fin();
    }

    void cancel(){
        // TODO
    }

    void set_session_arg(void* arg){
        hdcs_arg = arg;
    }

    bool start( ProcessMsg _process_msg){
        return true;
    }

    int async_send(std::string send_buffer, uint64_t _seq_id){
        /*
        Message msg(send_buffer,_seq_id);
        std::string serialization_msg(std::move(msg.to_buffer()));
        rdma_connection->async_send(serialization_msg.c_str(), serialization_msg.size()+1);
        */
        rdma_connection->async_send(send_buffer.c_str(), send_buffer.size()+1);
        return 0;
    }

    ssize_t communicate(std::string send_buffer, uint64_t _seq_id){
        rdma_connection->async_send(send_buffer.c_str(), send_buffer.size()+1);
        sleep(1); // TODO add wait_event.
        return 0;
    }

    void aio_communicate(std::string& send_buffer, uint64_t _seq_id){
        char* buf_ptr = new char[4096];
        memcpy(buf_ptr, send_buffer.c_str(), send_buffer.size());

        
        /*
        Message msg(send_buffer, _seq_id);
        std::string serialization_msg(std::move(msg.to_buffer()));

        rdma_connection->async_send(serialization_msg.c_str(), serialization_msg.size()+1);
        std::cout<<"rdma_session: aio_communicate over...."<<std::endl;
        */
        // now, RDMA 4k msg.
        //rdma_connection->async_send(send_buffer.c_str(), send_buffer.size()+1);
        rdma_connection->async_send(buf_ptr, send_buffer.size());
    }

    void set_option(){
    }

    bool if_timeout(){
        return true;
    }

    // rdma interface
    int sync_connection(std::string ip_address, std::string port, ProcessMsgClient _process_msg){
        return 0;
        //
    }

    int async_connection(){
        return 0;
        //
    }

    COMMUNICATION_TYPE communication_type(){
        return (COMMUNICATION_TYPE)1;
    }


};//class rdma_session

}//networking
}//hdcs
#endif
