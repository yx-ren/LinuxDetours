#ifndef __SG_DETOUR_H__
#define __SG_DETOUR_H__

#include <memory>
#include <skyguard/LinuxDetours/common.h>

#if 0
struct ClientHandler;
/*
 * @function: connect DLP server, create a client handle running on block mode
 * @param[in] serverIP : SPE server ip address
 * @param[in] serverPort : SPE server port
 * @param[in] uuid : agent's uuid
 * @param[in] flags : flags in this client, may be the merge of PF_ASYNC, PF_MONITOR, PF_VERBOSE etc.
 * @return: Client pointer if success, null if failure
 */
SGPB_EXPORT struct ClientHandler* sgpb_init_client(
        const char* serverIP,
        int serverPort,
        const char* uuid,
        ProtocolFlags flags
        );
#endif


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

#endif
