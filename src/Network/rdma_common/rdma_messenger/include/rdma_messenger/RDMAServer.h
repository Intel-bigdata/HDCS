#ifndef RDMASERVER_H
#define RDMASERVER_H

#include <string>
#include <mutex>
#include <thread>
#include <condition_variable>

#include "rdma_messenger/Acceptor.h"

class RDMAServer {
  public:
    RDMAServer(struct sockaddr *addr) : server_addr(addr), finished(false) {
      stack =  new RDMAStack();
      stack->init(true);
      acceptor = new Acceptor(server_addr); 
      acceptor->set_network_stack(stack);
    }
    ~RDMAServer() {
      delete stack;
      delete acceptor;
    }
    void start(Callback *accept_callback) {
      acceptor->set_accept_callback(accept_callback);
      acceptor->listen();
    }
    void wait() {
      std::unique_lock<std::mutex> l(finish_mtx);
      cv.wait(l, [&] { return finished; });
    }
    void close() {
      acceptor->shutdown(); 
      std::unique_lock<std::mutex> l(finish_mtx);
      finished = true;
    }
  private:
    RDMAStack *stack;
    Acceptor *acceptor;
    struct sockaddr *server_addr;
    std::mutex finish_mtx;
    std::condition_variable cv;
    bool finished;
};

#endif
