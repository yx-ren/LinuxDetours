#ifndef __DETOURDETECTOR_H__
#define __DETOURDETECTOR_H__

#include <vector>
#include <string>
#include "statistics.h"

class DetourDetector
{
public:
    static bool detect(const std::vector<std::string>& dylibs, std::vector<DynamicLibDetectedResultSPtr>& results);

    static bool get_dylib_functions(const std::string& dylib_path, std::map<std::string, void*>& functions);

    static bool exec_sh(const std::string& cmd, std::string& echo);
};

#endif
