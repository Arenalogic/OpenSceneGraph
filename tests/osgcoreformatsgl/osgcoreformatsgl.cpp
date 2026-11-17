/* osgcoreformatsgl: legacy texture formats and cube-map mipmaps on a real core profile context (Arenalogic fork test,
 * ctest). Needs a GL context of version 3.3 or later and a build without legacy formats (OSG_GL3_AVAILABLE: GLCORE);
 * without either it exits 77, which ctest reports as skipped (SKIP_RETURN_CODE).
 *
 * Why a context: the mapping itself is checked without one (osgcoreformats); what only a driver can show is that
 * the stand-in uploads without a GL error and samples, through its texture swizzle, what the legacy format sampled.
 *
 *  1. Each legacy format: a 1x1 image is uploaded through osg::Texture2D::apply, sampled by a #version 330 core
 *     program into a GL_RGBA32F target and read back. Expected, from the OpenGL 2.1 specification (table 3.20):
 *     luminance (L, L, L, 1), alpha (0, 0, 0, A), luminance-alpha (L, L, L, A), intensity (I, I, I, I); a core
 *     internal format from legacy data samples the legacy expansion (L -> (L, L, L, 1)). No GL error on the way.
 *     The texture is then read back with osg::Image::readImageFromCurrentTexture: the same bytes, labelled with the
 *     legacy format the swizzle gives the texture.
 *  2. Cube-map mipmaps: six solid 4x4 faces without mip levels, trilinear, hardware mipmaps. The chain is generated
 *     once after the sixth face: no GL error (glGenerateMipmap on a cube that is not cube complete is
 *     GL_INVALID_OPERATION), and every face's 1x1 level holds the face's colour.
 *
 * Exit code 0 when every check holds, 1 otherwise (each failed check is printed), 77 when skipped.
 */

#include <osg/FrameBufferObject>
#include <osg/GraphicsContext>
#include <osg/GLExtensions>
#include <osg/Image>
#include <osg/Notify>
#include <osg/State>
#include <osg/Texture2D>
#include <osg/TextureCubeMap>
#include <osgViewer/GraphicsWindow>

#include <cmath>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

USE_GRAPHICSWINDOW()

#ifndef GL_CONTEXT_PROFILE_MASK
    #define GL_CONTEXT_PROFILE_MASK 0x9126
#endif
#ifndef GL_COLOR_ATTACHMENT0
    #define GL_COLOR_ATTACHMENT0 0x8CE0
#endif

namespace
{
    const int kSkip = 77;
    int s_failures = 0;

    void check(bool ok, const std::string& what)
    {
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

    std::string glErrors()
    {
        std::string errors;
        for (GLenum e = glGetError(); e != GL_NO_ERROR; e = glGetError()) errors += (errors.empty() ? "" : " ") + hex(e);
        return errors;
    }

    GLuint compile(const osg::GLExtensions& ext, GLenum type, const char* source)
    {
        GLuint shader = ext.glCreateShader(type);
        ext.glShaderSource(shader, 1, &source, 0);
        ext.glCompileShader(shader);
        GLint ok = 0;
        ext.glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
        check(ok != 0, std::string("the sampling program's ") + (type == GL_VERTEX_SHADER ? "vertex" : "fragment") + " shader compiles");
        return shader;
    }

    // Draws one fragment that samples unit 0 at the texel centre into a 1x1 GL_RGBA32F target.
    class Sampler
    {
    public:
        explicit Sampler(const osg::GLExtensions& ext) : _ext(ext), _program(0), _vao(0), _fbo(0), _target(0)
        {
            const char* vertex =
                "#version 330 core\n"
                "void main() { vec2 p = vec2((gl_VertexID & 1) * 4.0 - 1.0, (gl_VertexID & 2) * 2.0 - 1.0); gl_Position = vec4(p, 0.0, 1.0); }\n";
            const char* fragment =
                "#version 330 core\n"
                "uniform sampler2D t;\n"
                "out vec4 colour;\n"
                "void main() { colour = texture(t, vec2(0.5)); }\n";
            GLuint vs = compile(ext, GL_VERTEX_SHADER, vertex), fs = compile(ext, GL_FRAGMENT_SHADER, fragment);
            _program = ext.glCreateProgram();
            ext.glAttachShader(_program, vs);
            ext.glAttachShader(_program, fs);
            ext.glLinkProgram(_program);
            GLint ok = 0;
            ext.glGetProgramiv(_program, GL_LINK_STATUS, &ok);
            check(ok != 0, "the sampling program links");
            ext.glDeleteShader(vs);
            ext.glDeleteShader(fs);

            ext.glGenVertexArrays(1, &_vao);
            glGenTextures(1, &_target);
            glBindTexture(GL_TEXTURE_2D, _target);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F_ARB, 1, 1, 0, GL_RGBA, GL_FLOAT, 0);
            glBindTexture(GL_TEXTURE_2D, 0);
            ext.glGenFramebuffers(1, &_fbo);
            ext.glBindFramebuffer(GL_FRAMEBUFFER_EXT, _fbo);
            ext.glFramebufferTexture2D(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, _target, 0);
            check(ext.glCheckFramebufferStatus(GL_FRAMEBUFFER_EXT) == GL_FRAMEBUFFER_COMPLETE_EXT, "the GL_RGBA32F target is complete");
            ext.glBindFramebuffer(GL_FRAMEBUFFER_EXT, 0);
        }

        ~Sampler()
        {
            _ext.glDeleteFramebuffers(1, &_fbo);
            glDeleteTextures(1, &_target);
            _ext.glDeleteVertexArrays(1, &_vao);
            _ext.glDeleteProgram(_program);
        }

        osg::Vec4f sample() const
        {
            _ext.glBindFramebuffer(GL_FRAMEBUFFER_EXT, _fbo);
            glViewport(0, 0, 1, 1);
            _ext.glUseProgram(_program);
            _ext.glUniform1i(_ext.glGetUniformLocation(_program, "t"), 0);
            _ext.glBindVertexArray(_vao);
            glDrawArrays(GL_TRIANGLES, 0, 3);
            osg::Vec4f texel;
            glReadPixels(0, 0, 1, 1, GL_RGBA, GL_FLOAT, texel.ptr());
            _ext.glBindVertexArray(0);
            _ext.glUseProgram(0);
            _ext.glBindFramebuffer(GL_FRAMEBUFFER_EXT, 0);
            return texel;
        }

    private:
        const osg::GLExtensions& _ext;
        GLuint _program, _vao, _fbo, _target;
    };

    struct FormatCase
    {
        const char* name;
        GLint internalFormat;
        GLenum pixelFormat;
        GLenum dataType;
        float data[2];          // the client channels, in [0, 1]
        float expected[4];      // the legacy sample
        GLenum readbackPixelFormat;     // 0: no legacy label expected
        GLint readbackInternalFormat;
    };

    const float L = 64.0f / 255.0f, A = 192.0f / 255.0f;

    const FormatCase s_formats[] =
    {
        { "GL_LUMINANCE8",          GL_LUMINANCE8,          GL_LUMINANCE,       GL_UNSIGNED_BYTE, { L, 0 }, { L, L, L, 1 }, GL_LUMINANCE,       GL_LUMINANCE8 },
        { "luminance (1)",          1,                      GL_LUMINANCE,       GL_UNSIGNED_BYTE, { L, 0 }, { L, L, L, 1 }, GL_LUMINANCE,       GL_LUMINANCE8 },
        { "GL_ALPHA8",              GL_ALPHA8,              GL_ALPHA,           GL_UNSIGNED_BYTE, { A, 0 }, { 0, 0, 0, A }, GL_ALPHA,           GL_ALPHA8 },
        { "GL_ALPHA",               GL_ALPHA,               GL_ALPHA,           GL_UNSIGNED_BYTE, { A, 0 }, { 0, 0, 0, A }, GL_ALPHA,           GL_ALPHA8 },
        { "GL_LUMINANCE8_ALPHA8",   GL_LUMINANCE8_ALPHA8,   GL_LUMINANCE_ALPHA, GL_UNSIGNED_BYTE, { L, A }, { L, L, L, A }, GL_LUMINANCE_ALPHA, GL_LUMINANCE8_ALPHA8 },
        { "GL_INTENSITY8",          GL_INTENSITY8,          GL_LUMINANCE,       GL_UNSIGNED_BYTE, { L, 0 }, { L, L, L, L }, GL_LUMINANCE,       GL_INTENSITY8 },
        { "GL_LUMINANCE32F",        GL_LUMINANCE32F_ARB,    GL_LUMINANCE,       GL_FLOAT,         { L, 0 }, { L, L, L, 1 }, GL_LUMINANCE,       GL_LUMINANCE32F_ARB },
        { "GL_RGBA8 from luminance data", GL_RGBA8,         GL_LUMINANCE,       GL_UNSIGNED_BYTE, { L, 0 }, { L, L, L, 1 }, 0,                  0 },
    };

    void checkFormat(osg::State& state, const Sampler& sampler, const FormatCase& c)
    {
        const unsigned int channels = c.pixelFormat == GL_LUMINANCE_ALPHA ? 2u : 1u;
        osg::ref_ptr<osg::Image> image = new osg::Image;
        image->allocateImage(1, 1, 1, c.pixelFormat, c.dataType);
        image->setInternalTextureFormat(c.internalFormat);
        for (unsigned int i = 0; i < channels; ++i)
        {
            if (c.dataType == GL_FLOAT) reinterpret_cast<float*>(image->data())[i] = c.data[i];
            else image->data()[i] = static_cast<unsigned char>(c.data[i] * 255.0f + 0.5f);
        }

        osg::ref_ptr<osg::Texture2D> texture = new osg::Texture2D(image.get());
        texture->setFilter(osg::Texture::MIN_FILTER, osg::Texture::NEAREST);
        texture->setFilter(osg::Texture::MAG_FILTER, osg::Texture::NEAREST);
        texture->setResizeNonPowerOfTwoHint(false);

        glErrors();
        state.setActiveTextureUnit(0);
        texture->apply(state);
        const std::string uploadErrors = glErrors();
        check(uploadErrors.empty(), std::string(c.name) + ": uploads without a GL error (" + uploadErrors + ")");

        const osg::Vec4f texel = sampler.sample();
        for (int i = 0; i < 4; ++i)
        {
            std::ostringstream what;
            what << c.name << ": channel " << i << " samples " << texel[i] << ", the legacy format sampled " << c.expected[i];
            check(std::fabs(texel[i] - c.expected[i]) <= 0.5f / 255.0f, what.str());
        }

        // The texture is still bound on unit 0: read it back.
        glErrors();
        osg::ref_ptr<osg::Image> back = new osg::Image;
        back->readImageFromCurrentTexture(state.getContextID(), false, c.dataType);
        const std::string readErrors = glErrors();
        check(readErrors.empty(), std::string(c.name) + ": reads back without a GL error (" + readErrors + ")");
        if (c.readbackPixelFormat != 0)
        {
            check(back->getPixelFormat() == c.readbackPixelFormat,
                  std::string(c.name) + ": the readback's pixel format is " + hex(back->getPixelFormat()) + ", expected " + hex(c.readbackPixelFormat));
            check(back->getInternalTextureFormat() == c.readbackInternalFormat,
                  std::string(c.name) + ": the readback's internal format is " + hex(back->getInternalTextureFormat()) + ", expected " + hex(c.readbackInternalFormat));
            const unsigned int bytes = channels * (c.dataType == GL_FLOAT ? 4u : 1u);
            check(back->data() != 0 && std::memcmp(back->data(), image->data(), bytes) == 0, std::string(c.name) + ": the readback holds the uploaded bytes");
        }

        texture->releaseGLObjects(&state);
    }

    void checkCubeMipmaps(osg::State& state)
    {
        const unsigned char colours[6][4] = {
            { 255, 0, 0, 255 }, { 0, 255, 0, 255 }, { 0, 0, 255, 255 }, { 255, 255, 0, 255 }, { 0, 255, 255, 255 }, { 255, 0, 255, 255 } };

        osg::ref_ptr<osg::TextureCubeMap> cube = new osg::TextureCubeMap;
        for (unsigned int face = 0; face < 6; ++face)
        {
            osg::ref_ptr<osg::Image> image = new osg::Image;
            image->allocateImage(4, 4, 1, GL_RGBA, GL_UNSIGNED_BYTE);
            image->setInternalTextureFormat(GL_RGBA8);
            for (unsigned int i = 0; i < 16; ++i) std::memcpy(image->data() + 4 * i, colours[face], 4);
            cube->setImage(face, image.get());
        }
        cube->setFilter(osg::Texture::MIN_FILTER, osg::Texture::LINEAR_MIPMAP_LINEAR);
        cube->setFilter(osg::Texture::MAG_FILTER, osg::Texture::LINEAR);
        cube->setUseHardwareMipMapGeneration(true);

        glErrors();
        state.setActiveTextureUnit(0);
        cube->apply(state);
        const std::string errors = glErrors();
        check(errors.empty(), "the cube map uploads and generates its mipmaps without a GL error (" + errors + ")");

        cube->getTextureObject(state.getContextID())->bind();
        for (unsigned int face = 0; face < 6; ++face)
        {
            unsigned char texel[4] = { 0, 0, 0, 0 };
            glGetTexImage(GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, 2, GL_RGBA, GL_UNSIGNED_BYTE, texel);
            std::ostringstream what;
            what << "cube face " << face << ": its 1x1 level holds the face's colour (" << int(texel[0]) << ", " << int(texel[1]) << ", "
                 << int(texel[2]) << ", " << int(texel[3]) << ")";
            check(std::memcmp(texel, colours[face], 4) == 0, what.str());
        }
        cube->releaseGLObjects(&state);
    }
}

int main(int, char**)
{
    osg::setNotifyLevel(osg::WARN);

#if !defined(OSG_GL3_AVAILABLE)
    std::cout << "osgcoreformatsgl: skipped, a build with legacy texture formats maps none" << std::endl;
    return kSkip;
#else
    osg::ref_ptr<osg::GraphicsContext::Traits> traits = new osg::GraphicsContext::Traits;
    traits->x = 0;
    traits->y = 0;
    traits->width = 16;
    traits->height = 16;
    traits->windowDecoration = false;
    traits->doubleBuffer = false;
    traits->hidden = true;
    traits->windowName = "osgcoreformatsgl";

    osg::ref_ptr<osg::GraphicsContext> context = osg::GraphicsContext::createGraphicsContext(traits.get());
    if (!context.valid() || !context->realize() || !context->makeCurrent())
    {
        std::cout << "osgcoreformatsgl: skipped, no GL context (traits ask for " << traits->glContextVersion << ")" << std::endl;
        return kSkip;
    }

    const osg::GLExtensions* ext = context->getState()->get<osg::GLExtensions>();
    const char* renderer = reinterpret_cast<const char*>(glGetString(GL_RENDERER));
    std::cout << "osgcoreformatsgl: " << (renderer ? renderer : "?") << ", GL " << ext->glVersion << std::endl;
    if (ext->glVersion < 3.3f)
    {
        std::cout << "osgcoreformatsgl: skipped, the context is below OpenGL 3.3 (no core texture swizzle)" << std::endl;
        context->releaseContext();
        context->close();
        return kSkip;
    }

    {
        Sampler sampler(*ext);
        for (size_t i = 0; i < sizeof(s_formats) / sizeof(s_formats[0]); ++i) checkFormat(*context->getState(), sampler, s_formats[i]);
        checkCubeMipmaps(*context->getState());
    }

    context->releaseContext();
    context->close();

    if (s_failures == 0) std::cout << "osgcoreformatsgl: every check holds" << std::endl;
    else std::cout << "osgcoreformatsgl: " << s_failures << " check(s) failed" << std::endl;
    return s_failures == 0 ? 0 : 1;
#endif
}
