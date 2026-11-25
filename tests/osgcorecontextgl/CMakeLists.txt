SET(TARGET_SRC osgcorecontextgl.cpp )
SET(TARGET_ADDED_LIBRARIES osgViewer )

SET(TARGET_NAME osgcorecontextgl)
SETUP_EXE(1)
SET_TARGET_PROPERTIES(${TARGET_TARGETNAME} PROPERTIES FOLDER "Tests")

# a hidden window's OpenGL 4.6 core forward-compatible context on a GLCORE build; exits 77 (skipped) without either
ADD_TEST(NAME osgcorecontextgl COMMAND ${TARGET_TARGETNAME})
SET_TESTS_PROPERTIES(osgcorecontextgl PROPERTIES SKIP_RETURN_CODE 77)
