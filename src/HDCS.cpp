#include <iostream>
#include <boost/program_options.hpp>

#include "HDCSController.h"


namespace po = boost::program_options;

int main(int argc, char **argv) {
  std::string name;
  std::string config_name;
  std::string hdcs_role;

  po::options_description desc("Usage:");
  desc.add_options()
      ("help,h", "Show brief usage message")
      ("version,v", "Display the version number")
      ("config,c", po::value<std::string>()->default_value("general.conf"), "Path to Config file")
      ("name,n", po::value<std::string>()->default_value("HDCS01"), "Name defined in Config file")
      ("role,r", po::value<std::string>()->default_value("slave"), "Role")
      ;

  po::variables_map args;

  try {
      po::store(po::parse_command_line(argc, argv, desc), args);

      if (args.count("help")) {
        std::cout << desc << std::endl;
        return 0;
      } else if (args.count("version")) {
        //TODO(): extract from header file
        std::cout << "HDCS 1.4.1" << std::endl;
        return 0;
      }

  } catch (po::error const& e) {
      std::cerr << e.what() << '\n';
      exit( EXIT_FAILURE );
  }

  po::notify(args);

  name = args["name"].as<std::string>();
  config_name = args["config"].as<std::string>();
  hdcs_role = args["role"].as<std::string>();

  hdcs::HDCSController controller(name, config_name, hdcs_role);
  return 0;
}
