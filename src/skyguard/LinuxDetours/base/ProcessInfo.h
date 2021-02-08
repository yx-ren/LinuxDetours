#ifndef __SG_DETOUR_BASE_H__
#define __SG_DETOUR_BASE_H__

#include <string>
#include <skyguard/LinuxDetours/common.h>

SGLD_BEGIN

class ProcessInfo
{
public:
    static int getProcessID();

    static std::string getProcessName();

    static std::string getProcessPath();

};

SGLD_END

#endif
