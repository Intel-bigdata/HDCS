#ifndef CONTEXT_H
#define CONTEXT_H

#include "Config.h"
#include "CacheMap.h"
#include "WorkQueue.h"
#include "ThreadPool.h"
#include "Mempool.h"
#include "Log.h"
#include "rbc/Message.h"
#include "rbc/CacheEntry.h"
#include "rbc/SimpleBlockCacher.h"
#include "rbc/MetaStore.h"
#include "rbc/BackendStore.h"
#include "rbc/common/Admin.h"
#include "rbc/HttpHandler/HttpRequestHandler.h"
#include <atomic>
#include "rbc/Messenger/AsioListener.h"
#include "rbc/Messenger/asioMessenger/AsioClient.h"
#include <boost/thread/shared_mutex.hpp>
#include <boost/lexical_cast.hpp>
#include <chrono>
#include <condition_variable>

#define THREADPOOLSIZE 64

namespace rbc {


class Context{
public:
    bool go;
    Config *config;
    WorkQueue<void*> request_queue;
    SimpleBlockCacher *cacher;
    MetaStore *metastore;
    BackendStore *backendstore;

    uint64_t object_size;
    uint64_t cache_total_size;
    uint64_t cache_flush_queue_depth;

    Mempool *mempool;
    Admin *admin;
    HttpRequestHandler *admin_socket;

    CacheMap *cache_map;
    std::mutex cachemap_lock;
    smutex cachemap_access;
    LRU_LIST<char*> *lru_dirty;
    LRU_LIST<char*> *lru_clean;

    std::atomic<std::uint64_t> backend_aio_read_count;
    std::atomic<std::uint64_t> flush_op_count;

    std::mutex flush_qd_mutex, flush_stop_mutex;
    std::unique_lock<std::mutex> flush_qd_scoped_lock, flush_stop_scoped_lock;
    std::condition_variable flush_qd_cond, flush_stop_cond;

    AsioListener *server;
    std::vector<AsioClient*> asio_client_vec;
    /*
    AsioClient *client_for_slave;
    AsioClient *client_for_slave_1;
    */
    bool process_mode;
    bool if_master;
    std::string log_path;
    int stderr_no;

    Context(const char* rbd_name, bool process_mode = false, bool if_master = false):
        if_master(if_master),process_mode(process_mode) {
        go =  true;
        config = new Config(rbd_name);
        bool enable_mem_tracker = config->configValues["enable_MemoryUsageTracker"] == "true"?true:false;
        mempool = new Mempool( enable_mem_tracker );
        log_path = config->configValues["log_to_file"];

        std::cout << "log_path: " << log_path << std::endl;
        if( log_path!="false" ){
            stderr_no = dup(fileno(stderr));
            log_fd = fopen( log_path.c_str(), "w" );
            dup2(fileno(log_fd), STDERR_FILENO);
        }

        object_size = stoull(config->configValues["object_size"]);
        cache_total_size = stoull(config->configValues["cache_total_size"]);

        cache_map = new CacheMap( mempool );

        lru_dirty = new LRU_LIST<char*>(mempool);
        lru_clean = new LRU_LIST<char*>(mempool);

        admin = new Admin(lru_dirty, lru_clean, object_size);
        cacher = new SimpleBlockCacher(config->configValues["cache_dir_dev"],
                                       config->configValues["cache_dir_run"],
                                       cache_total_size, object_size, mempool);
        metastore = new MetaStore(config->configValues["cache_dir_meta"]);
        log_print( "backendstore construction start\n");
        backendstore = new BackendStore("rbcclient");
        log_print( "backendstore construction finish\n");

        if( process_mode ){
            short port = stoi(config->configValues["messenger_port"]);
            server = new AsioListener( port, &request_queue );
            if( if_master ){
                //  according to replication amounts, generating AsioClient objects. 
                //  one AsioClient represent one communication between master and slave.
                int replica_num=std::stoi(config->configValues["replication_num"]);
                auto ip_vec=config->slave_ip_vec;
                auto port_vec=config->slave_messenger_vec;
                for( int i=0; i< replica_num ; i++){
                    AsioClient* client_for_slave=new AsioClient( ip_vec[i], port_vec[i] );
                    asio_client_vec.push_back(client_for_slave);
                }
                /*
                std::string target_ip = config->configValues["slave_ip"];
                std::string target_port = config->configValues["slave_messenger_port"];
                client_for_slave = new AsioClient( target_ip, target_port ); 
                // 3 replication
                std::string target_ip_1 = config->configValues["slave_ip_1"];
                std::string target_port_1 = config->configValues["slave_messenger_port_1"];
                client_for_slave_1 = new AsioClient( target_ip_1, target_port_1 );
                */
            }
        }
        //TODO(yuan): disable admin socket for temporarily until we fix
        // the port race with multiple volumes
        //admin_socket = new HttpRequestHandler(admin);

        cache_flush_queue_depth = stoi(config->configValues["cache_flush_queue_depth"]);
    }

    ~Context(){
        log_print("going to delete messenger\n");
        if(process_mode){
            delete server;
            if(if_master){
                for(int i=0; i<asio_client_vec.size(); i++){
                    delete asio_client_vec[i];
                }
                /*
                delete client_for_slave;
                delete client_for_slave_1;
                */
            }
        }
        //TODO: should call cache_entry destruction
        //delete admin_socket;
        uint64_t cache_count = cache_map->size();
        char** c_entry_list = (char**)mempool->malloc( sizeof(char*) * (cache_count) );
        cache_map->get_values( c_entry_list );
        for(uint64_t i = 0; i<cache_count; i++){
            delete ((CacheEntry*)c_entry_list[i]);
        }
        mempool->free( (void*)c_entry_list, sizeof(char*) * (cache_count) );
        delete cache_map;
        delete lru_dirty;
        delete lru_clean;
        log_print("going to delete cacher\n");
        delete cacher;
        log_print("going to delete metastore\n");
        delete metastore;
        log_print("going to delete backendstore\n");
        delete backendstore;
        delete config;
        delete mempool;
        if( log_path!="false" ){
            dup2( stderr_no, STDERR_FILENO );
            fclose(log_fd);
        }
    }

};
}

#endif
