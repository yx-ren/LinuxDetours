PROJECT(detours_sdk CXX)
SET(target "detours_sdk")

FILE(GLOB SOURCES
  ../*.cpp
  *.cpp)

FILE(GLOB HEADERS
  ../*.h
  *.h)

IF (PLATFORM MATCHES "macos_x64")
    SET(TRAMPOLINE_ASM "trampoline_x86.cpp")
ELSEIF (PLATFORM MATCHES "linux_x64")
    SET(TRAMPOLINE_ASM "trampoline_x86.cpp")
ELSEIF (PLATFORM MATCHES "linux_mips64")
    SET(TRAMPOLINE_ASM "trampoline_mips.cpp")
ELSEIF (PLATFORM MATCHES "linux_arm")
    SET(TRAMPOLINE_ASM "trampoline_arm.cpp")
ELSEif (PLATFORM MATCHES "linux_arm64")
    SET(TRAMPOLINE_ASM "trampoline_arm.cpp")
ENDIF()

ADD_LIBRARY(${target} SHARED ${SOURCES} ${HEADERS})
ADD_LIBRARY(${target}_static STATIC ${SOURCES} ${HEADERS})

SET_TARGET_PROPERTIES(${target}_static PROPERTIES OUTPUT_NAME ${target})

SET(DETOUR_SDK_HEADRES
    SGDetour.h
    )

INSTALL(FILES ${DETOUR_SDK_HEADRES} DESTINATION include/skyguard/LinuxDetours/detours/sdk COMPONENT dev)
INSTALL(TARGETS ${target} DESTINATION lib COMPONENT dev)
INSTALL(TARGETS ${target}_static DESTINATION lib COMPONENT dev)

# copy 3rd(glog)
INSTALL(DIRECTORY /usr/local/include/gflags /usr/local/include/glog DESTINATION include USE_SOURCE_PERMISSIONS COMPONENT dev)
INSTALL(FILES /usr/local/lib/libglog.so.0.0.0 DESTINATION lib COMPONENT dev)
