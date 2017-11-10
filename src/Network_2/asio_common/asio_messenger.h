#ifndef ASIO_MESSENGER
#define ASIO_MESSENGER
#include <boost/bind.hpp>
#include <boost/asio.hpp>
#include <condition_variable>
#include <memory>
#include <string>
#include "../common/Message.h"
#include "../common/aio_complete.h"
#include "../common/networking_common.h"
namespace hdcs{
namespace networking{
using boost::asio::ip::tcp;

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

public:
    asio_messenger( IOService& ios, int _role)
        : io_service_(ios)
        , strand_(ios)
        , socket_(ios)
        , header_len(sizeof(MsgHeader))
        , msg_header(new char[sizeof(MsgHeader)])
        , resolver_(ios)
        , role(_role) // 0 is client, and 1 is server
    {
        if(role==1){ 
            is_closed = false;
            //set_socket_option();
        }else{
            is_closed = false;
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
            return -1;
        }
        return 0;
    }

    void close(){
        if(is_closed){
            return;
        }
        boost::system::error_code ec;
        socket_.close(ec);
        if(ec){
            std::cout<<"asio_messenger::close, close failed: "<<ec.message()<<std::endl;
            assert(0);
        }
        is_closed = true;
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

    int sync_connection(std::string ip_address, std::string port){
        boost::system::error_code ec;
        boost::asio::ip::tcp::resolver::iterator iter =
            resolver_.resolve(boost::asio::ip::tcp::resolver::query(ip_address, port), ec);
        if(ec){
            std::cout<<"asio_messenger::sync_connection, resolver failed: "<<ec.message()<<std::endl;
            assert(0);
        }
        endpoint_ = *iter;
        socket_.connect(endpoint_, ec);
        if(ec){
            std::cout<<"asio_messenger::sync_connection, connect  failed: "<<ec.message()<<std::endl;
            assert(0);
        }
        int ret = set_socket_option();
        if(ret){
            assert(0);
        }
        is_closed = false;
        return 0;
    }

    int async_connection(){
        return 0;
    }

    //called by client.
    int sync_send( std::string send_buffer ) {
        boost::system::error_code ec;
        uint64_t ret;
        Message msg(send_buffer); // due to sync send, local object can live out sync_send operation.
        uint64_t send_bytes = msg.to_buffer().size();
        ret=boost::asio::write(socket_, boost::asio::buffer(std::move(msg.to_buffer())), ec);
        if(ec){
            std::cout<<"asio_messenger::sync_send failed: "<<ec.message()<<std::endl;
            assert(0);
        }
        if(ret != send_bytes){
            std::cout<<"asio_messenger::sync_send failed: ret != send_bytes "<<std::endl;
            assert(0);
        }
        return 0;
    }

    // called by server. client will call aio_communicate.
    int async_send(std::string send_buffer) {
        // sure that send_buffer live out async_operation..
        std::string* send_string = new std::string( Message(send_buffer).to_buffer());
        uint64_t ret = send_string->size();
        boost::asio::async_write(socket_, boost::asio::buffer(*send_string, ret),
            [this, ret, send_string](  
                const boost::system::error_code& ec, uint64_t cb) {
                if (!ec) {
                    if(ret != cb){
                        std::cout<<"asio_session::aync_send failed: ret != cb"<<std::endl;
                        assert(0);
                    }
                }else{
                    std::cout<<"asio_session::aync_send failed: "<<ec.message()<<std::endl;
                }
                delete send_string;
            });
    }

    // call by client.
    ssize_t sync_receive( ProcessMsgClient _process_msg ){
        boost::system::error_code ec;
        uint64_t ret;
        char msg_header[header_len];
        ret = boost::asio::read(socket_, boost::asio::buffer(msg_header, header_len), ec); 
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

        _process_msg(callback_arg, std::move(std::string(receive_buffer, content_size)));
        delete[] receive_buffer;
        return 0;
   }

    ssize_t communicate(std::string send_buffer, ProcessMsgClient _process_msg ){
        sync_send(send_buffer);
        sync_receive(_process_msg); 
        return 0;
    }
    
    // called by client.
    void aio_communicate(std::string send_buffer){
        std::string* send_string = new std::string( Message(send_buffer).to_buffer());
        uint64_t ret = send_string->size();
        boost::asio::async_write(socket_, boost::asio::buffer(*send_string, ret),
            [this, ret, send_string](  
                const boost::system::error_code& ec, uint64_t cb) {
                if (!ec) {
                    if(ret != cb){
                        std::cout<<"asio_session::aync_send failed: ret != cb"<<std::endl;
                        assert(0);
                    }
                }else{
                    std::cout<<"asio_session::aync_send failed: "<<ec.message()<<std::endl;
                }
                delete send_string; 
            });
    }

    // client receive loop
    void aio_receive(ProcessMsgClient process_msg){
        boost::asio::async_read(socket_, boost::asio::buffer(msg_header, header_len),
            [ this, process_msg](const boost::system::error_code& err, uint64_t cb) {
                if(!err){
                    if(cb != header_len){
                        std::cout<<"asio_messenger::async_receive, firstly async_read failed: cb != header_len" << std::endl;
                        assert(0);
                    }
                    uint64_t content_size = ((MsgHeader*)msg_header)->get_data_size();
                    char* data_buffer = new char[content_size+1]();
                    boost::asio::async_read( socket_, boost::asio::buffer(data_buffer, content_size ),
                        [this, data_buffer, content_size, process_msg]( const boost::system::error_code& err, uint64_t cb ){
                            if(!err){
                                if( content_size != cb ){
                                    std::cout<<"asio_messenger::async_receive, firstly async_read failed: cb != header_len" << std::endl;
                                    assert(0);
                                }
                                if(!check_crc(msg_header, data_buffer, content_size)){
                                    std::cout<<"asio_messenger::async_receive, CRC failed." << std::endl;
                                    assert(0);
                                }
				aio_receive(process_msg);
				process_msg(callback_arg, std::move(std::string(data_buffer, content_size)));
                            }else{ 
                                std::cout << "asio_messenger::async_receive: secondly async_read failed: " << err.message() << std::endl;
                            }
 			    delete[] data_buffer;
                        });
               }else{ 
                   std::cout << "asio_messenger::async_receive: firstly async_read failed: " << err.message() << std::endl;
              }
        });
    }
    
    // server receive loop
    // receive msg, then handle it.
   void aio_receive(void* arg, ProcessMsg process_msg){
        boost::asio::async_read(socket_, boost::asio::buffer(msg_header, header_len),
            [ this, process_msg, arg](const boost::system::error_code& err, uint64_t cb) {
                if(!err){
                    if(cb != header_len){
                        std::cout<<"asio_messenger::async_receive, firstly async_read failed: cb != header_len" << std::endl;
                        assert(0);
                    }
                    uint64_t content_size = ((MsgHeader*)msg_header)->get_data_size();
                    char* data_buffer = new char[content_size+1]();
                    boost::asio::async_read( socket_, boost::asio::buffer(data_buffer, content_size ),
                        [this, arg, data_buffer, content_size, process_msg]( const boost::system::error_code& err, uint64_t cb ){
                            if(!err){
                                if( content_size != cb ){
                                    std::cout<<"asio_messenger::async_receive, firstly async_read failed: cb != header_len" << std::endl;
                                    assert(0);
                                }
                                if(!check_crc(msg_header, data_buffer, content_size)){
                                    std::cout<<"asio_messenger::async_receive, CRC failed." << std::endl;
                                    assert(0);
                                }
				aio_receive(arg, process_msg);
				process_msg(arg, std::move(std::string(data_buffer,content_size))); //process_msg will call async_send.
                            }else{ 
                                std::cout << "asio_messenger::async_receive: secondly async_read failed: " << err.message() << std::endl;
                            }
 			    delete[] data_buffer;
                        });
               }else{ 
                   std::cout << "asio_messenger::async_receive: firstly async_read failed: " << err.message() << std::endl;
              }
        });
    }

};

}
}

#endif

