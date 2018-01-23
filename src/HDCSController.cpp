// Copyright [2017] <Intel>
#include "HDCSController.h"

namespace hdcs {
HDCSController::HDCSController(struct hdcs_repl_options repl_opt, std::string config_name): replication_options(repl_opt), config_name(config_name) {
  config = new Config("", replication_options, config_name);
  std::string log_path = config->configValues["log_to_file"];
  std::cout << "log_path: " << log_path << std::endl;
  if( log_path!="false" ){
    int stderr_no = dup(fileno(stderr));
    log_fd = fopen( log_path.c_str(), "w" );
	  if(log_fd==NULL){}
    if(-1==dup2(fileno(log_fd), STDERR_FILENO)){}
  }

  std::string _port = "9000";
  if ("hdcs_replica" == config->configValues["role"]) {
    _port = "9001";
  }

  //TODO(): seperate public/cluster network

  networking::ServerOptions server_options;
  server_options._io_service_num = 3;
  server_options._session_num = 3;
  server_options._thd_num_on_one_session = 3;
  server_options._process_msg = ([&](void* p, std::string s){handle_request(p, s);});
  server_options._port_num_vec.push_back(_port);
  server_options._communication_type_vec.push_back((networking::COMMUNICATION_TYPE)0); // TCP
  //server_options._port_num_vec.push_back("7777");
  //server_options._communication_type_vec.push_back((networking::COMMUNICATION_TYPE)1); //RDMA
  network_service = new networking::server(server_options);

  // TODO: combine two interfaces into one sync_run() interface...sdh
  network_service->start();
  network_service->sync_run();
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
        core::HDCSCore* core_inst = new core::HDCSCore(name, config->configValues["cfg_file_path"], replication_options);
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
      int ret = posix_memalign((void**)&aligned_data, 4096, io_ctx->length);
      if (ret < 0) {
        break;
      }

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
      if (io_ctx->offset == 871018496) {
        struct timespec spec;
        clock_gettime(CLOCK_REALTIME, &spec);
        fprintf(stderr, "%lu: hdcscontroller received %lu - %lu\n", (spec.tv_sec * 1000000000L + spec.tv_nsec), io_ctx->offset, io_ctx->offset + io_ctx->length);
      }
      hdcs_inst = (core::HDCSCore*)io_ctx->hdcs_inst; 
      void* cli_comp = io_ctx->comp;
      char* aligned_data;
      int ret = posix_memalign((void**)&aligned_data, 4096, io_ctx->length);
      if (ret < 0) {
        break;
      }
      memcpy(aligned_data, data, io_ctx->length);

      comp = new AioCompletionImp([this, session_id, aligned_data, hdcs_inst, cli_comp](ssize_t r){
        HDCS_REQUEST_CTX msg_content(HDCS_WRITE_REPLY, hdcs_inst, cli_comp, 0, 0, (void*)r);
        network_service->send(session_id, std::move(std::string(msg_content.data(), msg_content.size())));
        free(aligned_data);
      });
      //comp->complete(0);
      std::lock_guard<std::mutex> lock(hdcs_inst->core_lock);
      hdcs_inst->aio_write(aligned_data, io_ctx->offset, io_ctx->length, comp);
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
