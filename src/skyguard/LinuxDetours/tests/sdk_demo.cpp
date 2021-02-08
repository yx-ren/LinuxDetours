#include <unistd.h>
#include <thread>
#include <iostream>
#include <functional>
#include <skyguard/LinuxDetours/detours/sdk/SGDetour.h>
#if 0
#include <skyguard/LinuxDetours/logger/Logger.h>
#include <skyguard/LinuxDetours/logger/LoggerManager.h>
#endif

using namespace SGLD_NAMESPACE;

unsigned int sleep_detour(unsigned int seconds)
{
    std::cout << "hook!!!, trigger sleep_detour(), arguments:[ " << seconds << " ], start to sleep:[" << seconds << "] sec" << std::endl;
    return sleep(seconds);
}

unsigned int test_custom(unsigned int a, unsigned int b, unsigned int c, unsigned int d,
        unsigned int e, unsigned int f, unsigned int g, unsigned int h)
{
    std::cout << "detours_test: Called test_custom() with params:\n"
        << "arg1:[" << a << "]\narg2:[" << b << "]\narg3:[" << c << "]\narg4:[" << d <<"]\n"
        << "arg5:[" << e << "]\narg6:[" << f << "]\narg7:[" << g << "]\narg8:[" << h <<"]\n"
        << std::endl;

    return a + b + c + d + e + f + g + h;
}

unsigned int test_custom_detour(unsigned int a, unsigned int b, unsigned int c, unsigned int d,
        unsigned int e, unsigned int f, unsigned int g, unsigned int h)
{
    int replace_arg1 = a * a;
    std::cout << "hook!!!, trigger test_custom_detour(), with params:\n"
        << "arg1:[" << a << "]\narg2:[" << b << "]\narg3:[" << c << "]\narg4:[" << d <<"]\n"
        << "arg5:[" << e << "]\narg6:[" << f << "]\narg7:[" << g << "]\narg8:[" << h <<"]\n"
        << "modify arg1:[ " << a << " ] -> [ " << replace_arg1 << " ]"
        << std::endl;

    return test_custom(replace_arg1, b, c, d, e, f, g, h);
}

void* test_sleep_runner(int n)
{
    for (int i = 1; i <= n; i++)
    {
        std::cout << "test_sleep_runner: calling sleep() for :[ " << i << " ] second" << std::endl;
        sleep(i);
    }

    return NULL;
}

void* test_custom_runner(int val)
{
    int ret = test_custom(val, val + 1, val + 2, val + 3,
                val + 4, val + 5, val + 6, val + 7);
    std::cout << "test_custom_runner() Function 'test_custom()' returned:[ " << ret << " ]" << std::endl;

    return NULL;
}

int main(int argc, const char* argv[])
{
    SGDetour::globalInstall();

    SGDetourSPtr sg_detour_sleep(std::make_shared<SGDetour>());
    if (!sg_detour_sleep->installHook((void*)sleep, (void*)sleep_detour))
    {
        std::cout << "call installHook() failed for fun:[ sleep() ]" << std::endl;
        return -1;
    }

    SGDetourSPtr sg_detour_custom(std::make_shared<SGDetour>());
    if (!sg_detour_custom->installHook((void*)test_custom, (void*)test_custom_detour))
    {
        std::cout << "call installHook() failed for fun:[ test_custom() ]" << std::endl;
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
    std::cout << "uninstall hook, start to sleep:[ " << sleep_sec << " ]" << std::endl;
    sleep(sleep_sec);

    std::cout << "sleep done" << std::endl;
    SGDetour::globalUnstall();

    return 0;
}
