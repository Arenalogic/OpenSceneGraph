SET(TARGET_SRC osgrenderstagebindings.cpp )
SET(TARGET_ADDED_LIBRARIES osgUtil osgViewer )

SET(TARGET_NAME osgrenderstagebindings)
SETUP_EXE(1)
SET_TARGET_PROPERTIES(${TARGET_TARGETNAME} PROPERTIES FOLDER "Tests")

# per-camera RenderStage generic block bindings on a shared GL 4.6 core context; exits 77 (skipped) without hardware GL
ADD_TEST(NAME osgrenderstagebindings COMMAND ${TARGET_TARGETNAME})
SET_TESTS_PROPERTIES(osgrenderstagebindings PROPERTIES SKIP_RETURN_CODE 77)
