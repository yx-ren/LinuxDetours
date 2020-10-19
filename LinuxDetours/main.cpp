#include <cstdio>
#define _X86_
#include "detours.h"
#include <glog/logging.h>
#include <iostream>

#define USE_CUSTOM_FUNCTION 0

unsigned int sleep_detour(unsigned int seconds)
{
    LOG(INFO) << "detours_test: Called sleep_detour";
    return sleep(seconds);
}
unsigned int test_detour_b(unsigned int seconds, unsigned int a, unsigned int b, unsigned int c, unsigned int d, unsigned int e)
{
    LOG(INFO) << "detours_test: Called test_detour_b";
    return seconds + 1;
}
unsigned int test_detour_a(unsigned int seconds, unsigned int a, unsigned int b, unsigned int c, unsigned int d, unsigned int e)
{
    LOG(INFO) << "detours_test: Detoured function 'test_detour_b' -> function 'test_detour_a' with params: "
              << a << ", " << b << ", " << c << ", " << d << ", " << e;
    return test_detour_b(seconds + 2, a, b, c, d, e);
}

VOID* test_runner(void*)
{
#if USE_CUSTOM_FUNCTION
    LOG(INFO) << "detours_test: Function 'test_detour_b' returned " << test_detour_b(1, 2, 3, 4, 5, 6);
#endif

    LOG(INFO) << "detours_test: Calling sleep for 1 second";
    sleep(1);
    LOG(INFO) << "detours_test: Calling sleep again for 2 seconds";
    sleep(2);
    
    LOG(INFO)  << "detours_test: Done sleeping\n";

    return NULL;
}

int test_glog(char * argv)
{
    google::InitGoogleLogging(argv);
    FLAGS_logtostderr = true;

    LOG(INFO) << "detours_test: Starting detours tests";
    return 1;
}


int main(int argc, char * argv[])
{
    test_glog(argv[0]);

    DetourBarrierProcessAttach();
    DetourCriticalInitialize();

    ULONG ret = 0;

    LONG sleep_detour_callback = 0;
    TRACED_HOOK_HANDLE sleep_detour_handle = new HOOK_TRACE_INFO();    
    DetourInstallHook((void*)sleep, (void*)sleep_detour, &sleep_detour_callback, sleep_detour_handle);
    ret = DetourSetExclusiveACL(new ULONG(), 1, (TRACED_HOOK_HANDLE)sleep_detour_handle);

#if USE_CUSTOM_FUNCTION
    LONG test_detour_callback = 0;
    TRACED_HOOK_HANDLE test_detour_handle = new HOOK_TRACE_INFO();
    DetourInstallHook((void*)test_detour_b, (void*)test_detour_a, &test_detour_callback, test_detour_handle);
    ret = DetourSetExclusiveACL(new ULONG(), 1, (TRACED_HOOK_HANDLE)test_detour_handle);
#endif

#if 0
    LOG(INFO) << "main: calling [sleep()] with 1 seconds before test thread";
    sleep(1);
#endif

    pthread_t t;
    pthread_create(&t, NULL, test_runner, NULL);
    pthread_join(t, NULL);

    DetourUninstallHook(sleep_detour_handle);
    delete sleep_detour_handle;

#if USE_CUSTOM_FUNCTION
    delete test_detour_handle;
    DetourUninstallHook(test_detour_handle);
#endif

    sleep(1);

    DetourCriticalFinalize();
    DetourBarrierProcessDetach();

    std::cout << "hello asm" << std::endl;

    return 0;
}
