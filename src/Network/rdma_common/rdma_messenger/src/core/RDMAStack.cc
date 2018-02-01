#include <assert.h>

#include <iostream>

#include "rdma_messenger/RDMAStack.h"

RDMAStack::RDMAStack() : stop(false) {
  con_mgr = new RDMAConMgr(this); 
}

RDMAStack::~RDMAStack() {
  stop.store(true); 
  delete con_mgr;
}

void RDMAStack::init(bool is_server_) {
  is_server = is_server_;
  cm_channel = rdma_create_event_channel();
  rdma_create_id(cm_channel, &cm_id, NULL, RDMA_PS_TCP);
}

void RDMAStack::cm_event_handler() {
  pthread_testcancel();
  int ret = 0;
  struct rdma_cm_event *event;
  while (rdma_get_cm_event(cm_channel, &event) == 0) {
    printf("got cm event: %s.\n", rdma_event_str(event->event));
    if (event->event == RDMA_CM_EVENT_CONNECT_REQUEST) {
      struct rdma_cm_id *event_cm_id = event->id;
      accept(event_cm_id);
    } else {
      switch (event->event) {
	case RDMA_CM_EVENT_ADDR_RESOLVED:

	  ret = rdma_resolve_route(cm_id, 2000);
	  if (ret) {
	    sem_post(&sem);
	  }
	  break;

	case RDMA_CM_EVENT_ROUTE_RESOLVED:
	  sem_post(&sem);
	  break;

	case RDMA_CM_EVENT_CONNECT_REQUEST:
	  sem_post(&sem);
	  break;

	case RDMA_CM_EVENT_ESTABLISHED:
	  sem_post(&sem);
	  if (!is_server)
	    return;
	  break;

	case RDMA_CM_EVENT_ADDR_ERROR:
	case RDMA_CM_EVENT_ROUTE_ERROR:
	case RDMA_CM_EVENT_CONNECT_ERROR:
	case RDMA_CM_EVENT_UNREACHABLE:
	case RDMA_CM_EVENT_REJECTED:
	  fprintf(stderr, "cma event %s, error %d\n", rdma_event_str(event->event), event->status);
	  sem_post(&sem);
	  ret = -1;
	  break;

	case RDMA_CM_EVENT_DISCONNECTED:
	  sem_post(&sem);
	  break;

	case RDMA_CM_EVENT_DEVICE_REMOVAL:
	  fprintf(stderr, "cma detected device removal!!!!\n");
	  sem_post(&sem);
	  ret = -1;
	  break;

	default:
	  fprintf(stderr, "unhandled event: %s, ignoring\n",
		  rdma_event_str(event->event));
	  break;
      }
    }
    rdma_ack_cm_event(event);
  }
}

void RDMAStack::cq_event_handler(struct ibv_comp_channel *cq_channel, struct ibv_cq *cq) {
  struct ibv_cq *event_cq;
  void *event_ctx;
  while (1) {
    pthread_testcancel();
    if (!cq_channel) continue;
    ibv_get_cq_event(cq_channel, &event_cq, &event_ctx);
    ibv_ack_cq_events(event_cq, 1);
    ibv_req_notify_cq(event_cq, 0);

    assert(cq == event_cq);
    struct ibv_wc wc;
    while (ibv_poll_cq(cq, 1, &wc) == 1) {
      if (wc.status) {
	//printf("connection error: %s.\n", ibv_wc_status_str(wc.status));
	handle_err(&wc);
	continue;
      }
      switch (wc.opcode) {
	case IBV_WC_SEND:
	  {
	    handle_send(&wc);
	    break;
	  }
	case IBV_WC_RECV:
	  {
	    handle_recv(&wc);
	    break;
	  }
	default:
	  assert(0 == "bug");
      }
    }
  }
  return;
}

void RDMAStack::handle_recv(struct ibv_wc *wc) {
  RDMAConnection *con = con_mgr->get_connection(wc->qp_num);
  if (wc->byte_len == 0) {
    if (con) {
      con_mgr->del(con->get_con_id(), wc->qp_num);
      con->close(); 
      con = NULL;
    }
  } else {
    Chunk *ck = (Chunk*)wc->wr_id; 
    ck->chk_size = wc->byte_len;
    if (!con) return;
    if (con->read_callback) {
      con->read_callback->entry(con, ck); 
    }
    con->post_recv_buffer(ck);
  }
}

void RDMAStack::handle_send(struct ibv_wc *wc) {
  RDMAConnection *con = con_mgr->get_connection(wc->qp_num);
  if (wc->byte_len == 0 && wc->wr_id == 0) {
    if (con) {
      con_mgr->del(con->get_con_id(), wc->qp_num);
      con->close(); 
      con = NULL;
    }
  } else {
    Chunk *ck = (Chunk*)wc->wr_id;
    if (!con) return;
    con->reap_chunk(&ck);
  }
}

void RDMAStack::handle_err(struct ibv_wc *wc) {
  RDMAConnection *con = con_mgr->get_connection(wc->qp_num);
  if (con) {
    con_mgr->del(con->get_con_id(), wc->qp_num);
    con->close();
  }
}

void RDMAStack::listen(struct sockaddr *addr) {
  rdma_bind_addr(cm_id, addr);
  rdma_listen(cm_id, 1);
}

void RDMAStack::accept(struct rdma_cm_id *event_cm_id) {
  struct rdma_event_channel *event_channel;
  event_channel = rdma_create_event_channel();
  rdma_migrate_id(event_cm_id, event_channel);
  struct rdma_cm_id *new_cm_id;
  new_cm_id = event_cm_id;
  RDMAConnection *con = connection_establish(new_cm_id);
  rdma_accept(new_cm_id, NULL);
  if (accept_callback) {
    accept_callback->entry(con); 
  }
}

void RDMAStack::connect(struct sockaddr *addr) {
  rdma_resolve_addr(cm_id, NULL, addr, 5000);
  sem_wait(&sem);

  RDMAConnection *con = connection_establish(cm_id);
  
  struct rdma_conn_param cm_params;
  memset(&cm_params, 0, sizeof(cm_params));
  cm_params.responder_resources = 1;
  cm_params.retry_count = 7;
  rdma_connect(cm_id, &cm_params);
  sem_wait(&sem);

  if (connect_callback) {
    connect_callback->entry(con);  
  }
}

void RDMAStack::shutdown() {
  rdma_disconnect(cm_id);
  assert(this);
}

RDMAConnection* RDMAStack::connection_establish(struct rdma_cm_id* cm_id_) {
  return con_mgr->new_connection(cm_id_);
}

void RDMAStack::set_accept_callback(Callback *accept_callback_) {
  accept_callback = accept_callback_;
}

void RDMAStack::set_connect_callback(Callback *connect_callback_) {
  connect_callback = connect_callback_;
}

RDMAConMgr::RDMAConMgr(RDMAStack *stack_) : stack(stack_) {
  cq_threads.reserve(IO_WORKER_NUMS); 
}

RDMAConMgr::~RDMAConMgr() {
  //for (auto cq_thread : cq_threads) {
  //  cq_thread->join();
  //}

  for (auto m : qp_con_map) {
    delete m.second;
  }
}

RDMAConnection* RDMAConMgr::get_connection(uint32_t qp_num) {
  if (qp_con_map.count(qp_num) != 0) {
    return qp_con_map[qp_num]; 
  } else {
    return nullptr; 
  }
}

RDMAConnection* RDMAConMgr::new_connection(struct rdma_cm_id *cm_id_) {
  struct ibv_pd *pd = ibv_alloc_pd(cm_id_->verbs); 
  struct ibv_cq *cq;
  if (!(con_id / IO_WORKER_NUMS)) {
    struct ibv_comp_channel *cq_channel = ibv_create_comp_channel(cm_id_->verbs);
    cq = ibv_create_cq(cm_id_->verbs, 1024*2, NULL, cq_channel, 0);
    ibv_req_notify_cq(cq, 0);
    CQThread *cq_thread = new CQThread(stack, cq_channel, cq);
    cq_thread->start();
    //cq_thread->set_affinity(con_id+22);
    cq_threads.push_back(cq_thread);
  } else {
    cq = cq_map[con_id % IO_WORKER_NUMS];  
  }
  RDMAConnection *new_con = new RDMAConnection(pd, cq, cm_id_, con_id);
  seq_map.insert(std::pair<uint64_t, RDMAConnection*>(con_id, new_con));
  qp_con_map.insert(std::pair<uint32_t, RDMAConnection*>(new_con->get_qp()->qp_num, new_con));
  cq_map.insert(std::pair<uint64_t, struct ibv_cq*>(con_id, cq));
  con_id++;
  new_con->post_recv_buffers();
  return new_con;
}

void RDMAConMgr::del(uint64_t con_id_, uint32_t qp_num_) {
  //printf("del con_id: %ld, qp_num: %u.\n", con_id_, qp_num_);
  seq_map.erase(con_id_);
  qp_con_map.erase(qp_num_);
  if (con_id_ >= IO_WORKER_NUMS) cq_map.erase(con_id_);
}
