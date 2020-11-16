#include <cstdio>
#define _X86_
//#include "detours.h"
#include <skyguard/LinuxDetours/detours/detours.h>
#include <stdio.h>
#include <unistd.h>
#include <glog/logging.h>
#include <iostream>
#include <dlfcn.h>
#include <iostream>
#include <chrono>
#include <dlfcn.h>
#include <mutex>
#include <memory>
#include <thread>
#include <set>
#include <vector>
#include <algorithm>
#include <map>
#include <list>
#include <sstream>
#include <fstream>
#include <atomic>
#include <condition_variable>
#include <string.h>
#include <unistd.h>

#define USE_CUSTOM_FUNCTION 0

unsigned int sleep_detour(unsigned int seconds)
{
    LOG(INFO) << "detours_test: Called sleep_detour()";
    return sleep(seconds);
}
unsigned int test_detour_b(unsigned int seconds, unsigned int a, unsigned int b, unsigned int c, unsigned int d, unsigned int e)
{
    LOG(INFO) << "detours_test: Called test_detour_b";
    return seconds + 1;
}
unsigned int test_detour_a(unsigned int seconds, unsigned int a, unsigned int b, unsigned int c, unsigned int d, unsigned int e)
{
    LOG(INFO) << "detours_test: Detoured function 'test_detour_b' -> function 'test_detour_a' with params: "
              << a << ", " << b << ", " << c << ", " << d << ", " << e;
    return test_detour_b(seconds + 2, a, b, c, d, e);
}

VOID* test_runner(void*)
{
#if USE_CUSTOM_FUNCTION
    LOG(INFO) << "detours_test: Function 'test_detour_b' returned " << test_detour_b(1, 2, 3, 4, 5, 6);
#endif

    sleep(1);
    LOG(INFO) << "detours_test: Calling sleep for 1 second";
    sleep(2);
    LOG(INFO) << "detours_test: Calling sleep again for 2 seconds";
    
    LOG(INFO)  << "detours_test: Done sleeping\n";

    return NULL;
}

int test_glog(char * argv)
{
    google::InitGoogleLogging(argv);
    FLAGS_logtostderr = true;

    LOG(INFO) << "detours_test: Starting detours tests";
    return 1;
}

bool exec_sh(const std::string& cmd, std::string& echo)
{
    const int SIZE = 0xffff;
    char charBuff[SIZE];

    if (system(nullptr) == 0)
    {
        std::cerr << "shell not available" << std::endl;
        return false;
    }

    auto cmdOut = popen(cmd.c_str(), "r");
    if (cmdOut == nullptr)
    {
        std::cerr << "popen error" << std::endl;
        return false;
    }

    while (fgets(charBuff, SIZE, cmdOut) != nullptr)
        echo.append(charBuff);

    pclose(cmdOut);

    return true;
}

bool get_dylib_functions(const std::string& dylib_path, std::map<std::string, void*>& functions)
{
    std::ostringstream oss;
    oss << "nm -D " << dylib_path << " | awk '{print $3}'";

    std::string cmd = oss.str();
    std::string result;
    if (exec_sh(cmd, result))
    {
        std::vector<std::string> functions_name;
        std::istringstream iss(result);
        std::string fun_name;
        while (iss >> fun_name)
            functions_name.push_back(fun_name);

        void* libfunctions = dlopen(dylib_path.c_str(), RTLD_NOW);
        if (libfunctions == NULL)
        {
            printf("call dlopen() failed, failed to open dynamic lib:[%s]\n", dylib_path.c_str());
            return false;
        }

        for (const auto& fun_name : functions_name)
        {
            void* fun_address = dlsym(libfunctions, fun_name.c_str()); 
            if (fun_address == NULL)
            {
                printf("call dlsym() failed, failed to found fun:[%s] entry point\n", fun_name.c_str());
                continue;
            }

            functions.insert(std::make_pair(fun_name.c_str(), fun_address));
        }
    }
}

int main(int argc, char * argv[])
{
    sleep(1);
    test_glog(argv[0]);

#if 0
    std::cout << "main() start long sleeping" << std::endl;
    sleep(10000000);
#endif

    DetourBarrierProcessAttach();
    DetourCriticalInitialize();

    ULONG ret = 0;

    LONG sleep_detour_callback = 0;
    TRACED_HOOK_HANDLE sleep_detour_handle = new HOOK_TRACE_INFO();    
    DetourInstallHook((void*)sleep, (void*)sleep_detour, &sleep_detour_callback, sleep_detour_handle);
    ret = DetourSetExclusiveACL(new ULONG(), 1, (TRACED_HOOK_HANDLE)sleep_detour_handle);

#if USE_CUSTOM_FUNCTION
    LONG test_detour_callback = 0;
    TRACED_HOOK_HANDLE test_detour_handle = new HOOK_TRACE_INFO();
    DetourInstallHook((void*)test_detour_b, (void*)test_detour_a, &test_detour_callback, test_detour_handle);
    ret = DetourSetExclusiveACL(new ULONG(), 1, (TRACED_HOOK_HANDLE)test_detour_handle);
#endif

#if 0
    LOG(INFO) << "main: calling [sleep()] with 1 seconds before test thread";
    sleep(1);
#endif

    pthread_t t;
    pthread_create(&t, NULL, test_runner, NULL);
    pthread_join(t, NULL);

    DetourUninstallHook(sleep_detour_handle);
    delete sleep_detour_handle;

#if USE_CUSTOM_FUNCTION
    delete test_detour_handle;
    DetourUninstallHook(test_detour_handle);
#endif

    sleep(1);

    DetourCriticalFinalize();
    DetourBarrierProcessDetach();

    std::cout << "hello asm" << std::endl;

    return 0;
}
