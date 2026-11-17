SET(TARGET_SRC osgcoreformatsgl.cpp )
SET(TARGET_ADDED_LIBRARIES osgViewer )

SET(TARGET_NAME osgcoreformatsgl)
SETUP_EXE(1)
SET_TARGET_PROPERTIES(${TARGET_TARGETNAME} PROPERTIES FOLDER "Tests")

# a hidden window's GL context (3.3 or later) on a GLCORE build; exits 77 (skipped) without either
ADD_TEST(NAME osgcoreformatsgl COMMAND ${TARGET_TARGETNAME})
SET_TESTS_PROPERTIES(osgcoreformatsgl PROPERTIES SKIP_RETURN_CODE 77)
