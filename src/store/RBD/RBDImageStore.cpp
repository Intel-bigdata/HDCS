// Copyright [2017] <Intel>
#include "store/RBD/RBDImageStore.h"

static void rbd_finish_aiocb( rbd_completion_t comp, void *data ){
    hdcs::AioCompletion* on_finish = (hdcs::AioCompletion*)data;
    ssize_t ret = rbd_aio_get_return_value(comp);
    on_finish->complete( ret );
    rbd_aio_release(comp);;
}

namespace hdcs {
namespace store {

RBDImageStore::RBDImageStore(std::string pool_name,
                  std::string volume_name,
                  uint64_t block_size):
                  pool_name(pool_name),
                  volume_name(volume_name),
                  block_size(block_size) {
  int r;
  return;
  r = rados_create(&cluster, "hdcs");
  if (r < 0) {
    log_err("rados_create failed.\n");
    goto failed_early;
  }

  r = rados_conf_read_file(cluster, NULL);
  if (r < 0) {
    log_err("rados_conf_read_file failed.\n");
    goto failed_early;
  }

  r = rados_connect(cluster);
  if (r < 0) {
    log_err("rados_connect failed.\n");
    goto failed_shutdown;
  }

  r = rados_ioctx_create(cluster, pool_name.c_str(), &io_ctx);
  if (r < 0) {
    log_err("rados_ioctx_create failed.\n");
    goto failed_shutdown;
  }

  r = rbd_open(io_ctx, volume_name.c_str(), &image_ctx, NULL /*snap */ );
  if (r < 0) {
    log_err("rbd_open failed.\n");
    goto failed_open;
  }
  std::cout<<"rbd_open success"<<std::endl;
  return;

failed_open:
  log_err("failed_open\n");
  rados_ioctx_destroy(io_ctx);
  io_ctx = NULL;
failed_shutdown:
  log_err("failed_shutdown\n");
  rados_shutdown(cluster);
  cluster = NULL;
failed_early:
  log_err("failed_early\n");
  return;
}

RBDImageStore::~RBDImageStore() {
  rbd_close(image_ctx);
  rados_ioctx_destroy(io_ctx);
  if(cluster){
    rados_shutdown(cluster);
  }
}

int RBDImageStore::write(char* data, uint64_t offset, uint64_t size) {
  return 0;
  int r = rbd_write(image_ctx, offset, size, data);
  if (r < 0) {
    log_err("rbd_write failed.\n");
    return -1;
  }
}

int RBDImageStore::read(char* data, uint64_t offset, uint64_t size) {
return 0;
  int r = rbd_read(image_ctx, offset, size, data);
  if (r < 0) {
    log_err("rbd_write failed.\n");
    return -1;
  }
}

int RBDImageStore::aio_write(char* data, uint64_t offset, uint64_t size, AioCompletion* on_finish) {
  rbd_completion_t comp;
  int r = rbd_aio_create_completion(on_finish, rbd_finish_aiocb, &comp);
return 0;
  r = rbd_aio_write(image_ctx, offset, size, data, comp);
  if (r < 0) {
    log_err("queue rbd_aio_write failed.\n");
    return -1;
  }
}

int RBDImageStore::aio_read(char* data, uint64_t offset, uint64_t size, AioCompletion* on_finish) {
  rbd_completion_t comp;
  int r = rbd_aio_create_completion(on_finish, rbd_finish_aiocb, &comp);
return 0;
  r = rbd_aio_read(image_ctx, offset, size, data, comp);
  if (r < 0) {
    log_err("queue rbd_aio_write failed.\n");
    return -1;
  }
}

int RBDImageStore::block_write(uint64_t block_id, char* data) {
return 0;
  int r = rbd_write(image_ctx, block_id * block_size, block_size, data);
  if (r < 0) {
    log_err("rbd_write failed.\n");
    return -1;
  }
}

int RBDImageStore::block_read(uint64_t block_id, char* data) {
return 0;
  int r = rbd_read(image_ctx, block_id * block_size, block_size, data);
  if (r < 0) {
    log_err("rbd_write failed.\n");
    return -1;
  }
}

int RBDImageStore::block_aio_write(uint64_t block_id, char* data, AioCompletion* on_finish) {
  rbd_completion_t comp;
  int r = rbd_aio_create_completion(on_finish, rbd_finish_aiocb, &comp);
return 0;
  r = rbd_aio_write(image_ctx, block_id * block_size, block_size, data, comp);
  if (r < 0) {
    log_err("queue rbd_aio_write failed.\n");
    return -1;
  }
}

int RBDImageStore::block_aio_read(uint64_t block_id, char* data, AioCompletion* on_finish) {
  rbd_completion_t comp;
  int r = rbd_aio_create_completion(on_finish, rbd_finish_aiocb, &comp);
return 0;
  r = rbd_aio_read(image_ctx, block_id * block_size, block_size, data, comp);
  if (r < 0) {
    log_err("queue rbd_aio_read failed.\n");
    return -1;
  }
}

}// hdcs
}// store
