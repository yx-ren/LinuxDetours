#ifndef __STATISTICS_H__
#define __STATISTICS_H__

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <algorithm>

class DynamicLibDetectedResult;
class FunctionDetectedResult
{
public:
    FunctionDetectedResult(std::shared_ptr<DynamicLibDetectedResult> parent)
        : mParent(parent)
        , mTrampolineAddress(NULL)
        , mEntryPoint(NULL)
        , mName("")
    {}

    std::string toString();

    std::string getStatistics();

    void* getTrampolineAddress() const
    {
        return mTrampolineAddress;
    }

    void setTrampolineAddress(void* address)
    {
        mTrampolineAddress = address;
    }

    void* getEntryPoint() const
    {
        return mEntryPoint;
    }

    void setEntryPoint(void* address)
    {
        mEntryPoint = address;
    }

    const std::string& getName() const
    {
        return mName;
    }

    void setName(const std::string& name)
    {
        mName = name;
    }

    std::weak_ptr<DynamicLibDetectedResult> getParent()
    {
        return mParent;
    }

private:
    std::weak_ptr<DynamicLibDetectedResult> mParent;
    void* mTrampolineAddress;
    void* mEntryPoint;
    std::string mName;
};
typedef std::shared_ptr<FunctionDetectedResult> FunctionDetectedResultSPtr;
typedef std::weak_ptr<FunctionDetectedResult> FunctionDetectedResultWPtr;

class DynamicLibDetectedResult
{
public:
    DynamicLibDetectedResult()
        : mLibName("")
    {}

    DynamicLibDetectedResult(const std::string& libName)
        : mLibName(libName)
    {}

    std::string toString();

    std::string getStatistics();

    const std::string& getName() const
    {
        return mLibName;
    }

    void setName(const std::string& name)
    {
        mLibName = name;
    }

    void insert(FunctionDetectedResultSPtr result)
    {
        mResults.push_back(result);
    }

    const std::vector<FunctionDetectedResultSPtr>& getChildren() const
    {
        return mResults;
    }

    size_t size() const
    {
        return mResults.size();
    }

    size_t alloc_size() const
    {
        return std::count_if(mResults.begin(), mResults.end(),
                [](const FunctionDetectedResultSPtr res) { return res->getTrampolineAddress() != NULL; } );
    }

    size_t failed_size() const
    {
        return std::count_if(mResults.begin(), mResults.end(),
                [](const FunctionDetectedResultSPtr res) { return res->getTrampolineAddress() == NULL; } );
    }

private:
    std::vector<FunctionDetectedResultSPtr> mResults;
    std::string mLibName;
};
typedef std::shared_ptr<DynamicLibDetectedResult> DynamicLibDetectedResultSPtr;
typedef std::weak_ptr<DynamicLibDetectedResult> DynamicLibDetectedResultWPtr;

class ProcessDetectedResult
{
    //TODO......
public:
    ProcessDetectedResult()
        : mProcessName("")
    {}

    ProcessDetectedResult(const std::string& processName)
        : mProcessName(processName)
    {}

private:
    std::string mProcessName;
};
typedef std::shared_ptr<ProcessDetectedResult> ProcessLibDetectedResultSPtr;
typedef std::weak_ptr<ProcessDetectedResult> ProcessLibDetectedResultWPtr;

#endif
