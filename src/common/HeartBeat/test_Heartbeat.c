#include "common/HeartBeat/HeartBeatService.h"
#include "common/AioCompletionImp.h"
#include <iostream>

int main(int argc, char *argv[]) {
  if (argc == 5 || (argc == 3 && argv[2][0] == 'c')) {
  } else {
   printf("Usage:\n\t%s -r server/client -p 11001\n\n", argv[0]);
   return -1;
  }
  hdcs::HeartBeatOpts hb_opts(1000000000, 1000000000);
  hdcs::HeartBeatService hb_service(std::move(hb_opts));

  std::string cmd;
  std::string node;
  if (argv[2][0] == 'c') {
    while (true) {
      printf("Please input your cmd(add/del/bye 127.0.0.1:11000): ");
      std::cin >> cmd;
      std::cin >> node;
      if (cmd.compare("bye") == 0) break;
      else if(cmd.compare("add") == 0) {
        std::shared_ptr<hdcs::AioCompletion> err_handler = std::make_shared<hdcs::AioCompletionImp>([node](ssize_t r){
          printf("bad news, heartbeat to %s failed.\n", node.c_str());    
        }, -1);
        hb_service.register_node(node, err_handler);
        std::cout << node << " registered to heartbeat service.\n";
      } else if (cmd.compare("del") == 0) {
        hb_service.unregister_node(node);
        std::cout << node << " unregistered from heartbeat service.\n";
      }
    }
  }

  if (argv[2][0] == 's') {
    hb_service.add_listener(argv[4]);
  }
  return 0;
}
