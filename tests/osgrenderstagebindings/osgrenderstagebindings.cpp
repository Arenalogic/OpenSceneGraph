/* osgrenderstagebindings: per-camera generic block buffer bindings on osgUtil::RenderStage (Arenalogic fork test,
 * ctest; foundation F2.L1).
 *
 * Verifies on a real OpenGL 4.6 core context (exits 77 without hardware GL or on non-GLCORE builds):
 *  1. Two cameras sharing one GL context bind different UBO buffers (and ranges) to the same binding index (slot 0)
 *     via RenderStage::setBlockBinding: each stage's shader reads its own block colour into its RGBA32F FBO.
 *  2. Reverse draw order (B then A vs A then B) produces identical per-camera colours with zero binding leakage.
 *  3. Unbound sibling stage drawn right after a bound stage sees buffer 0 on slot 0 (no leak out of a stage).
 *  4. Nested stage drawn inside an outer stage overrides slot 0 for the inner draw and restores the outer stage's
 *     binding for subsequent draws in the outer stage.
 *  5. Repeated frames with changing ring-buffer offsets (glBindBufferRange) read each frame's exact slice.
 *  6. Context close and recreation reusing the same context ID binds and reads fresh buffers cleanly with zero GL errors.
 *  7. Existing stage-bin transfer (RenderStage::takeBinsFrom) preserves per-stage bindings on both source and target.
 *
 * No ACT or dtRender dependency. Exit code 0 when every check holds, 1 otherwise, 77 when skipped.
 */

#include <osg/Array>
#include <osg/FrameBufferObject>
#include <osg/FrameStamp>
#include <osg/Geode>
#include <osg/Geometry>
#include <osg/GLExtensions>
#include <osg/GraphicsContext>
#include <osg/Group>
#include <osg/Notify>
#include <osg/Program>
#include <osg/RenderInfo>
#include <osg/Shader>
#include <osg/State>
#include <osg/StateSet>
#include <osg/Viewport>
#include <osgUtil/RenderStage>
#include <osgUtil/SceneView>
#include <osgViewer/GraphicsWindow>

#include <cmath>
#include <cstring>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

USE_GRAPHICSWINDOW()

#ifndef GL_UNIFORM_BUFFER
    #define GL_UNIFORM_BUFFER 0x8A11
#endif
#ifndef GL_UNIFORM_BUFFER_BINDING
    #define GL_UNIFORM_BUFFER_BINDING 0x8A28
#endif
#ifndef GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT
    #define GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT 0x8A34
#endif
#ifndef GL_CONTEXT_PROFILE_MASK
    #define GL_CONTEXT_PROFILE_MASK 0x9126
#endif
#ifndef GL_CONTEXT_CORE_PROFILE_BIT
    #define GL_CONTEXT_CORE_PROFILE_BIT 0x00000001
#endif
#ifndef GL_COLOR_ATTACHMENT0
    #define GL_COLOR_ATTACHMENT0 0x8CE0
#endif

namespace
{
    const int kSkip = 77;
    const unsigned int kCreateForwardCompatibleBit = 0x00000002u;
    const GLuint kBlockSlot = 0u;

    unsigned int s_checks = 0;
    unsigned int s_failures = 0;

    void check(bool ok, const std::string& what)
    {
        ++s_checks;
        if (!ok)
        {
            ++s_failures;
            std::cout << "FAILED: " << what << std::endl;
        }
    }

    std::string drainGlErrors()
    {
        std::ostringstream out;
        for (GLenum e = glGetError(); e != GL_NO_ERROR; e = glGetError())
        {
            if (!out.str().empty()) out << " ";
            out << "0x" << std::hex << e;
        }
        return out.str();
    }

    typedef void (GL_APIENTRY * GetIntegeri_vProc)(GLenum, GLuint, GLint*);

    GLint queryBoundUbo(GLuint slot)
    {
        static GetIntegeri_vProc getIntegeri_v =
            reinterpret_cast<GetIntegeri_vProc>(osg::getGLExtensionFuncPtr("glGetIntegeri_v"));
        GLint bound = -1;
        if (getIntegeri_v) getIntegeri_v(GL_UNIFORM_BUFFER_BINDING, slot, &bound);
        return bound;
    }

    osg::ref_ptr<osg::GraphicsContext> createCoreContext(const char* name)
    {
        osg::ref_ptr<osg::GraphicsContext::Traits> traits = new osg::GraphicsContext::Traits;
        traits->x = 0;
        traits->y = 0;
        traits->width = 16;
        traits->height = 16;
        traits->windowDecoration = false;
        traits->doubleBuffer = false;
        traits->hidden = true;
        traits->windowName = name;
        traits->glContextVersion = "4.6";
        traits->glContextProfileMask = GL_CONTEXT_CORE_PROFILE_BIT;
        traits->glContextFlags = kCreateForwardCompatibleBit;
        return osg::GraphicsContext::createGraphicsContext(traits.get());
    }

    // Draws a fullscreen triangle reading vec4 color from UBO binding = 0 and records the GL-bound buffer name.
    struct BlockProbe
    {
        GLint boundUbo;
        osg::Vec4f pixel;
        int draws;
        BlockProbe() : boundUbo(-1), pixel(-1, -1, -1, -1), draws(0) {}
    };

    class BlockReadDrawCallback : public osg::Drawable::DrawCallback
    {
    public:
        BlockReadDrawCallback(BlockProbe* probe, GLuint fbo) : _probe(probe), _fbo(fbo) {}

        virtual void drawImplementation(osg::RenderInfo& renderInfo, const osg::Drawable* drawable) const
        {
            osg::State& state = *renderInfo.getState();
            const osg::GLExtensions* ext = state.get<osg::GLExtensions>();
            _probe->boundUbo = queryBoundUbo(kBlockSlot);
            ++_probe->draws;
            if (_fbo != 0 && ext && ext->glBindFramebuffer)
            {
                ext->glBindFramebuffer(GL_FRAMEBUFFER_EXT, _fbo);
                glViewport(0, 0, 4, 4);
                drawable->drawImplementation(renderInfo);
                glReadPixels(2, 2, 1, 1, GL_RGBA, GL_FLOAT, _probe->pixel.ptr());
                ext->glBindFramebuffer(GL_FRAMEBUFFER_EXT, 0);
            }
        }

    private:
        BlockProbe* _probe;
        GLuint _fbo;
    };

    // Triggers a nested RenderStage inside an outer stage's RenderBin::draw.
    class NestedStageTriggerCallback : public osg::Drawable::DrawCallback
    {
    public:
        explicit NestedStageTriggerCallback(osgUtil::RenderStage* nestedStage) : _nestedStage(nestedStage) {}

        virtual void drawImplementation(osg::RenderInfo& renderInfo, const osg::Drawable*) const
        {
            osgUtil::RenderLeaf* prev = 0;
            bool doCopy = false;
            _nestedStage->drawInner(renderInfo, prev, doCopy);
        }

    private:
        osg::ref_ptr<osgUtil::RenderStage> _nestedStage;
    };

    osg::ref_ptr<osg::Program> makeUboProgram()
    {
        osg::ref_ptr<osg::Program> program = new osg::Program;
        program->addBindAttribLocation("in_pos", 0);
        program->addShader(new osg::Shader(osg::Shader::VERTEX,
            "#version 460 core\n"
            "layout(location = 0) in vec3 in_pos;\n"
            "void main() { gl_Position = vec4(in_pos, 1.0); }\n"));
        program->addShader(new osg::Shader(osg::Shader::FRAGMENT,
            "#version 460 core\n"
            "layout(std140, binding = 0) uniform StageBlock { vec4 stageColor; };\n"
            "out vec4 out_col;\n"
            "void main() { out_col = stageColor; }\n"));
        return program;
    }

    osg::ref_ptr<osg::Geode> makeProbeGeode(osg::Program* program, BlockProbe* probe, GLuint fbo)
    {
        osg::ref_ptr<osg::Vec3Array> verts = new osg::Vec3Array;
        verts->push_back(osg::Vec3(-1.0f, -1.0f, 0.0f));
        verts->push_back(osg::Vec3( 3.0f, -1.0f, 0.0f));
        verts->push_back(osg::Vec3(-1.0f,  3.0f, 0.0f));

        osg::ref_ptr<osg::Geometry> geom = new osg::Geometry;
        geom->setUseDisplayList(false);
        geom->setUseVertexBufferObjects(true);
        geom->setCullingActive(false);
        geom->setVertexAttribArray(0, verts.get(), osg::Array::BIND_PER_VERTEX);
        geom->addPrimitiveSet(new osg::DrawArrays(GL_TRIANGLES, 0, 3));
        geom->setDrawCallback(new BlockReadDrawCallback(probe, fbo));

        osg::StateSet* ss = geom->getOrCreateStateSet();
        ss->setAttributeAndModes(program, osg::StateAttribute::ON);
        ss->setMode(GL_DEPTH_TEST, osg::StateAttribute::OFF);
        ss->setMode(GL_BLEND, osg::StateAttribute::OFF);
        ss->setMode(GL_CULL_FACE, osg::StateAttribute::OFF);

        osg::ref_ptr<osg::Geode> geode = new osg::Geode;
        geode->addDrawable(geom.get());
        return geode;
    }

    osg::ref_ptr<osg::Geode> makeTriggerGeode(osgUtil::RenderStage* nestedStage)
    {
        osg::ref_ptr<osg::Vec3Array> verts = new osg::Vec3Array;
        verts->push_back(osg::Vec3(0, 0, 0));
        osg::ref_ptr<osg::Geometry> geom = new osg::Geometry;
        geom->setUseDisplayList(false);
        geom->setCullingActive(false);
        geom->setVertexArray(verts.get());
        geom->setDrawCallback(new NestedStageTriggerCallback(nestedStage));
        osg::ref_ptr<osg::Geode> geode = new osg::Geode;
        geode->addDrawable(geom.get());
        return geode;
    }

    osg::ref_ptr<osgUtil::SceneView> makeSceneView(osg::State* state, osg::Node* scene)
    {
        osg::ref_ptr<osgUtil::SceneView> sv = new osgUtil::SceneView;
        sv->setDefaults(0);
        sv->setFrameStamp(new osg::FrameStamp);
        sv->setViewport(0, 0, 4, 4);
        sv->setCullingMode(osg::CullSettings::NO_CULLING);
        sv->setComputeNearFarMode(osg::CullSettings::DO_NOT_COMPUTE_NEAR_FAR);
        sv->setProjectionMatrixAsOrtho(-1, 1, -1, 1, -1, 1);
        sv->setViewMatrix(osg::Matrixd::identity());
        sv->setSceneData(scene);
        sv->setState(state);
        sv->getRenderStage()->setClearMask(0);
        return sv;
    }

    void checkColour(const osg::Vec4f& actual, const osg::Vec4f& expected, const std::string& what)
    {
        bool ok = true;
        for (int i = 0; i < 4; ++i)
            if (std::fabs(actual[i] - expected[i]) > 1e-4f) ok = false;
        std::ostringstream msg;
        msg << what << ": got (" << actual[0] << ", " << actual[1] << ", " << actual[2] << ", " << actual[3]
            << "), expected (" << expected[0] << ", " << expected[1] << ", " << expected[2] << ", " << expected[3] << ")";
        check(ok, msg.str());
    }

    void runContextSuite(osg::GraphicsContext& gc, const std::string& passLabel)
    {
        osg::State& state = *gc.getState();
        state.setUseVertexAttributeAliasing(false);
        state.setUseModelViewAndProjectionUniforms(false);
        const osg::GLExtensions* ext = state.get<osg::GLExtensions>();
        check(ext && ext->glGenBuffers && ext->glBindBufferRange && ext->glGenFramebuffers,
              passLabel + ": required GL 4.6 buffer/FBO entry points loaded");
        if (!ext || !ext->glGenBuffers || !ext->glBindBufferRange || !ext->glGenFramebuffers) return;

        // Create RGBA32F FBO target.
        GLuint fboTex = 0, fbo = 0;
        glGenTextures(1, &fboTex);
        glBindTexture(GL_TEXTURE_2D, fboTex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F_ARB, 4, 4, 0, GL_RGBA, GL_FLOAT, 0);
        glBindTexture(GL_TEXTURE_2D, 0);
        ext->glGenFramebuffers(1, &fbo);
        ext->glBindFramebuffer(GL_FRAMEBUFFER_EXT, fbo);
        ext->glFramebufferTexture2D(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fboTex, 0);
        check(ext->glCheckFramebufferStatus(GL_FRAMEBUFFER_EXT) == GL_FRAMEBUFFER_COMPLETE_EXT,
              passLabel + ": RGBA32F FBO complete");
        ext->glBindFramebuffer(GL_FRAMEBUFFER_EXT, 0);

        // Query UBO offset alignment so ring slices are spec-aligned.
        GLint align = 256;
        glGetIntegerv(GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT, &align);
        if (align < 16) align = 16;
        const GLsizeiptr stride = static_cast<GLsizeiptr>(align);

        // Create UBO A (single block), UBO B (3 ring slices), UBO Inner (single block).
        const osg::Vec4f colA(0.90f, 0.10f, 0.20f, 1.0f);
        const osg::Vec4f colB[3] = {
            osg::Vec4f(0.10f, 0.85f, 0.25f, 1.0f),
            osg::Vec4f(0.15f, 0.35f, 0.95f, 1.0f),
            osg::Vec4f(0.95f, 0.80f, 0.10f, 1.0f)
        };
        const osg::Vec4f colInner(0.60f, 0.20f, 0.80f, 1.0f);

        GLuint uboA = 0, uboB = 0, uboInner = 0;
        ext->glGenBuffers(1, &uboA);
        ext->glBindBuffer(GL_UNIFORM_BUFFER, uboA);
        ext->glBufferData(GL_UNIFORM_BUFFER, sizeof(osg::Vec4f), colA.ptr(), GL_STATIC_DRAW);

        std::vector<unsigned char> ringData(static_cast<size_t>(stride * 3), 0);
        for (int i = 0; i < 3; ++i)
            std::memcpy(&ringData[static_cast<size_t>(stride * i)], colB[i].ptr(), sizeof(osg::Vec4f));
        ext->glGenBuffers(1, &uboB);
        ext->glBindBuffer(GL_UNIFORM_BUFFER, uboB);
        ext->glBufferData(GL_UNIFORM_BUFFER, stride * 3, &ringData[0], GL_STATIC_DRAW);

        ext->glGenBuffers(1, &uboInner);
        ext->glBindBuffer(GL_UNIFORM_BUFFER, uboInner);
        ext->glBufferData(GL_UNIFORM_BUFFER, sizeof(osg::Vec4f), colInner.ptr(), GL_STATIC_DRAW);
        ext->glBindBuffer(GL_UNIFORM_BUFFER, 0);

        osg::ref_ptr<osg::Program> program = makeUboProgram();

        // 1 & 2. Two cameras sharing one context: A->B order and B->A reverse order.
        BlockProbe probeA, probeB, probeUnbound;
        osg::ref_ptr<osgUtil::SceneView> svA = makeSceneView(&state, makeProbeGeode(program.get(), &probeA, fbo).get());
        osg::ref_ptr<osgUtil::SceneView> svB = makeSceneView(&state, makeProbeGeode(program.get(), &probeB, fbo).get());
        osg::ref_ptr<osgUtil::SceneView> svUnbound = makeSceneView(&state, makeProbeGeode(program.get(), &probeUnbound, 0).get());

        svA->getRenderStage()->setBlockBinding(GL_UNIFORM_BUFFER, kBlockSlot, uboA, 0, sizeof(osg::Vec4f));
        svB->getRenderStage()->setBlockBinding(GL_UNIFORM_BUFFER, kBlockSlot, uboB, 0, sizeof(osg::Vec4f));

        drainGlErrors();
        // Order A -> B -> Unbound
        svA->cull(); svA->draw();
        svB->cull(); svB->draw();
        svUnbound->cull(); svUnbound->draw();

        check(probeA.boundUbo == static_cast<GLint>(uboA), passLabel + " A->B: camera A saw uboA");
        checkColour(probeA.pixel, colA, passLabel + " A->B: camera A pixel");
        check(probeB.boundUbo == static_cast<GLint>(uboB), passLabel + " A->B: camera B saw uboB");
        checkColour(probeB.pixel, colB[0], passLabel + " A->B: camera B pixel");
        check(probeUnbound.boundUbo == 0, passLabel + " A->B: unbound stage after B sees buffer 0 (no leak)");

        // Reverse order B -> A -> Unbound
        probeA = BlockProbe(); probeB = BlockProbe(); probeUnbound = BlockProbe();
        svB->cull(); svB->draw();
        svA->cull(); svA->draw();
        svUnbound->cull(); svUnbound->draw();

        check(probeB.boundUbo == static_cast<GLint>(uboB), passLabel + " B->A: camera B saw uboB");
        checkColour(probeB.pixel, colB[0], passLabel + " B->A: camera B pixel");
        check(probeA.boundUbo == static_cast<GLint>(uboA), passLabel + " B->A: camera A saw uboA");
        checkColour(probeA.pixel, colA, passLabel + " B->A: camera A pixel");
        check(probeUnbound.boundUbo == 0, passLabel + " B->A: unbound stage after A sees buffer 0 (no leak)");

        // 4. Nested stage: Outer binds uboA, bin 0 draws outerBefore, bin 5 invokes nested stage (binds uboInner),
        //    bin 10 draws outerAfter -> must see uboA restored!
        BlockProbe probeOuterBefore, probeInner, probeOuterAfter;
        osg::ref_ptr<osgUtil::SceneView> svInner = makeSceneView(&state, makeProbeGeode(program.get(), &probeInner, fbo).get());
        svInner->getRenderStage()->setBlockBinding(GL_UNIFORM_BUFFER, kBlockSlot, uboInner, 0, sizeof(osg::Vec4f));
        svInner->cull();

        osg::ref_ptr<osg::Group> nestedRoot = new osg::Group;
        osg::ref_ptr<osg::Geode> gBefore = makeProbeGeode(program.get(), &probeOuterBefore, fbo);
        gBefore->getOrCreateStateSet()->setRenderBinDetails(0, "RenderBin");
        osg::ref_ptr<osg::Geode> gTrigger = makeTriggerGeode(svInner->getRenderStage());
        gTrigger->getOrCreateStateSet()->setRenderBinDetails(5, "RenderBin");
        osg::ref_ptr<osg::Geode> gAfter = makeProbeGeode(program.get(), &probeOuterAfter, fbo);
        gAfter->getOrCreateStateSet()->setRenderBinDetails(10, "RenderBin");
        nestedRoot->addChild(gBefore.get());
        nestedRoot->addChild(gTrigger.get());
        nestedRoot->addChild(gAfter.get());

        osg::ref_ptr<osgUtil::SceneView> svOuter = makeSceneView(&state, nestedRoot.get());
        svOuter->getRenderStage()->setBlockBinding(GL_UNIFORM_BUFFER, kBlockSlot, uboA, 0, sizeof(osg::Vec4f));
        svOuter->cull();
        svOuter->draw();

        check(probeOuterBefore.boundUbo == static_cast<GLint>(uboA), passLabel + " nested: outer before saw uboA");
        checkColour(probeOuterBefore.pixel, colA, passLabel + " nested: outer before pixel");
        check(probeInner.boundUbo == static_cast<GLint>(uboInner), passLabel + " nested: inner saw uboInner");
        checkColour(probeInner.pixel, colInner, passLabel + " nested: inner pixel");
        check(probeOuterAfter.boundUbo == static_cast<GLint>(uboA), passLabel + " nested: outer after restored uboA");
        checkColour(probeOuterAfter.pixel, colA, passLabel + " nested: outer after pixel");
        check(queryBoundUbo(kBlockSlot) == 0, passLabel + " nested: slot 0 unbound after outer exits");

        // 5. Repeated frames with changing ring-buffer offsets on camera B.
        for (int frame = 0; frame < 3; ++frame)
        {
            probeB = BlockProbe();
            svB->getRenderStage()->setBlockBinding(GL_UNIFORM_BUFFER, kBlockSlot, uboB, stride * frame, sizeof(osg::Vec4f));
            svB->cull();
            svB->draw();
            std::ostringstream flabel;
            flabel << passLabel << " ring frame " << frame;
            checkColour(probeB.pixel, colB[frame], flabel.str());
        }

        // 7. Existing stage-bin transfer (takeBinsFrom) with distinct per-stage bindings on source and target.
        BlockProbe probeSplitSrc, probeSplitDst;
        osg::ref_ptr<osg::Group> splitRoot = new osg::Group;
        osg::ref_ptr<osg::Geode> gLow = makeProbeGeode(program.get(), &probeSplitSrc, fbo);
        gLow->getOrCreateStateSet()->setRenderBinDetails(0, "RenderBin");
        osg::ref_ptr<osg::Geode> gHigh = makeProbeGeode(program.get(), &probeSplitDst, fbo);
        gHigh->getOrCreateStateSet()->setRenderBinDetails(20, "RenderBin");
        splitRoot->addChild(gLow.get());
        splitRoot->addChild(gHigh.get());

        osg::ref_ptr<osgUtil::SceneView> svSplit = makeSceneView(&state, splitRoot.get());
        osg::ref_ptr<osgUtil::RenderStage> splitTarget = new osgUtil::RenderStage;
        splitTarget->setClearMask(0);
        splitTarget->setBlockBinding(GL_UNIFORM_BUFFER, kBlockSlot, uboInner, 0, sizeof(osg::Vec4f));
        svSplit->getRenderStage()->setBlockBinding(GL_UNIFORM_BUFFER, kBlockSlot, uboA, 0, sizeof(osg::Vec4f));

        svSplit->cull();
        splitTarget->reset();
        splitTarget->setViewport(svSplit->getRenderStage()->getViewport());
        const unsigned int moved = splitTarget->takeBinsFrom(*svSplit->getRenderStage(), 10);
        svSplit->getRenderStage()->addPostRenderStage(splitTarget.get(), 1);
        svSplit->draw();

        check(moved == 1u, passLabel + " takeBinsFrom: moved bin 20");
        checkColour(probeSplitSrc.pixel, colA, passLabel + " takeBinsFrom: source stage kept uboA");
        checkColour(probeSplitDst.pixel, colInner, passLabel + " takeBinsFrom: target stage used uboInner");

        const std::string errors = drainGlErrors();
        check(errors.empty(), passLabel + ": zero GL errors (" + errors + ")");

        // Cleanup GL resources for this context.
        svA->releaseAllGLObjects();
        svB->releaseAllGLObjects();
        svUnbound->releaseAllGLObjects();
        svInner->releaseAllGLObjects();
        svOuter->releaseAllGLObjects();
        svSplit->releaseAllGLObjects();
        splitTarget->releaseGLObjects(&state);
        ext->glDeleteBuffers(1, &uboA);
        ext->glDeleteBuffers(1, &uboB);
        ext->glDeleteBuffers(1, &uboInner);
        ext->glDeleteFramebuffers(1, &fbo);
        glDeleteTextures(1, &fboTex);
    }
}

int main(int, char**)
{
    osg::setNotifyLevel(osg::WARN);

#if !defined(OSG_GL3_AVAILABLE)
    std::cout << "osgrenderstagebindings: skipped, non-GLCORE build" << std::endl;
    return kSkip;
#else
    // Pass 1: initial context.
    osg::ref_ptr<osg::GraphicsContext> gc1 = createCoreContext("osgrenderstagebindings_1");
    if (!gc1.valid() || !gc1->realize() || !gc1->makeCurrent())
    {
        std::cout << "osgrenderstagebindings: skipped, no OpenGL 4.6 core context available" << std::endl;
        return kSkip;
    }
    const unsigned int ctxId1 = gc1->getState()->getContextID();
    const char* renderer = reinterpret_cast<const char*>(glGetString(GL_RENDERER));
    const char* version = reinterpret_cast<const char*>(glGetString(GL_VERSION));
    std::cout << "osgrenderstagebindings: " << (renderer ? renderer : "?") << ", GL " << (version ? version : "?") << std::endl;

    runContextSuite(*gc1, "ctx1");
    gc1->releaseContext();
    gc1->close();
    gc1 = 0;

    // Pass 2: recreated context reusing the freed context ID.
    osg::ref_ptr<osg::GraphicsContext> gc2 = createCoreContext("osgrenderstagebindings_2");
    check(gc2.valid() && gc2->realize() && gc2->makeCurrent(), "recreated context realizes and becomes current");
    if (gc2.valid() && gc2->isRealized())
    {
        check(gc2->getState()->getContextID() == ctxId1, "recreated context reuses context ID");
        runContextSuite(*gc2, "ctx2");
        gc2->releaseContext();
        gc2->close();
    }

    std::cout << "osgrenderstagebindings: " << s_checks << " checks, " << s_failures << " failed" << std::endl;
    return s_failures == 0 ? 0 : 1;
#endif
}
