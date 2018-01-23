#ifndef CONNECT_H
#define CONNECT_H

#include "rdma_messenger/Callback.h"
#include "rdma_messenger/RDMAStack.h"

class ConnectThread : public ThreadWrapper {
  public:
    ConnectThread(RDMAStack* stack_) : stack(stack_) {}
    virtual ~ConnectThread() {}

    virtual void entry() override; 
    virtual void abort() override;
  private:
    RDMAStack *stack;
};

class Connector {
  public:
    Connector(struct sockaddr* addr);
    ~Connector();

    void connect();
    void join();
    void shutdown();
    void set_network_stack(RDMAStack *stack_);
    void set_connect_callback(Callback *callback);
  private:
    Callback *connect_callback;
    ConnectThread *connect_thread;
    RDMAStack *stack;
    struct sockaddr* connector_addr;
};

#endif
