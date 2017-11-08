#ifndef AIO_COMPLETE_IMPL
#define AIO_COMPLETE_IMPL

#include <string>
#include <memory>
#include "aio_complete.h"
#include "networking_common.h"

namespace hdcs{
namespace networking{

class aio_server_send_complete: public aio_complete{
public:
    aio_server_send_complete( OnSentServer _cb , uint64_t _send_num):
            is_default(false), send_num(_send_num){
       cb=_cb;
    }
    aio_server_send_complete( server* _x, OnSentServerDefault _cb, uint64_t _send_num)
        :send_num(_send_num), is_default(true){
            cb_default=_cb;
            arg=_x;
    }
    void complete(int _error_code, uint64_t _byte_num, char* msg, void* _arg){
        if(is_default){
            (arg->*cb_default)( _error_code, _byte_num, _arg);
        }else{
            cb( _error_code, _byte_num, _arg);
        }
    }
private:
    bool is_default;
    OnSentServer cb;
    OnSentServerDefault cb_default;
    ssize_t send_num;
    server *arg;
};

class aio_session_send_complete: public aio_complete {
public:
    aio_session_send_complete(std::shared_ptr<aio_complete> arg){
        cmp=arg;
    }

    ~aio_session_send_complete(){
    }

    void complete(int error_code, uint64_t _byte_num, char* msg, void* arg){
        cmp->complete(error_code, _byte_num, msg, arg);
    }
private:
    std::shared_ptr<aio_complete> cmp;
};

class aio_client_send_complete: public aio_complete{
public:
    aio_client_send_complete( Client* _x, Session* s_id, OnSentClient _cb){
        arg=_x;
        cb=_cb;
        session_id=s_id; 
    }

    ~aio_client_send_complete(){
    }

    void complete(int _error_code, uint64_t _byte_num, char* msg, void* _arg){
            (arg->*cb)( _error_code, session_id, _arg);
    }

private:
    OnSentClient cb;
    Client *arg;
    Session* session_id;
};

class aio_client_receive_complete: public aio_complete{
public: 
    aio_client_receive_complete( Client* _x, Session* s_id, OnReceivedClient _cb){
        arg=_x;
        session_id = s_id;
        cb = _cb;
    }

    ~aio_client_receive_complete(){
    }

    void complete(int _error_code, uint64_t _byte_num, char* msg, void* _arg){
        (arg->*cb)(_error_code, session_id, msg, _byte_num);
    }
private:
    OnReceivedClient cb;
    Client* arg;
    Session* session_id;
};

class aio_session_start_complete: public aio_complete{
public:
    aio_session_start_complete(Session* s_id, ProcessMsg _process_msg){
        session_id = s_id;
        process_msg = _process_msg;
    }

    ~aio_session_start_complete(){
    }

    void complete( int _error_code, uint64_t _byte_num, char* msg, void* _arg){
        process_msg( (void*)session_id, std::move(std::string(msg, _byte_num)));
        delete[] msg;
        session_id->start(process_msg);
    }
private:
    ProcessMsg process_msg;
    Session* session_id;
};

class aio_session_receive_complete: public aio_complete{
public:
    aio_session_receive_complete(std::shared_ptr<aio_complete> _onfinish){
        cmp=_onfinish;
    }

    ~aio_session_receive_complete(){
    }

    void complete(int _error_code, uint64_t _byte_num, char* msg, void* _arg){
        cmp->complete(_error_code, _byte_num, msg, _arg);
    }
private:
    std::shared_ptr<aio_complete> cmp;
};

}
}// 
#endif
