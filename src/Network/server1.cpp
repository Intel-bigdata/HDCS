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
#include "io_pool.h"
#include "Message.h"
#include "../HDCS_REQUEST_CTX.h"


class session : public std::enable_shared_from_this<session>
{
public:
    session(boost::asio::io_service& ios)
        : io_service_(ios)
        , socket_(ios)
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

    void aio_write(char* data, uint64_t size) {
      Message msg(std::string(data, size));
        boost::asio::async_write(socket_, boost::asio::buffer(msg.to_buffer()),
            [this, self = shared_from_this(), data](
                const boost::system::error_code& err, size_t cb) {
            if (!err) {
                free(data);
                aio_read();
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
                aio_write(data_buffer, sizeof(hdcs::HDCS_REQUEST_CTX_T));
              } else {
                printf("received: %lu data, not finished yet\n", exact_read_bytes);
              }
            }
        });
    }

private:
    boost::asio::io_service& io_service_;
    boost::asio::ip::tcp::socket socket_;
    char* buffer_;
};

class server
{
public:
    server(int thread_count, char const* host, char const* port)
        : thread_count_(thread_count)
        , service_pool_(thread_count)
        , acceptor_(service_pool_.get_io_service())
    {
        auto endpoint = boost::asio::ip::tcp::endpoint(
          boost::asio::ip::address::from_string(host), atoi(port));
        acceptor_.open(endpoint.protocol());
        acceptor_.set_option(boost::asio::ip::tcp::acceptor::reuse_address(1));
        acceptor_.bind(endpoint);
        acceptor_.listen();
    }

    void start() {
        accept();
    }

    void wait() {
        service_pool_.run();
    }
private:
    void accept() {
        std::shared_ptr<session> new_session(new session(
            service_pool_.get_io_service()));
        auto& socket = new_session->socket();
        acceptor_.async_accept(socket,
            [this, new_session = std::move(new_session)](
                const boost::system::error_code& err) {
            if (!err) {
                new_session->start();
                accept();
            }
        });
    }

private:
    int const thread_count_;
    io_service_pool service_pool_;
    boost::asio::ip::tcp::acceptor acceptor_;    
};


void server_test1(int thread_count, char const* host, char const* port) {
    server s(thread_count, host, port);
    s.start();
    s.wait();
}

int main(int argc, char* argv[]) {
  if (argc != 4) {
    std::cout << "Usage:\n\t" << argv[0] << " ip port threads_num\n" << std::endl;
    return -1;
  }
  server_test1(std::atoi(argv[3]), argv[1], argv[2]);
  return 0;
}
