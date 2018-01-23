#ifndef RDMACLIENT_H
#define RDMACLIENT_H

#include "rdma_messenger/Connector.h"

class RDMAClient {
  public:
    RDMAClient(struct sockaddr *addr, uint32_t con_nums_) : client_addr(addr), con_nums(con_nums_) {
      stack = new RDMAStack();
      connector = new Connector(addr);
      connector->set_network_stack(stack);
    }
    ~RDMAClient() {
      delete stack;
      delete connector;
    }
    void connect(Callback *connect_callback) {
      assert(stack);
      connector->set_connect_callback(connect_callback); 
      for (uint32_t i = 0; i < con_nums; ++i) {
        stack->init(false);
        printf("connecting...\n");
        connector->connect(); 
      }
    }
    void wait() {
      std::unique_lock<std::mutex> l(finish_mtx); 
      cv.wait(l, [&] {return finished; });
    }
    void close() {
      connector->shutdown();
      std::unique_lock<std::mutex> l(finish_mtx);
      finished = true;
    }
  private:
    struct sockaddr *client_addr;
    RDMAStack *stack;
    Connector *connector;
    uint32_t con_nums;
    std::mutex finish_mtx;
    std::condition_variable cv;
    bool finished;
};

#endif
