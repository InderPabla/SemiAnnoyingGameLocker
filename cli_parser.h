#pragma once
#include "types.h"

class CliParser {
public:
    static CliArgs parse(int argc, char* argv[]);
    static void    printUsage();

private:
    static void requireNext(int argc, char* argv[], int i, const std::string& flag);
};
