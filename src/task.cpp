#include <algorithm>
#include <filesystem>

#include "dirs.h"
#include "error.h"
#include "fs.h"
#include "message.h"
#include "sh.h"
#include "task.h"
#include "util.h"

using std::to_string;

using std::filesystem::copy_file;
using std::filesystem::create_directory;
using std::filesystem::filesystem_error;

namespace cptools::task {

std::vector<std::pair<std::string, std::string>>
generate_io_files(const std::string &testset, std::ostream &, std::ostream &err,
                  bool gen_output) {
  std::vector<std::string> sets{"samples", "manual", "random"};

  auto it = std::find(sets.begin(), sets.end(), testset);

  if (it != sets.end()) {
    sets.clear();
    sets.push_back(testset);
  }

  auto input_dir{std::string(CP_TOOLS_BUILD_DIR) + "/input/"};
  auto output_dir{std::string(CP_TOOLS_BUILD_DIR) + "/output/"};
  auto program{std::string(CP_TOOLS_BUILD_DIR) + "/solution"};

  auto config = cptools::util::read_json_file("config.json");
  auto source =
      "solutions/" + cptools::util::get_json_value(config, "solutions|default",
                                                   std::string("ERROR"));

  bool fsres = false;
  try {
    fsres = create_directory(input_dir);
  } catch (const filesystem_error &error) {
  }

  if (not fsres) {
    err << message::failure("Can't create '" + input_dir + "'\n");
    return {};
  }

  try {
    fsres = create_directory(output_dir);
  } catch (const filesystem_error &error) {
  }

  if (not fsres) {
    err << message::failure("Can't create '" + output_dir + "'\n");
    return {};
  }

  if (source == "solutions/ERROR") {
    err << message::failure("Default solution file not found!\n");
    return {};
  }

  auto res = cptools::sh::build(program, source);

  if (res.rc != CP_TOOLS_OK) {
    err << message::failure("Can't compile solution '" + source + "'!") << "\n";
    err << message::trace(res.output) << '\n';
    return {};
  }

  std::vector<std::pair<std::string, std::string>> io_files;
  int next = 1;

  for (auto s : sets) {
    if (s == "random") {
      source = cptools::util::get_json_value(config, "tools|generator",
                                             std::string("ERROR"));

      if (source == "tools/ERROR") {
        err << message::failure("Generator file not found!\n");
        return {};
      }

      auto generator = std::string(CP_TOOLS_BUILD_DIR) + "/generator";

      res = cptools::sh::build(generator, source);

      if (res.rc != CP_TOOLS_OK) {
        err << message::failure("Can't compile generator '" + source + "'!")
            << "\n";
        err << message::trace(res.output) << '\n';
        return {};
      }

      auto inputs = cptools::util::get_json_value(config, "tests|random",
                                                  std::vector<std::string>{});

      for (auto parameters : inputs) {
        std::string dest{input_dir + std::to_string(next++)};

        auto res = sh::execute(generator, parameters, "", dest);

        if (res.rc != CP_TOOLS_OK) {
          err << message::failure("Error generating '" + dest +
                                  "' with parameters " + parameters)
              << "\n";
          err << message::trace(res.output) << '\n';
          return {};
        }

        io_files.emplace_back(std::make_pair(dest, ""));
      }
    } else {
      auto inputs = cptools::util::get_json_value(
          config, "tests|" + s, std::map<std::string, std::string>{});

      for (auto [input, comment] : inputs) {
        std::string dest{input_dir + std::to_string(next++)};

        bool res = false;
        try {
          res = copy_file(input, dest);
        } catch (const filesystem_error &error) {
        }

        if (not res) {
          err << message::failure("Can't copy input '" + input + "' on " +
                                  dest + "!");
          return {};
        }

        io_files.emplace_back(std::make_pair(dest, ""));
      }
    }
  }

  if (gen_output) {
    for (int i = 1; i < next; ++i) {
      std::string input{io_files[i - 1].first};
      std::string output{output_dir + std::to_string(i)};

      auto res = cptools::sh::execute(program, "", input, output);

      if (res.rc != CP_TOOLS_OK) {
        err << message::failure("Can't generate output for input '" + input +
                                "'!")
            << "\n";
        err << message::trace(res.output) << '\n';
        return {};
      }

      io_files[i - 1].second = output;
    }
  }

  return io_files;
}

int build_tools(string &error, int tools, const string &where) {
  auto dest_dir{where + "/" + CP_TOOLS_BUILD_DIR + "/"};

  bool fsres = false;
  try {
    fsres = create_directory(dest_dir);
  } catch (const filesystem_error &error) {
  }

  if (not fsres)
    return CP_TOOLS_ERROR_CPP_FILESYSTEM_CREATE_DIRECTORY;

  auto config = cptools::util::read_json_file("config.json");

  for (int mask = 1; mask <= tools; mask <<= 1) {
    int tool = tools & mask;
    string program = "";

    switch (tool) {
    case tools::VALIDATOR:
      program = "validator";
      break;

    case tools::CHECKER:
      program = "checker";
      break;

    case tools::GENERATOR:
      program = "generator";
      break;

    case tools::INTERACTOR:
      program = "interactor";
      break;

    default:
      error = "Invalid tool required: (" + to_string(tools) + ")";
      return CP_TOOLS_ERROR_TASK_INVALID_TOOL;
    }

    auto source = cptools::util::get_json_value(config, "tools|" + program,
                                                std::string(""));

    if (source.empty()) {
      error = "Can't find source for '" + program + "' in config file";
      return CP_TOOLS_ERROR_TASK_INVALID_TOOL;
    }

    auto dest = dest_dir + program;

    auto res = cptools::sh::build(dest, source);

    if (res.rc != CP_TOOLS_OK) {
      error + message::failure("Can't compile '" + source + "'!") + "\n";
      error + message::trace(res.output) + '\n';
      return res.rc;
    }
  }

  return CP_TOOLS_OK;
}

int gen_exe(string &error, const string &source, const string &dest,
            const string &where) {
  auto dest_dir{where + "/" + CP_TOOLS_BUILD_DIR + "/"};

  bool fsres = false;
  try {
    fsres = create_directory(dest_dir);
  } catch (const filesystem_error &error) {
  }

  if (not fsres)
    return CP_TOOLS_ERROR_CPP_FILESYSTEM_CREATE_DIRECTORY;

  auto program{dest_dir + dest};

  auto res = sh::remove_file(program);

  if (res.rc != CP_TOOLS_OK) {
    error += message::failure("Can't remove file '" + program + "'!") + "\n";
    error += message::trace(res.output) + '\n';
    return res.rc;
  }

  res = sh::build(program, source);

  if (res.rc != CP_TOOLS_OK) {
    error += message::failure("Can't build solution '" + source + "'!") + "\n";
    error += message::trace(res.output) + '\n';
    return res.rc;
  }

  return CP_TOOLS_OK;
}
} // namespace cptools::task
