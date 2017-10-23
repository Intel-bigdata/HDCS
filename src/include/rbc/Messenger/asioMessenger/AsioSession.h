#ifndef RBC_ASYNC_MESSENGER_SESSION_H
#define RBC_ASYNC_MESSENGER_SESSION_H
#include <cstdlib>
#include <iostream>
#include "rbc/common/Log.h"
#include "rbc/common/ThreadPool.h"
#include "rbc/common/Request.h"
#include "rbc/common/AsyncIO.h"
#include "rbc/common/ThreadPool.h"
#include "rbc/common/WorkQueue.h"
#include "rbc/common/Request.h"

#include "rbc/Message.h"
#include "rbc/Messenger/Session.h"
#include "rbc/Messenger/asioMessenger/AsioMessenger.h"
namespace rbc{

static void handle_msg(void* arg, Msg* msg);

class AsioSession: public Session{
public:
    bool go;
    AsioMessenger* asio_messenger;
    WorkQueue<void*>* request_queue;

public:
    AsioSession(boost::asio::io_service& io_service, void* tcp_socket, WorkQueue<void*>* request_queue = NULL, const char* host = NULL, const char* port = NULL):
        request_queue(request_queue){
        go = true;
        asio_messenger = new AsioMessenger(io_service, tcp_socket, host, port); // for AsioListen, tcp_socket != NULL
        asio_messenger->set_callback( handle_msg, (void*)this );
    }

    ~AsioSession(){
        std::cout<<"******asiosession will be delete ********"<<std::endl;
        go = false;
        delete asio_messenger;
    }
    
    void start_listen(){
        asio_messenger->start_receive();
    }

    void receive_msg( Msg* msg ){
        if( !msg ){
            return;
        }
        if(request_queue){
            async_io_unit *io_u = new async_io_unit( msg, asio_messenger );
            Request *req = new Request( io_u->msg, REQ_MESSENGER, (void*)io_u );
            request_queue->enqueue((void*)req);
            //req->complete();
            //delete req;
        }else{
            /*client mode*/
            ssize_t r;
            AsyncInflightIO_u *io_u = (AsyncInflightIO_u*)msg->header.seq_id;
            if( msg->header.type == MSG_REPLY_STAT ){
                r =msg->get_reserve();
            }else if(msg->header.type == MSG_REPLY_DATA){
                r = msg->content_length();
            }            
            AioCompletion* comp = (AioCompletion*)io_u->comp;
            comp->complete(r);
            delete msg; // this msg
            delete io_u->msg; // last msg
            delete io_u;
        }
    }

    uint32_t get_local_port(){
        return asio_messenger->get_local_port();
    }

    int send_request( Msg* msg, void* arg = NULL ){
        AsyncInflightIO_u *io_u = new AsyncInflightIO_u( arg, msg );
        return asio_messenger->send_request( msg, io_u );
    }

};

static void handle_msg( void* arg, Msg* msg ){
    AsioSession* asio_s = (AsioSession*)arg;
    asio_s->receive_msg( msg );
} 

}
#endif
