#######################################################
# Arenalogic fork tests (BUILD_OSG_TESTS, off by default): small self-contained programs, each one ctest test.
# They exit 0 when every check holds; run them with ctest from the build directory.
#######################################################
SET(TARGET_DEFAULT_PREFIX "test_")
SET(TARGET_DEFAULT_LABEL_PREFIX "Tests")

SET(TARGET_COMMON_LIBRARIES
    OpenThreads
    osg
)

ADD_SUBDIRECTORY(osgstatemodes)
ADD_SUBDIRECTORY(osgcoreformats)
ADD_SUBDIRECTORY(osgcoreformatsgl)
ADD_SUBDIRECTORY(osgrenderstagetakebins)
