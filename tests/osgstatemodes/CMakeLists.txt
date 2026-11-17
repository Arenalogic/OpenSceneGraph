SET(TARGET_SRC osgstatemodes.cpp )

SET(TARGET_NAME osgstatemodes)
SETUP_EXE(1)
SET_TARGET_PROPERTIES(${TARGET_TARGETNAME} PROPERTIES FOLDER "Tests")

# no window and no GL context: osg::State's mode table only
ADD_TEST(NAME osgstatemodes COMMAND ${TARGET_TARGETNAME})
