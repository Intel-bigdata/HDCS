#include "rdma_messenger/RDMAServer.h"

class ReadCallback : public Callback {
  public:
    virtual void entry(void *param) override {
      RDMAConnection *con = static_cast<RDMAConnection*>(param); 
      char buf[4096];
      if (con->read_buffer(buf, 4096) == 0) {
        assert(0 == "read bug"); 
      }
      con->async_send("hello client", 13);
    }
};

class AcceptCallback : public Callback {
  public:
    virtual void entry(void *param) override {
      RDMAConnection *con = static_cast<RDMAConnection*>(param); 
      assert(read_callback);
      con->set_read_callback(read_callback);
    }
    void set_read_callback(ReadCallback *read_callback_) {
      read_callback = read_callback_;
    }
  private:
    ReadCallback *read_callback;
};

int main() {
  struct sockaddr_in sin;
  sin.sin_family = AF_INET; 
  sin.sin_port = htons(20082);
  sin.sin_addr.s_addr = INADDR_ANY;
  
  // 1
  RDMAServer *server = new RDMAServer((struct sockaddr*)&sin);

  AcceptCallback *accept_callback = new AcceptCallback();

  // set process_msg
  ReadCallback *read_callback = new ReadCallback();
  accept_callback->set_read_callback(read_callback);

  //
  server->start(accept_callback);

  server->wait();

  delete read_callback;
  delete accept_callback;
  delete server;
  return 0;
}
