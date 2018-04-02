// Copyright [2017] <Intel>
#include "HDCSController.h"
#include "core/HDCSCoreStatMachine.h"

namespace hdcs {
HDCSController::HDCSController(
  std::string name,
  std::string config_file_path,
  std::string role):
  name(name),
  config_file_path(config_file_path),
  role(role),
  config(name, config_file_path) {
  conf_of_HDCSController = config.get_config("HDCSController");
  std::string log_path = conf_of_HDCSController["log_to_file"];
  std::cout << "log_path: " << log_path << std::endl;
  if( log_path!="false" ){
    int stderr_no = dup(fileno(stderr));
    log_fd = fopen( log_path.c_str(), "w" );
	  if(log_fd==NULL){}
    if(-1==dup2(fileno(log_fd), STDERR_FILENO)){}
  }

  std::vector<std::string> ip_port;
  std::string _port;
  auto local_addr_it = conf_of_HDCSController.find(name);
  if (local_addr_it != conf_of_HDCSController.end()) {
    boost::split(ip_port, local_addr_it->second, boost::is_any_of(":"));
    _port = ip_port[1];
  }

  //setup HAClient
  hdcs::ha::HAConfig ha_config(config.get_config("HDCSManager"));
  ha_client = new ha::HAClient(name, std::move(ha_config));
  ha_client->add_ha_server(config.get_config("HDCSManager")["hdcs_HAManager_name"]);

  //TODO(): seperate public/cluster network

  networking::ServerOptions server_options;
  server_options._io_service_num = 3;
  server_options._session_num = 3;
  server_options._thd_num_on_one_session = 3;
  server_options._process_msg = ([&](void* p, std::string s){handle_request(p, s);});
  server_options._port_num_vec.push_back(_port);
  server_options._communication_type_vec.push_back((networking::TCP_COMMUNICATION)); // TCP
  //server_options._port_num_vec.push_back("7777");
  //server_options._communication_type_vec.push_back((networking::RDMA_COMMUNICATION)); //RDMA
  network_service = new networking::server(server_options);

  // TODO: combine two interfaces into one sync_run() interface...sdh
  network_service->start();
  network_service->sync_run();
}

HDCSController::~HDCSController() {
  delete ha_client;
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
      std::string volume_name(data, io_ctx->length);

      std::cout << "start to look for core id in hdcs_core_map" << std::endl;
      hdcs_core_map_mutex.lock();
      auto it = hdcs_core_map.find(volume_name);
      if (it == hdcs_core_map.end()) {
        //get replication_options from HDCSManager
        std::vector<std::string> replication_nodes;
        bool first = true;
        //get replication nodes from ha_client
        for (auto &host_name : ha_client->get_domain_item()) {
          if (first) {
            first = false;
            continue;
          }
          auto host_addr_it = conf_of_HDCSController.find(host_name);
          if ( host_addr_it != conf_of_HDCSController.end() ) {
            replication_nodes.push_back(host_addr_it->second);
          }
        }
        hdcs_repl_options replication_options(role, std::move(replication_nodes));

        //core::HDCSCore* core_inst = new core::HDCSCore(name, volume_name, config_file_path, std::move(replication_options));
        // insert new core_stat_machine. This only happens to a clean volume.
        core::HDCSCoreStatGuard *core_statmachine_inst = new core::HDCSCoreStatGuard(
          name, volume_name, config_file_path, std::move(replication_options)
        );
        auto ret = hdcs_core_map.insert(
          std::pair<std::string, core::HDCSCoreStatGuard*>(volume_name, core_statmachine_inst)
        );
        assert (ret.second);
        it = ret.first;
      }
      hdcs_core_map_mutex.unlock();
      std::cout << "Found core id in hdcs_core_map" << std::endl;
      
      // Now we can get hdcs core inst from core stat machine.
      hdcs_inst = it->second->get_hdcs_core();
      if (hdcs_inst == nullptr) {
        std::cout << "got core id from stat machine, but which is null" << std::endl;
      } else {
        std::cout << "got core id from stat machine" << std::endl;
      }
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
