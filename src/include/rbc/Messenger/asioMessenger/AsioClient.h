#ifndef RBC_ASIOCLIENT_H
#define RBC_ASIOCLIENT_H

#define IO_SERVICE_NUM 10
#define THREAD_NUM_FOR_IOS 10
#include "rbc/Messenger/asioMessenger/AsioSession.h"
#include "rbc/Messenger/mqueueMessenger/MqueueSession.h"
#include "rbc/Messenger/io_pool.h"
#include <thread>
#include <memory>
#include <vector>

namespace rbc{
class AsioClient{
public:
    std::vector<std::shared_ptr<AsioSession>> session_vec;
    int session_num;
    io_service_pool service_pool_;
    int index;
    MqueueSession* mqueue_session;
    AsioClient(std::string dest_ip, std::string port):mqueue_session(NULL),
        service_pool_(IO_SERVICE_NUM,THREAD_NUM_FOR_IOS), session_num(10),index(0){
        for(int i=0; i<session_num; ++i){
            std::shared_ptr<AsioSession> asio_session( 
                new AsioSession(service_pool_.get_io_service(), NULL, NULL, dest_ip.c_str(), port.c_str()) ); 
            if( false ){
                mqueue_session = new MqueueSession( asio_session->get_local_port() );
            }else{
                asio_session->start_listen();
            }
            session_vec.push_back(asio_session);
        }
        service_pool_.run();
    }

    ~AsioClient(){
	service_pool_.stop();
    }

    // called by application
    int send_request( Msg* msg, void* arg ){
        session_vec[index++]->send_request( msg, arg );
        if(index==session_num){
            index=0;
        }
        return 1;
    }
};
}
#endif
