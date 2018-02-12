#include "ha/HAManager.h"
#include "common/Config.h"
#include <iostream>
#include <stdio.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>

#define RUNNING_DIR	"/var/run/"
#define LOCK_FILE	"hdcs.lock"
#define LOG_FILE	"hdcs.log"

#include <boost/program_options.hpp>

void signal_handler(int sig) {
	switch (sig) {
	case SIGHUP:
		break;
	case SIGTERM:
		exit(0);
		break;
	}
}

void daemonize() {
  int i,lfp;
  char str[10];
	if (getppid()==1)
    return; /* already a daemon */
	i = fork();
	if (i < 0)
    exit(1); /* fork error */
	if ( i> 0)
    exit(0); /* parent exits */

	setsid();
	for (i = getdtablesize(); i>=0; --i)
    close(i);

  // handle standard I/O
	i = open("/dev/null",O_RDWR);
  dup(i);
  dup(i);
	umask(027); /* set newly created file permissions */

	chdir(RUNNING_DIR); /* change running directory */
	lfp = open(LOCK_FILE, O_RDWR|O_CREAT, 0640);
	if (lfp < 0) exit(1); /* can not open */
	if (lockf(lfp, F_TLOCK, 0) < 0) exit(0); /* can not lock */
	/* first instance continues */
	sprintf(str, "%d\n", getpid());
	write(lfp, str, strlen(str)); /* record pid to lockfile */
	signal(SIGCHLD, SIG_IGN); /* ignore child */
	signal(SIGTSTP, SIG_IGN); /* ignore tty signals */
	signal(SIGTTOU, SIG_IGN);
	signal(SIGTTIN, SIG_IGN);
	signal(SIGHUP, signal_handler); /* catch hangup signal */
	signal(SIGTERM, signal_handler); /* catch kill signal */
}

int main(int argc, char *argv[]) {
  std::string name;
  std::string config_name;
  bool daemonize_flag;

  boost::program_options::options_description desc("Usage:");
  desc.add_options()
      ("help,h", "Show brief usage message")
      ("version,v", "Display the version number")
      ("config,c", boost::program_options::value<std::string>()->default_value("general.conf"), "Path to Config file")
      ("name,n", boost::program_options::value<std::string>()->default_value("HDCSManager"), "Name defined in Config file")
      ("daemonize,d", boost::program_options::bool_switch(&daemonize_flag), "Deamonize HDCS")
      ;

  boost::program_options::variables_map args;

  try {
      boost::program_options::store(boost::program_options::parse_command_line(argc, argv, desc), args);

      if (args.count("help")) {
        std::cout << desc << std::endl;
        return 0;
      } else if (args.count("version")) {
        //TODO(): extract from header file
        std::cout << "HDCS 1.4.1" << std::endl;
        return 0;
      }

  } catch (boost::program_options::error const& e) {
      std::cerr << e.what() << '\n';
      exit( EXIT_FAILURE );
  }

  boost::program_options::notify(args);

  name = args["name"].as<std::string>();
  config_name = args["config"].as<std::string>();

  hdcs::Config config(name, config_name);
  hdcs::ha::HAConfig ha_config(config.get_config("HDCSManager"));
  hdcs::ha::HAManager ha_mgr(name, std::move(ha_config));

  if (daemonize_flag) {
    daemonize(); 
  }
  std::string cmd;
  while(1) {
    std::cin >> cmd;
  }
  return 0;
}
