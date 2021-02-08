cmake_minimum_required(VERSION 2.8.11)
project(EndpointInjectionFramework CXX)

set(EIF_MAJOR_VERSION 3)
set(EIF_MINOR_VERSION 9)
set(EIF_PATCH_VERSION 0)
set(EIF_BUILD_NO 001)


message("CMAKE_SYSTEM_VERSION: ${CMAKE_SYSTEM_VERSION}")
message("CMAKE_SYSTEM_PROCESSOR: ${CMAKE_SYSTEM_PROCESSOR}")
message("CMAKE_SYSTEM_NAME: ${CMAKE_SYSTEM_NAME}")
message("CMAKE_SYSTEM: ${CMAKE_SYSTEM}")
message("CMAKE_LIBRARY_ARCHITECTURE: ${CMAKE_LIBRARY_ARCHITECTURE}")
message("EIF_VERSION: ${EIF_MAJOR_VERSION}.${EIF_MINOR_VERSION}.${EIF_PATCH_VERSION}-${EIF_BUILD_NO}")

if (NOT CPU_COUNT)
    set(CPU_COUNT 4)
endif()

IF("${CMAKE_SYSTEM}" MATCHES "Linux" AND "${CMAKE_SYSTEM_PROCESSOR}" MATCHES "mips64")
	SET(LINUX 1)
	SET(LINUX_QILIN 1)
ENDIF()

if (NOT EXTERNAL_PREFIX)
    if(${PLATFORM_TYPE} STREQUAL "Win32")
        set(EXTERNAL_PREFIX "${PROJECT_SOURCE_DIR}/../external32")
    else()
        set(EXTERNAL_PREFIX "${PROJECT_SOURCE_DIR}/../external")
    endif()
endif()

if (NOT INTERNAL_PREFIX)
    if(${PLATFORM_TYPE} STREQUAL "Win32")
        set(INTERNAL_PREFIX "${PROJECT_SOURCE_DIR}/../internal32")
    else()
        set(INTERNAL_PREFIX "${PROJECT_SOURCE_DIR}/../internal")
    endif()
endif()

macro(PATH_UNIX2DOS DosPath UnixPath)
    string(REPLACE "/" "\\" ${UnixPath} "${DosPath}")
endmacro()

if (APPLE)
    set(CMAKE_MACOSX_RPATH 1)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mmacosx-version-min=10.9")
    #disable some warnings
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-deprecated-register -Wno-unused-local-typedef -Wno-macro-redefined")
endif()

message("EXTERNAL_PREFIX: ${EXTERNAL_PREFIX}.")
set(CMAKE_PREFIX_PATH ${EXTERNAL_PREFIX} ${CMAKE_PREFIX_PATH})
set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake-modules")



set(CMAKE_VERBOSE_MAKEFILE ON)
set(BUILD_SHARED_LIBS ON)
set(BUILD_STATIC_LIBS ON)

set(Boost_USE_STATIC_LIBS OFF)
set(Boost_USE_MULTITHREAD ON)
set(Boost_USE_STATIC_RUNTIME OFF)
#find_package(Boost 1.58.0 COMPONENTS thread filesystem system program_options chrono iostreams locale regex date_time )
#find_package(CPPUNIT)
find_package(Log4CXX)
#find_package(OEM)

#FIND_PACKAGE(PkgConfig REQUIRED)
#PKG_CHECK_MODULES(GTK3 REQUIRED gtk+-3.0)

if (NOT WIN32)
    set(CMAKE_CXX_FLAGS "-Wall -pthread -std=c++0x ${CMAKE_CXX_FLAGS}")
    set(CMAKE_CXX_FLAGS_DEBUG "-D_DEBUG -g")
    set(CMAKE_CXX_FLAGS_RELEASE "-O2")

    add_definitions(-DOBJECT_RESOLVER_MODULE)
    set(OBJECT_RESOLVER_MODULE 1)
else()
    add_definitions(-DOBJECT_RESOLVER_MODULE)
    set(OBJECT_RESOLVER_MODULE 1)

    add_definitions(-DWIN32_LEAN_AND_MEAN)
    set(CMAKE_CXX_FLAGS_DEBUG "/D_DEBUG /MDd /Zi /Ob0 /Od /RTC1")
	 set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MP")
	 set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /Zi /MD")
    set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /INCREMENTAL:NO /DEBUG /OPT:REF /OPT:ICF")
    set(CMAKE_SHARED_LINKER_FLAGS_RELEASE "${CMAKE_SHARED_LINKER_FLAGS_RELEASE} /INCREMENTAL:NO /DEBUG /OPT:REF /OPT:ICF")
endif()

if (WIN32)
    if (NOT TOOLSET)
        set(TOOLSET "v140_xp")
    endif()
    set(EXTRA_P "/p:MultiProcessorCompilation=true /p:PlatformToolset=${TOOLSET}")
endif()


include_directories(${PROJECT_SOURCE_DIR}/include)
include_directories(${EXTERNAL_PREFIX}/include)
include_directories(${INTERNAL_PREFIX}/include)
include_directories(${INTERNAL_PREFIX}/oem)
include_directories(${PROJECT_SOURCE_DIR}/src)

#if (CPPUNIT_FOUND)
#   enable_testing()
#else()
#   message("cppunit is not found, the tests will not be run.")
#endif()

#if (Log4CXX_FOUND)
#    include_directories(${Log4CXX_INCLUDE_DIRS})
#else()
#    if (NOT WIN32)
#        message(FATAL_ERROR "Log4CXX is not found on this machine.")
#    else()
#        message("log4cxx is not found.")
#    endif()
#endif()

#if (NOT OEM_FOUND)
#   message(FATAL_ERROR, "OEM is not found.")
#endif()

set(CMAKE_INSTALL_PREFIX ${CMAKE_CURRENT_BINARY_DIR}/export)
set(CMAKE_INSTALL_CONFIG_NAME ${CMAKE_BUILD_TYPE})
set(CPACK_PACKAGE_VENDOR ${OEM_PRODUCT_PUBLISHER})
set(CPACK_PACKAGE_VERSION ${EIF_MAJOR_VERSION}.${EIF_MINOR_VERSION}.${EIF_PATCH_VERSION}-${EIF_BUILD_NO})
set(CPACK_PACKAGE_VERSION_MAJOR ${EIF_MAJOR_VERSION})
set(CPACK_PACKAGE_VERSION_MINOR ${EIF_MINOR_VERSION})
set(CPACK_PACKAGE_VERSION_PATCH ${EIF_PATCH_VERSION})

if(WIN32 OR APPLE OR UNIX)
    set(CPACK_GENERATOR TGZ)
endif()

if (WIN32)
    if ("${PLATFORM_TYPE}" STREQUAL x64)
        set(CPACK_PACKAGE_ARCHITECTURE win32-x86_64)
    else()
        set(CPACK_PACKAGE_ARCHITECTURE win32-x86)
    endif()
elseif (APPLE)
    set(CPACK_PACKAGE_ARCHITECTURE OSX-x86_64)
elseif (LINUX_QILIN)
    set(CPACK_PACKAGE_ARCHITECTURE neokylin7-ls64)
else()
    set(CPACK_PACKAGE_ARCHITECTURE amd64)
  endif()

set(CPACK_PACKAGE_FILE_NAME ${CMAKE_PROJECT_NAME}_${CPACK_PACKAGE_VERSION}_${CPACK_PACKAGE_ARCHITECTURE})

set(CPACK_ARCHIVE_COMPONENT_INSTALL ON)
set(CPACK_PROJECT_CONFIG_FILE ${CMAKE_CURRENT_SOURCE_DIR}/cmake-modules/package.cmake)
include(CPack)


set(EXTERNAL_INCLUDE_PATH ${EXTERNAL_PREFIX}/include)
set(EXTERNAL_LIB_PATH ${EXTERNAL_PREFIX}/lib)
set(EXTERNAL_BIN_PATH ${EXTERNAL_PREFIX}/bin)

set(INTERNAL_LIB_PATH ${INTERNAL_PREFIX}/lib)


link_directories(${EXTERNAL_LIB_PATH})
link_directories(${INTERNAL_LIB_PATH})
if (${PLATFORM_TYPE} STREQUAL "Win32")
   link_directories(${INTERNAL_LIB_PATH}/x86)
else()
   link_directories(${INTERNAL_LIB_PATH}/x64)
endif()

if (Boost_FOUND)
  message("Use the system boost libaries.")
  set(UseSystemBoost TRUE)
else()
   message("Use the boost libaries build from 3rdparty.")
   if (WIN32)
       set(Boost_INCLUDE_DIRS ${EXTERNAL_INCLUDE_PATH}/boost-1_58)
       if(CMAKE_BUILD_TYPE MATCHES Debug)
           message("debug build........")
           if (MSVC14)
	       set(Boost_LIBRARIES ${EXTERNAL_LIB_PATH}/libboost_thread-vc140-mt-gd-1_58.lib
		       ${EXTERNAL_LIB_PATH}/libboost_filesystem-vc140-mt-gd-1_58.lib
		       ${EXTERNAL_LIB_PATH}/libboost_system-vc140-mt-gd-1_58.lib
		       ${EXTERNAL_LIB_PATH}/libboost_program_options-vc140-mt-gd-1_58.lib
		       ${EXTERNAL_LIB_PATH}/libboost_chrono-vc140-mt-gd-1_58.lib
		       ${EXTERNAL_LIB_PATH}/libboost_date_time-vc140-mt-gd-1_58.lib
		       ${EXTERNAL_LIB_PATH}/libboost_regex-vc140-mt-gd-1_58.lib
                   )
           else()
		   set(Boost_LIBRARIES ${EXTERNAL_LIB_PATH}/libboost_thread-vc120-mt-gd-1_58.lib
			   ${EXTERNAL_LIB_PATH}/libboost_filesystem-vc120-mt-gd-1_58.lib
			   ${EXTERNAL_LIB_PATH}/libboost_system-vc120-mt-gd-1_58.lib
			   ${EXTERNAL_LIB_PATH}/libboost_program_options-vc120-mt-gd-1_58.lib
			   ${EXTERNAL_LIB_PATH}/libboost_chrono-vc120-mt-gd-1_58.lib
			   ${EXTERNAL_LIB_PATH}/libboost_date_time-vc120-mt-gd-1_58.lib
			   ${EXTERNAL_LIB_PATH}/libboost_regex-vc120-mt-gd-1_58.lib
                   )
           endif()
       else()
           message("release build........")
           if (MSVC14)
               set(Boost_LIBRARIES ${EXTERNAL_LIB_PATH}/libboost_thread-vc140-mt-1_58.lib
                   ${EXTERNAL_LIB_PATH}/libboost_filesystem-vc140-mt-1_58.lib
                   ${EXTERNAL_LIB_PATH}/libboost_system-vc140-mt-1_58.lib
                   ${EXTERNAL_LIB_PATH}/libboost_program_options-vc140-mt-1_58.lib
                   ${EXTERNAL_LIB_PATH}/libboost_chrono-vc140-mt-1_58.lib
                   ${EXTERNAL_LIB_PATH}/libboost_date_time-vc140-mt-1_58.lib
                   ${EXTERNAL_LIB_PATH}/libboost_regex-vc140-mt-1_58.lib
                   )
           else()
               set(Boost_LIBRARIES ${EXTERNAL_LIB_PATH}/libboost_thread-vc120-mt-1_58.lib
                   ${EXTERNAL_LIB_PATH}/libboost_filesystem-vc120-mt-1_58.lib
                   ${EXTERNAL_LIB_PATH}/libboost_system-vc120-mt-1_58.lib
                   ${EXTERNAL_LIB_PATH}/libboost_program_options-vc120-mt-1_58.lib
                   ${EXTERNAL_LIB_PATH}/libboost_chrono-vc120-mt-1_58.lib
                   ${EXTERNAL_LIB_PATH}/libboost_date_time-vc120-mt-1_58.lib
                   ${EXTERNAL_LIB_PATH}/libboost_regex-vc120-mt-1_58.lib
                   )
           endif()
       endif()
   elseif (APPLE)
       set(Boost_INCLUDE_DIRS ${THIRDPARTY_INCLUDE_PATH})
       set(Boost_LIBRARIES ${EXTERNAL_LIB_PATH}/libboost_thread.dylib
           ${EXTERNAL_LIB_PATH}/libboost_filesystem.dylib
           ${EXTERNAL_LIB_PATH}/libboost_system.dylib
           ${EXTERNAL_LIB_PATH}/libboost_program_options.dylib
           ${EXTERNAL_LIB_PATH}/libboost_chrono.dylib
           ${EXTERNAL_LIB_PATH}/libboost_date_time.dylib
           ${EXTERNAL_LIB_PATH}/libboost_iostreams.dylib
           #${EXTERNAL_LIB_PATH}/libboost_locale.dylib
           )
   else()
	   set(Boost_INCLUDE_DIRS ${EXTERNAL_INCLUDE_PATH})
       set(Boost_LIBRARIES ${EXTERNAL_LIB_PATH}/libboost_thread.so
           ${EXTERNAL_LIB_PATH}/libboost_filesystem.so
           ${EXTERNAL_LIB_PATH}/libboost_system.so
           ${EXTERNAL_LIB_PATH}/libboost_program_options.so
           ${EXTERNAL_LIB_PATH}/libboost_chrono.so
           ${EXTERNAL_LIB_PATH}/libboost_iostreams.so
           ${EXTERNAL_LIB_PATH}/libboost_locale.so
           ${EXTERNAL_LIB_PATH}/libboost_regex.so
           )
   endif()
endif()

include_directories(${Boost_INCLUDE_DIRS})
message("boost libraries:${Boost_LIBRARIES}")

include_directories(${GTK3_INCLUDE_DIRS})
message("gtk3 include: ${GTK3_INCLUDE_DIRS}")
message("gtk3 libraries: ${GTK3_LIBRARIES}")

    set(SGEndpoint_BaseLib_LIBRARIES ${INTERNAL_LIB_PATH}/libEndpointBaseLib.a)
    set(SGEndpoint_IPCFramework_LIBRARIES ${INTERNAL_LIB_PATH}/libEndpointIPCFramework.a)
    set(SGBase_LIBRARIES ${INTERNAL_LIB_PATH}/libsgbase.so)

    set(Thrift_LIBRARIES ${EXTERNAL_LIB_PATH}/libthrift.so)
    set(JsonCpp_LIBRARIES ${EXTERNAL_LIB_PATH}/libjsoncpp.so)
    set(OpenSSL_LIBRARIES ${EXTERNAL_LIB_PATH}/libssl.a
        ${EXTERNAL_LIB_PATH}/libcrypto.a
    )

    set(Libevent_LIBRARIES 
        ${EXTERNAL_LIB_PATH}/libevent_core.so
        ${EXTERNAL_LIB_PATH}/libevent_openssl.so
        ${EXTERNAL_LIB_PATH}/libevent_extra.so
    )

include_directories(${PROJECT_SOURCE_DIR}/src)

add_subdirectory(src)
