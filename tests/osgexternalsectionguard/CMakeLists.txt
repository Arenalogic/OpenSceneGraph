SET(TARGET_SRC osgexternalsectionguard.cpp )
SET(TARGET_ADDED_LIBRARIES osgViewer )

SET(TARGET_NAME osgexternalsectionguard)
SETUP_EXE(1)
SET_TARGET_PROPERTIES(${TARGET_TARGETNAME} PROPERTIES FOLDER "Tests")

# osg::State::ExternalSectionGuard reconciliation on a real GL 4.6 core context; exits 77 (skipped) without hardware GL
ADD_TEST(NAME osgexternalsectionguard COMMAND ${TARGET_TARGETNAME})
SET_TESTS_PROPERTIES(osgexternalsectionguard PROPERTIES SKIP_RETURN_CODE 77)
