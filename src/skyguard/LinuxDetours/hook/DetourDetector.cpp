#include "DetourDetector.h"
#include <skyguard/LinuxDetours/detours/detours.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <dlfcn.h>

bool DetourDetector::detect(const std::vector<std::string>& dylibs, std::vector<DynamicLibDetectedResultSPtr>& results)
{
    for (const auto& dylib : dylibs)
    {
        std::map<std::string, void*> functions;
        DynamicLibDetectedResultSPtr dylib_result(std::make_shared<DynamicLibDetectedResult>(dylib));
        if (get_dylib_functions(dylib, functions))
        {
            std::cout << "get dylib:[" << dylib << "] info" << std::endl;
            for (const auto fun : functions)
            {
                auto trampoline = detour_alloc_trampoline((PBYTE)fun.second);
                if (trampoline != NULL)
                    std::cout << "call detour_alloc_trampoline() success for fun: " << fun.first << "() "
                        << "trampoline address:[" << std::hex << trampoline << "]" << std::endl;
                else
                    std::cout << "call detour_alloc_trampoline() failed for fun: " << fun.first << "() "
                        << "belong to lib:[" << dylib_result->getName() << "]" << std::endl;

                FunctionDetectedResultSPtr fun_result(std::make_shared<FunctionDetectedResult>(dylib_result));
                fun_result->setTrampolineAddress(trampoline);
                fun_result->setEntryPoint(fun.second);
                fun_result->setName(fun.first);
                dylib_result->insert(fun_result);
            }
        }

        results.push_back(dylib_result);
    }

#if 0
    std::cout << "\n-------------------- dump begin the statistics info --------------------\n";
    for (const auto& dylib_result : results)
    {
        std::ostringstream oss;
        oss << "dylib:[" << dylib_result->getName() << "], "
            << "symbol table total has functions:[" << dylib_result->size() << "], "
            << "successed alloc:[" << dylib_result->alloc_size() << "], "
            << "failed alloc:[" << dylib_result->failed_size() << "]"
            << std::endl;

        std::cout << oss.str();
    }
    std::cout << "\n-------------------- dump end the statistics info --------------------\n";
#endif

    return true;
}

bool DetourDetector::exec_sh(const std::string& cmd, std::string& echo)
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

bool DetourDetector::get_dylib_functions(const std::string& dylib_path, std::map<std::string, void*>& functions)
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
