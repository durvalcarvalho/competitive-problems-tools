#include <getopt.h>
#include <unistd.h>

#include "commands/clean.h"
#include "defs.h"
#include "dirs.h"
#include "error.h"
#include "fs.h"
#include "message.h"
#include "sh.h"

// Raw strings
static const string help_message{
    R"message(
Remove autogenerated files. These files are located in '.cp-build' and '.cp-tmp' directories 
of the working folder.

    Option          Description

    -h              Generates this help message.
    --help

    -w              Directory to be cleaned.
    --working-dir

)message"};

namespace cptools::commands::clean {

// Global variables
static struct option longopts[] = {
    {"help", no_argument, NULL, 'h'},
    {"working-dir", required_argument, NULL, 'w'},
    {0, 0, 0, 0}};

// Auxiliary routines
string usage() { return "Usage: " NAME " clean [-h] [-w working-dir]"; }

string help() { return usage() + help_message; }

int remove_autogenerated_files(const string &target, ostream &out,
                               ostream &err) {
  out << message::info("Cleaning autogenerated files...") << "\n";

  // Finds directories that store the generated files
  string build_dir{target + "/" + CP_TOOLS_BUILD_DIR};
  string temp_dir{target + "/" + CP_TOOLS_TEMP_DIR};

  auto res1 = cptools::fs::is_directory(build_dir);
  auto res2 = cptools::fs::is_directory(temp_dir);

  if (res1.ok and res2.ok) {
    out << message::warning("No autogenerated files found!") << "\n";
    return CP_TOOLS_OK;
  }

  // Deletes the directories
  for (auto dir : {build_dir, temp_dir}) {
    auto res = cptools::fs::remove(dir);
    if (not res.ok) {
      err << message::failure(res.error_message);
      return res.rc;
    }
  }

  out << message::success() << '\n';
  return CP_TOOLS_OK;
}

// API functions
int run(int argc, char *const argv[], ostream &out, ostream &err) {
  int option = -1;
  string target{"."};

  while ((option = getopt_long(argc, argv, "hw:", longopts, NULL)) != -1) {
    switch (option) {
    case 'h':
      out << help() << '\n';
      return 0;

    case 'w':
      target = string(optarg);
      break;

    default:
      err << help() << '\n';
      return CP_TOOLS_ERROR_CLEAN_INVALID_OPTION;
    }
  }

  return remove_autogenerated_files(target, out, err);
}
} // namespace cptools::commands::clean
