#ifndef RDMASTACK_H
#define RDMASTACK_H

#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <semaphore.h>
#include <rdma/rdma_cma.h>
#include <stdio.h>
#include <malloc.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <set>
#include <unordered_map>
#include <atomic>

#include "rdma_messenger/Callback.h"
#include "rdma_messenger/Common.h"
#include "rdma_messenger/RDMAConnection.h"
#include "rdma_messenger/ThreadWrapper.h"

enum cm_event_state {
  IDLE = 1,
  CONNECT_REQUEST,
  ADDR_RESOLVED,
  ROUTE_RESOLVED,
  CONNECTED,
  RDMA_READ_ADV,
  RDMA_READ_COMPLETE,
  RDMA_WRITE_ADV,
  RDMA_WRITE_COMPLETE,
  DISCONNECTED,
  ERROR  
};

class RDMAStack;
class CQThread;

class RDMAConMgr {
  public:
    RDMAConMgr(RDMAStack* stack_);
    ~RDMAConMgr();
    RDMAConnection* get_connection(uint32_t qp_num);
    RDMAConnection* new_connection(struct rdma_cm_id *cm_id_);
    void add();
    void del(uint64_t con_id_, uint32_t qp_num_);
  private:
    uint64_t con_id = 0;
    RDMAStack *stack;
    std::unordered_map<uint32_t, RDMAConnection*> qp_con_map;
    std::unordered_map<uint64_t, RDMAConnection*> seq_map;
    std::unordered_map<uint64_t, struct ibv_cq*> cq_map;
    std::vector<CQThread*> cq_threads;
};

class RDMAStack {
  public:
    RDMAStack();
    ~RDMAStack();
    void init(bool is_server_);
    void cm_event_handler();
    void cq_event_handler(struct ibv_comp_channel *cq_channel, struct ibv_cq *cq);
    void handle_recv(struct ibv_wc *wc);
    void handle_send(struct ibv_wc *wc);
    void handle_err(struct ibv_wc *wc);
    void listen(struct sockaddr *addr);
    void accept(struct rdma_cm_id *cm_id);
    void connect(struct sockaddr *addr);
    void accept_abort();
    void connection_abort();
    void shutdown();
    void set_accept_callback(Callback *accept_callback_);
    void set_connect_callback(Callback *connect_callback_);
  private:
    RDMAConnection* connection_establish(struct rdma_cm_id* cm_id_);
  private:
    sem_t sem;
    cm_event_state state;      
    struct rdma_event_channel *cm_channel;
    struct rdma_cm_id *cm_id;
    Callback *accept_callback;
    Callback *connect_callback;
    RDMAConMgr *con_mgr;
    std::atomic<bool> stop;
    bool is_server;
};

class CQThread : public ThreadWrapper {
  public:
    CQThread(RDMAStack *stack_, struct ibv_comp_channel *cq_channel_, struct ibv_cq *cq_) : stack(stack_), cq_channel(cq_channel_), cq(cq_) {}
    virtual ~CQThread() {}
    virtual void entry() {
      stack->cq_event_handler(cq_channel, cq);
    }
    virtual void abort() {
    }
  private:
    RDMAStack *stack;
    struct ibv_comp_channel *cq_channel;
    struct ibv_cq *cq;
};

#endif
