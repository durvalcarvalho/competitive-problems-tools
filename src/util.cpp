#include <filesystem>
#include <fstream>
#include <iomanip>
#include <openssl/ssl.h>
#include <sstream>

#include "exceptions.h"
#include "fs.h"
#include "json.hpp"
#include "util.h"

namespace cptools::util {

vector<string> split(const string &s, char delim) {
  enum State { NONE, ESCAPED, TOKEN };
  enum State state = NONE;

  std::string temp = s + delim;
  int start = -1, count = 0;
  std::vector<std::string> tokens;

  for (size_t i = 0; i < temp.size(); ++i) {
    auto c = temp[i];

    switch (state) {
    case NONE:
      if (c == '\"') {
        state = ESCAPED;
        start = i + 1;
        count = 0;
      } else if (c != delim) {
        state = TOKEN;
        start = i;
        count = 1;
      }

      break;

    case ESCAPED:
      if (c == '\"')
        state = TOKEN;
      else
        ++count;

      break;

    case TOKEN:
      if (c == delim) {
        if (count)
          tokens.push_back(temp.substr(start, count));
        else
          tokens.push_back("");

        count = 0;
        state = NONE;
      } else
        ++count;

      break;
    }
  }

  return tokens;
}

string sha_512(const string &s) {
  unsigned char hash[SHA512_DIGEST_LENGTH];
  std::ostringstream output;
  SHA512_CTX sha512;

  SHA512_Init(&sha512);
  SHA512_Update(&sha512, s.c_str(), s.size());
  SHA512_Final(hash, &sha512);

  output << std::hex << std::setfill('0');
  for (int i = 0; i < SHA512_DIGEST_LENGTH; i++) {
    output << std::setw(2) << (int)hash[i];
  }

  return output.str();
}

static string strip(const string &s, const string &delim) {
  auto i = s.find_first_not_of(delim);
  auto j = s.find_last_not_of(delim);

  return (i == string::npos or j < i) ? "" : s.substr(i, j - i + 1);
}

std::string strip(const std::string &s) { return strip(s, " \t\n\r\b"); }

std::string strip(const std::string &s, char c) {
  return strip(s, string(1, c));
}

nlohmann::json read_json_file(const std::string &config_file_path) {
  auto result = fs::exists(config_file_path);
  if (not result.ok)
    throw(exceptions::inexistent_file_error(config_file_path));

  std::ifstream config_file(config_file_path);
  nlohmann::json config;

  config_file >> config;

  return config;
}

} // namespace cptools::util
