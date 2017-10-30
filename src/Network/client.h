#include <algorithm>
#include <iostream>
#include <list>
#include <string>
#include <mutex>
#include <assert.h>
#include <vector>
#include <memory>
#include <thread>
#include <boost/asio/steady_timer.hpp>
#include "io_pool.h"
#include "common/C_AioRequestCompletion.h"
#include "Network/Message.h"

ssize_t request_handler(char msg_content[]);

class session
{
public:
    session(boost::asio::io_service& ios)
        : io_service_(ios),
        socket_(ios),
        counts(0),
        buffer_(new char[sizeof(MsgHeader)]) {}

    ~session() {
        delete[] buffer_;
    }

    void aio_write(std::string send_buffer) {
      Message msg(send_buffer);
      boost::asio::async_write(socket_, boost::asio::buffer(msg.to_buffer()),
        [this](const boost::system::error_code& err, uint64_t cb) {
        if (!err) {
          counts++;
        } else {
        }
      });
    }

    void write(std::string send_buffer) {
      Message msg(send_buffer);
      boost::asio::write(socket_,  boost::asio::buffer(msg.to_buffer()));
      counts++;
    }

    void aio_communicate(std::string send_buffer) {
      Message msg(send_buffer);
      boost::asio::async_write(socket_, boost::asio::buffer(msg.to_buffer()),
        [this](const boost::system::error_code& err, uint64_t cb) {
        if (!err) {
          counts++;
          read();
        } else {
        }
      });
    }

    void aio_read() {
      boost::asio::async_read(socket_, boost::asio::buffer(buffer_, sizeof(MsgHeader)),
        [this](const boost::system::error_code& err, uint64_t exact_read_bytes) {
        if (!err) {
          counts--;
          assert(exact_read_bytes == sizeof(MsgHeader));
          uint64_t content_size = ((MsgHeader*)buffer_)->get_data_size();
          //printf("client about to receive ack, size is %lu\n", content_size);
          char* data_buffer = (char*)malloc(content_size);
          exact_read_bytes = socket_.read_some(boost::asio::buffer(data_buffer, content_size));
          if (exact_read_bytes == content_size) {
            request_handler(data_buffer); 
            free(data_buffer);
          } else {
            printf("client received bytes : %lu, less than expected %lu bytes\n", exact_read_bytes, content_size);
          }
        } else {
        }
      });
    }

    ssize_t read() {
      //printf("client session %p about to receive header\n", this);
      ssize_t exact_read_bytes = socket_.read_some(boost::asio::buffer(buffer_, sizeof(MsgHeader)));
      ssize_t ret = 0;
      counts--;
      assert(exact_read_bytes == sizeof(MsgHeader));
      uint64_t content_size = ((MsgHeader*)buffer_)->get_data_size();
      //printf("client session %p about to receive ack, size is %lu\n", this, content_size);
      char* data_buffer = (char*)malloc(content_size);
      exact_read_bytes = socket_.read_some(boost::asio::buffer(data_buffer, content_size));
      //printf("client session %p received content\n", this);
      if (exact_read_bytes == content_size) {
        ret = request_handler(data_buffer); 
        free(data_buffer);
      }
      return ret;
    }

    void start(boost::asio::ip::tcp::endpoint endpoint) {
      try {
        socket_.connect(endpoint);
        boost::asio::ip::tcp::no_delay no_delay(true);
        socket_.set_option(no_delay);
      } catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << "\n";
      }
    }

    void stop() {
        uint64_t tmp;
        int retry = 0;
        while(counts > 0 && retry < 1){
          tmp = counts;
          printf("session %p still has %lu inflight ios\n", this, tmp);
          sleep(1);
          retry++;
        }
        io_service_.post([this]() {
            socket_.close();
            printf("session %p closed\n", this);
        });
    }

private:
    boost::asio::io_service& io_service_;
    boost::asio::ip::tcp::socket socket_;
    uint64_t block_size_;
    char* buffer_;
    std::atomic<uint64_t> counts;
};

class Connection {
public:
    Connection()
        : thread_count_(8), s_id(0), session_count(8){
        io_services_.resize(thread_count_);
        io_works_.resize(thread_count_);
        threads_.resize(thread_count_);

        for (auto i = 0; i < thread_count_; ++i) {
            io_services_[i].reset(new boost::asio::io_service);
            io_works_[i].reset(new boost::asio::io_service::work(*io_services_[i]));
            threads_[i].reset(new std::thread([this, i]() {
                auto& io_service = *io_services_[i];
                //io_service.run();
                try {
                    io_service.run();
                } catch (std::exception& e) {
                    std::cerr << "Exception: " << e.what() << "\n";
                    close();
                }
            }));
        }

        resolver_.reset(new boost::asio::ip::tcp::resolver(*io_services_[0]));
        for (size_t i = 0; i < session_count; ++i)
        {
            auto& io_service = *io_services_[i % thread_count_];
            std::unique_ptr<session> new_session(new session(io_service));
            sessions_.emplace_back(std::move(new_session));
        }
    }

    ~Connection() {
        for (auto& io_work : io_works_) {
            io_work.reset();
        }
    }

    void close() {
        for (auto& session : sessions_) {
          session->stop();
        }
    }

    void connect( std::string host, std::string port ) {
        boost::asio::ip::tcp::resolver::iterator iter =
            resolver_->resolve(boost::asio::ip::tcp::resolver::query(host, port));
        endpoint_ = *iter;
        start();
    }

    void start() {
        for (auto& session : sessions_) {
            session->start(endpoint_);
        }
    }

    void wait() {
        for (auto& thread : threads_) {
            thread->join();
        }
    }

    int aio_communicate(std::string send_buffer) {
        sessions_[s_id]->aio_communicate(send_buffer);
        if (++s_id >= session_count) s_id = 0;
        return 0;
    }

    ssize_t communicate(std::string send_buffer) {
      ssize_t ret = 0;
      sessions_[s_id]->write(send_buffer);
      ret = sessions_[s_id]->read();
      if (++s_id >= session_count) s_id = 0;
      return ret;
    }

private:
    std::atomic<int> s_id;
    int const thread_count_;
    int const session_count;
    std::vector<std::unique_ptr<boost::asio::io_service>> io_services_;
    std::vector<std::unique_ptr<boost::asio::io_service::work>> io_works_;
    std::unique_ptr<boost::asio::ip::tcp::resolver> resolver_;
    std::vector<std::unique_ptr<std::thread>> threads_;
    std::vector<std::unique_ptr<session>> sessions_;
    boost::asio::ip::tcp::endpoint endpoint_;
};



