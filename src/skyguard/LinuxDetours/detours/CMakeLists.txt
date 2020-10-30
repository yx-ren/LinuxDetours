PROJECT(detours)
SET(target "detours")

FILE(GLOB SOURCES
  *.cpp)

FILE(GLOB HEADERS
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

ADD_LIBRARY(${target} SHARED ${SOURCES} ${HEADERS} ${TRAMPOLINE_ASM})
TARGET_LINK_LIBRARIES(${target} -lglog)
#ADD_DEPENDENCIES(${target} detours)
#TARGET_LINK_LIBRARIES(${target} detours)
