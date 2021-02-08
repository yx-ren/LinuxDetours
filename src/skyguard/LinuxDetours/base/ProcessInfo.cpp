#include <skyguard/LinuxDetours/base/ProcessInfo.h>
#include <string.h>
#include <unistd.h>

SGLD_BEGIN

int ProcessInfo::getProcessID()
{
    return getpid();
}

std::string ProcessInfo::getProcessName()
{
    char ProcessPath[1024] = {0};
    if(readlink("/proc/self/exe", ProcessPath,1024) <=0)
        return "";

    const char* ProcessName = strrchr(ProcessPath, '/');

    std::string process_name = "";
    if (ProcessName != NULL)
        process_name = ++ProcessName;

    return process_name;
}

std::string ProcessInfo::getProcessPath()
{
    char ProcessPath[1024] = {0};
    if(readlink("/proc/self/exe", ProcessPath,1024) <=0)
        return "";

    return ProcessPath;
}

SGLD_END
