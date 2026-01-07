#pragma once

#include <sys/types.h>  // for pit_t

#include <string>   // for std::string
#include <vector>   // for std::vector

#include "profile_config.h"  // for ProfileConfig


class ArgsParser {
 public:
  /**
   * @brief Parse the command-line options and save the config to struct ProfileConfig
   * 
   * @param[out] profile_config Reference to ProfileConfig object with parsed profiling options stored
   * @param[in] argc Argument count from main()
   * @param[in] argv Argument vector from main()
   * @return true Parse successful success
   * @return false Parse failed
   */
  bool parse(ProfileConfig &profile_config, int argc, char **argv);

  /**
   * @brief Print the Profile config to stdout
   * 
   * @param config Reference to ProfileConfig object
   */
  void print_profile_config(const ProfileConfig &profile_config);

 private:
  /**
   * @brief Print helps about command-line options
   * 
   * @param program_name 
   */
  void print_help(const char *program_name);

  /**
   * @brief Convert the string of comma-seperated CPU list into the vector of CPU
   *
   * E.g., "1,3-5,7" -> [1, 3, 4, 5, 7]
   * @param cpu_id_str 
   * @return std::vector<int> 
   */
  std::vector<int> parse_comma_sperated_list(std::string cpu_id_str);
};
