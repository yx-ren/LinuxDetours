#ifndef __STATISTICS_H__
#define __STATISTICS_H__

#include <vector>
#include <algorithm>
#include <string>

class FunctionDetectedResult
{
public:
    FunctionDetectedResult()
        : mTrampolineAddress(NULL)
        , mEntryPoint(NULL)
        , mName("")
    {}

    void* mTrampolineAddress;
    void* mEntryPoint;
    std::string mName;
};

class DetourDetectedResults
{
public:
    size_t size() const { return mResults.size(); }

    size_t alloc_size() const
    {
        return std::count_if(mResults.begin(), mResults.end(),
                [](const FunctionDetectedResult& res) { return res.mTrampolineAddress != NULL; } );
    }

    size_t failed_size() const
    {
        return std::count_if(mResults.begin(), mResults.end(),
                [](const FunctionDetectedResult& res) { return res.mTrampolineAddress == NULL; } );
    }

    std::vector<FunctionDetectedResult> mResults;
};

#endif
