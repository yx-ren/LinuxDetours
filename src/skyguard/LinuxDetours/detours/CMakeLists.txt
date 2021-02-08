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

list(REMOVE_ITEM SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/trampoline_x86.cpp")
list(REMOVE_ITEM SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/trampoline_mips.cpp")
list(REMOVE_ITEM SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/trampoline_arm.cpp")

ADD_LIBRARY(${target} STATIC ${SOURCES} ${HEADERS})

add_custom_command(TARGET ${target}
    DEPENDS ${TRAMPOLINE_ASM}
    COMMAND g++ -c -Wall -Wno-null-conversion -std=c++11 -g -O0 -fno-omit-frame-pointer -fPIC -shared -D_MIPS64_ -D_UNIX64
        ${CMAKE_CURRENT_SOURCE_DIR}/${TRAMPOLINE_ASM} -o ${CMAKE_CURRENT_SOURCE_DIR}/${TRAMPOLINE_ASM}.o
        && ${CMAKE_CURRENT_SOURCE_DIR}/dump_obj.sh ${CMAKE_CURRENT_SOURCE_DIR}/${TRAMPOLINE_ASM}.o
        && ${CMAKE_CURRENT_SOURCE_DIR}/gen_ins.sh ${CMAKE_CURRENT_SOURCE_DIR}/${TRAMPOLINE_ASM}.o.obj.dump
        && ${CMAKE_CURRENT_SOURCE_DIR}/replace_hard_code.sh
           ${CMAKE_CURRENT_SOURCE_DIR}/${TRAMPOLINE_ASM}.o.obj.dump.disassemble
           ${CMAKE_CURRENT_SOURCE_DIR}/detours.cpp
    COMMENT "Now Generating mips asm files..."
    COMMENT "build dir:[ ${CMAKE_CURRENT_BINARY_DIR} ]"
    )

ADD_SUBDIRECTORY(sdk)
