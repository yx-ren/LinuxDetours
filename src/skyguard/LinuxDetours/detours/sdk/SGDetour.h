#ifndef __SG_DETOUR_H__
#define __SG_DETOUR_H__

#include <memory>
#include <skyguard/LinuxDetours/common.h>

SGLD_BEGIN

class SGDetourImpl;
class SGDetour
{
public:
    SGDetour();

    static void globalInstall();

    static void globalUnstall();

    bool installHook(void* SrcEntryPoint, void* HookEntryPoint);

    bool uninstallHook();

    bool isEnbale() const;

    const void* getSrcAddress() const;

    const void* getHookAddress() const;

private:
    std::shared_ptr<SGDetourImpl> mImpl;
};
typedef std::shared_ptr<SGDetour> SGDetourSPtr;

SGLD_END

#endif
