#include <thread>
#include <iostream>
#include <functional>
#include <glog/logging.h>
#include <skyguard/LinuxDetours/detours/sdk/SGDetour.h>

using namespace SGLD_NAMESPACE;

unsigned int sleep_detour(unsigned int seconds)
{
    LOG(INFO) << "hook!!!, trigger sleep_detour(), arguments:[ " << seconds << " ], start to sleep:[" << seconds << "] sec";
    return sleep(seconds);
}

unsigned int test_custom(unsigned int a, unsigned int b, unsigned int c, unsigned int d,
        unsigned int e, unsigned int f, unsigned int g, unsigned int h)
{
    LOG(INFO) << "detours_test: Called test_custom() with params:\n"
        << "arg1:[" << a << "]\narg2:[" << b << "]\narg3:[" << c << "]\narg4:[" << d <<"]\n"
        << "arg5:[" << e << "]\narg6:[" << f << "]\narg7:[" << g << "]\narg8:[" << h <<"]\n"
        ;

    return a + b + c + d + e + f + g + h;
}

unsigned int test_custom_detour(unsigned int a, unsigned int b, unsigned int c, unsigned int d,
        unsigned int e, unsigned int f, unsigned int g, unsigned int h)
{
    int replace_arg1 = a * a;
    LOG(INFO) << "hook!!!, trigger test_custom_detour(), with params:\n"
        << "arg1:[" << a << "]\narg2:[" << b << "]\narg3:[" << c << "]\narg4:[" << d <<"]\n"
        << "arg5:[" << e << "]\narg6:[" << f << "]\narg7:[" << g << "]\narg8:[" << h <<"]\n"
        << "modify arg1:[ " << a << " ] -> [ " << replace_arg1 << " ]"
        ;

    return test_custom(replace_arg1, b, c, d, e, f, g, h);
}

void* test_sleep_runner(int n)
{
    for (int i = 1; i <= n; i++)
    {
        LOG(INFO) << "test_sleep_runner: calling sleep() for :[ " << i << " ] second";
        sleep(i);
    }

    return NULL;
}

void* test_custom_runner(int val)
{
    int ret = test_custom(val, val + 1, val + 2, val + 3,
                val + 4, val + 5, val + 6, val + 7);
    LOG(INFO) << "test_custom_runner() Function 'test_custom()' returned:[ " << ret << " ]";

    return NULL;
}

int main(int argc, const char* argv[])
{
    SGDetour::globalInstall();

    SGDetourSPtr sg_detour_sleep(std::make_shared<SGDetour>());
    if (!sg_detour_sleep->installHook((void*)sleep, (void*)sleep_detour))
    {
        LOG(INFO) << "call installHook() failed for fun:[ sleep() ]";
        return -1;
    }

    SGDetourSPtr sg_detour_custom(std::make_shared<SGDetour>());
    if (!sg_detour_custom->installHook((void*)test_custom, (void*)test_custom_detour))
    {
        LOG(INFO) << "call installHook() failed for fun:[ test_custom() ]";
        return -1;
    }

    int repeat_times = 3;
    std::thread thread_test_sleep_detour(std::bind(test_sleep_runner, repeat_times));
    thread_test_sleep_detour.join();

    int val = 10;
    std::thread thread_test_custom_detour(std::bind(test_custom_runner, val));
    thread_test_custom_detour.join();

    sg_detour_sleep->uninstallHook();
    sg_detour_custom->uninstallHook();

    int sleep_sec = 3;
    LOG(INFO) << "uninstall hook, start to sleep:[ " << sleep_sec << " ]";
    sleep(sleep_sec);

    LOG(INFO) << "sleep done";
    SGDetour::globalUnstall();

    return 0;
}
