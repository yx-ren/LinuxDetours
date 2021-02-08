#include <skyguard/LinuxDetours/detours/sdk/SGDetourImpl.h>
#include <fstream>
#include <iterator>
#include <skyguard/LinuxDetours/base/ProcessInfo.h>
#include <skyguard/LinuxDetours/logger/Logger.h>
#include <skyguard/LinuxDetours/logger/LoggerManager.h>

SGLD_BEGIN
bool SGDetourImpl::mIsLoggerEnabled = false;
SGDetourImpl::SGDetourImpl()
    : mHookHandle(NULL)
    , mSrcEntryPoint(NULL)
    , mHookEntryPoint(NULL)
{}

SGDetourImpl::~SGDetourImpl()
{
    if (mHookHandle)
        uninstallHook();
}

void SGDetourImpl::globalInstall()
{
    DetourBarrierProcessAttach();
    DetourCriticalInitialize();
    loggerInstall();
}

void SGDetourImpl::globalUnstall()
{
    DetourCriticalFinalize();
    DetourBarrierProcessDetach();
}

void SGDetourImpl::preInstallHook(void* SrcEntryPoint, void* HookEntryPoint)
{
    CBT_INFO("SGDetour()", "preInstallHook() hook fun entry point:" << SrcEntryPoint
            << ", dump the proc self maps:\n"
            << fetchSelfMaps());
}

bool SGDetourImpl::installHook(void* SrcEntryPoint, void* HookEntryPoint)
{
    CBT_DEBUG("SGDetourImpl", "installHook() trigger, "
            << "Src Entry Point:" << SrcEntryPoint << ", "
            << "Hook Entry PointSrc:" << HookEntryPoint);

    std::shared_ptr<HOOK_TRACE_INFO> detour_handle = std::make_shared<HOOK_TRACE_INFO>();
    if (!detour_handle)
    {
        CB_ERROR("SGDetourImpl::installHook() alloc memory failed for struct:[HOOK_TRACE_INFO]");
        return false;
    }

    int ret = 0;
    uint64_t detour_callback = 0;
    ret = DetourInstallHook(SrcEntryPoint, HookEntryPoint, &detour_callback, detour_handle.get());
    if (ret != NO_ERROR)
    {
        CB_ERROR("SGDetourImpl::installHook() call DetourInstallHook() failed, error code:[ " << ret << "] "
            << "Src Entry Point:[ " << SrcEntryPoint << " ]"
            << "Hook Entry PointSrc:[ " << HookEntryPoint << " ]");
        return false;
    }

    ret = DetourSetExclusiveACL(new ULONG(), 1, detour_handle.get());
    if (ret != 0)
    {
        CB_ERROR("SGDetourImpl::installHook() call DetourSetExclusiveACL() failed, error code:[ " << ret << "] "
            << "Src Entry Point:[ " << SrcEntryPoint << " ]"
            << "Hook Entry PointSrc:[ " << HookEntryPoint << " ]");
        return false;
    }

    mSrcEntryPoint = SrcEntryPoint;
    mHookEntryPoint = HookEntryPoint;
    mHookHandle = detour_handle;

    return true;
}

void SGDetourImpl::postInstallHook(void* SrcEntryPoint, void* HookEntryPoint, bool hooked)
{
    CBT_INFO("SGDetour()", "postInstallHook() fun:" << SrcEntryPoint
            << ", result was hooked :" << std::boolalpha << hooked);
    CBT_INFO("SGDetour()", "postInstallHook() hook fun entry point:" << SrcEntryPoint
            << ", dump the proc self maps:\n"
            << fetchSelfMaps());
}

bool SGDetourImpl::uninstallHook()
{
    if (mHookHandle)
        DetourUninstallHook(mHookHandle.get());
    mHookHandle.reset();

    return true;
}

bool SGDetourImpl::isEnbale() const
{
    return mHookHandle.get() != NULL;
}

const void* SGDetourImpl::getSrcAddress() const
{
    return mSrcEntryPoint;
}

const void* SGDetourImpl::getHookAddress() const
{
    return mHookEntryPoint;
}

void SGDetourImpl::loggerInstall()
{
    if (mIsLoggerEnabled)
        return;

    LoggerParameter logger_SysLog_param;
    logger_SysLog_param.module_tag = "SysLog";
    logger_SysLog_param.file_path = "/tmp/lep/log/"\
                                    + std::to_string(getuid())\
                                    + "/"\
                                    + ProcessInfo::getProcessName() + "."
                                    + std::to_string(ProcessInfo::getProcessID()) + "."
                                    + "detour.log"
                                    ;
    logger_SysLog_param.file_size = "10240000";
    logger_SysLog_param.level = "DEBUG";
    LoggerSPtr logger_SysLog = std::make_shared<Logger>();
    logger_SysLog->init(logger_SysLog_param);

    LoggerManager::addLogger(logger_SysLog->getRawLogger());

    CBT_DEBUG("loggerInstall()", "init log module");

    mIsLoggerEnabled = true;
}

std::string SGDetourImpl::fetchSelfMaps()
{
    std::string maps_path="/proc/self/maps";
    std::ifstream ifs(maps_path, std::ios::binary);
    std::string buffer;
    if (!ifs.is_open())
    {
        CBT_WARN("SGDetourImpl()", "fetchSelfMaps() failed to open maps file:" << maps_path);
        return buffer;
    }

    buffer = std::string((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
    ifs.close();

    return buffer;
}


SGLD_END
