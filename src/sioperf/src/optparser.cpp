#include "optparser.h"
#include "dbg.h"

using namespace std;

OptParser::OptParser() {}
OptParser::~OptParser(){}

bool OptParser::Parse(const int argc, char **argv)
{
    int i;
    string arg;
    string opt;
    string opt_val;
    size_t found;
    bool scan_opt_val;

    scan_opt_val = false;
    // start with index 1, since 0 element is always app name
    for (i = 1; i < argc; i++) {
        arg = string(argv[i]);
        if (arg[0] == '-') {
            if (scan_opt_val) {
                // set an empty value for previous option
                _opt_val_map[opt] = string("");
            }
            // argument is option name
            // read
            found = arg.rfind("-");
            opt = arg.substr(found+1, arg.length()-found-1);
            scan_opt_val = true;
        } else {
            if (scan_opt_val) {
                // Reading option value
                _opt_val_map[opt] = arg;
                scan_opt_val = false;
            } else {
                LOG_ERR("Unexpected option: %s\n", arg.c_str());
                return false;
            }
        }
    }
    // push the last option without value
    if (scan_opt_val) {
        _opt_val_map[opt] = string("");
    }

    return true;
}

bool OptParser::GetOpt(const char *opt, string* opt_val)
{
    map<string, string>::iterator it;

    it = _opt_val_map.find(string(opt));
    if (it == _opt_val_map.end()) {
        return false;
    }

    *opt_val = it->second;
    return true;
}