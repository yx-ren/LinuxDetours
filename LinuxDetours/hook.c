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
#include "statistics.h"
using namespace std::placeholders;

bool parse_dylibs_path(const std::string& filepath, std::vector<std::string>& dylibs_path)
{
    std::ifstream ifs(filepath);
    if (!ifs.is_open())
    {
        std::cerr << "failed to open file:[" << filepath << "]" << std::endl;
        return false;
    }

    std::string file_buf((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
    std::istringstream iss(file_buf);
    std::string dylib_path;
    while (iss >> dylib_path)
    {
        std::cout << dylib_path << std::endl;
        dylibs_path.push_back(dylib_path);
    }

    return true;
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

void dump_dylibs(const std::vector<std::string>& dylibs)
{
    std::map<std::string, DetourDetectedResults> dylibs_detected_results;
    for (const auto& dylib : dylibs)
    {
        std::map<std::string, void*> functions;
        DetourDetectedResults detected_results;
        if (get_dylib_functions(dylib, functions))
        {
            LOG(INFO) << "get dylib:[" << dylib << "] info" << std::endl;
            for (const auto fun : functions)
            {
                //LOG(INFO) << "    " << "fun:[" << fun.first << "()], entry point:[" << std::hex << fun.second << "]" << std::endl;
                auto trampoline = detour_alloc_trampoline((PBYTE)fun.second);

                if (trampoline != NULL)
                    LOG(INFO) << "call detour_alloc_trampoline() success for fun: " << fun.first << "() "
                        << "trampoline address:[" << std::hex << trampoline << "]";
                else
                    LOG(INFO) << "call detour_alloc_trampoline() failed for fun: " << fun.first << "()";

                FunctionDetectedResult result;
                result.mTrampolineAddress = trampoline;
                result.mEntryPoint = fun.second;
                result.mName = fun.first;
                detected_results.mResults.push_back(result);
            }
        }

        dylibs_detected_results.insert(std::make_pair(dylib, detected_results));
    }

    LOG(INFO) << "-------------------- dump begin the statistics info --------------------";
    for (const auto& dylib_result : dylibs_detected_results)
    {
        std::ostringstream oss;
        const DetourDetectedResults& results = dylib_result.second;
        oss << "dylib:[" << dylib_result.first << "], "
            << "symbol table total has functions:[" << results.size() << "], "
            << "successed alloc:[" << results.alloc_size() << "], "
            << "failed alloc:[" << results.failed_size() << "]";

            LOG(INFO) << oss.str();
    }
    LOG(INFO) << "-------------------- dump end the statistics info --------------------";
}

static void __attribute__((constructor)) ctor()
{
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

                const char* libs_path = "/home/renyunxiang/work/github/LinuxDetours/LinuxDetours/config/dylibs";
                std::vector<std::string> dylibs;
                if (!parse_dylibs_path(libs_path, dylibs))
                {
                    std::cerr << "call parse_dylibs_path() failed" << std::endl;
                    return;
                }

                dump_dylibs(dylibs);

                exit(0);
            }
            , sleep_seconds
            );

    std::thread t1(thread_function);
    t1.detach();

    printf("-------------------- quit from [__attribute__((constructor)) ctor()] --------------------\n");
}
