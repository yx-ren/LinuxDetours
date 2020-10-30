#include "statistics.h"
#include <iostream>
#include <fstream>
#include <sstream>

std::string FunctionDetectedResult::toString()
{
    std::ostringstream oss;
    oss << "fun name:[" << mName << "], "
        << "entry point:[" << std::hex << mEntryPoint << "], "
        << "trampoline:[" << std::hex << mTrampolineAddress << "]";

    return std::move(oss.str());
}

std::string DynamicLibDetectedResult::toString()
{
    std::ostringstream oss;
    oss << "dylib:[" << mLibName << "]" << std::endl;

    for (const auto& result : mResults)
    {
        oss << "    "
            << result->toString() << std::endl;
    }

    return std::move(oss.str());
}

std::string DynamicLibDetectedResult::getStatistics()
{
    std::ostringstream oss;
#if 0
    oss << "\n-------------------- dump begin the statistics info --------------------\n";

    for (const auto& dylib_result : results)
    {
        oss << "dylib:[" << dylib_result->getName() << "], "
            << "symbol table total has functions:[" << dylib_result->size() << "], "
            << "successed alloc:[" << dylib_result->alloc_size() << "], "
            << "failed alloc:[" << dylib_result->failed_size() << "]"
            << std::endl;
    }

    oss << "\n-------------------- dump end the statistics info --------------------\n";
#else
    oss << "dylib:[" << getName() << "], "
        << "symbol table total has functions:[" << size() << "], "
        << "successed alloc:[" << alloc_size() << "], "
        << "failed alloc:[" << failed_size() << "]";
#endif

    return std::move(oss.str());
}
