#include <stdlib.h>
#include <stdio.h>
#include "include/libhdcs.h"
#include <iostream>
#include <string>

struct hdcs_data {
  void* io;
};

static void finish_aio_cb(void* comp, void* data) {
  return;
}

int main(int argc, char **argv) {
  if (argc < 2) {
    printf("usage:\n\t %s ${hdcs_instance_name}\n\ttips: Make sure there is a general.conf under execution dir.\n\n", argv[0]);
    return 1;
  }
  char help_str[] = "usage:\n"
      "\tYou Entered Interract mode, please input your cmd.\n"
      "\tflush_all\n"
      "\tpromote_all\n"
      "\tread ${offset} ${length}\n"
      "\twrite ${offset} ${data}\n"
      "\tquit\n";

  printf("%s", help_str);

  void* hdcs;
  hdcs_open(&hdcs, argv[1]);
  std::string input;
  std::string arg1;
  std::string arg2;
  ssize_t r;
  do {
  std::cout << ">> ";
  std::cin >> input;
  if ( input.compare("read")==0 ) {
    std::cin >> arg1 >> arg2;
    void* comp;
    char* data;
    char c = 'a';
    posix_memalign((void**)&data, 4096, stoi(arg2));
	  hdcs_aio_create_completion(&c, finish_aio_cb, &comp);
    hdcs_aio_read(hdcs, data, stoi(arg1), stoi(arg2), comp);
    hdcs_aio_wait_for_complete(comp);
    r = hdcs_aio_get_return_value(comp);
    if (r==0) {
      printf(">> Read Completed\n");
      if (stoi(arg2) < 4096) printf(">> %s\n", data);
      else {
        for (int i = 0; i < stoi(arg2); i++) {
          if(i%32==0) printf("\n");
          if(i%8==0) printf("  ");
          if(i%2==0) printf(" ");
          printf("%02X", (unsigned char)data[i]);
        }
        printf("\n");
      }
    }
    else printf(">> Read Failed, ret = %ld\n", r);
    free(data);
  } else if ( input.compare("write")==0 ) {
    std::cin >> arg1 >> arg2;
    void* comp;
    char c = 'a';
    int length = arg2.size();
	  hdcs_aio_create_completion(&c, finish_aio_cb, &comp);
    hdcs_aio_write(hdcs, arg2.c_str(), stoi(arg1), length, comp);
    hdcs_aio_wait_for_complete(comp);
    r = hdcs_aio_get_return_value(comp);
    if (r==0) {
      printf(">> Write Completed\n");
    } else {
      printf(">> Write Failed, ret = %ld\n", r);
    }
  } else if ( input.compare("flush_all")==0 ) {
    hdcs_flush_all(hdcs);
  } else if ( input.compare("promote_all")==0 ) {
    hdcs_promote_all(hdcs);
  } else if ( input.compare("quit")==0 || input.compare("q")==0 ) {
    break;
  } else {
    printf("Unknown cmd.\n%s", help_str);
  }
  } while(true);

  hdcs_close(hdcs);
  return 0;

failed:
	if (hdcs)
		free(hdcs);
	return 1;
}
