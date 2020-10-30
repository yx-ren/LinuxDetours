PROJECT(hook)
SET(target "hook")

FILE(GLOB SOURCES
  *.cpp)

FILE(GLOB HEADERS
  *.h)

add_library(${target} SHARED ${SOURCES} ${HEADERS})
ADD_DEPENDENCIES(${target} detours)
TARGET_LINK_LIBRARIES(${target} detours)


add_custom_command(TARGET ${target}
    POST_BUILD
    COMMAND cp -f ${CMAKE_CURRENT_SOURCE_DIR}/hook.sh ${CMAKE_CURRENT_BINARY_DIR}
    #COMMAND copy ${CMAKE_CURRENT_BINARY_DIR_DOS}\\..\\..\\${CMAKE_BUILD_TYPE}\\* ${UGMLTEST_BIN_DIR_DOS}\\ /y
    #COMMAND copy ${THIRDPARTY_BIN_PATH_DOS}\\*.dll ${UGMLTEST_BIN_DIR_DOS}\\ /y
    #COMMAND copy ${THIRDPARTY_BIN_PATH_DOS}\\..\\lib\\*.dll ${UGMLTEST_BIN_DIR_DOS}\\ /y
    #COMMAND copy ${INTERNAL_LIB_PATH_DOS}\\sgbase.dll ${UGMLTEST_BIN_DIR_DOS}\\ /y
    )
