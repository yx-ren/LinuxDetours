#include <cstdio>
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
#include "detours.h"
using namespace std::placeholders;

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

void dump_dylibs(const std::vector<std::string>& dylibs)
{
    for (const auto& dylib : dylibs)
    {
        std::map<std::string, void*> functions;
        if (get_dylib_functions(dylib, functions))
        {
            LOG(INFO) << "get dylib:[" << dylib << "] info" << std::endl;
            for (const auto fun : functions)
            {
                LOG(INFO) << "    " << "fun:[" << fun.first << "], entry point:[" << std::hex << fun.second << "]" << std::endl;
                auto trampoline = detour_alloc_trampoline((PBYTE)fun.second);

                if (trampoline != NULL)
                    LOG(INFO) << "call detour_alloc_trampoline() success for fun" << fun.first << "()"
                        << "trampoline address:[" << std::hex << trampoline << "]";
                else
                    LOG(INFO) << "call detour_alloc_trampoline() failed for fun" << fun.first << "()";
            }
        }
    }
}

static void __attribute__((constructor)) ctor()
{
#if 1
    int sleep_seconds = 5;
    auto thread_function = std::bind([](int sleep_sec)
            {
                int left_sec = sleep_sec;
                while (left_sec > 0)
                {
                    std::cout << "left:[" << left_sec << "] seconds" << std::endl;
                    sleep(1);
                    left_sec--;
                }

                std::vector<std::string> dylibs =
                {
                    "/lib64/libc.so.6",
                    "/lib64/libgtk.so"
                };
                dump_dylibs(dylibs);
            }
            , sleep_seconds
            );

    std::thread t1(thread_function);
    t1.detach();
#endif

    printf("FFFFFFFFFFFHHHHHAHA\n");
}
