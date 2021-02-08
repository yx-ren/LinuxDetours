#ifndef __SG_DETOUR_IMPL_H__
#define __SG_DETOUR_IMPL_H__

#include <memory>
#include <skyguard/LinuxDetours/common.h>
#include <skyguard/LinuxDetours/detours/detours.h>

SGLD_BEGIN
class SGDetourImpl
{
public:
    SGDetourImpl();

    ~SGDetourImpl();

    static void globalInstall();

    static void globalUnstall();

    void preInstallHook(void* SrcEntryPoint, void* HookEntryPoint);
    bool installHook(void* SrcEntryPoint, void* HookEntryPoint);
    void postInstallHook(void* SrcEntryPoint, void* HookEntryPoint, bool hooked);

    bool uninstallHook();

    bool isEnbale() const;

    const void* getSrcAddress() const;

    const void* getHookAddress() const;

private:
    static void loggerInstall();

    std::string fetchSelfMaps();

private:
    std::shared_ptr<HOOK_TRACE_INFO> mHookHandle;
    void* mSrcEntryPoint;
    void* mHookEntryPoint;
    static bool mIsLoggerEnabled;
};
typedef std::shared_ptr<SGDetourImpl> SGDetourImplSPtr;
SGLD_END

#endif
