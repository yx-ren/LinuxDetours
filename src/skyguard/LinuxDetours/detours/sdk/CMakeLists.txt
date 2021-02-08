PROJECT(detours_sdk CXX)
SET(target "detours_sdk")

FILE(GLOB SOURCES
  ../*.cpp
  *.cpp)

FILE(GLOB HEADERS
  ../*.h
  *.h)

FILE(GLOB DETOUR_BASE_HEADERS
  ../../base/*.h
    )

FILE(GLOB DETOUR_BASE_SOURCES
  ../../base/*.cpp
    )

FILE(GLOB LOGGER_HEADERS
  ../../logger/*.h
    )

FILE(GLOB LOGGER_SOURCES
  ../../logger/*.cpp
    )

list(REMOVE_ITEM SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/../trampoline_x86.cpp")
list(REMOVE_ITEM SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/../trampoline_mips.cpp")
list(REMOVE_ITEM SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/../trampoline_arm.cpp")

ADD_LIBRARY(${target} STATIC
    ${SOURCES} ${HEADERS}
    ${DETOUR_BASE_HEADERS} ${DETOUR_BASE_SOURCES}
    ${LOGGER_HEADERS} ${LOGGER_SOURCES}
    )

SET(DETOUR_SDK_HEADRES
    SGDetour.h
    )

INSTALL(FILES ${DETOUR_SDK_HEADRES} DESTINATION include/skyguard/LinuxDetours/detours/sdk COMPONENT dev)
INSTALL(TARGETS ${target} DESTINATION lib COMPONENT dev)
