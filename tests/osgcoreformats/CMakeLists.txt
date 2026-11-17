SET(TARGET_SRC osgcoreformats.cpp )

SET(TARGET_NAME osgcoreformats)
SETUP_EXE(1)
SET_TARGET_PROPERTIES(${TARGET_TARGETNAME} PROPERTIES FOLDER "Tests")

# no window and no GL context: osg::Texture's legacy format tables only
ADD_TEST(NAME osgcoreformats COMMAND ${TARGET_TARGETNAME})
