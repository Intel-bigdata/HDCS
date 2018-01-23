#ifndef RDMACONNECTION_H
#define RDMACONNECTION_H

#include <stdint.h>
#include <rdma/rdma_cma.h>
#include <assert.h>
#include <string.h>
#include <malloc.h>

#include <vector>
#include <mutex>

#include "rdma_messenger/Common.h"
#include "rdma_messenger/Callback.h"
#include "rdma_messenger/Buffer.h"

enum connection_state {
  INACTIVE = 1,
  ACTIVE,
  CLOSE
};

class Chunk {
  public:
    Chunk(struct ibv_mr *_mr, char *_chk_buf, uint32_t _chk_size) : 
		mr(_mr), chk_buf(_chk_buf), chk_size(_chk_size) {}
    struct ibv_mr *mr;
    char *chk_buf;
    uint32_t chk_size;
};

class RDMAConnection {
  public:
    RDMAConnection(struct ibv_pd *pd_, struct ibv_cq *cq_, struct rdma_cm_id *cm_id_, uint64_t con_id_);
    ~RDMAConnection();

    void async_send(const char *raw_msg, uint32_t raw_msg_size);
    void async_recv(const char *raw_msg, uint32_t raw_msg_size);

    void async_sendv(std::vector<char*> &raw_msg, std::vector<uint32_t> &raw_msg_size);

    // post & recv rdma buffer
    void post_recv_buffers();
    void post_recv_buffer(Chunk* ck);
    void fin();
  
    // maintain chunk list for posting buffer
    void get_chunk(Chunk **ck);
    void reap_chunk(Chunk **ck);

    struct ibv_qp* get_qp();
    struct ibv_cq* get_cq();
    uint64_t get_con_id(); 

    uint32_t write_buffer(const char *raw_msg, uint32_t raw_msg_size);
    uint32_t read_buffer(char *raw_msg, uint32_t raw_msg_size);

    void set_read_callback(Callback *callback_);

    void close();
  public:
    // connection status
    connection_state state = INACTIVE;
    Callback *read_callback;
  private:
    // create rdma qp
    void qp_setup();
    // allocate new memory for rdma mr
    void mr_setup();
    void post_send(const char *raw_msg, uint32_t raw_msg_size);
    void post_send_batch(std::vector<char*> &raw_msg, std::vector<uint32_t> &raw_msg_size);
  private:
    struct ibv_pd *pd;
    struct ibv_cq *cq;
    struct ibv_qp *qp;
    struct rdma_cm_id *cm_id;
    // wrapper for rdma buffer
    Chunk **recv_chunk;
    Chunk **send_chunk;
    // real rdma buffer
    char *recv_buf;
    char *send_buf;
    std::vector<Chunk*> free_chunks;
    std::mutex chk_mtx;
    uint64_t con_id;
    Buffer con_buf;
};

#endif

