#include <mutex>
#include <chrono>

#include "rdma_messenger/RDMAClient.h"

std::mutex mtx;
uint64_t timestamp_now() {
  return std::chrono::high_resolution_clock::now().time_since_epoch() / std::chrono::milliseconds(1);
}

class ReadCallback : public Callback {
  public:
    virtual void entry(void *param, void *msg = nullptr) override {
      RDMAConnection *con = static_cast<RDMAConnection*>(param); 
      Chunk *ck = static_cast<Chunk*>(msg);
      char buf[4096];
      memcpy(buf, ck->chk_buf, ck->chk_size);
      std::unique_lock<std::mutex> l(mtx);
      if (sum == 0) {
        printf("chunk buf is %s, chunk size is %u.\n", ck->chk_buf, ck->chk_size);
        start = timestamp_now(); 
      }
      sum++;
      if (sum >= 10000000) {
        con->fin();
        end = timestamp_now();
        printf("finished, consumes %f s.\n", (end-start)/1000.0);
        return; 
      }
      l.unlock();
      con->async_send("hello server", 13);
    }
  private:
    uint64_t sum = 0;
    uint64_t start, end = 0;
};

class ConnectCallback : public Callback {
  public:
    virtual ~ConnectCallback() {}
    virtual void entry(void *param, void *msg = nullptr) override {
      RDMAConnection *con = static_cast<RDMAConnection*>(param); 
      assert(read_callback);
      con->set_read_callback(read_callback);
      con->async_send("hello server", 13);
    }
    void set_read_callback(ReadCallback* read_callback_) {
      read_callback = read_callback_; 
    }
  private:
    ReadCallback *read_callback;
};

int main(int argc, char *argv[]) {
  struct addrinfo *res, *t;
  struct addrinfo	hints;
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = 0;
  hints.ai_protocol = 0;          /* Any protocol */
  
  int n = getaddrinfo("127.0.0.1", "20082", &hints, &res);

  if (n < 0) {
    printf("failed to get addr info.\n"); 
  }
  RDMAClient *client;
  ConnectCallback *connect_callback;
  ReadCallback *read_callback;
  for (t = res; t; t = t->ai_next) {
    client = new RDMAClient(t->ai_addr, 10);
    connect_callback = new ConnectCallback();
    read_callback = new ReadCallback();
    connect_callback->set_read_callback(read_callback);
    client->connect(connect_callback);
  }

  client->wait();

  delete read_callback;
  delete connect_callback;
  delete client;
  return 0;
}
