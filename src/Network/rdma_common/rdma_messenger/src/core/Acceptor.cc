#include <thread>

#include "rdma_messenger/Acceptor.h"

void AcceptThread::entry() {
  assert(stack);
  stack->cm_event_handler();
}

void AcceptThread::abort() {
  perror("acceptor error.");
  exit(1);
}

Acceptor::Acceptor(struct sockaddr* addr) : accept_callback(nullptr), server_addr(addr) {}

Acceptor::~Acceptor() {
  delete accept_thread;
}

void Acceptor::listen() {
  assert(stack);
  accept_thread = new AcceptThread(stack);
  accept_thread->start(true);
  stack->listen(server_addr);
}

void Acceptor::join() {
  accept_thread->join();
}

void Acceptor::shutdown() {
  stack->shutdown();
  join();
}

void Acceptor::set_network_stack(RDMAStack *stack_) {
  stack = stack_; 
}

void Acceptor::set_accept_callback(Callback *callback) {
  accept_callback = callback;
  stack->set_accept_callback(callback);
}

