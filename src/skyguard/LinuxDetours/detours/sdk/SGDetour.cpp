#include <skyguard/LinuxDetours/detours/sdk/SGDetour.h>
#include <skyguard/LinuxDetours/detours/sdk/SGDetourImpl.h>

SGDetour::SGDetour()
{
    mImpl = std::make_shared<SGDetourImpl>();
}

void SGDetour::globalInstall()
{
    return SGDetourImpl::globalInstall();
}

void SGDetour::globalUnstall()
{
    return SGDetourImpl::globalUnstall();
}

bool SGDetour::installHook(void* SrcEntryPoint, void* HookEntryPoint)
{
    return mImpl->installHook(SrcEntryPoint, HookEntryPoint);
}

bool SGDetour::uninstallHook()
{
    return mImpl->uninstallHook();
}

bool SGDetour::isEnbale() const
{
    return mImpl->isEnbale();
}

const void* SGDetour::getSrcAddress() const
{
    return mImpl->getSrcAddress();
}

const void* SGDetour::getHookAddress() const
{
    return mImpl->getHookAddress();
}
