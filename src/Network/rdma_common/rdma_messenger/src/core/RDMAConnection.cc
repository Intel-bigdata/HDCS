#include "rdma_messenger/RDMAConnection.h"

RDMAConnection::RDMAConnection(struct ibv_pd *pd_, struct ibv_cq *cq_, struct rdma_cm_id *cm_id_, uint64_t con_id_) {
  pd = pd_; 
  cq = cq_;
  cm_id = cm_id_;
  con_id = con_id_;
  qp_setup();
  mr_setup();
  state = ACTIVE;
}

RDMAConnection::~RDMAConnection() {
  for (int i = 0; i < RECV_QUEUE_SIZE_PER_QP; ++i) {
    delete *(recv_chunk+i);
  }
  free(recv_chunk);
  for (int i = 0; i < SEND_QUEUE_SIZE_PER_QP; ++i) {
    delete *(send_chunk+i);
  } 
  free(send_buf);
}

void RDMAConnection::async_send(const char *raw_msg, uint32_t raw_msg_size) {
  post_send(raw_msg, raw_msg_size);
}

void RDMAConnection::async_sendv(std::vector<char*> &raw_msg, std::vector<uint32_t> &raw_msg_size) {
  post_send_batch(raw_msg, raw_msg_size);
}

struct ibv_qp* RDMAConnection::get_qp() {
  return qp;
}

struct ibv_cq* RDMAConnection::get_cq() {
  return cq;
}

uint64_t RDMAConnection::get_con_id() {
  return con_id;
}

uint32_t RDMAConnection::write_buffer(const char *raw_msg, uint32_t raw_msg_size) {
  if (con_buf.write_buf(raw_msg, raw_msg_size) > 0) {
    return raw_msg_size;
  }
  return 0;
}

uint32_t RDMAConnection::read_buffer(char *raw_msg, uint32_t raw_msg_size) {
  if (con_buf.read_buf(raw_msg, raw_msg_size) > 0) 
    return raw_msg_size;
  return 0;
}

void RDMAConnection::set_read_callback(Callback *callback_) {
  read_callback = callback_;
}

void RDMAConnection::close() {
  delete this;   
}

void RDMAConnection::post_recv_buffers() {
  int8_t ret = 0;

  struct ibv_recv_wr *bad_wr;
  struct ibv_sge recv_sge;
  struct ibv_recv_wr recv_wr;
  for (int buffer_index = 0; buffer_index < RECV_QUEUE_SIZE_PER_QP; ++buffer_index) {
    recv_sge.addr = (uintptr_t) (*(recv_chunk+buffer_index))->chk_buf; 
    recv_sge.length = QUEUE_BUFFER_SIZE; 
    recv_sge.lkey = (*(recv_chunk+buffer_index))->mr->lkey;

    recv_wr.sg_list = &recv_sge; 
    recv_wr.num_sge = 1;
    recv_wr.next = NULL;
    recv_wr.wr_id = (uint64_t) (*(recv_chunk+buffer_index));
    ret = ibv_post_recv(qp, &recv_wr, &bad_wr);
    if (ret) {
      fprintf(stderr, "ibv_post_recv failed: %d\n", ret);
    }
  }
}

void RDMAConnection::post_recv_buffer(Chunk *ck) {
  int8_t ret = 0;

  struct ibv_recv_wr *bad_wr;
  struct ibv_sge recv_sge;
  struct ibv_recv_wr recv_wr;

  recv_sge.addr = (uintptr_t) (ck->chk_buf); 
  recv_sge.length = QUEUE_BUFFER_SIZE; 
  recv_sge.lkey = ck->mr->lkey;

  recv_wr.sg_list = &recv_sge; 
  recv_wr.num_sge = 1;
  recv_wr.next = NULL;
  recv_wr.wr_id = (uint64_t) ck;
  ret = ibv_post_recv(qp, &recv_wr, &bad_wr);
  assert(ret == 0);
  if (ret) {
    fprintf(stderr, "ibv_post_recv failed: %d\n", ret);
  }
}

void RDMAConnection::post_send(const char *raw_msg, uint32_t raw_msg_size) {
  struct ibv_send_wr *bad_wr;
  uint32_t sge_index = 0;
  struct ibv_sge send_sge;
  struct ibv_send_wr send_wr;
  Chunk *ck = NULL;
  get_chunk(&ck);
  assert(raw_msg_size < QUEUE_BUFFER_SIZE);
  memcpy(ck->chk_buf, (char*)raw_msg, raw_msg_size);
  ck->chk_size = raw_msg_size;

  send_sge.addr = (uintptr_t) ck->chk_buf;
  send_sge.length = raw_msg_size;
  send_sge.lkey = ck->mr->lkey;

  send_wr.opcode = IBV_WR_SEND;
  send_wr.send_flags = IBV_SEND_SIGNALED;
  send_wr.sg_list = &send_sge;
  send_wr.num_sge = 1;
  send_wr.wr_id = (uint64_t) ck;
  send_wr.next = NULL;
  sge_index++;
  int ret = ibv_post_send(qp, &send_wr, &bad_wr);
  if (ret) {
    printf("ibv send error.\n"); 
  }
}

void RDMAConnection::post_send_batch(std::vector<char*> &raw_msg, std::vector<uint32_t> &raw_msg_size) {
  struct ibv_send_wr *bad_wr;
  uint32_t sge_index = 0;
  int chunk_size = raw_msg.size();
  struct ibv_sge send_sge[chunk_size];
  struct ibv_send_wr send_wr[chunk_size];
  for (uint32_t base = 0; base < chunk_size; ++base) {
    Chunk *ck = NULL;
    get_chunk(&ck);
    assert(raw_msg_size[base] < QUEUE_BUFFER_SIZE);
    memcpy(ck->chk_buf, raw_msg[base], raw_msg_size[base]);
    ck->chk_size = raw_msg_size[base];

    send_sge[sge_index].addr = (uintptr_t) ck->chk_buf;
    send_sge[sge_index].length = raw_msg_size[base];
    send_sge[sge_index].lkey = ck->mr->lkey;

    send_wr[sge_index].opcode = IBV_WR_SEND;
    send_wr[sge_index].send_flags = IBV_SEND_SIGNALED;
    send_wr[sge_index].sg_list = &send_sge[sge_index];
    send_wr[sge_index].num_sge = 1;
    send_wr[sge_index].wr_id = (uint64_t) ck;
    send_wr[sge_index].next = NULL;
    sge_index++;
  }
}

void RDMAConnection::fin() {
  struct ibv_send_wr send_wr;
  struct ibv_send_wr *bad_wr;
  struct ibv_sge send_sge;
  memset(&send_wr, 0, sizeof(send_wr));
  Chunk *ck = NULL;
  get_chunk(&ck);

  send_sge.addr = (uintptr_t) ck->chk_buf;
  send_sge.length = 0;
  send_sge.lkey = ck->mr->lkey;

  send_wr.wr_id = 0;
  send_wr.sg_list = &send_sge;
  send_wr.num_sge = 1;
  send_wr.opcode = IBV_WR_SEND;
  send_wr.send_flags = IBV_SEND_SIGNALED;
  send_wr.next = NULL;
  int ret = ibv_post_send(qp, &send_wr, &bad_wr);
  if (ret) {
    printf("fin, ibv send error.\n"); 
  }
}

void RDMAConnection::qp_setup() {
  struct ibv_qp_init_attr init_attr;
  memset(&init_attr, 0, sizeof(init_attr));
  init_attr.cap.max_send_wr = SEND_QUEUE_SIZE_PER_QP;
  init_attr.cap.max_recv_wr = RECV_QUEUE_SIZE_PER_QP;
  init_attr.cap.max_recv_sge = 1;
  init_attr.cap.max_send_sge = 1;
  init_attr.qp_type = IBV_QPT_RC;
  init_attr.send_cq = cq;
  init_attr.recv_cq = cq;

  rdma_create_qp(cm_id, pd, &init_attr); 
  qp = cm_id->qp;
}

void RDMAConnection::mr_setup() {
  recv_chunk = (Chunk**)calloc(RECV_QUEUE_SIZE_PER_QP, sizeof(Chunk*));
  send_chunk = (Chunk**)calloc(SEND_QUEUE_SIZE_PER_QP, sizeof(Chunk*));
 
  struct ibv_mr *mr = NULL;
  int buffer_index = 0; 
  uint64_t len = RECV_QUEUE_SIZE_PER_QP*QUEUE_BUFFER_SIZE;
  char* base = (char*)memalign(4096, len);
  recv_buf = base;
  for (uint64_t offset = 0; offset < len; offset += QUEUE_BUFFER_SIZE) {
    mr = ibv_reg_mr(pd, base+offset, QUEUE_BUFFER_SIZE, IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_READ | IBV_ACCESS_REMOTE_WRITE);
    Chunk *ck = new Chunk(mr, base+offset, QUEUE_BUFFER_SIZE);
    *(recv_chunk+buffer_index) = ck;
    buffer_index++;
  }
 
  buffer_index = 0;
  len = SEND_QUEUE_SIZE_PER_QP*QUEUE_BUFFER_SIZE;
  send_buf = (char*)memalign(4096, len);

  base = send_buf;
  int i = 0;
  for (uint64_t offset = 0; offset < len; offset += QUEUE_BUFFER_SIZE) {
    mr = ibv_reg_mr(pd, base+offset, QUEUE_BUFFER_SIZE, IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_READ | IBV_ACCESS_REMOTE_WRITE);
    Chunk *ck = new Chunk(mr, base+offset, QUEUE_BUFFER_SIZE);
    *(send_chunk+buffer_index) = ck;
    reap_chunk(&ck);
    buffer_index++;
    i++;
  }
}

void RDMAConnection::get_chunk(Chunk **ck) {
  std::lock_guard<std::mutex> l(chk_mtx);
  if (free_chunks.size() == 0) return;
  *ck = free_chunks.back();
  assert(*ck);
  free_chunks.pop_back();
}

void RDMAConnection::reap_chunk(Chunk **ck) {
  std::lock_guard<std::mutex> l(chk_mtx); 
  assert(*ck);
  free_chunks.push_back(*ck);
}

