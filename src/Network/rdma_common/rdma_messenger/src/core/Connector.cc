#include "rdma_messenger/Connector.h"

void ConnectThread::entry() {
  stack->cm_event_handler();
}

void ConnectThread::abort() {
  perror("connector error.");
  exit(1);
}

Connector::Connector(struct sockaddr *addr) : connect_callback(nullptr), connector_addr(addr) {}

Connector::~Connector() {
  delete connect_thread;
}

void Connector::connect() {
  assert(stack);
  connect_thread = new ConnectThread(stack);
  connect_thread->start(true);
  stack->connect(connector_addr);
}

void Connector::join() {
  connect_thread->join();
}

void Connector::shutdown() {
  stack->shutdown();
  join();
}

void Connector::set_network_stack(RDMAStack *stack_) {
  stack = stack_;
}

void Connector::set_connect_callback(Callback *callback) {
  connect_callback = callback;
  stack->set_connect_callback(callback);
}
