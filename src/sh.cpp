#include <chrono>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <vector>

#include <sys/stat.h>

#include "dirs.h"
#include "error.h"
#include "sh.h"
#include "util.h"

using std::ifstream;
using std::map;
using std::ostream;
using std::ostringstream;
using std::to_string;
using std::vector;

using timer = std::chrono::high_resolution_clock;

namespace cptools::sh {

static double parse_time_output(const string &out) {
  ifstream in(out);
  string line;

  while (getline(in, line),
         line.find("Maximum resident set size") == string::npos)
    ;

  long long kbs = 0;

  for (auto c : line) {
    if (isdigit(c)) {
      kbs *= 10;
      kbs += (c - '0');
    }
  }

  double mbs = kbs / 1024.0;

  return mbs;
}

static int execute_command(const string &command, string &out) {
  auto fp = popen(command.c_str(), "r");

  if (fp == NULL)
    return CP_TOOLS_ERROR_SH_POPEN_FAILED;

  ostringstream oss;
  char buffer[1024 * 1024];

  int amount;

  while ((amount = fread(buffer, sizeof(char), 1024 * 1024, fp)) > 0) {
    oss << string(buffer, amount);
  }

  out = oss.str();

  return pclose(fp);
}

Result make_dir(const string &path) {
  string command{"mkdir -p " + path + " 2>&1"}, error;

  auto rc = execute_command(command, error);

  return {rc == 0 ? CP_TOOLS_OK : CP_TOOLS_ERROR_SH_CREATE_DIRECTORY, error};
}

Result copy_dir(const string &dest, const string &src) {
  string command{"cp -r -n " + src + "/* 2>&1 " + dest}, error;

  auto rc = execute_command(command, error);

  return {rc == 0 ? CP_TOOLS_OK : CP_TOOLS_ERROR_SH_COPY_DIRECTORY, error};
}

Result remove_dir(const string &path) {
  string command{"rm -rf " + path + " 2>&1"}, error;

  auto rc = execute_command(command, error);

  return {rc == 0 ? CP_TOOLS_OK : CP_TOOLS_ERROR_SH_REMOVE_DIRECTORY, error};
}

Result same_dirs(const string &dirA, const string &dirB) {
  string command{"diff -r " + dirA + " " + dirB + " 2>&1"}, error;

  auto rc = execute_command(command, error);

  return {rc == 0 ? CP_TOOLS_TRUE : CP_TOOLS_FALSE, error};
}

Result is_dir(const string &path) {
  string command{"test -d " + path + " 2>&1"}, error;

  auto rc = execute_command(command, error);

  return {rc == 0 ? CP_TOOLS_TRUE : CP_TOOLS_FALSE, error};
}

long int last_modified(const string &filepath) {
  struct stat sb;

  if (lstat(filepath.c_str(), &sb) == -1)
    return 0;

  return sb.st_atime;
}

Result copy_file(const string &dest, const string &src) {
  string command{"cp " + src + " " + dest + " 2>&1"}, error;

  auto rc = execute_command(command, error);

  return {rc == 0 ? CP_TOOLS_OK : CP_TOOLS_ERROR_SH_COPY_FILE, error};
}

Result remove_file(const string &path) {
  string command{"rm -f " + path + " 2>&1"}, error;

  auto rc = execute_command(command, error);

  return {rc == 0 ? CP_TOOLS_OK : CP_TOOLS_ERROR_SH_REMOVE_FILE, error};
}

Result is_file(const string &path) {
  string command{"test -f " + path}, error;

  auto rc = execute_command(command, error);

  return {rc == 0 ? CP_TOOLS_TRUE : CP_TOOLS_FALSE, error};
}

Result compile_cpp(const string &output, const string &src) {
  string command{"g++ -o " + output + " -O2 -std=c++17 -W -Wall " + src +
                 " 2>&1"},
      error;

  auto rc = execute_command(command, error);

  return {rc == 0 ? CP_TOOLS_OK : CP_TOOLS_ERROR_SH_CPP_COMPILATION_ERROR,
          error};
}

Result build_py(const string &output, const string &src) {
  vector<string> commands{
      "echo '#!/usr/bin/python3' > " + output,
      "cat " + src + " >> " + output,
      "chmod 755 " + output,
  };

  for (auto command : commands) {
    string error;
    auto rc = execute_command(command, error);

    if (rc != CP_TOOLS_OK)
      return {CP_TOOLS_ERROR_SH_PY_BUILD_ERROR, error};
  }

  return {CP_TOOLS_OK, ""};
}

Result compile_java(const string &output, const string &src) {
  auto dir = util::split(src, '/').front();
  auto filename = util::split(src, '.').front();
  auto name = util::split(filename, '/').back();

  vector<string> commands{
      "javac " + src,
      "echo '#!/bin/bash' > " + output,
      "echo 'java -cp " + dir + "/ " + name + "' > " + output,
      "chmod 755 " + output,
  };

  for (auto command : commands) {
    string error;
    auto rc = execute_command(command, error);

    if (rc != CP_TOOLS_OK)
      return {CP_TOOLS_ERROR_SH_PY_BUILD_ERROR, error};
  }

  return {CP_TOOLS_OK, ""};
}

Result build_tex(const string &output, const string &src) {
  string outdir{"."};

  if (output.find('/') != string::npos) {
    auto tokens = util::split(output, '/');
    outdir = tokens.front();
  }

  string command{string("export TEXINPUTS=\".:") + CP_TOOLS_CLASSES_DIR +
                 ":\" && pdflatex -halt-on-error -output-directory=" + outdir +
                 " " + src};

  string error;
  auto rc = execute_command(command, error);

  // Roda duas vezes para garantir que estilos que tenham referências sejam
  // renderizados corretamente
  if (rc == CP_TOOLS_OK)
    rc = execute_command(command, error);

  return {rc == 0 ? CP_TOOLS_OK : CP_TOOLS_ERROR_SH_PDFLATEX_ERROR, error};
}

map<string, Result (*)(const string &, const string &)> fs{
    {"cpp", compile_cpp},
    {"java", compile_java},
    {"tex", build_tex},
    {"py", build_py},
};

Result build(const string &output, const string &src) {
  auto tokens = util::split(src, '.');
  auto ext = tokens.back();
  auto it = fs.find(ext);

  auto x = last_modified(src);
  auto y = last_modified(output);

  if (x <= y) {
    return {CP_TOOLS_OK, ""};
  }

  if (it == fs.end())
    return {CP_TOOLS_ERROR_SH_BUILD_EXT_NOT_FOUND, "Extension not found!"};

  return it->second(output, src);
}

Result execute(const string &program, const string &args, const string &infile,
               const string &outfile, int timeout) {
  // Prepara o comando para o terminal
  string command{program + " " + args};

  if (not infile.empty())
    command += " < " + infile;

  if (not outfile.empty())
    command += " > " + outfile;

  command += " 2>&1";

  if (timeout > 0)
    command = " timeout " + to_string(timeout) + "s " + command;

  // Executa o comando
  string output;
  auto rc = execute_command(command, output);

  return {WEXITSTATUS(rc), output};
}

Info profile(const string &program, const string &args, int timeout,
             const string &infile, const string &outfile) {
  Info info;

  // Prepara o arquivo que conterá a saída do comando /usr/bin/time
  auto res = make_dir(CP_TOOLS_TEMP_DIR);

  if (res.rc != CP_TOOLS_OK) {
    info.rc = res.rc;
    return info;
  }

  string out{string(CP_TOOLS_TEMP_DIR) + "/.time_output"};

  // Prepara o comando para o terminal
  string command{"/usr/bin/time -v -o " + out};

  if (timeout > 0)
    command += " timeout " + to_string(timeout) + "s ";

  command += " " + program + "  " + args;

  if (not infile.empty())
    command += " < " + infile;

  if (not outfile.empty())
    command += " > " + outfile;

  // Executa o comando
  auto start = timer::now();

  auto fp = popen(command.c_str(), "r");

  auto rc = pclose(fp);

  auto end = timer::now();

  if (fp == NULL) {
    info.rc = CP_TOOLS_ERROR_SH_POPEN_FAILED;
    return info;
  }

  // Prepara o retorno
  auto t =
      std::chrono::duration_cast<std::chrono::duration<double>>(end - start);

  info.rc = rc;
  info.elapsed = t.count();
  info.memory = parse_time_output(out);

  return info;
}
} // namespace cptools::sh
