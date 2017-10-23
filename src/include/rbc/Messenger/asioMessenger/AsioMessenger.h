#ifndef RBC_ASIO_MESSENGER
#define RBC_ASIO_MESSENGER

#include <boost/bind.hpp>
#include <boost/asio.hpp>
#include <mutex>
#include <condition_variable>

namespace rbc{
using boost::asio::ip::tcp;
typedef void (*asio_callback_t)(void *arg, Msg* msg);

class AsioMessenger: public Messenger{
private:
    tcp::resolver resolver;
    tcp::socket* socket_;
    bool if_server;
    bool connected;
    char msg_header[MSG_HEADER_LEN];
    asio_callback_t handle_cb;
    void* handle_arg;
public:
    AsioMessenger(boost::asio::io_service& io_service, void* accepted_socket, const char* host = NULL, const char* port = NULL):connected(false), resolver(io_service),handle_cb(NULL),handle_arg(NULL){
        if(accepted_socket){ // for AsioListen
            if_server=true;
            socket_ = (tcp::socket*)accepted_socket;
            socket_->set_option( boost::asio::ip::tcp::no_delay( true ) );
            start_receive();
        }else{
            if_server=false;
            socket_ = new tcp::socket( io_service );
            tcp::resolver::query query( host, port );
            tcp::endpoint endpoint = *resolver.resolve(query);
            socket_->connect(endpoint);
            socket_->set_option( boost::asio::ip::tcp::no_delay( true ) );
            std::cout<<"Asioclient::connection success"<<std::endl;
        }
    }

    ~AsioMessenger(){
        stop();
        delete socket_;
    }

    uint32_t get_local_port(){
        return socket_->local_endpoint().port();
    }

    void stop(){
        try{
            socket_->close();
        }catch(std::exception& e){
            std::cout << "Exception: " << e.what() << "\n";
        }
        connected = false;
    }

    void set_to_connected(){
        printf("set socket to connected\n");
        connected = true;
        socket_->set_option( boost::asio::ip::tcp::no_delay(true) );
    }

    void start(){
        start_receive();
    }

    void start_receive(){
        async_read(
            *socket_,
            boost::asio::buffer( msg_header, MSG_HEADER_LEN ),
            boost::bind(
                &AsioMessenger::handle_receive,
                this,
                boost::asio::placeholders::error,
                boost::asio::placeholders::bytes_transferred
            )
        );
    }

    void receive_data( char* data, ssize_t bytes_to_recv ){
        ssize_t exact_bytes_received = 0;
        ssize_t left = bytes_to_recv;
        ssize_t offset = 0;
        while( left ){
            exact_bytes_received = socket_->read_some(
                boost::asio::buffer( &data[offset], left )
            );
            left -= exact_bytes_received;
            offset += exact_bytes_received;
        }
    }

    void handle_receive(const boost::system::error_code& error, size_t bytes_recvd){
        if (!error){
            if( bytes_recvd ){
                std::string data(msg_header, MSG_HEADER_LEN);
                Msg *msg = new Msg( data );
                //std::cout << "msg received, content_length:" << msg->content_length() << std::endl;
                uint64_t location_id_length = msg->length() - msg->content_length();
                char* location_id = new char[location_id_length+1]();
                receive_data( location_id, location_id_length );
                msg->set_location_id( location_id );
                if( msg->content_length() ){
                    char* content = new char[msg->content_length()+1]();
                    receive_data( content, msg->content_length() );
                    msg->set_content( content );
                }
                if(handle_cb) handle_cb( handle_arg, msg );
            }
        }else{
            if( error == boost::asio::error::eof ){
                if(if_server){
                    log_print("asiolisten::AsioMessenger::handle_receive Socket closed by peer\n");
                }else{
                    log_print("asioclient::AsioMessenger::handle_receive Socket closed by peer\n");
                }
            }else{
                log_print("AsioMessenger::handle_receive failed, error:%s \n", error.message().c_str());
            }
            stop(); 
            return;
        }
        start_receive();
    }

    int send_request( Msg* msg, void* arg = NULL ){
        int ret = 0;
        if(msg->header.type == MSG_REPLY_STAT || msg->header.type == MSG_REPLY_DATA ){
            ret = start_send( msg->toString() );
            msg->delete_location_id();
            msg->delete_content();
        }else if(msg->header.type == MSG_WRITE || msg->header.type == MSG_READ){
            if( arg != NULL ) msg->set_callback( arg );
            ret = start_send( msg->toString() );
        }
        return ret;
    }

    int start_send( const char* data_, ssize_t length ){
        ssize_t offset = 0;
        ssize_t exact_send_bytes = 0;
        ssize_t left = length - offset;

        while( left ){
            try{
                exact_send_bytes = socket_->send(
                    boost::asio::buffer( &data_[offset], left )
                );
            }catch (std::exception& e){
                std::cout << "ASIO Messenger sending MSG failed, Exception: " << e.what() << "\n";
                return -1;
            }
            left -= exact_send_bytes;
            offset += exact_send_bytes;
        }
        return 0;
    }

/*
    int start_send(const char* data_, ssize_t length){
        boost::asio::async_write(*socket_, boost::asio::buffer(data_, length),
            [this](
                 const boost::system::error_code& err, size_t cb) {
             if (err) {
                 assert(0);
             }
         });
       return 0;
    }
*/
    int start_send(std::string data){
        return start_send( data.c_str(), data.length() );
    }

    void set_callback( asio_callback_t cb, void* arg ){
        handle_cb = cb;
        handle_arg = arg;
    }
    
};

}
#endif
