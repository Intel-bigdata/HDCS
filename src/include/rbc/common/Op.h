#ifndef OP_H
#define OP_H

#include "rbc/common/AioCompletion.h"
#include "rbc/common/Context.h"
#include "rbc/C_AioBackendCompletion.h"

namespace rbc {
class Op{

public:
    char* data;
    uint64_t offset;
    uint64_t length;

    Request* req;
    Context* cct;
    Mempool *mempool;
    CacheEntry* cache_entry;

    std::mutex op_lock;
    std::string image_name;
    std::time_t ts;

    Op( Context *cct, std::string location_id, uint64_t offset, char* data, uint64_t length, Mempool* mempool = NULL, Request* req = NULL):
        cct(cct),image_name(location_id),offset(offset),data(data),length(length),mempool(mempool),req(req){
            mempool->add( "Op", sizeof(Op) );
            if (req) {
                ts = req->ts;
            } else {
                ts = std::time(nullptr);
            }

        }

    ~Op(){
        mempool->rm( "Op", sizeof(Op) );
    }
    int commit( ssize_t ret ){

        assert(req != NULL);

        req->req_lock.lock();
        req->update_status( ret );
        req->uncomplete_count--;
        if(0 == req->uncomplete_count){
            req->complete();
            req->req_lock.unlock();
            delete req;
            op_lock.unlock();
            return ret;
        }
        req->req_lock.unlock();
        op_lock.unlock();
        return ret;
    }
    ssize_t datastore_read(){
        uint64_t start_off = offset%this->cct->object_size;
        uint64_t block_id = offset/this->cct->object_size;
        //log_print("start read op %p, off=%lu, len=%lu\n", this, offset, length);
        ssize_t ret = this->cct->cacher->_read( block_id, data, start_off, length );
        //log_print("finish read op %p, off=%lu, len=%lu\n", this, offset, length);
        return ret;
    }
    ssize_t datastore_aio_read(){
        return 0;
    }
    int datastore_write(){
        uint64_t start_off = offset%this->cct->object_size;
        uint64_t block_id = offset/this->cct->object_size;
        //log_print("start write op %p, off=%lu, len=%lu\n", this, offset, length);
        int ret = this->cct->cacher->_write( block_id, data, start_off, length, ts);
        //int ret = 0;
        //log_print("finish write op %p, off=%lu, len=%lu\n", this, offset, length);
        if( ret < 0 ){
            log_print("CacheService::datastore_update unable to write to cache\n");
        }
        return ret;
    }
    int datastore_aio_write(){
        return 0;
    }
    int replica_aio_write( AioCompletion *comp , int replica_seq_id ){
        Msg* msg = new Msg( image_name.c_str(), offset, data, length, MSG_WRITE );
        (cct->asio_client_vec)[ replica_seq_id ]->send_request(msg, (void*)comp);
        /*
        if(replica_seq_id==0){
            cct->client_for_slave->send_request(msg, (void*)comp);
        }else if(replica_seq_id==1){
            cct->client_for_slave_1->send_request(msg, (void*)comp);
        }
        */
    }
    ssize_t backstore_aio_read( C_AioBackendCompletion *onfinish ){
        while( cct->backend_aio_read_count > 256 ){
            usleep( 1 );
        }
        ssize_t r = this->cct->backendstore->aio_read( image_name, offset, length, data, "rbd", onfinish );
        cct->backend_aio_read_count++;
        return r;
    }
    ssize_t backstore_read(){
        ssize_t r = this->cct->backendstore->read( image_name, offset, length, data, "rbd" );
        //log_print("finish read op %p, off=%lu, len=%lu\n", this, offset, length);
        return r;
    }
    int backstore_aio_write( C_AioBackendCompletion *onfinish ){
        this->cct->flush_op_count++;
        int r = this->cct->backendstore->aio_write( image_name, offset, length, data, "rbd", onfinish );
        return r;
    }
    int metastore_update(){
        int ret = 0;
        ret = this->cct->metastore->put( this->cache_entry->get_name(), this->cache_entry->toString() );
        return ret;
    }
};

}
#endif
