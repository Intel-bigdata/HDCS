// Copyright [2017] <Intel>
#include "HDCSController.h"

namespace hdcs {
HDCSController::HDCSController(std::string name, std::string config_name): config_name(config_name) {
  config = new Config(name, config_name); 
  std::string log_path = config->configValues["log_to_file"];
  std::cout << "log_path: " << log_path << std::endl;
  if( log_path!="false" ){
    int stderr_no = dup(fileno(stderr));
    log_fd = fopen( log_path.c_str(), "w" );
	  if(log_fd==NULL){}
    if(-1==dup2(fileno(log_fd), STDERR_FILENO)){}
  }
  //network_service = new networking::server(64, "0.0.0.0", "9000");
  // param 3: session number  
  // param 4: thread number of one session.
  network_service = new networking::server("0.0.0.0", config->configValues["local_port"], 5, 5);
  network_service->start([&](void* p, std::string s){handle_request(p, s);});
  network_service->run();
}

HDCSController::~HDCSController() {
  delete network_service;
  for (auto it = hdcs_core_map.begin(); it != hdcs_core_map.end(); it++) {
    delete (it->second);
    hdcs_core_map.erase(it); 
  }
}

void HDCSController::handle_request(void* session_id, std::string msg_content) {
  HDCS_REQUEST_CTX_T *io_ctx = (HDCS_REQUEST_CTX_T*)(msg_content.c_str());
  char* data = &msg_content[sizeof(HDCS_REQUEST_CTX_T)];
  core::HDCSCore* hdcs_inst;
  AioCompletion *comp;
  switch (io_ctx->type) {
    case HDCS_CONNECT:
    {
      std::string name(data, io_ctx->length);
      hdcs_core_map_mutex.lock();
      auto it = hdcs_core_map.find(name);
      if (it == hdcs_core_map.end()) {
        core::HDCSCore* core_inst = new core::HDCSCore(name, config_name);
        auto ret = hdcs_core_map.insert(std::pair<std::string, core::HDCSCore*>(name, core_inst));
        assert (ret.second);
        it = ret.first;
      }
      hdcs_core_map_mutex.unlock();
      hdcs_inst = it->second;
      io_ctx->type = HDCS_CONNECT_REPLY;
      io_ctx->hdcs_inst = (void*)hdcs_inst;
      io_ctx->length = 0;
      network_service->send(session_id, std::move(std::string((char*)io_ctx, io_ctx->size()))); 
      break;
    }
    case HDCS_READ:
    {
      hdcs_inst = (core::HDCSCore*)io_ctx->hdcs_inst; 
      void* cli_comp = io_ctx->comp;
      void* ret_data_ptr = io_ctx->ret_data_ptr;
      uint64_t length = io_ctx->length;
      char* aligned_data;
      posix_memalign((void**)&aligned_data, 4096, io_ctx->length);

      comp = new AioCompletionImp([this, session_id, aligned_data,
                    hdcs_inst, cli_comp, length, ret_data_ptr](ssize_t r){
        HDCS_REQUEST_CTX msg_content(HDCS_READ_REPLY, hdcs_inst,
                                     cli_comp, 0, length, aligned_data);
        if (r >= 0) {
          msg_content.set_ret_data_ptr(ret_data_ptr);
        } else {
          msg_content.set_ret_data_ptr((void*)-1);
        }
        network_service->send(session_id,
                              std::move(std::string(
                              msg_content.data(), 
                              msg_content.size())));
        free(aligned_data);
      });
      hdcs_inst->aio_read(aligned_data, io_ctx->offset, io_ctx->length, comp);
    }
      break;
    case HDCS_WRITE:
    {
      hdcs_inst = (core::HDCSCore*)io_ctx->hdcs_inst; 
      void* cli_comp = io_ctx->comp;
      char* aligned_data;
      posix_memalign((void**)&aligned_data, 4096, io_ctx->length);
      memcpy(aligned_data, data, io_ctx->length);

      comp = new AioCompletionImp([this, session_id, aligned_data, hdcs_inst, cli_comp](ssize_t r){
        HDCS_REQUEST_CTX msg_content(HDCS_WRITE_REPLY, hdcs_inst, cli_comp, 0, 0, (void*)r);
        network_service->send(session_id, std::move(std::string(msg_content.data(), msg_content.size())));
        free(aligned_data);
      });
      //std::lock_guard<std::mutex> lock(hdcs_inst->core_lock);
      comp->complete(0);
      //hdcs_inst->aio_write(aligned_data, io_ctx->offset, io_ctx->length, comp);
      //printf("[SUBMIT]HDCS WRITE REQ: %lu - %lu\n", io_ctx->offset, (io_ctx->offset + io_ctx->length));
    }
      break;
    case HDCS_FLUSH:
      hdcs_inst = (core::HDCSCore*)io_ctx->hdcs_inst; 
      hdcs_inst->flush_all();
      io_ctx->type = HDCS_FLUSH_REPLY;
      io_ctx->length = 0;
      network_service->send(session_id, std::move(std::string((char*)io_ctx, io_ctx->size())));
      break;
    case HDCS_PROMOTE:
      hdcs_inst = (core::HDCSCore*)io_ctx->hdcs_inst; 
      hdcs_inst->promote_all();
      io_ctx->type = HDCS_PROMOTE_REPLY;
      io_ctx->length = 0;
      network_service->send(session_id, std::move(std::string((char*)io_ctx, io_ctx->size())));
      break;
    case HDCS_SET_CONFIG:
      break;
    case HDCS_GET_STATUS:
      break;
    default:
      break;
  }
}
}// hdcs
