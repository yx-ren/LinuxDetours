file(GLOB WFE_INTERNAL_SOURCES
    "*.cpp"
    )

file(GLOB WFE_INTERNAL_HEADERS
    "*.h"
    )

#add_executable(wfe_internal_util ${WFE_INTERNAL_HEADERS} ${WFE_INTERNAL_SOURCES})
#add_dependencies(wfe_internal_util WFEModuleControlThrift)
#set(DIAGNOSE_ON 0) # diagnose is not supported yet
#if (DIAGNOSE_ON)
#    add_definitions(-DDIAGNOSE_ON)
#    add_dependencies(wfe_internal_util thrift-diag)
#endif()
#
#target_include_directories(wfe_internal_util PUBLIC ${Boost_INCLUDE_DIRS})
#target_include_directories(wfe_internal_util PUBLIC ${Thrift_INCLUDE_DIRS})
#
#target_link_libraries(wfe_internal_util WFEModuleControlThrift)
#if (DIAGNOSE_ON)
#    target_link_libraries(wfe_internal_util thrift-diag)
#endif()
#target_link_libraries(wfe_internal_util ${Boost_LIBRARIES})
#target_link_libraries(wfe_internal_util ${Thrift_LIBRARIES})
#target_link_libraries(wfe_internal_util ${JSONCPP_LIBRARIES})
#target_link_libraries(wfe_internal_util ${Sgbase_LIBRARIES})
#
#install(TARGETS wfe_internal_util DESTINATION bin COMPONENT dev)
