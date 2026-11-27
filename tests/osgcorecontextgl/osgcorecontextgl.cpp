/* osgcorecontextgl: OpenGL 4.6 core forward-compatible context invariants on a GLCORE build (Arenalogic fork test,
 * ctest; foundation F1). Needs a hardware GL 4.6 core context and a build without fixed function (OSG_GL3_AVAILABLE:
 * GLCORE); without either it exits 77, which ctest reports as skipped (SKIP_RETURN_CODE).
 *
 * Checks on a real 4.6 core forward-compatible context:
 *  1. Context realization: Traits asking for 4.6 core forward-compatible realize a context with GL_MAJOR_VERSION >= 4,
 *     GL_MINOR_VERSION >= 6, GL_CONTEXT_CORE_PROFILE_BIT, GL_CONTEXT_FLAG_FORWARD_COMPATIBLE_BIT, and
 *     State::isForwardCompatible() == true.
 *  2. Mixed StateSet application: applying valid core modes (GL_BLEND, GL_DEPTH_TEST, GL_CULL_FACE, GL_SCISSOR_TEST),
 *     refused fixed-function modes (GL_LIGHTING, GL_FOG, GL_ALPHA_TEST, GL_TEXTURE_2D on unit 0), and core halves of
 *     LogicOp, Point, PolygonMode, LineWidth (1.0f, forward-compatible), and Hint produces zero GL errors, applies the
 *     core state in GL, and records the fixed-function refusals.
 *  3. Core VAO + VBO Geometry draw into a GL_RGBA32F FBO using a #version 460 core shader program: zero GL errors and
 *     exact readback colour.
 *  4. Per-context object release and context recreation on the same context ID: closing the context releases
 *     GLExtensions and GL objects cleanly; a fresh 4.6 core context reusing the context ID draws cleanly with zero GL
 *     errors and exact readback colour.
 *  5. Second hidden context creation preserves the first context as current (Win32 sample-context preservation): while
 *     context A is current, realizing a second hidden context B leaves context A usable with zero GL errors.
 *  6. Program::apply() deduplication: redundant applies of the same linked PerContextProgram skip glUseProgram, while
 *     haveAppliedAttribute(PROGRAM), dirtyAllAttributes(), ExternalSectionGuard, program/define switches, relinks, and
 *     releaseGLObjects() invalidate or re-bind cleanly with zero GL errors.
 *
 * Exit code 0 when every check holds, 1 otherwise (each failed check is printed), 77 when skipped.
 */

#include <osg/Array>
#include <osg/FrameBufferObject>
#include <osg/Geometry>
#include <osg/GLExtensions>
#include <osg/GraphicsContext>
#include <osg/Hint>
#include <osg/LineWidth>
#include <osg/LogicOp>
#include <osg/Notify>
#include <osg/Point>
#include <osg/PolygonMode>
#include <osg/Program>
#include <osg/RenderInfo>
#include <osg/Shader>
#include <osg/State>
#include <osg/StateSet>
#include <osgViewer/GraphicsWindow>

#include <cmath>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

USE_GRAPHICSWINDOW()

#ifndef GL_MAJOR_VERSION
    #define GL_MAJOR_VERSION 0x821B
#endif
#ifndef GL_MINOR_VERSION
    #define GL_MINOR_VERSION 0x821C
#endif
#ifndef GL_CONTEXT_FLAGS
    #define GL_CONTEXT_FLAGS 0x821E
#endif
#ifndef GL_CONTEXT_FLAG_FORWARD_COMPATIBLE_BIT
    #define GL_CONTEXT_FLAG_FORWARD_COMPATIBLE_BIT 0x00000001
#endif
// Context-creation bit (WGL_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB == GLX_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB == 0x0002;
// distinct from GL_CONTEXT_FLAG_FORWARD_COMPATIBLE_BIT 0x0001 returned by glGetIntegerv(GL_CONTEXT_FLAGS)).
static const unsigned int kCreateForwardCompatibleBit = 0x00000002u;
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

    std::string hex(GLint value)
    {
        std::ostringstream out;
        out << "0x" << std::hex << value;
        return out.str();
    }

    std::string drainGlErrors()
    {
        std::string errors;
        for (GLenum e = glGetError(); e != GL_NO_ERROR; e = glGetError())
            errors += (errors.empty() ? "" : " ") + hex(static_cast<GLint>(e));
        return errors;
    }

    osg::ref_ptr<osg::GraphicsContext> createCore46Context(const char* name)
    {
        for (int attempt = 0; attempt < 2; ++attempt)
        {
            osg::ref_ptr<osg::GraphicsContext::Traits> traits = new osg::GraphicsContext::Traits;
            traits->x = 0;
            traits->y = 0;
            traits->width = 16;
            traits->height = 16;
            traits->windowDecoration = false;
            traits->doubleBuffer = false;
            traits->pbuffer = (attempt == 0);
            traits->hidden = (attempt != 0);
            traits->windowName = name;
            traits->glContextVersion = "4.6";
            traits->glContextProfileMask = GL_CONTEXT_CORE_PROFILE_BIT;
            traits->glContextFlags = kCreateForwardCompatibleBit;
            osg::ref_ptr<osg::GraphicsContext> gc = osg::GraphicsContext::createGraphicsContext(traits.get());
            if (gc.valid() && gc->realize()) return gc;
        }
        return 0;
    }

    void checkContextProperties(osg::State& state, const std::string& label)
    {
        drainGlErrors();
        GLint major = 0, minor = 0, profile = 0, flags = 0;
        glGetIntegerv(GL_MAJOR_VERSION, &major);
        glGetIntegerv(GL_MINOR_VERSION, &minor);
        glGetIntegerv(GL_CONTEXT_PROFILE_MASK, &profile);
        glGetIntegerv(GL_CONTEXT_FLAGS, &flags);
        const std::string errors = drainGlErrors();
        check(errors.empty(), label + ": querying GL version/profile/flags raises no GL error (" + errors + ")");
        check(major > 4 || (major == 4 && minor >= 6), label + ": realized GL version is at least 4.6");
        check((profile & GL_CONTEXT_CORE_PROFILE_BIT) != 0, label + ": realized context has GL_CONTEXT_CORE_PROFILE_BIT");
        check((flags & GL_CONTEXT_FLAG_FORWARD_COMPATIBLE_BIT) != 0, label + ": realized context has GL_CONTEXT_FLAG_FORWARD_COMPATIBLE_BIT");
        check(state.isForwardCompatible(), label + ": osg::State::isForwardCompatible() is true");
    }

    void checkMixedStateSet(osg::State& state)
    {
        osg::State::resetFixedFunctionRefusals();
        osg::ref_ptr<osg::StateSet> ss = new osg::StateSet;

        // Valid 4.6 core modes.
        ss->setMode(GL_BLEND, osg::StateAttribute::ON);
        ss->setMode(GL_DEPTH_TEST, osg::StateAttribute::ON);
        ss->setMode(GL_CULL_FACE, osg::StateAttribute::ON);
        ss->setMode(GL_SCISSOR_TEST, osg::StateAttribute::OFF);

        // Fixed-function modes that GLCORE must refuse without touching GL.
        ss->setMode(0x0B50, osg::StateAttribute::ON); // GL_LIGHTING
        ss->setMode(0x0B60, osg::StateAttribute::ON); // GL_FOG
        ss->setMode(0x0BC0, osg::StateAttribute::ON); // GL_ALPHA_TEST
        ss->setTextureMode(0, 0x0DE1, osg::StateAttribute::ON); // GL_TEXTURE_2D

        // Core halves of attributes that used to issue legacy calls.
        ss->setAttributeAndModes(new osg::LogicOp(osg::LogicOp::COPY), osg::StateAttribute::ON);
        osg::ref_ptr<osg::Point> point = new osg::Point(2.0f);
        ss->setAttribute(point.get(), osg::StateAttribute::ON);
        ss->setAttribute(new osg::PolygonMode(osg::PolygonMode::FRONT_AND_BACK, osg::PolygonMode::FILL), osg::StateAttribute::ON);
        // LineWidth > 1.0f is invalid on a forward-compatible core context unless OSG clamps/guards it via isForwardCompatible().
        ss->setAttribute(new osg::LineWidth(3.0f), osg::StateAttribute::ON);
        ss->setAttribute(new osg::Hint(GL_FRAGMENT_SHADER_DERIVATIVE_HINT, GL_NICEST), osg::StateAttribute::ON);

        drainGlErrors();
        state.apply(ss.get());
        const std::string errors = drainGlErrors();
        check(errors.empty(), "mixed core + fixed-function StateSet applies with zero GL errors (" + errors + ")");
        check(glIsEnabled(GL_BLEND) == GL_TRUE, "GL_BLEND enabled in GL after apply");
        check(glIsEnabled(GL_DEPTH_TEST) == GL_TRUE, "GL_DEPTH_TEST enabled in GL after apply");
        check(glIsEnabled(GL_CULL_FACE) == GL_TRUE, "GL_CULL_FACE enabled in GL after apply");
        check(glIsEnabled(GL_SCISSOR_TEST) == GL_FALSE, "GL_SCISSOR_TEST disabled in GL after apply");
        check(osg::State::getFixedFunctionRefusals().size() >= 4, "fixed-function modes recorded refusals on live core context");

        // Reset back to empty default state without GL errors.
        state.apply(new osg::StateSet);
        const std::string resetErrors = drainGlErrors();
        check(resetErrors.empty(), "resetting StateSet after mixed apply raises zero GL errors (" + resetErrors + ")");
        osg::State::resetFixedFunctionRefusals();
    }

    osg::Vec4f drawGeometryToRgba32f(osg::State& state, const osg::Vec4f& colour, const std::string& label)
    {
        const osg::GLExtensions* ext = state.get<osg::GLExtensions>();
        check(ext != 0 && ext->glGenFramebuffers != 0, label + ": GLExtensions loaded");
        if (!ext || !ext->glGenFramebuffers) return osg::Vec4f(-1, -1, -1, -1);

        GLuint targetTex = 0, fbo = 0;
        glGenTextures(1, &targetTex);
        glBindTexture(GL_TEXTURE_2D, targetTex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F_ARB, 4, 4, 0, GL_RGBA, GL_FLOAT, 0);
        glBindTexture(GL_TEXTURE_2D, 0);

        ext->glGenFramebuffers(1, &fbo);
        ext->glBindFramebuffer(GL_FRAMEBUFFER_EXT, fbo);
        ext->glFramebufferTexture2D(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, targetTex, 0);
        check(ext->glCheckFramebufferStatus(GL_FRAMEBUFFER_EXT) == GL_FRAMEBUFFER_COMPLETE_EXT, label + ": RGBA32F FBO complete");

        osg::ref_ptr<osg::Program> program = new osg::Program;
        program->addBindAttribLocation("in_pos", 0);
        program->addBindAttribLocation("in_col", 3);
        program->addShader(new osg::Shader(osg::Shader::VERTEX,
            "#version 460 core\n"
            "layout(location = 0) in vec3 in_pos;\n"
            "layout(location = 3) in vec4 in_col;\n"
            "out vec4 v_col;\n"
            "void main() { v_col = in_col; gl_Position = vec4(in_pos, 1.0); }\n"));
        program->addShader(new osg::Shader(osg::Shader::FRAGMENT,
            "#version 460 core\n"
            "in vec4 v_col;\n"
            "out vec4 out_col;\n"
            "void main() { out_col = v_col; }\n"));

        osg::ref_ptr<osg::Vec3Array> verts = new osg::Vec3Array;
        verts->push_back(osg::Vec3(-1.0f, -1.0f, 0.0f));
        verts->push_back(osg::Vec3( 3.0f, -1.0f, 0.0f));
        verts->push_back(osg::Vec3(-1.0f,  3.0f, 0.0f));

        osg::ref_ptr<osg::Vec4Array> cols = new osg::Vec4Array;
        cols->push_back(colour);
        cols->push_back(colour);
        cols->push_back(colour);

        osg::ref_ptr<osg::Geometry> geom = new osg::Geometry;
        geom->setUseDisplayList(false);
        geom->setUseVertexBufferObjects(true);
        geom->setVertexAttribArray(0, verts.get(), osg::Array::BIND_PER_VERTEX);
        geom->setVertexAttribArray(3, cols.get(), osg::Array::BIND_PER_VERTEX);
        geom->addPrimitiveSet(new osg::DrawArrays(GL_TRIANGLES, 0, 3));

        osg::ref_ptr<osg::StateSet> ss = new osg::StateSet;
        ss->setAttributeAndModes(program.get(), osg::StateAttribute::ON);
        ss->setMode(GL_DEPTH_TEST, osg::StateAttribute::OFF);
        ss->setMode(GL_BLEND, osg::StateAttribute::OFF);
        ss->setMode(GL_CULL_FACE, osg::StateAttribute::OFF);

        drainGlErrors();
        glViewport(0, 0, 4, 4);
        glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        state.setUseVertexAttributeAliasing(false);
        state.setUseModelViewAndProjectionUniforms(false);
        state.apply(ss.get());

        osg::RenderInfo renderInfo(&state, 0);
        geom->draw(renderInfo);

        osg::Vec4f pixel(-1.0f, -1.0f, -1.0f, -1.0f);
        glReadPixels(2, 2, 1, 1, GL_RGBA, GL_FLOAT, pixel.ptr());

        const std::string drawErrors = drainGlErrors();
        check(drawErrors.empty(), label + ": core VAO/VBO Geometry draw + readback raises zero GL errors (" + drawErrors + ")");

        geom->releaseGLObjects(&state);
        ss->releaseGLObjects(&state);
        state.apply(new osg::StateSet);
        ext->glBindFramebuffer(GL_FRAMEBUFFER_EXT, 0);
        ext->glDeleteFramebuffers(1, &fbo);
        glDeleteTextures(1, &targetTex);
        const std::string cleanupErrors = drainGlErrors();
        check(cleanupErrors.empty(), label + ": cleanup raises zero GL errors (" + cleanupErrors + ")");
        return pixel;
    }

    void checkPixelMatches(const osg::Vec4f& actual, const osg::Vec4f& expected, const std::string& label)
    {
        bool match = true;
        for (int i = 0; i < 4; ++i)
        {
            if (std::fabs(actual[i] - expected[i]) > 1e-4f) match = false;
        }
        std::ostringstream msg;
        msg << label << ": readback (" << actual[0] << ", " << actual[1] << ", " << actual[2] << ", " << actual[3]
            << ") matches expected (" << expected[0] << ", " << expected[1] << ", " << expected[2] << ", " << expected[3] << ")";
        check(match, msg.str());
    }

    GLint currentGlProgram()
    {
        GLint prog = 0;
        glGetIntegerv(GL_CURRENT_PROGRAM, &prog);
        return prog;
    }

    void checkProgramApplyDedup(osg::State& state)
    {
        const osg::GLExtensions* ext = state.get<osg::GLExtensions>();
        check(ext != 0 && ext->glUseProgram != 0, "program dedup: GLExtensions loaded");
        if (!ext || !ext->glUseProgram) return;

        osg::ref_ptr<osg::Program> progA = new osg::Program;
        progA->addShader(new osg::Shader(osg::Shader::VERTEX,
            "#version 460 core\n"
            "void main() { gl_Position = vec4(0.0, 0.0, 0.0, 1.0); }\n"));
        progA->addShader(new osg::Shader(osg::Shader::FRAGMENT,
            "#version 460 core\n"
            "#pragma import_defines(ALT_COLOR)\n"
            "out vec4 out_col;\n"
            "void main() {\n"
            "#ifdef ALT_COLOR\n"
            "    out_col = vec4(1.0, 0.0, 0.0, 1.0);\n"
            "#else\n"
            "    out_col = vec4(0.0, 1.0, 0.0, 1.0);\n"
            "#endif\n"
            "}\n"));

        osg::ref_ptr<osg::Program> progB = new osg::Program;
        progB->addShader(new osg::Shader(osg::Shader::VERTEX,
            "#version 460 core\n"
            "void main() { gl_Position = vec4(0.0, 0.0, 0.0, 1.0); }\n"));
        progB->addShader(new osg::Shader(osg::Shader::FRAGMENT,
            "#version 460 core\n"
            "out vec4 out_col;\n"
            "void main() { out_col = vec4(0.0, 0.0, 1.0, 1.0); }\n"));

        drainGlErrors();

        // 1. Initial apply links and binds progA.
        osg::ref_ptr<osg::StateSet> ssA = new osg::StateSet;
        ssA->setAttributeAndModes(progA.get(), osg::StateAttribute::ON);
        state.apply(ssA.get());
        const osg::Program::PerContextProgram* pcpA = state.getLastAppliedProgramObject();
        check(pcpA != 0 && pcpA->getHandle() != 0, "program dedup: initial apply produces valid PCP");
        if (!pcpA) return;
        check(currentGlProgram() == static_cast<GLint>(pcpA->getHandle()), "program dedup: initial apply binds GL program");

        // 2. Redundant apply of the same PCP skips glUseProgram (verified by perturbing GL without notifying osg::State).
        ext->glUseProgram(0);
        progA->apply(state);
        check(currentGlProgram() == 0, "program dedup: redundant Program::apply skips glUseProgram for same PCP");
        check(state.getLastAppliedProgramObject() == pcpA, "program dedup: cache stays on pcpA");

        // Unrelated define change via StateSet also re-invokes Program::apply with same PCP and skips glUseProgram.
        osg::ref_ptr<osg::StateSet> ssUnrelated = new osg::StateSet;
        ssUnrelated->setDefine("UNRELATED_DEFINE", osg::StateAttribute::ON);
        state.pushStateSet(ssA.get());
        state.pushStateSet(ssUnrelated.get());
        state.apply();
        check(currentGlProgram() == 0, "program dedup: unrelated define change skips redundant glUseProgram");
        state.popStateSet();
        state.popStateSet();

        // 3. haveAppliedAttribute(Type) clears cache and forces State::apply(ssA) to re-bind.
        state.haveAppliedAttribute(osg::StateAttribute::PROGRAM);
        check(state.getLastAppliedProgramObject() == 0, "program dedup: haveAppliedAttribute(PROGRAM) clears cache");
        state.apply(ssA.get());
        check(state.getLastAppliedProgramObject() == pcpA && currentGlProgram() == static_cast<GLint>(pcpA->getHandle()),
              "program dedup: State::apply after haveAppliedAttribute(Type) re-binds");

        // 4. haveAppliedAttribute(const StateAttribute*) clears both attribute cache and PCP so State::apply re-binds.
        ext->glUseProgram(0);
        state.haveAppliedAttribute(progA.get());
        check(state.getLastAppliedProgramObject() == 0, "program dedup: haveAppliedAttribute(const StateAttribute*) clears cache");
        state.apply(ssA.get());
        check(state.getLastAppliedProgramObject() == pcpA && currentGlProgram() == static_cast<GLint>(pcpA->getHandle()),
              "program dedup: State::apply after haveAppliedAttribute(ptr) re-binds");

        // 5. dirtyAllAttributes() clears cache and forces State::apply(ssA) to re-bind.
        ext->glUseProgram(0);
        state.dirtyAllAttributes();
        check(state.getLastAppliedProgramObject() == 0, "program dedup: dirtyAllAttributes() clears cache");
        state.apply(ssA.get());
        check(state.getLastAppliedProgramObject() == pcpA && currentGlProgram() == static_cast<GLint>(pcpA->getHandle()),
              "program dedup: State::apply after dirtyAllAttributes() re-binds");

        // 6. ExternalSectionGuard invalidates cache and forces State::apply(ssA) to re-bind.
        {
            osg::State::ExternalSectionGuard guard(state);
            ext->glUseProgram(0);
        }
        check(state.getLastAppliedProgramObject() == 0, "program dedup: ExternalSectionGuard clears cache");
        state.apply(ssA.get());
        check(state.getLastAppliedProgramObject() == pcpA && currentGlProgram() == static_cast<GLint>(pcpA->getHandle()),
              "program dedup: State::apply after ExternalSectionGuard re-binds");

        // 7. Switching to progB and back to progA binds each handle.
        osg::ref_ptr<osg::StateSet> ssB = new osg::StateSet;
        ssB->setAttributeAndModes(progB.get(), osg::StateAttribute::ON);
        state.apply(ssB.get());
        const osg::Program::PerContextProgram* pcpB = state.getLastAppliedProgramObject();
        check(pcpB != 0 && pcpB != pcpA && currentGlProgram() == static_cast<GLint>(pcpB->getHandle()),
              "program dedup: switching to progB binds pcpB");
        state.apply(ssA.get());
        check(state.getLastAppliedProgramObject() == pcpA && currentGlProgram() == static_cast<GLint>(pcpA->getHandle()),
              "program dedup: switching back to progA binds pcpA");

        // 8. Relevant shader define switches to a distinct PCP and binds its handle.
        osg::ref_ptr<osg::StateSet> ssAlt = new osg::StateSet;
        ssAlt->setAttributeAndModes(progA.get(), osg::StateAttribute::ON);
        ssAlt->setDefine("ALT_COLOR", osg::StateAttribute::ON);
        state.pushStateSet(ssAlt.get());
        state.apply();
        const osg::Program::PerContextProgram* pcpAlt = state.getLastAppliedProgramObject();
        check(pcpAlt != 0 && pcpAlt != pcpA, "program dedup: shader-imported define switches PCP");
        if (pcpAlt)
        {
            check(currentGlProgram() == static_cast<GLint>(pcpAlt->getHandle()),
                  "program dedup: shader-imported define binds new handle");

            // 9. Relinking an already-active PCP re-binds it even though cache pointer matches.
            ext->glUseProgram(0);
            progA->dirtyProgram();
            progA->apply(state);
            check(state.getLastAppliedProgramObject() == pcpAlt && currentGlProgram() == static_cast<GLint>(pcpAlt->getHandle()),
                  "program dedup: relinked active PCP re-binds GL program");
        }
        state.popStateSet();

        // 10. releaseGLObjects clears both attribute cache and PCP so subsequent State::apply re-links and binds;
        // also releasing another program after a nullptr release never dereferences a stale PCP pointer.
        progA->releaseGLObjects(&state);
        check(state.getLastAppliedProgramObject() == 0,
              "program dedup: releaseGLObjects clears cached active PCP");
        state.apply(ssA.get());
        const osg::Program::PerContextProgram* pcpARecreated = state.getLastAppliedProgramObject();
        check(pcpARecreated != 0 && currentGlProgram() == static_cast<GLint>(pcpARecreated->getHandle()),
              "program dedup: State::apply after releaseGLObjects re-links and binds");
        progA->releaseGLObjects(0);
        progB->releaseGLObjects(&state);
        state.haveAppliedAttribute(osg::StateAttribute::PROGRAM);
        state.apply(new osg::StateSet);
        const std::string errors = drainGlErrors();
        check(errors.empty(), "program dedup: zero GL errors (" + errors + ")");
    }
}

int main(int, char**)
{
    osg::setNotifyLevel(osg::WARN);

#if !defined(OSG_GL3_AVAILABLE)
    std::cout << "osgcorecontextgl: skipped, non-GLCORE build" << std::endl;
    return kSkip;
#else
    // 1. Realize first 4.6 core forward-compatible context.
    osg::ref_ptr<osg::GraphicsContext> ctxA = createCore46Context("osgcorecontextgl_A");
    if (!ctxA.valid() || !ctxA->realize() || !ctxA->makeCurrent())
    {
        std::cout << "osgcorecontextgl: skipped, no OpenGL 4.6 core context available" << std::endl;
        return kSkip;
    }

    const unsigned int contextIdA = ctxA->getState()->getContextID();
    const char* renderer = reinterpret_cast<const char*>(glGetString(GL_RENDERER));
    const char* version = reinterpret_cast<const char*>(glGetString(GL_VERSION));
    std::cout << "osgcorecontextgl: " << (renderer ? renderer : "?") << ", GL " << (version ? version : "?") << std::endl;

    checkContextProperties(*ctxA->getState(), "context A");
    checkMixedStateSet(*ctxA->getState());
    checkProgramApplyDedup(*ctxA->getState());

    const osg::Vec4f colourA(0.25f, 0.50f, 0.75f, 1.0f);
    checkPixelMatches(drawGeometryToRgba32f(*ctxA->getState(), colourA, "context A draw"), colourA, "context A draw");

    // 5. Creating and realizing a second hidden context must keep context A current and usable (Win32 sample-context fix).
    osg::ref_ptr<osg::GraphicsContext> ctxB = createCore46Context("osgcorecontextgl_B");
    check(ctxB.valid() && ctxB->realize(), "second hidden core 4.6 context B realizes while A is current");
    const osg::Vec4f colourA2(0.80f, 0.20f, 0.40f, 1.0f);
    checkPixelMatches(drawGeometryToRgba32f(*ctxA->getState(), colourA2, "context A after realizing B"), colourA2, "context A after realizing B");

    if (ctxB.valid())
    {
        ctxB->close();
        ctxB = 0;
    }

    // 4. Close context A cleanly (releases GLExtensions + GL objects) and recreate a fresh context reusing contextIdA.
    ctxA->releaseContext();
    ctxA->close();
    ctxA = 0;

    osg::ref_ptr<osg::GraphicsContext> ctxA2 = createCore46Context("osgcorecontextgl_A2");
    check(ctxA2.valid() && ctxA2->realize() && ctxA2->makeCurrent(), "recreated core 4.6 context realizes and becomes current");
    if (ctxA2.valid() && ctxA2->isRealized())
    {
        check(ctxA2->getState()->getContextID() == contextIdA, "recreated context reuses freed context ID");
        checkContextProperties(*ctxA2->getState(), "recreated context");
        const osg::Vec4f colourRecreated(0.10f, 0.90f, 0.30f, 1.0f);
        checkPixelMatches(drawGeometryToRgba32f(*ctxA2->getState(), colourRecreated, "recreated context draw"), colourRecreated, "recreated context draw");
        ctxA2->releaseContext();
        ctxA2->close();
    }

    std::cout << "osgcorecontextgl: " << s_checks << " checks, " << s_failures << " failed" << std::endl;
    return s_failures == 0 ? 0 : 1;
#endif
}
