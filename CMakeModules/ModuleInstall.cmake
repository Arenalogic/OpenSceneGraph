# INSTALL and SOURCE_GROUP commands for OSG/OT/Producer Modules

# Required Vars:
# ${LIB_NAME}
# ${TARGET_H}

SET(INSTALL_INCDIR include)
SET(INSTALL_BINDIR bin)
IF(WIN32)
    SET(INSTALL_LIBDIR bin)
    SET(INSTALL_ARCHIVEDIR lib)
ELSE()
    SET(INSTALL_LIBDIR lib${LIB_POSTFIX})
    SET(INSTALL_ARCHIVEDIR lib${LIB_POSTFIX})
ENDIF()

SET(HEADERS_GROUP "Header Files")

SOURCE_GROUP(
    ${HEADERS_GROUP}
    FILES ${TARGET_H}
)

IF(MSVC AND OSG_MSVC_VERSIONED_DLL)
    HANDLE_MSVC_DLL()
ENDIF()

INSTALL(
    TARGETS ${LIB_NAME}
    EXPORT osg-targets
    RUNTIME DESTINATION ${INSTALL_BINDIR} COMPONENT libopenscenegraph
    LIBRARY DESTINATION ${INSTALL_LIBDIR} COMPONENT libopenscenegraph
    ARCHIVE DESTINATION ${INSTALL_ARCHIVEDIR} COMPONENT libopenscenegraph-dev    
)

IF(MSVC AND DYNAMIC_OPENSCENEGRAPH)
    GET_TARGET_PROPERTY(PREFIX ${LIB_NAME} PREFIX)
    IF("${PREFIX}" STREQUAL PREFIX-NOTFOUND) # Fix for PREFIX-NOTFOUND left in file names
	SET(PREFIX "")
    ENDIF()
    IF ( ${CMAKE_GENERATOR} STREQUAL "Ninja" )
	INSTALL(FILES ${CMAKE_CURRENT_BINARY_DIR}/${PREFIX}${LIB_NAME}${CMAKE_RELEASE_POSTFIX}.pdb DESTINATION ${INSTALL_BINDIR} COMPONENT libopenscenegraph CONFIGURATIONS Release)
	INSTALL(FILES ${CMAKE_CURRENT_BINARY_DIR}/${PREFIX}${LIB_NAME}${CMAKE_RELWITHDEBINFO_POSTFIX}.pdb DESTINATION ${INSTALL_BINDIR} COMPONENT libopenscenegraph CONFIGURATIONS RelWithDebInfo)
	INSTALL(FILES ${CMAKE_CURRENT_BINARY_DIR}/${PREFIX}${LIB_NAME}${CMAKE_DEBUG_POSTFIX}.pdb DESTINATION ${INSTALL_BINDIR} COMPONENT libopenscenegraph CONFIGURATIONS Debug)
    ELSE ( ${CMAKE_GENERATOR} STREQUAL "Ninja" )
	INSTALL(FILES ${OUTPUT_BINDIR}/${PREFIX}${LIB_NAME}${CMAKE_RELWITHDEBINFO_POSTFIX}.pdb DESTINATION ${INSTALL_BINDIR} COMPONENT libopenscenegraph CONFIGURATIONS RelWithDebInfo)
	INSTALL(FILES ${OUTPUT_BINDIR}/${PREFIX}${LIB_NAME}${CMAKE_DEBUG_POSTFIX}.pdb DESTINATION ${INSTALL_BINDIR} COMPONENT libopenscenegraph CONFIGURATIONS Debug)
    ENDIF ( ${CMAKE_GENERATOR} STREQUAL "Ninja" )
ENDIF(MSVC AND DYNAMIC_OPENSCENEGRAPH)

IF(NOT OSG_COMPILE_FRAMEWORKS)
    INSTALL (
        FILES        ${TARGET_H}
        DESTINATION ${INSTALL_INCDIR}/${LIB_NAME}
        COMPONENT libopenscenegraph-dev
    )
ELSE()
    SET(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
    SET(CMAKE_INSTALL_RPATH "${OSG_COMPILE_FRAMEWORKS_INSTALL_NAME_DIR}")
    
    SET_TARGET_PROPERTIES(${LIB_NAME} PROPERTIES
         FRAMEWORK TRUE
         FRAMEWORK_VERSION ${OPENSCENEGRAPH_SOVERSION}
         PUBLIC_HEADER  "${TARGET_H}"
         INSTALL_NAME_DIR "${OSG_COMPILE_FRAMEWORKS_INSTALL_NAME_DIR}"
    )
    # MESSAGE("${OSG_COMPILE_FRAMEWORKS_INSTALL_NAME_DIR}")
ENDIF()

