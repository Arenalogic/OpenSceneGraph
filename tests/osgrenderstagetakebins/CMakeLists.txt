SET(TARGET_SRC osgrenderstagetakebins.cpp )
SET(TARGET_ADDED_LIBRARIES osgUtil osgViewer )

SET(TARGET_NAME osgrenderstagetakebins)
SETUP_EXE(1)
SET_TARGET_PROPERTIES(${TARGET_TARGETNAME} PROPERTIES FOLDER "Tests")

# the split is checked without a context; the draws need a hidden window's GL context and exit 77 (skipped) without one
ADD_TEST(NAME osgrenderstagetakebins COMMAND ${TARGET_TARGETNAME})
SET_TESTS_PROPERTIES(osgrenderstagetakebins PROPERTIES SKIP_RETURN_CODE 77)
