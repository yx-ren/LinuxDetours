#include <thread>
#include <iostream>
#include <functional>
#include <glog/logging.h>
#include <skyguard/LinuxDetours/detours/sdk/SGDetour.h>

using namespace SGLD_NAMESPACE;

void* test_runner(int n)
{
    for (int i = 0; i != n; i++)
    {
        LOG(INFO) << "test_runner: calling sleep() for :[ " << i << " ] second";
        sleep(i);
    }

    return NULL;
}

unsigned int sleep_detour(unsigned int seconds)
{
    LOG(INFO) << "hook!!!, trigger sleep_detour(), arguments:[ " << seconds << " ]";
    return 0;
}

int main(int argc, const char* argv[])
{
    SGDetour::globalInstall();

    SGDetourSPtr sg_detour(std::make_shared<SGDetour>());
    if (!sg_detour->installHook((void*)sleep, (void*)sleep_detour))
    {
        LOG(INFO) << "call installHook() failed";
        return -1;
    }

    int repeat_times = 10;
    std::thread thread_test_detour(std::bind(test_runner, repeat_times));
    thread_test_detour.join();

    sg_detour->uninstallHook();

    int sleep_sec = 3;
    LOG(INFO) << "uninstall hook, start to sleep:[ " << sleep_sec << " ]";
    sleep(sleep_sec);

    LOG(INFO) << "sleep done";
    SGDetour::globalUnstall();

    return 0;
}
