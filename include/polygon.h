#ifndef CP_TOOLS_POLYGON_H
#define CP_TOOLS_POLYGON_H

#include <iostream>

using std::ostream;
using std::string;

namespace cptools::polygon {

    struct Credentials {
        string key;
        string secret;
    };

    int run(int argc, char * const argv[], ostream& out, ostream& err);

    string help();
    string usage();

}

#endif
