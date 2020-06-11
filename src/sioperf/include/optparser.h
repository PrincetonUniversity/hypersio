#ifndef __OPTPARSER_H__
#define __OPTPARSER_H__

#include <string>
#include <vector>
#include <map>

class OptParser
{
public:
    OptParser();
    ~OptParser();
    bool GetOpt(const char *opt, std::string* opt_val);
    bool Parse(const int argc, char **argv);
private:
    std::map<std::string, std::string> _opt_val_map;
};

#endif