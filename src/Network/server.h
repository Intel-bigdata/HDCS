#include <algorithm>
#include <iostream>
#include <list>
#include <string>
#include <mutex>
#include <assert.h>
#include <vector>
#include <memory>
#include <queue>
#include <stack>
#include <thread>
#include <future>
#include "io_pool.h"
#include "Message.h"
#include "../HDCS_REQUEST_CTX.h"

typedef std::function<void(void*, std::string)> callback_t;

class session : public std::enable_shared_from_this<session>
{
public:
    session(boost::asio::io_service& ios, callback_t cb)
        : io_service_(ios)
        , socket_(ios)
        , cb(cb)  
        , buffer_(new char[sizeof(MsgHeader)]) {
    }

    ~session() {
        delete[] buffer_;
    }

    boost::asio::ip::tcp::socket& socket() {
        return socket_;
    }

    void start() {
        boost::asio::ip::tcp::no_delay no_delay(true);
        socket_.set_option(no_delay);
        aio_read();
    }

    void aio_write(std::string send_buffer) {
      Message msg(send_buffer);
      boost::asio::async_write(socket_, boost::asio::buffer(msg.to_buffer()),
          [this, self = shared_from_this()](
            const boost::system::error_code& err, size_t cb) {
            if (!err) {
            }
      });
    }

    void aio_read() {
      boost::asio::async_read(socket_, boost::asio::buffer(buffer_, sizeof(MsgHeader)),
          [this, self = shared_from_this()](
              const boost::system::error_code& err, size_t exact_read_bytes) {
          if (!err) {
            assert(exact_read_bytes == sizeof(MsgHeader));
            uint64_t content_size = ((MsgHeader*)buffer_)->get_data_size();
            char* data_buffer = (char*)malloc(content_size);
            exact_read_bytes = socket_.read_some(boost::asio::buffer(data_buffer, content_size));
            if (exact_read_bytes == content_size) {
              cb((void*)this, std::move(std::string(data_buffer, content_size)));
              free(data_buffer);
              aio_read();
            } else {
              printf("received: %lu data, not equal to expected size: %lu\n", exact_read_bytes, content_size);
            }
          }
      });
    }

private:
    boost::asio::io_service& io_service_;
    boost::asio::ip::tcp::socket socket_;
    char* buffer_;
    callback_t cb;
};

class server
{
public:
  server(int thread_count, std::string host, std::string port)
        : thread_count_(thread_count)
        , service_pool_(thread_count)
        , acceptor_(service_pool_.get_io_service()) {
        auto endpoint = boost::asio::ip::tcp::endpoint(
          boost::asio::ip::address::from_string(host), stoi(port));
        acceptor_.open(endpoint.protocol());
        acceptor_.set_option(boost::asio::ip::tcp::acceptor::reuse_address(1));
        acceptor_.bind(endpoint);
        acceptor_.listen();
    }

    void start(callback_t task) {
        std::shared_ptr<session> new_session(new session(
            service_pool_.get_io_service(), task));
        auto& socket = new_session->socket();
        acceptor_.async_accept(socket,
            [this, new_session = std::move(new_session), task](
                const boost::system::error_code& err) {
            if (!err) {
                new_session->start();
                start(task);
            }
        });
    }

    void wait() {
      service_pool_.run();
    }
    
    void send(void* session_id, std::string send_buffer) {
      ((session*)session_id)->aio_write(send_buffer);
    }
private:
    int const thread_count_;
    io_service_pool service_pool_;
    boost::asio::ip::tcp::acceptor acceptor_;    
};
