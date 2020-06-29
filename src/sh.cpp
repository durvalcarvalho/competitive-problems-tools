#include <map>
#include <chrono>
#include <vector>
#include <fstream>
#include <sstream>
#include <iostream>

#include <sys/stat.h>

#include "sh.h"
#include "util.h"
#include "dirs.h"
#include "error.h"

using std::map;
using std::vector;
using std::ostream;
using std::ifstream;
using std::to_string;
using std::ostringstream;

using timer = std::chrono::high_resolution_clock;

namespace cptools::sh {

    static double parse_time_output(const string& out)
    {
        ifstream in(out);
        string line;

        while (getline(in, line), line.find("Maximum resident set size") == string::npos);

        long long kbs = 0;

        for (auto c : line)
        {
            if (isdigit(c))
            {
                kbs *= 10;
                kbs += (c - '0');
            }
        }

        double mbs = kbs / 1024.0;

        return mbs; 
    }

    static int execute_command(const string& command, string& out)
    {
        auto fp = popen(command.c_str(), "r");

        if (fp == NULL)
            return CP_TOOLS_ERROR_SH_POPEN_FAILED;

        ostringstream oss;
        char buffer[4096];

        while (fread(buffer, sizeof(char), 4096, fp) > 0)
        {
            oss << buffer;
        }

        out = oss.str();

        return pclose(fp);
    }

    Result make_dir(const string& path)
    {
        string command { "mkdir -p " + path + " 2>&1" }, error;

        auto rc = execute_command(command, error); 

        return { rc == 0 ? CP_TOOLS_OK : CP_TOOLS_ERROR_SH_CREATE_DIRECTORY, error };
    }

    Result copy_dir(const string& dest, const string& src)
    {
        string command { "cp -r -n " + src + "/* 2>&1 " + dest }, error;

        auto rc = execute_command(command, error);

        return { rc == 0 ? CP_TOOLS_OK : CP_TOOLS_ERROR_SH_COPY_DIRECTORY, error };
    }

    Result remove_dir(const string& path)
    {
        string command { "rm -rf " + path + " 2>&1" }, error;

        auto rc = execute_command(command, error);

        return { rc == 0 ? CP_TOOLS_OK : CP_TOOLS_ERROR_SH_REMOVE_DIRECTORY, error };
    }

    Result same_dirs(const string& dirA, const string& dirB)
    {
        string command { "diff -r " + dirA + " " + dirB + " 2>&1" }, error;

        auto rc = execute_command(command, error);

        return { rc == 0 ? CP_TOOLS_TRUE : CP_TOOLS_FALSE, error };
    }

    bool is_dir(const string& path, string& error)
    {
        string command { "test -d " + path };
    
        auto rc = execute_command(command, error);

        return rc == 0;
    }


    long int last_modified(const string& filepath)
    {
        struct stat sb;

        if (lstat(filepath.c_str(), &sb) == -1)
            return 0;

        return sb.st_atime;
    }

    int copy_file(const string& dest, const string& src)
    {
        string command { "cp " + src + " " + dest };

        auto rc = system(command.c_str());

        return rc == 0 ? CP_TOOLS_OK : CP_TOOLS_ERROR_SH_COPY_FILE;
    }

    int remove_file(const string& path, string& error)
    {
        string command { "rm -f " + path + " 2>&1" };

        auto rc = execute_command(command, error); 

        return rc == 0 ? CP_TOOLS_OK : CP_TOOLS_ERROR_SH_REMOVE_FILE;
    }

    bool is_file(const string& path)
    {
        string command { "test -f " + path };

        auto rc = system(command.c_str());

        return rc == 0;
    }


    int compile_cpp(const string& output, const string& src)
    {
        string command { "g++ -o " + output + " -O2 -std=c++17 -W -Wall " + src };

        auto rc = system(command.c_str());

        return rc == 0 ? CP_TOOLS_OK : CP_TOOLS_ERROR_SH_CPP_COMPILATION_ERROR;
    }

    int build_py(const string& output, const string& src)
    {
        vector<string> commands {
            "echo '#!/usr/bin/python' > " + output,
            "cat " + src + " >> " + output,
            "chmod 755 " + output,
         };

        for (auto command : commands)
        {
            auto rc = system(command.c_str());

            if (rc != CP_TOOLS_OK)
                return CP_TOOLS_ERROR_SH_PY_BUILD_ERROR;
        }

        return CP_TOOLS_OK;
    }


    int build_tex(const string& output, const string& src)
    {
        string outdir { "." };

        if (output.find('/') != string::npos)
        {
            auto tokens = util::split(output, '/');
            outdir = tokens.front();
        }

        string command { string("export TEXINPUTS=\".:") + CP_TOOLS_CLASSES_DIR 
            + ":\" && pdflatex -output-directory=" + outdir + " " + src };

        auto rc = system(command.c_str());

        // Roda duas vezes para garantir que estilos que tenham referências sejam
        // renderizados corretamente
        if (rc == 0)
            rc = system(command.c_str());

        return rc == 0 ? CP_TOOLS_OK : CP_TOOLS_ERROR_SH_PDFLATEX_ERROR;
 
    }

    map<string, int (*)(const string&, const string&)> fs {
        { "cpp", compile_cpp },
        { "tex", build_tex },
        { "py", build_py },
    };

    int build(const string& output, const string& src)
    {
        auto tokens = util::split(src, '.');
        auto ext = tokens.back();
        auto it = fs.find(ext);

        auto x = last_modified(src);
        auto y = last_modified(output);
    
        if (x <= y)
        {
            return CP_TOOLS_OK;
        }

        if (it == fs.end())
            return CP_TOOLS_ERROR_SH_BUILD_EXT_NOT_FOUND;

        return it->second(output, src); 
    }

    int process(const string& input, const string& program, const string& output,
        int timeout)
    {
        string command { "timeout " + to_string(timeout) + "s " + program + " < " 
            + input + " > " + output };

        auto rc = system(command.c_str());

        return rc == 0 ? CP_TOOLS_OK : CP_TOOLS_ERROR_SH_PROCESS_ERROR;
    }

    int exec(const string& program, const string& args, const string& output, int timeout)
    {
        string command;

        if (timeout > 0)
        {
            command = "timeout " + to_string(timeout) + "s " + program + "  " 
                + args + " > " + output;
        } else
        {
            command = program + "  " + args + " > " + output;;
        }

        int rc = system(command.c_str());

        return WEXITSTATUS(rc);
    }

    Result execute(const string& program, const string& args, const string& infile, 
        const string& outfile, int timeout)
    {
        // Prepara o comando para o terminal
        string command { program + " " + args };

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

        return { WEXITSTATUS(rc), output };
    }

    Info profile(const string& program, const string& args, int timeout, const string& infile, 
        const string& outfile)
    {
        Info info;

        // Prepara o arquivo que conterá a saída do comando /usr/bin/time
        auto res = make_dir(CP_TOOLS_TEMP_DIR);
    
        if (res.rc != CP_TOOLS_OK)
        {
            info.rc = res.rc;
            return info;
        }

        string out { string(CP_TOOLS_TEMP_DIR) + "/.time_output" };

        // Prepara o comando para o terminal
        string command { "/usr/bin/time -v -o " + out };

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

        if (fp == NULL)
        {
            info.rc = CP_TOOLS_ERROR_SH_POPEN_FAILED;
            return info;
        }

        // Prepara o retorno
        auto t = std::chrono::duration_cast<std::chrono::duration<double>>(end - start);

        info.rc = rc;
        info.elapsed = t.count();
        info.memory = parse_time_output(out);

        return info;
    }
}

