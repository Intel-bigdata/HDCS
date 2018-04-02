#ifndef HDCSDATASTOREGUARD_H
#define HDCSDATASTOREGUARD_H

#define HDCS_COMMIT_LOG_PRELOAD_LENGTH 3000
#include "common/Log.h"
#include <stdlib.h>
#include <cstring>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <mutex>

namespace hdcs {
namespace store {

struct DataStoreCommitLogSuperBlock {
  //uint32_t last_submit_pos; //TODO later
  DataStoreCommitLogSuperBlock():last_commit_pos(0){}
  uint32_t last_commit_pos;
};

struct DataStoreCommitLogItem {
  DataStoreCommitLogItem():flag(0xF0F0F0F0) {}
  DataStoreCommitLogItem(uint32_t tid, uint64_t offset, uint64_t length):
    tid(tid), offset(offset), length(length), flag(0xF0F0F0F0) {}
  uint32_t flag;
  uint32_t tid;
  uint64_t offset;
  uint64_t length;
};

class DataStoreCommitLog {
public:
  DataStoreCommitLog (std::string dir_path, uint32_t log_size, std::string hdcs_core_type):
    log_size(log_size),
    hdcs_core_type(hdcs_core_type) {
    std::string path = dir_path + ".commitlog"; 
    size = sizeof(DataStoreCommitLogSuperBlock)  + log_size * sizeof(DataStoreCommitLogItem);
    int mode = O_CREAT | O_NOATIME | O_RDWR | O_SYNC, permission = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP;
    //int mode = O_CREAT | O_NOATIME | O_RDWR, permission = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP;
    log_fd = ::open( path.c_str(), mode, permission );
    if (log_fd < 0) {
      printf( "[ERROR] DataStoreCommitLog::open_and_init, unable to open %s, error: %s \n", path, std::strerror(log_fd) );
        ::close(log_fd);
        return;
    }

    struct stat file_st;
    memset(&file_st, 0, sizeof(file_st));
    ::fstat(log_fd, &file_st);
    if (file_st.st_size < size) {
      if (-1 == ftruncate(log_fd, size)) {
        ::close(log_fd);
        return;
      }
    }
    init_superblock();
    std::cout << "last commit pos:" << super_block.last_commit_pos << std::endl;
  }

  ~DataStoreCommitLog () {
    ::close(log_fd);
  }

  bool sync() {
    if (hdcs_core_type.compare("slave") == 0) {
      sync_data_from_master();
    }
    return true;
  }

  int init_superblock () {
    int ret = ::pread(log_fd, (char*)(&super_block), sizeof(DataStoreCommitLogSuperBlock), 0); 
    if (ret < 0) {
      return ret;
    }
    // successfully read data
    // TODO: replay data from last commit
    std::cout << "last_commit_pos: " << super_block.last_commit_pos << std::endl;
    uint32_t last_commit_pos = find_last_pos(super_block.last_commit_pos);
    super_block.last_commit_pos = last_commit_pos;

    //uint32_t last_submit_pos = find_last_pos(super_block.last_submit_pos);
    //super_block.last_submit_pos = last_submit_pos;
    //assert(super_block.last_submit_pos == super_block.last_commit_pos);
    return 0;
  }

  uint32_t find_last_pos(uint32_t start_pos) {
    uint32_t target_pos;
    // go through commit log from the last commit, size by ~64k.
    DataStoreCommitLogItem* log_item_array = new DataStoreCommitLogItem[HDCS_COMMIT_LOG_PRELOAD_LENGTH];
    uint32_t read_length = HDCS_COMMIT_LOG_PRELOAD_LENGTH;

    uint32_t max_tid = 0;
    uint64_t step = 1;
    target_pos = start_pos;
    uint32_t remain_length = log_size - target_pos;
    uint32_t unsearched_length = log_size;
    while (unsearched_length) {
      read_length = remain_length > HDCS_COMMIT_LOG_PRELOAD_LENGTH ? HDCS_COMMIT_LOG_PRELOAD_LENGTH : remain_length;
      std::cout << "read_length: " << read_length << std::endl;
      if (read_length == 0) {
        target_pos = 1;
        remain_length = log_size - target_pos;
        continue;
      }
      int ret = ::pread(log_fd, (char*)(log_item_array),
          sizeof(DataStoreCommitLogItem) * read_length,
          sizeof(DataStoreCommitLogSuperBlock) + sizeof(DataStoreCommitLogItem) * target_pos); 
      if (ret < 0) {
        delete log_item_array;
        return ret;
      }
      remain_length -= read_length;
      unsearched_length -= read_length;

      // check data
      for (uint32_t i = 0; i < read_length; i++) {
        std::cout << "tid: " << log_item_array[i].tid << std::endl;
        if (log_item_array[i].tid > max_tid) {
          max_tid = log_item_array[i].tid;
          step = log_item_array[i].length % 4096?log_item_array[i].length / 4096 + 1:log_item_array[i].length / 4096;
        } else if (log_item_array[i].tid == 1){
          // tid ran out, back to 1
          max_tid = log_item_array[i].tid;
          step = log_item_array[i].length % 4096?log_item_array[i].length / 4096 + 1:log_item_array[i].length / 4096;
        } else {
          delete log_item_array;
          return target_pos;
        }
        target_pos += step;
      }
    }
    delete log_item_array;
    return start_pos;
  }

  void sync_data_from_master () {
    //1. compare local commit log to master commit log
    //2. request data from master of those tid differs with master commit log 
  }

  int update_superblock (uint32_t pos) {
    super_block.last_commit_pos = pos;
    int ret = ::pwrite(log_fd, (char*)(&super_block), sizeof(DataStoreCommitLogSuperBlock), 0); 
    return ret;
    /*if (mmapped_superblock == NULL) {
      mmapped_superblock = ::mmap(NULL, sizeof(DataStoreCommitLogSuperBlock),
          PROT_READ | PROT_WRITE, MAP_SHARED | MAP_POPULATE, log_fd, 0);
      if (mmapped_superblock == MAP_FAILED) {
        return -1;
      }
    }
    memcpy(mmapped_superblock, super_block, sizeof(DataStoreCommitLogSuperBlock));*/ 
  }

  int append_log (uint32_t tid, uint32_t pos, uint64_t offset, uint64_t length) {
    //uint32_t pos = super_block.last_commit_pos;
    DataStoreCommitLogItem data(tid, offset, length);
    //printf("pwrite(%d, %p, %d, %d)\n", log_fd, &data, sizeof(DataStoreCommitLogItem), sizeof(DataStoreCommitLogSuperBlock) + sizeof(DataStoreCommitLogItem) * pos);
    int ret = ::pwrite(log_fd, (char*)&data,
        sizeof(DataStoreCommitLogItem),
        sizeof(DataStoreCommitLogSuperBlock) + sizeof(DataStoreCommitLogItem) * pos); 
    return ret;
  }

  void replay_uncommitted_block () {
    // TODO: used for transactional support, 
    // replay inconsistent data between last_submit and last_commit
    /*
    uint32_t read_log_length = (last_submit_pos - last_commit_pos)/sizeof(DataStoreCommitLogItem);
    DataStoreCommitLogItem* log_item_array = new DataStoreCommitLogItem[read_log_length]();
    int ret = ::pread(log_fd, (char*)(&log_item_array),
        sizeof(DataStoreCommitLogItem) * read_log_length,
        super_block.freed_block_pos + last_commit_pos + sizeof(DataStoreCommitLogItem)); 
    if (ret < 0) {
      return ret;
    }
    // Till now we loaded the uncommitted part into memory
    for (uint32_t i = 0; i <= read_log_length; i++) {
      data_store_journal.replay(log_item_array[i].tid, log_item_array[i].block_id);
    }
    */
  }

private:
  int log_fd;
  DataStoreCommitLogSuperBlock super_block;
  DataStoreCommitLogItem* commit_log_head_ptr; 
  //void* mmapped_superblock; 
  uint32_t log_size;
  uint64_t size;
  std::string hdcs_core_type;
};

class DataStoreGuard {
public:
  DataStoreGuard (std::string path, uint32_t log_size,
      std::string hdcs_core_type) :
    commit_log (path, log_size, hdcs_core_type)  {

  }

  ~DataStoreGuard () {

  }

  bool check_data_consistency () {
    // compare local hash with commit log from master
    return commit_log.sync();
  }

  int commit(uint32_t tid, uint32_t commit_pos, uint64_t offset, uint64_t length) {
    //std::lock_guard<std::mutex> lock(commit_log_mutex);
    commit_log.append_log(tid, commit_pos, offset, length);
    commit_log.update_superblock(commit_pos);
    return 0;
  }

private:
  DataStoreCommitLog commit_log;
  std::mutex commit_log_mutex;
};

}
}

#endif
