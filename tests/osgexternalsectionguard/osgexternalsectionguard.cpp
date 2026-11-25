/* osgexternalsectionguard: osg::State::ExternalSectionGuard regression test on a real OpenGL 4.6 core context
 * (Arenalogic fork test, ctest; foundation F3d).
 *
 * Verifies:
 *  1. Entry unbinds GL_PIXEL_PACK_BUFFER and GL_PIXEL_UNPACK_BUFFER immediately so external texture uploads succeed.
 *  2. Reconcile restores raw GL objects (VAO 0, program 0, array/element/pixel buffers 0, active texture unit 0,
 *     recorded osg::Viewport) and syncs osg::State caches to match.
 *  3. No dirtyAllModes() flip trap: both when external GL leaves a mode OFF while the next StateSet wants ON, and
 *     when external GL leaves a mode ON while the next StateSet wants OFF, OSG applies the exact requested mode.
 *  4. Positional CLIPPLANE attributes stay last-applied across reconcile(), while non-positional attributes (BlendFunc)
 *     are dirtied so OSG re-applies them cleanly.
 *  5. An OSG Program + Texture2D + VAO/VBO Geometry draw immediately after reconcile() renders exact expected colour
 *     into an RGBA32F FBO with zero GL errors.
 *  6. Explicit reconcile() is idempotent and destructor does not overwrite state applied afterward.
 *
 * No ACT or dtRender dependency. Exit code 0 when every check holds, 1 otherwise, 77 when skipped.
 */

#include <osg/Array>
#include <osg/BlendFunc>
#include <osg/ClipPlane>
#include <osg/FrameBufferObject>
#include <osg/Geometry>
#include <osg/GLExtensions>
#include <osg/GraphicsContext>
#include <osg/Image>
#include <osg/Notify>
#include <osg/Program>
#include <osg/RenderInfo>
#include <osg/Shader>
#include <osg/State>
#include <osg/StateSet>
#include <osg/Texture2D>
#include <osg/Viewport>
#include <osgViewer/GraphicsWindow>

#include <cmath>
#include <cstring>
#include <iostream>
#include <sstream>
#include <string>

USE_GRAPHICSWINDOW()

#ifndef GL_CONTEXT_PROFILE_MASK
    #define GL_CONTEXT_PROFILE_MASK 0x9126
#endif
#ifndef GL_CONTEXT_CORE_PROFILE_BIT
    #define GL_CONTEXT_CORE_PROFILE_BIT 0x00000001
#endif
#ifndef GL_ARRAY_BUFFER_BINDING
    #define GL_ARRAY_BUFFER_BINDING 0x8894
#endif
#ifndef GL_ELEMENT_ARRAY_BUFFER_BINDING
    #define GL_ELEMENT_ARRAY_BUFFER_BINDING 0x8895
#endif
#ifndef GL_PIXEL_PACK_BUFFER
    #define GL_PIXEL_PACK_BUFFER 0x88EB
#endif
#ifndef GL_PIXEL_UNPACK_BUFFER
    #define GL_PIXEL_UNPACK_BUFFER 0x88EC
#endif
#ifndef GL_PIXEL_PACK_BUFFER_BINDING
    #define GL_PIXEL_PACK_BUFFER_BINDING 0x88ED
#endif
#ifndef GL_PIXEL_UNPACK_BUFFER_BINDING
    #define GL_PIXEL_UNPACK_BUFFER_BINDING 0x88EF
#endif
#ifndef GL_VERTEX_ARRAY_BINDING
    #define GL_VERTEX_ARRAY_BINDING 0x85B5
#endif
#ifndef GL_CURRENT_PROGRAM
    #define GL_CURRENT_PROGRAM 0x8B8D
#endif
#ifndef GL_ACTIVE_TEXTURE
    #define GL_ACTIVE_TEXTURE 0x84E0
#endif
#ifndef GL_TEXTURE0
    #define GL_TEXTURE0 0x84C0
#endif
#ifndef GL_TEXTURE3
    #define GL_TEXTURE3 0x84C3
#endif
#ifndef GL_COLOR_ATTACHMENT0
    #define GL_COLOR_ATTACHMENT0 0x8CE0
#endif

namespace
{
    const int kSkip = 77;
    const unsigned int kCreateForwardCompatibleBit = 0x00000002u;

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

    GLint queryInt(GLenum pname)
    {
        GLint v = -1;
        glGetIntegerv(pname, &v);
        return v;
    }

    osg::ref_ptr<osg::GraphicsContext> createCoreContext()
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
            traits->windowName = "osgexternalsectionguard";
            traits->glContextVersion = "4.6";
            traits->glContextProfileMask = GL_CONTEXT_CORE_PROFILE_BIT;
            traits->glContextFlags = kCreateForwardCompatibleBit;
            osg::ref_ptr<osg::GraphicsContext> gc = osg::GraphicsContext::createGraphicsContext(traits.get());
            if (gc.valid() && gc->realize() && gc->makeCurrent()) return gc;
        }
        return 0;
    }
}

int main(int, char**)
{
    osg::setNotifyLevel(osg::WARN);

#if !defined(OSG_GL3_AVAILABLE)
    std::cout << "osgexternalsectionguard: skipped, non-GLCORE build" << std::endl;
    return kSkip;
#else
    osg::ref_ptr<osg::GraphicsContext> gc = createCoreContext();
    if (!gc.valid())
    {
        std::cout << "osgexternalsectionguard: skipped, no OpenGL 4.6 core context available" << std::endl;
        return kSkip;
    }

    osg::State& state = *gc->getState();
    state.setUseVertexAttributeAliasing(false);
    state.setUseModelViewAndProjectionUniforms(false);
    const osg::GLExtensions* ext = state.get<osg::GLExtensions>();
    const char* renderer = reinterpret_cast<const char*>(glGetString(GL_RENDERER));
    const char* version = reinterpret_cast<const char*>(glGetString(GL_VERSION));
    std::cout << "osgexternalsectionguard: " << (renderer ? renderer : "?") << ", GL " << (version ? version : "?") << std::endl;

    drainGlErrors();

    // Establish known OSG-tracked state: Viewport(2, 3, 12, 10), BLEND=OFF, DEPTH_TEST=ON, CULL_FACE=OFF,
    // positional ClipPlane(0) and non-positional BlendFunc.
    osg::ref_ptr<osg::Viewport> osgVp = new osg::Viewport(2, 3, 12, 10);
    osg::ref_ptr<osg::BlendFunc> osgBlend = new osg::BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    osg::ref_ptr<osg::ClipPlane> osgClip = new osg::ClipPlane(0, 0.0, 0.0, 1.0, 0.0);

    osg::ref_ptr<osg::StateSet> preSS = new osg::StateSet;
    preSS->setAttribute(osgVp.get(), osg::StateAttribute::ON);
    preSS->setAttribute(osgBlend.get(), osg::StateAttribute::ON);
    preSS->setMode(GL_BLEND, osg::StateAttribute::OFF);
    preSS->setMode(GL_DEPTH_TEST, osg::StateAttribute::ON);
    preSS->setMode(GL_CULL_FACE, osg::StateAttribute::OFF);
    state.apply(preSS.get());
    state.haveAppliedAttribute(osgClip.get());

    // Create raw external GL objects to simulate an external SDK dirtying GL state.
    GLuint extVao = 0, extBuf = 0, extPbo = 0;
    ext->glGenVertexArrays(1, &extVao);
    ext->glGenBuffers(1, &extBuf);
    ext->glGenBuffers(1, &extPbo);
    ext->glBindBuffer(GL_PIXEL_UNPACK_BUFFER, extPbo);

    // 1. Entry unbinds pixel pack/unpack buffers immediately.
    {
        osg::State::ExternalSectionGuard guard(state);
        check(!guard.isReconciled(), "guard starts unreconciled");
        check(queryInt(GL_PIXEL_PACK_BUFFER_BINDING) == 0, "entry unbinds GL_PIXEL_PACK_BUFFER");
        check(queryInt(GL_PIXEL_UNPACK_BUFFER_BINDING) == 0, "entry unbinds GL_PIXEL_UNPACK_BUFFER");
        check(state.getCurrentPixelBufferObject() == 0, "entry clears OSG current PBO cache");

        // External raw GL code corrupts every tracked binding and mode.
        ext->glBindVertexArray(extVao);
        ext->glBindBuffer(GL_ARRAY_BUFFER, extBuf);
        ext->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, extBuf);
        ext->glBindBuffer(GL_PIXEL_PACK_BUFFER, extPbo);
        ext->glBindBuffer(GL_PIXEL_UNPACK_BUFFER, extPbo);
        ext->glActiveTexture(GL_TEXTURE3);
        glViewport(0, 0, 5, 5);
        // Flip modes — notably keep GL_BLEND OFF (the SilverLining trap) and disable GL_DEPTH_TEST, enable GL_CULL_FACE.
        glDisable(GL_BLEND);
        glDisable(GL_DEPTH_TEST);
        glEnable(GL_CULL_FACE);

        // Explicit reconcile() + idempotency check.
        guard.reconcile();
        check(guard.isReconciled(), "reconcile() marks guard reconciled");

        // Modify a mode AFTER reconcile() to verify destructor does not reconcile a second time.
        glEnable(GL_SCISSOR_TEST);
    }

    // 6. Destructor did not clobber GL_SCISSOR_TEST applied after explicit reconcile().
    check(glIsEnabled(GL_SCISSOR_TEST) == GL_TRUE, "destructor after explicit reconcile() is a no-op");
    glDisable(GL_SCISSOR_TEST);

    // 2. GL objects & OSG caches restored to known neutral values + recorded viewport.
    check(queryInt(GL_VERTEX_ARRAY_BINDING) == 0, "reconcile binds VAO 0");
    check(queryInt(GL_CURRENT_PROGRAM) == 0, "reconcile binds program 0");
    check(queryInt(GL_ARRAY_BUFFER_BINDING) == 0, "reconcile unbinds GL_ARRAY_BUFFER");
    check(queryInt(GL_ELEMENT_ARRAY_BUFFER_BINDING) == 0, "reconcile unbinds GL_ELEMENT_ARRAY_BUFFER");
    check(queryInt(GL_PIXEL_PACK_BUFFER_BINDING) == 0, "reconcile unbinds GL_PIXEL_PACK_BUFFER");
    check(queryInt(GL_PIXEL_UNPACK_BUFFER_BINDING) == 0, "reconcile unbinds GL_PIXEL_UNPACK_BUFFER");
    check(queryInt(GL_ACTIVE_TEXTURE) == GL_TEXTURE0, "reconcile sets active texture GL_TEXTURE0");

    GLint vp[4] = { 0, 0, 0, 0 };
    glGetIntegerv(GL_VIEWPORT, vp);
    check(vp[0] == 2 && vp[1] == 3 && vp[2] == 12 && vp[3] == 10, "reconcile restores OSG viewport (2,3,12,10)");

    check(state.getCurrentVertexArrayObject() == 0, "OSG VAO cache is 0");
    check(state.getLastAppliedProgramObject() == 0, "OSG program cache is null");
    check(state.getCurrentPixelBufferObject() == 0, "OSG PBO cache is null");
    check(state.getActiveTextureUnit() == 0, "OSG active texture unit cache is 0");

    // 4. Positional CLIPPLANE preserved; non-positional BlendFunc dirtied.
    check(state.getLastAppliedAttribute(osg::StateAttribute::CLIPPLANE, 0) == osgClip.get(),
          "positional CLIPPLANE stays last-applied across reconcile()");
    check(state.getLastAppliedAttribute(osg::StateAttribute::BLENDFUNC, 0) == 0,
          "non-positional BlendFunc is dirtied by reconcile()");

    // 3. Modes restored to OSG cache values; next StateSet wanting opposite mode applies it cleanly (no flip trap!).
    check(glIsEnabled(GL_BLEND) == GL_FALSE, "GL_BLEND restored to OFF");
    check(glIsEnabled(GL_DEPTH_TEST) == GL_TRUE, "GL_DEPTH_TEST restored to ON");
    check(glIsEnabled(GL_CULL_FACE) == GL_FALSE, "GL_CULL_FACE restored to OFF");

    // Now test the exact SilverLining flip-trap case: OSG cached BLEND=OFF, external left BLEND=OFF,
    // next StateSet wants BLEND=ON (and DEPTH_TEST=OFF, CULL_FACE=ON).
    osg::ref_ptr<osg::StateSet> postSS = new osg::StateSet;
    postSS->setMode(GL_BLEND, osg::StateAttribute::ON);
    postSS->setMode(GL_DEPTH_TEST, osg::StateAttribute::OFF);
    postSS->setMode(GL_CULL_FACE, osg::StateAttribute::ON);
    state.apply(postSS.get());
    check(glIsEnabled(GL_BLEND) == GL_TRUE, "post-guard StateSet enables GL_BLEND (no dirtyAllModes flip trap)");
    check(glIsEnabled(GL_DEPTH_TEST) == GL_FALSE, "post-guard StateSet disables GL_DEPTH_TEST");
    check(glIsEnabled(GL_CULL_FACE) == GL_TRUE, "post-guard StateSet enables GL_CULL_FACE");

    // 5. Real OSG Program + Texture2D + VAO/VBO Geometry draw immediately after a second guard scope.
    osg::ref_ptr<osg::Image> texImg = new osg::Image;
    texImg->allocateImage(1, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE);
    texImg->setInternalTextureFormat(GL_RGBA8);
    texImg->data()[0] = 64; texImg->data()[1] = 192; texImg->data()[2] = 128; texImg->data()[3] = 255;
    osg::ref_ptr<osg::Texture2D> tex = new osg::Texture2D(texImg.get());
    tex->setFilter(osg::Texture::MIN_FILTER, osg::Texture::NEAREST);
    tex->setFilter(osg::Texture::MAG_FILTER, osg::Texture::NEAREST);

    osg::ref_ptr<osg::Program> prog = new osg::Program;
    prog->addBindAttribLocation("in_pos", 0);
    prog->addShader(new osg::Shader(osg::Shader::VERTEX,
        "#version 460 core\n"
        "layout(location = 0) in vec3 in_pos;\n"
        "void main() { gl_Position = vec4(in_pos, 1.0); }\n"));
    prog->addShader(new osg::Shader(osg::Shader::FRAGMENT,
        "#version 460 core\n"
        "uniform sampler2D u_tex;\n"
        "out vec4 out_col;\n"
        "void main() { out_col = texture(u_tex, vec2(0.5)); }\n"));

    osg::ref_ptr<osg::StateSet> drawSS = new osg::StateSet;
    drawSS->setAttributeAndModes(prog.get(), osg::StateAttribute::ON);
    drawSS->setTextureAttributeAndModes(0, tex.get(), osg::StateAttribute::ON);
    drawSS->addUniform(new osg::Uniform("u_tex", 0));
    drawSS->setMode(GL_BLEND, osg::StateAttribute::OFF);
    drawSS->setMode(GL_DEPTH_TEST, osg::StateAttribute::OFF);
    drawSS->setMode(GL_CULL_FACE, osg::StateAttribute::OFF);

    // Apply drawSS once so OSG caches program + texture, then corrupt GL inside a guard, then re-apply drawSS!
    state.apply(drawSS.get());
    {
        osg::State::ExternalSectionGuard guard2(state);
        ext->glBindVertexArray(extVao);
        ext->glUseProgram(0);
        glBindTexture(GL_TEXTURE_2D, 0);
        ext->glActiveTexture(GL_TEXTURE3);
    }

    GLuint fboTex = 0, fbo = 0;
    glGenTextures(1, &fboTex);
    glBindTexture(GL_TEXTURE_2D, fboTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F_ARB, 4, 4, 0, GL_RGBA, GL_FLOAT, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
    ext->glGenFramebuffers(1, &fbo);
    ext->glBindFramebuffer(GL_FRAMEBUFFER_EXT, fbo);
    ext->glFramebufferTexture2D(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fboTex, 0);

    osg::ref_ptr<osg::Vec3Array> verts = new osg::Vec3Array;
    verts->push_back(osg::Vec3(-1.0f, -1.0f, 0.0f));
    verts->push_back(osg::Vec3( 3.0f, -1.0f, 0.0f));
    verts->push_back(osg::Vec3(-1.0f,  3.0f, 0.0f));
    osg::ref_ptr<osg::Geometry> geom = new osg::Geometry;
    geom->setUseDisplayList(false);
    geom->setUseVertexBufferObjects(true);
    geom->setVertexAttribArray(0, verts.get(), osg::Array::BIND_PER_VERTEX);
    geom->addPrimitiveSet(new osg::DrawArrays(GL_TRIANGLES, 0, 3));

    glViewport(0, 0, 4, 4);
    state.apply(drawSS.get());
    osg::RenderInfo ri(&state, 0);
    geom->draw(ri);

    osg::Vec4f pixel(-1, -1, -1, -1);
    glReadPixels(2, 2, 1, 1, GL_RGBA, GL_FLOAT, pixel.ptr());
    const bool colourOk =
        std::fabs(pixel[0] - 64.0f / 255.0f) < 1e-2f &&
        std::fabs(pixel[1] - 192.0f / 255.0f) < 1e-2f &&
        std::fabs(pixel[2] - 128.0f / 255.0f) < 1e-2f &&
        std::fabs(pixel[3] - 1.0f) < 1e-2f;
    check(colourOk, "post-guard OSG Program + Texture2D + Geometry draw re-binds and reads exact texel");

    const std::string errors = drainGlErrors();
    check(errors.empty(), "zero GL errors across ExternalSectionGuard lifecycle (" + errors + ")");

    // Cleanup.
    geom->releaseGLObjects(&state);
    drawSS->releaseGLObjects(&state);
    ext->glBindFramebuffer(GL_FRAMEBUFFER_EXT, 0);
    ext->glDeleteFramebuffers(1, &fbo);
    glDeleteTextures(1, &fboTex);
    ext->glDeleteBuffers(1, &extBuf);
    ext->glDeleteBuffers(1, &extPbo);
    ext->glDeleteVertexArrays(1, &extVao);

    gc->releaseContext();
    gc->close();

    std::cout << "osgexternalsectionguard: " << s_checks << " checks, " << s_failures << " failed" << std::endl;
    return s_failures == 0 ? 0 : 1;
#endif
}
