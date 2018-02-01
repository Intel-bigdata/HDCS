#ifndef ACCEPT_H
#define ACCEPT_H

#include <stdio.h>

#include <cstdint>

#include "rdma_messenger/ThreadWrapper.h"
#include "rdma_messenger/RDMAStack.h"
#include "rdma_messenger/Callback.h"

class AcceptThread : public ThreadWrapper {
  public:
    AcceptThread(RDMAStack* stack_) : stack(stack_) {}
    virtual ~AcceptThread() {}

    virtual void entry() override; 
    virtual void abort() override;
  private:
    RDMAStack *stack;
};

class Acceptor {
  public:
    Acceptor() = delete;
    Acceptor(struct sockaddr* addr);
    ~Acceptor();

    void listen();
    void join();
    void shutdown();
    void set_network_stack(RDMAStack *stack_);
    void set_accept_callback(Callback *callback);
  private:
    Callback *accept_callback;
    AcceptThread *accept_thread;
    RDMAStack *stack;
    struct sockaddr* server_addr;
};

#endif
