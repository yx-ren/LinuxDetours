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

    bool installHook(void* SrcEntryPoint, void* HookEntryPoint);

    bool uninstallHook();

    bool isEnbale() const;

    const void* getSrcAddress() const;

    const void* getHookAddress() const;

private:
    std::shared_ptr<HOOK_TRACE_INFO> mHookHandle;
    void* mSrcEntryPoint;
    void* mHookEntryPoint;
};
typedef std::shared_ptr<SGDetourImpl> SGDetourImplSPtr;
SGLD_END

#endif
