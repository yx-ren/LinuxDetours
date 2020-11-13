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
#include "DetourDetector.h"

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

                std::vector<std::string> dylibs;
                std::string cfg = "./config/dylibs";
                if (!parse_dylibs_path(cfg, dylibs))
                {
                    std::cerr << "call parse_dylibs_path() failed" << std::endl;
                    return;
                }

                std::vector<DynamicLibDetectedResultSPtr> results;
                if (DetourDetector::detect(dylibs, results))
                {
                    for (const auto& dylib_result : results)
                        std::cout << dylib_result->toString() << std::endl;

                    for (const auto& dylib_result : results)
                        std::cout << dylib_result->getStatistics() << std::endl;
                }

                exit(0);
            }
            , sleep_seconds
            );

    std::thread t1(thread_function);
    t1.detach();

    printf("-------------------- quit from [__attribute__((constructor)) ctor()] --------------------\n");
}
