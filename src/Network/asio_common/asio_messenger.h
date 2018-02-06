#ifndef ASIO_MESSENGER
#define ASIO_MESSENGER

#include <boost/bind.hpp>
#include <boost/asio.hpp>
#include <condition_variable>
#include <memory>
#include <string>
#include <mutex>
#include <thread>
#include <pthread.h>
#include <atomic>

#include "../common/Message.h"
#include "../common/networking_common.h"
#include "../io_service/thread_group.h"
#include "../common/wait_event.h"
#include "../common/counter.h"
//#include "asio_session.h"

namespace hdcs{
namespace networking{
using boost::asio::ip::tcp;

class SessionArg{
public:
    SessionArg(const void* s_id, uint64_t _seq)
        :session_id(s_id),seq_id(_seq)
    {}
    SessionArg(const void* s_id)
        :session_id(s_id)
    {}
    ~SessionArg(){}

    void set_seq_id(uint64_t _seq_id){
        seq_id = _seq_id;
    }

    uint64_t get_seq_id(){
        return seq_id;
    }

    const void* get_session_id(){
        return session_id;
    }        

private:
    const void* session_id;
    uint64_t seq_id;
};

class asio_messenger{
private:
    boost::asio::io_service& io_service_;
    boost::asio::ip::tcp::socket socket_;
    boost::asio::ip::tcp::endpoint endpoint_;
    char* msg_header;
    boost::asio::ip::tcp::resolver resolver_;
    boost::asio::io_service::strand strand_;
    const ssize_t header_len;
    int counts;
    bool is_closed;
    int role;
    void* callback_arg;
    std::mutex send_lock;
    std::mutex receive_lock;
    ProcessMsgClient c_process_msg;
    ProcessMsg s_process_msg;
    std::atomic<bool> async_receive_loop;
    std::mutex async_receive_loop_mutex;
    std::map<uint64_t,MsgController*> pending_msg_map; 
    std::mutex pending_msg_map_lock;
    enum {
        STATUS_INIT = 0,
        STATUS_CONNECTING = 1,
        STATUS_CONNECTED =2,
        STATUS_CLOSED = 3,
    };
    volatile int _status;
    AtomicCounter64 send_index; 
    AtomicCounter64 receive_index;
    //ThreadGroup thread_worker; // post hdcs_handle_request into other io_service.

public:
    asio_messenger( IOService& ios, int _role)
        : io_service_(ios)
        , strand_(ios)
        , socket_(ios)
        , header_len(sizeof(MsgHeader))
        , msg_header(new char[sizeof(MsgHeader)])
        , resolver_(ios)
        , role(_role) // 0 is client, and 1 is server
        , async_receive_loop(false)
        , _status(STATUS_INIT)
        //, thread_worker(5)
    {

        if(_role==0){
            //aio_receive();
        }
    }

    ~asio_messenger() {
        if( !is_closed ){
            close();
        }
        delete[] msg_header;
    }
    
    void set_callback_arg(void* _arg){
        callback_arg = _arg;
    }

    int set_socket_option(){
        boost::system::error_code ec;
        boost::asio::ip::tcp::no_delay no_delay(true);
        socket_.set_option(no_delay, ec);
        if(ec){
            std::cout<<"asio_messenger::set_socket_option failed: "<<ec.message()<<std::endl;
            assert(0);
        }
        // 128k + header 
        boost::asio::socket_base::send_buffer_size s_b(140000);
        socket_.set_option(s_b, ec);
        if(ec){
            std::cout<<"asio_messenger::set_socket_option: send_buffer_size failed "<<ec.message()<<std::endl;
            assert(0);
        }
        boost::asio::socket_base::receive_buffer_size r_b(140000);
        socket_.set_option(r_b, ec);
        if(ec){
            std::cout<<"asio_messenger::set_socket_option: receive_buffer_size failed "<<ec.message()<<std::endl;
            assert(0);
        }

        return 0;
    }

    void close(){
        // just can be called only once.
        if(atomic_swap( &_status, (int)STATUS_CLOSED) != STATUS_CLOSED ){
            boost::system::error_code ec;
            socket_.shutdown(tcp::socket::shutdown_both, ec);
            if(ec){
                std::cout<<"close() failed: "<<ec.message()<<std::endl;
            }
        }
    }

    boost::asio::ip::tcp::socket& get_socket() {
        return socket_;

    }

    void cancel(){
        boost::system::error_code ec;
        socket_.cancel(ec);
        if(ec){
            std::cout<<"asio_messenger::cancel, cancle failed: "<<ec.message()<<std::endl;
        }
    }

    // called by server..
    void set_server_process_msg(ProcessMsg _process_msg){
       	s_process_msg = _process_msg;
    }

    int sync_connection(std::string ip_address, std::string port, ProcessMsgClient _process_msg){
        _status = STATUS_CONNECTING;
        boost::system::error_code ec;
        boost::asio::ip::tcp::resolver::iterator iter =
            resolver_.resolve(boost::asio::ip::tcp::resolver::query(ip_address, port), ec);
        if(ec){
            std::cout<<"asio_messenger::sync_connection, resolver failed: "<<ec.message()<<std::endl;
            assert(0);
        }
        _status = STATUS_CONNECTED;
        endpoint_ = *iter;
        socket_.connect(endpoint_, ec);
        if(ec){
            if(ec == boost::asio::error::operation_aborted){
                return 0;
            }
            std::cout<<"asio_messenger::sync_connection, connect  failed: "<<ec.message()<<std::endl;
            assert(0);
        }
        int ret = set_socket_option();
        if(ret){
            assert(0);
        }
        is_closed = false; 
        c_process_msg = _process_msg; 
        aio_receive();
        async_receive_loop.store(true);
        return 0;
    }

    // TODO
    int async_connection(){
        return 0;
    }

    //called by client.
    int sync_send( std::string send_buffer, uint64_t _seq_id) {
        boost::system::error_code ec;
        uint64_t ret;
        Message msg(send_buffer, _seq_id); 
        uint64_t send_bytes = msg.to_buffer().size();
        send_lock.lock();
        ret=boost::asio::write(socket_, boost::asio::buffer(std::move(msg.to_buffer())), ec);
        send_lock.unlock();
        if(ec){
            error_handing("asio_messenger::sync_send: sending failed", ec);
        }
        if(ret != send_bytes){
            std::cout<<"asio_messenger::sync_send failed: ret != send_bytes "<<std::endl;
            assert(0);
        }
        ++send_index;
        MsgController* msg_cntl = new MsgController(true); // ture express this msg is sync
        pending_msg_map[_seq_id]=msg_cntl;
        msg_cntl->wait(); // waitting until its ack signal. 

        return 0;
    }

    // called by server.
    int async_send(std::string& send_buffer, uint64_t _seq_id) {
        std::string* send_string = new std::string( Message(send_buffer, _seq_id).to_buffer());
        uint64_t ret = send_string->size();
        boost::asio::async_write(socket_, boost::asio::buffer(*send_string, ret),
            [this, ret, send_string](  
                const boost::system::error_code& ec, uint64_t cb) {
                if (!ec) {
                    if(ret != cb){
                        std::cout<<"asio_session::aync_send failed: ret != cb"<<std::endl;
                        assert(0);
                    }
                    ++send_index;
                }else{ 
                    error_handing("asio_messenger::async_send: sending failed", ec);
                }
                delete send_string;
            });
    }

    // call by client---disbale
    ssize_t sync_receive(){
        boost::system::error_code ec;
        uint64_t ret;
        char msg_header[header_len];
        receive_lock.lock();//
        ret = boost::asio::read(socket_, boost::asio::buffer(msg_header, header_len), ec); 
        if(!ec){
            error_handing("asio_messenger::sync_receive: receving header failed", ec);
        }
        if(ret<0){
            std::cout<<"asio_session::sync_receive msg_header failed:  "<<ec.message()<<std::endl;
            return -1;
        }
        if(ret != header_len){
            std::cout<<"asio_session::sync_receive msg_header failed: ret!=header_len "<<std::endl;
            return -1;
        }
        uint64_t content_size = ((MsgHeader*)msg_header)->get_data_size();
        char* receive_buffer = new char[content_size+1]();
        ret = boost::asio::read(socket_, boost::asio::buffer(receive_buffer, content_size), ec); 
        receive_lock.unlock();// 
        if(!ec){
            error_handing("asio_messenger::sync_receive: receving content failed", ec);
        }
        ++receive_index;
        if(ret<0){
            std::cout<<"asio_session::sync_receive content failed: "<<ec.message()<<std::endl;
            delete[] receive_buffer;
            return -1;
        }
        if(ret != content_size ){
            std::cout<<"asio_session::sync_receive content failed: ret!= content_size"<<std::endl;
            delete[] receive_buffer;
            return -1;
        }

        if(!check_crc(msg_header, receive_buffer, content_size)){
            std::cout<<"asio_session::sync_receive CRC failed. "<<std::endl;
            delete[] receive_buffer;
            return -1;
        }
        c_process_msg(callback_arg, std::move(std::string(receive_buffer, content_size)));
	//thread_worker.post(boost::bind(c_process_msg, callback_arg, std::move(std::string(data_buffer,content_size))));
        delete[] receive_buffer;
        return 0;
   }

    ssize_t communicate(std::string send_buffer, uint64_t _seq_id){
        sync_send(send_buffer, _seq_id);
        return 0;
    }
    
    // called by client.
    void aio_communicate(std::string& send_buffer, uint64_t _seq_id){
        Message msg(send_buffer, _seq_id);
        std::string send_string(std::move(msg.to_buffer()));
        uint64_t ret = send_string.size();
        send_lock.lock();
        boost::asio::async_write(socket_, boost::asio::buffer(send_string, ret), boost::asio::transfer_exactly(ret),
        //boost::asio::async_write(socket_, boost::asio::buffer(send_string, ret),
            [this, ret, send_string](  
                const boost::system::error_code& ec, uint64_t cb) {
                send_lock.unlock();
                if (!ec) {
                    if(ret != cb){
                        std::cout<<"asio_session::aync_send failed: ret != cb"<<std::endl;
                        assert(0);
                    }
                    ++send_index;
                }else{
                    error_handing("asio_messenger::aio_communicate: sending failed", ec);
                }
            });
    }

    // called by client.
    void aio_receive(){
        boost::asio::async_read(socket_, boost::asio::buffer(msg_header, header_len), boost::asio::transfer_exactly(header_len),
            [ this](const boost::system::error_code& err, uint64_t cb) {
                if(!err){
                    if(cb != header_len){
                        std::cout<<"asio_messenger::async_receive, firstly async_read failed: cb != header_len" << std::endl;
                        assert(0);
                    }
                    uint64_t content_size = ((MsgHeader*)msg_header)->get_data_size();
                    char* data_buffer = new char[content_size+1]();
                    uint64_t sequence_id = ((MsgHeader*)msg_header)->get_seq_id(); 
                    boost::asio::async_read( socket_, boost::asio::buffer(data_buffer, content_size ), boost::asio::transfer_exactly(content_size),
                        [this, data_buffer, content_size, sequence_id]( const boost::system::error_code& err, uint64_t cb ){
                            if(!err){
                                if( content_size != cb ){
                                    std::cout<<"asio_messenger::async_receive, firstly async_read failed: cb != header_len" << std::endl;
                                    assert(0);
                                }
                                if(!check_crc(msg_header, data_buffer, content_size)){
                                    std::cout<<"asio_messenger::async_receive, CRC failed." << std::endl;
                                    assert(0);
                                }
                                ++receive_index;
                                aio_receive();

                                //trigger request handler first
				c_process_msg(callback_arg, std::move(std::string(data_buffer, content_size)));
                                if(pending_msg_map.size()!=0){
                                    pending_msg_map_lock.lock();
                                    // lock? TODO
                                    pending_msg_map[sequence_id]->Done();
                                    delete pending_msg_map[sequence_id];
                                    pending_msg_map.erase(sequence_id);
                                    pending_msg_map_lock.unlock();
                                }
				//thread_worker.post(boost::bind(c_process_msg, callback_arg, std::move(std::string(data_buffer,content_size))));
                            }else{   
                                error_handing("asio_messenger::aio_receive: receiving content failed", err);
                            }
 			    delete[] data_buffer;
                        });
               }else{ 
                   error_handing("asio_messenger::aio_receive: receiving header failed", err);
              }
        });
    }

    // server receive loop
    // receive msg, hdcs handle it, then send ack.
   void aio_receive(SessionArg* arg){
        boost::asio::async_read(socket_, boost::asio::buffer(msg_header, header_len), boost::asio::transfer_exactly(header_len),
            [ this, arg](const boost::system::error_code& err, uint64_t cb) {
                if(!err){
                    if(cb != header_len){
                        std::cout<<"asio_messenger::async_receive, firstly async_read failed: cb != header_len" << std::endl;
                        assert(0);
                    }
                    uint64_t content_size = ((MsgHeader*)msg_header)->get_data_size();
                    arg->set_seq_id(((MsgHeader*)msg_header)->get_seq_id());
                    char* data_buffer = new char[content_size+1]();
                    boost::asio::async_read( socket_, boost::asio::buffer(data_buffer, content_size ), boost::asio::transfer_exactly(content_size),
                        [this, arg, data_buffer, content_size]( const boost::system::error_code& err, uint64_t cb ){
                            if(!err){
                                if( content_size != cb ){
                                    std::cout<<"asio_messenger::async_receive, firstly async_read failed: cb != header_len" << std::endl;
                                    assert(0);
                                }
                                if(!check_crc(msg_header, data_buffer, content_size)){
                                    std::cout<<"asio_messenger::async_receive, CRC failed." << std::endl;
                                    assert(0);
                                }
                                ++receive_index;
				s_process_msg((void*)arg, std::move(std::string(data_buffer,content_size)));
			        aio_receive(arg);
				//thread_worker.post(boost::bind(s_process_msg, arg, std::move(std::string(data_buffer,content_size))));
                            }else{ 
                                error_handing("asio_messenger::aio_receive: receiving content failed", err);
                            }
 			    delete[] data_buffer;
                        });
               }else{ 
                   error_handing("asio_messenger::aio_receive: receiving header failed", err);
               }
        });
    }

private:

   // will unitfy to encode error, then using switch.
   // TODO
   void error_handing(const std::string fail_reason, const boost::system::error_code& err){
       if(err == boost::asio::error::eof){
          close();
          return;
       }
       if(err == boost::asio::error::operation_aborted){
           if(send_index == receive_index){
               // TODO
           }else{
               std::cout<<fail_reason<<" : "<<err.message()<<std::endl;
           }
           close();
           return;
       }
       // other error just close socket.
       std::cout<<fail_reason<<" : "<<err.message()<<std::endl;
       close();
       return;
   }

}; 
}
}

#endif

