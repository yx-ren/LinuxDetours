#include <glog/logging.h>
#include <skyguard/LinuxDetours/detours/sdk/SGDetourImpl.h>

SGLD_BEGIN
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
}

void SGDetourImpl::globalUnstall()
{
    DetourCriticalFinalize();
    DetourBarrierProcessDetach();
}

bool SGDetourImpl::installHook(void* SrcEntryPoint, void* HookEntryPoint)
{
    std::shared_ptr<HOOK_TRACE_INFO> detour_handle = std::make_shared<HOOK_TRACE_INFO>();
    if (!detour_handle)
    {
        LOG(INFO) << "SGDetourImpl::installHook() alloc memory failed for struct:[HOOK_TRACE_INFO]";
        return false;
    }

    int ret = 0;
    uint64_t detour_callback = 0;
    ret = DetourInstallHook(SrcEntryPoint, HookEntryPoint, &detour_callback, detour_handle.get());
    if (ret != NO_ERROR)
    {
        LOG(INFO) << "SGDetourImpl::installHook() call DetourInstallHook() failed, error code:[ " << ret << "] "
            << "Src Entry Point:[ " << SrcEntryPoint << " ]"
            << "Hook Entry PointSrc:[ " << HookEntryPoint << " ]";
        return false;
    }

    ret = DetourSetExclusiveACL(new ULONG(), 1, detour_handle.get());
    if (ret != 0)
    {
        LOG(INFO) << "SGDetourImpl::installHook() call DetourSetExclusiveACL() failed, error code:[ " << ret << "] "
            << "Src Entry Point:[ " << SrcEntryPoint << " ]"
            << "Hook Entry PointSrc:[ " << HookEntryPoint << " ]";
        return false;
    }

    mHookHandle = detour_handle;

    return true;
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
SGLD_END
