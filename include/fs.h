#ifndef CP_TOOLS_FS_H
#define CP_TOOLS_FS_H

#include <filesystem>
#include <string>

namespace cptools::fs {

struct Result {
  bool ok;
  int rc;
  std::string error_message;
};

const Result make_result(bool res);
const Result make_result(bool res, int rc,
                         const std::filesystem::filesystem_error &e);
const Result make_result(bool res, int rc, const std::string err_msg);

const Result create_directory(const std::string &path);
const Result exists(const std::string &path);
const Result copy_file(const std::string &src, const std::string &dst);

std::string get_home_dir();
std::string get_default_config_path();

} // namespace cptools::fs

#endif