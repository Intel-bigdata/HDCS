#include "ha/HACmdHandler.h"
#include "Network/hdcs_networking.h"

void request_handler (void* session_arg, std::string msg_content) {
  hdcs::ha::HDCS_CMD_MSG cmd_msg(msg_content);
  if (cmd_msg.get_type() == HDCS_CMD_MSG_CONSOLE_REPLY)
    std::cout << cmd_msg.get_cmd() << std::endl;
}

void process_cmd (std::string addr, std::string port, std::string cmd) {
  //create connection to HAManager
  hdcs::networking::Connection conn([&](void* p, std::string s){request_handler(p, s);}, 1, 1);
  conn.connect(addr, port);

  //create cmd msg
  hdcs::ha::HDCS_CMD_MSG msg_content(HDCS_CMD_MSG_CONSOLE, cmd);
  conn.communicate(std::move(std::string(msg_content.data(), msg_content.size())));
  conn.close();
}

int main(int argc, char *argv[]) {
  if (argc == 2) {
  } else {
   printf("Usage:\n\t%s ${cmdline}\n\n", argv[0]);
   return -1;
  }
  process_cmd("127.0.0.1", "10001", argv[1]);
  return 0;
}
