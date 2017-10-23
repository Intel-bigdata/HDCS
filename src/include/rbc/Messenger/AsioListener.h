#ifndef RBC_ASIO_LISTENER_H
#define ASIO_LISTENER_H

#define MAX_SESSION_NUM 32
#define IO_SERVICE_NUM 10
#define THREAD_NUM_FOR_IOS 10
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/thread.hpp>
#include <thread>
#include <memory>
#include <string>
#include "rbc/common/ThreadPool.h"
#include "rbc/common/WorkQueue.h"
#include "rbc/Messenger/mqueueMessenger/MqueueSession.h"
#include "rbc/Messenger/asioMessenger/AsioSession.h"
#include "rbc/Messenger/io_pool.h"


namespace rbc{
using boost::asio::ip::tcp;
class AsioListener{
private:
    short port;
    std::shared_ptr<std::thread> listener_thread;
    std::shared_ptr<tcp::acceptor> acceptor_ptr;
    io_service_pool service_pool_;
    std::vector< std::shared_ptr<Session>> session_vector;
    WorkQueue<void*>* request_queue;
    tcp::socket* accepted_socket;

public:
    AsioListener( short port, WorkQueue<void*>* request_queue ):port(port), 
        request_queue(request_queue), service_pool_( IO_SERVICE_NUM, THREAD_NUM_FOR_IOS ){
        listener_thread.reset(new std::thread( [ this ](){ start_listen();}));
    }

    ~AsioListener(){
        acceptor_ptr->close();
        service_pool_.stop();
    }

    void start_listen(){
        acceptor_ptr.reset( new tcp::acceptor( service_pool_.get_io_service(), tcp::endpoint(tcp::v4(), port) ) );
        acceptor_ptr->set_option(boost::asio::ip::tcp::acceptor::reuse_address(true));
        acceptor_ptr->listen(MAX_SESSION_NUM);
        start_accept();
        service_pool_.run();  
    }
    

    void start_accept(){
        accepted_socket = new tcp::socket( service_pool_.get_io_service());
        acceptor_ptr->async_accept(
            *accepted_socket,
            boost::bind(
                &AsioListener::handle_accept,
                this,
                boost::asio::placeholders::error
            )
        );
    }

    void handle_accept(const boost::system::error_code& error){
        log_print("AsioListener::handle_accept got new connection\n");
        if (!error){
            std::string source_ip = accepted_socket->remote_endpoint().address().to_string();
            uint32_t source_port = accepted_socket->remote_endpoint().port();
            std::cout << "AsioListen: got new connection, source ip is: " << source_ip << ", port is:" << source_port << std::endl;
            if( false ){
                std::shared_ptr<Session> new_session( new MqueueSession( source_port, request_queue ));
                session_vector.push_back( new_session );
                accepted_socket->close();    
            }else{
                std::shared_ptr<Session> new_session( new AsioSession( accepted_socket->get_io_service(), accepted_socket, request_queue ));
                session_vector.push_back( new_session );
            }
            start_accept();
        }else{
            log_print("AsioListener::handle_accept failed to accept new connection\n");;
        }
    }

};
}
#endif
