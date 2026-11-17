/* osgcoreformats: osg::Texture's legacy texture formats on a core profile, checked without a window or a GL context
 * (Arenalogic fork test, ctest).
 *
 * The oracle is written out here from the OpenGL 2.1 specification, independent of the tables in src/osg/Texture.cpp:
 * a client group in GL_LUMINANCE becomes the RGBA group (L, L, L, 1), GL_ALPHA (0, 0, 0, A), GL_LUMINANCE_ALPHA
 * (L, L, L, A) (sec. 3.6.4); the internal format keeps its components of the group (table 3.15) and sampling expands
 * them again (table 3.20): luminance (L, L, L, 1), alpha (0, 0, 0, A), luminance-alpha (L, L, L, A), intensity
 * (I, I, I, I), red (R, 0, 0, 1). A core profile has none of the four legacy formats; the stand-in is a red or
 * red-green format of the same precision whose texture swizzle samples as the legacy format did.
 *
 *  - Texture::selectCoreFormat: the stand-in and the swizzle for each pair of internal and pixel format;
 *  - Texture::selectCorePixelFormat: the core pixel format that carries the same client bytes;
 *  - Texture::selectLegacyFormat: the readback's inverse;
 *  - on a build without legacy formats (OSG_GL3_AVAILABLE: GLCORE), a Texture maps a legacy format when it is set,
 *    keeps the request (getLegacyInternalFormat) and tells L8 from A8 although both stand in as GL_R8; on any other
 *    build it keeps the format as set.
 *
 * No call below reaches GL. Exit code 0 when every check holds, 1 otherwise; each failed check is printed.
 */

#include <osg/Texture2D>
#include <osg/Notify>
#include <osg/ref_ptr>

#include <iostream>
#include <sstream>
#include <string>

#ifndef GL_COMPRESSED_RED
    #define GL_COMPRESSED_RED 0x8225
#endif

namespace
{
    int s_failures = 0;

    std::string hex(GLint value)
    {
        std::ostringstream out;
        out << "0x" << std::hex << value;
        return out.str();
    }

    std::string swizzleText(const osg::Vec4i& s)
    {
        std::string text;
        for (int i = 0; i < 4; ++i)
        {
            switch (s[i])
            {
                case GL_RED: text += 'R'; break;
                case GL_GREEN: text += 'G'; break;
                case GL_BLUE: text += 'B'; break;
                case GL_ALPHA: text += 'A'; break;
                case GL_ZERO: text += '0'; break;
                case GL_ONE: text += '1'; break;
                default: text += '?'; break;
            }
        }
        return text;
    }

    void check(bool ok, const std::string& what)
    {
        if (!ok)
        {
            ++s_failures;
            std::cout << "FAILED: " << what << std::endl;
        }
    }

    struct CoreCase
    {
        GLint internalFormat;
        GLenum pixelFormat;     // 0: no client data (a render target)
        GLint coreFormat;
        const char* swizzle;    // R G B A 0 1, the channel each output reads
        const char* name;
    };

    // The spec's legacy sample against what the stand-in samples through its swizzle.
    const CoreCase s_cases[] =
    {
        // the legacy format as the image has it
        { GL_LUMINANCE,             GL_LUMINANCE,               GL_RED,             "RRR1", "luminance" },
        { 1,                        GL_LUMINANCE,               GL_RED,             "RRR1", "1 component (luminance)" },
        { GL_LUMINANCE8,            GL_LUMINANCE,               GL_R8,              "RRR1", "GL_LUMINANCE8" },
        { GL_LUMINANCE4,            GL_LUMINANCE,               GL_R8,              "RRR1", "GL_LUMINANCE4" },
        { GL_LUMINANCE16,           GL_LUMINANCE,               GL_R16,             "RRR1", "GL_LUMINANCE16" },
        { GL_LUMINANCE16F_ARB,      GL_LUMINANCE,               GL_R16F,            "RRR1", "GL_LUMINANCE16F" },
        { GL_LUMINANCE32F_ARB,      GL_LUMINANCE,               GL_R32F,            "RRR1", "GL_LUMINANCE32F" },
        { GL_LUMINANCE8UI_EXT,      GL_LUMINANCE_INTEGER_EXT,   GL_R8UI,            "RRR1", "GL_LUMINANCE8UI" },
        { GL_COMPRESSED_LUMINANCE,  GL_LUMINANCE,               GL_COMPRESSED_RED,  "RRR1", "GL_COMPRESSED_LUMINANCE" },
        { GL_ALPHA,                 GL_ALPHA,                   GL_RED,             "000R", "alpha" },
        { GL_ALPHA8,                GL_ALPHA,                   GL_R8,              "000R", "GL_ALPHA8" },
        { GL_ALPHA16,               GL_ALPHA,                   GL_R16,             "000R", "GL_ALPHA16" },
        { GL_ALPHA32F_ARB,          GL_ALPHA,                   GL_R32F,            "000R", "GL_ALPHA32F" },
        { GL_LUMINANCE_ALPHA,       GL_LUMINANCE_ALPHA,         GL_RG,              "RRRG", "luminance-alpha" },
        { 2,                        GL_LUMINANCE_ALPHA,         GL_RG,              "RRRG", "2 components (luminance-alpha)" },
        { GL_LUMINANCE8_ALPHA8,     GL_LUMINANCE_ALPHA,         GL_RG8,             "RRRG", "GL_LUMINANCE8_ALPHA8" },
        { GL_LUMINANCE16_ALPHA16,   GL_LUMINANCE_ALPHA,         GL_RG16,            "RRRG", "GL_LUMINANCE16_ALPHA16" },
        { GL_LUMINANCE_ALPHA32F_ARB, GL_LUMINANCE_ALPHA,        GL_RG32F,           "RRRG", "GL_LUMINANCE_ALPHA32F" },
        { GL_INTENSITY,             GL_LUMINANCE,               GL_RED,             "RRRR", "intensity from luminance data" },
        { GL_INTENSITY8,            GL_INTENSITY,               GL_R8,              "RRRR", "GL_INTENSITY8" },
        { GL_INTENSITY16F_ARB,      GL_INTENSITY,               GL_R16F,            "RRRR", "GL_INTENSITY16F" },

        // a legacy pixel format into a core internal format: the format stays, the swizzle expands the data
        { GL_RGBA8,                 GL_LUMINANCE,               GL_RGBA8,           "RRR1", "GL_RGBA8 from luminance data" },
        { GL_RGBA8,                 GL_ALPHA,                   GL_RGBA8,           "000R", "GL_RGBA8 from alpha data" },
        { GL_RGBA,                  GL_LUMINANCE_ALPHA,         GL_RGBA,            "RRRG", "GL_RGBA from luminance-alpha data" },
        { GL_RGB8,                  GL_LUMINANCE_ALPHA,         GL_RGB8,            "RRR1", "GL_RGB8 from luminance-alpha data" },
        { GL_R8,                    GL_LUMINANCE,               GL_R8,              "R001", "GL_R8 from luminance data" },
        { GL_R8,                    GL_ALPHA,                   GL_R8,              "0001", "GL_R8 from alpha data (no red)" },

        // a legacy internal format from core client data: the fewest channels that hold what it samples
        { GL_LUMINANCE8,            GL_RGBA,                    GL_R8,              "RRR1", "GL_LUMINANCE8 from RGBA data" },
        { GL_ALPHA8,                GL_RGBA,                    GL_RGBA8,           "000A", "GL_ALPHA8 from RGBA data" },
        { GL_ALPHA8,                GL_RGB,                     GL_R8,              "0001", "GL_ALPHA8 from RGB data (alpha 1)" },
        { GL_LUMINANCE_ALPHA,       GL_RGBA,                    GL_RGBA,            "RRRA", "luminance-alpha from RGBA data" },
        { GL_ALPHA,                 GL_LUMINANCE_ALPHA,         GL_RG,              "000G", "alpha from luminance-alpha data" },

        // no client data (a render target): a fragment's red is the luminance, its alpha the alpha
        { GL_LUMINANCE8,            0,                          GL_R8,              "RRR1", "GL_LUMINANCE8 target" },
        { GL_ALPHA8,                0,                          GL_RGBA8,           "000A", "GL_ALPHA8 target" },
        { GL_LUMINANCE8_ALPHA8,     0,                          GL_RGBA8,           "RRRA", "GL_LUMINANCE8_ALPHA8 target" },
        { GL_INTENSITY8,            0,                          GL_R8,              "RRRR", "GL_INTENSITY8 target" },

        // nothing legacy: unchanged, identity swizzle (3 and 4 become their base formats)
        { GL_RGBA8,                 GL_RGBA,                    GL_RGBA8,           "RGBA", "GL_RGBA8" },
        { GL_R8,                    GL_RED,                     GL_R8,              "RGBA", "GL_R8" },
        { GL_RGBA8,                 0,                          GL_RGBA8,           "RGBA", "GL_RGBA8 target" },
        { 3,                        GL_RGB,                     GL_RGB,             "RGBA", "3 components" },
        { 4,                        GL_RGBA,                    GL_RGBA,            "RGBA", "4 components" },
    };

    void checkCoreFormats()
    {
        for (size_t i = 0; i < sizeof(s_cases) / sizeof(s_cases[0]); ++i)
        {
            const CoreCase& c = s_cases[i];
            osg::Vec4i swizzle;
            const GLint core = osg::Texture::selectCoreFormat(c.internalFormat, c.pixelFormat, swizzle);
            check(core == c.coreFormat, std::string(c.name) + ": core format " + hex(core) + ", expected " + hex(c.coreFormat));
            check(swizzleText(swizzle) == c.swizzle,
                  std::string(c.name) + ": swizzle " + swizzleText(swizzle) + ", expected " + c.swizzle);
        }
    }

    void checkCorePixelFormats()
    {
        const GLenum pairs[][2] =
        {
            { GL_LUMINANCE, GL_RED }, { GL_ALPHA, GL_RED }, { GL_INTENSITY, GL_RED }, { GL_LUMINANCE_ALPHA, GL_RG },
            { GL_LUMINANCE_INTEGER_EXT, GL_RED_INTEGER_EXT }, { GL_ALPHA_INTEGER_EXT, GL_RED_INTEGER_EXT },
            { GL_LUMINANCE_ALPHA_INTEGER_EXT, GL_RG_INTEGER },
            { GL_RED, GL_RED }, { GL_RG, GL_RG }, { GL_RGB, GL_RGB }, { GL_RGBA, GL_RGBA }, { GL_BGRA, GL_BGRA }, { 0, 0 },
        };
        for (size_t i = 0; i < sizeof(pairs) / sizeof(pairs[0]); ++i)
        {
            const GLenum core = osg::Texture::selectCorePixelFormat(pairs[i][0]);
            check(core == pairs[i][1], "pixel format " + hex(pairs[i][0]) + " -> " + hex(core) + ", expected " + hex(pairs[i][1]));
        }
    }

    void checkLegacyFormats()
    {
        struct LegacyCase { GLint core; osg::Vec4i swizzle; bool found; GLint internalFormat; GLenum pixelFormat; const char* name; };
        const osg::Vec4i rrr1(GL_RED, GL_RED, GL_RED, GL_ONE), zzzr(GL_ZERO, GL_ZERO, GL_ZERO, GL_RED);
        const osg::Vec4i rrrr(GL_RED, GL_RED, GL_RED, GL_RED), rrrg(GL_RED, GL_RED, GL_RED, GL_GREEN);
        const osg::Vec4i identity(GL_RED, GL_GREEN, GL_BLUE, GL_ALPHA);
        const LegacyCase cases[] =
        {
            { GL_R8,    rrr1,     true,  GL_LUMINANCE8,          GL_LUMINANCE,             "GL_R8 RRR1" },
            { GL_R8,    zzzr,     true,  GL_ALPHA8,              GL_ALPHA,                 "GL_R8 000R" },
            { GL_R8,    rrrr,     true,  GL_INTENSITY8,          GL_LUMINANCE,             "GL_R8 RRRR" },
            { GL_RG8,   rrrg,     true,  GL_LUMINANCE8_ALPHA8,   GL_LUMINANCE_ALPHA,       "GL_RG8 RRRG" },
            { GL_R16,   rrr1,     true,  GL_LUMINANCE16,         GL_LUMINANCE,             "GL_R16 RRR1" },
            { GL_R32F,  rrr1,     true,  GL_LUMINANCE32F_ARB,    GL_LUMINANCE,             "GL_R32F RRR1" },
            { GL_RG16F, rrrg,     true,  GL_LUMINANCE_ALPHA16F_ARB, GL_LUMINANCE_ALPHA,    "GL_RG16F RRRG" },
            { GL_R8UI,  rrr1,     true,  GL_LUMINANCE8UI_EXT,    GL_LUMINANCE_INTEGER_EXT, "GL_R8UI RRR1" },
            { GL_R8,    identity, false, 0,                      0,                        "GL_R8 identity" },
            { GL_RG8,   rrr1,     false, 0,                      0,                        "GL_RG8 RRR1" },
            { GL_RGBA8, rrr1,     false, 0,                      0,                        "GL_RGBA8 RRR1" },
        };
        for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i)
        {
            const LegacyCase& c = cases[i];
            GLint internalFormat = 0;
            GLenum pixelFormat = 0;
            const bool found = osg::Texture::selectLegacyFormat(c.core, c.swizzle, internalFormat, pixelFormat);
            check(found == c.found, std::string(c.name) + (c.found ? ": no legacy format found" : ": a legacy format found"));
            if (found && c.found)
            {
                check(internalFormat == c.internalFormat, std::string(c.name) + ": internal format " + hex(internalFormat) + ", expected " + hex(c.internalFormat));
                check(pixelFormat == c.pixelFormat, std::string(c.name) + ": pixel format " + hex(pixelFormat) + ", expected " + hex(c.pixelFormat));
            }
        }
    }

    // What the build does with a Texture's own format.
    void checkTextureMembers()
    {
        osg::ref_ptr<osg::Texture2D> luminance = new osg::Texture2D;
        luminance->setInternalFormat(GL_LUMINANCE8);
        osg::ref_ptr<osg::Texture2D> alpha = new osg::Texture2D;
        alpha->setInternalFormat(GL_ALPHA8);

#if defined(OSG_GL3_AVAILABLE)
        check(luminance->getInternalFormat() == GL_R8, "a GL_LUMINANCE8 texture stands in as GL_R8, not " + hex(luminance->getInternalFormat()));
        check(luminance->getLegacyInternalFormat() == GL_LUMINANCE8, "a GL_LUMINANCE8 texture keeps its request");
        check(swizzleText(luminance->getFormatSwizzle()) == "RRR1", "a GL_LUMINANCE8 texture samples RRR1, not " + swizzleText(luminance->getFormatSwizzle()));
        // No image and no source format: a render target, whose fragments write their alpha into the alpha channel.
        check(alpha->getInternalFormat() == GL_RGBA8, "a GL_ALPHA8 texture without data stands in as GL_RGBA8, not " + hex(alpha->getInternalFormat()));
        check(swizzleText(alpha->getFormatSwizzle()) == "000A", "a GL_ALPHA8 texture without data samples 000A, not " + swizzleText(alpha->getFormatSwizzle()));
#else
        check(luminance->getInternalFormat() == GL_LUMINANCE8, "a build with legacy formats keeps GL_LUMINANCE8");
        check(luminance->getLegacyInternalFormat() == 0, "a build with legacy formats records no stand-in");
        check(swizzleText(luminance->getFormatSwizzle()) == "RGBA", "a build with legacy formats has the identity format swizzle");
#endif
        check(luminance->compare(*alpha) != 0, "GL_LUMINANCE8 and GL_ALPHA8 textures compare different");

        luminance->setInternalFormat(GL_RGBA8);
        check(luminance->getInternalFormat() == GL_RGBA8, "setting GL_RGBA8 afterwards gives GL_RGBA8");
        check(luminance->getLegacyInternalFormat() == 0, "setting GL_RGBA8 afterwards drops the legacy request");
        check(swizzleText(luminance->getFormatSwizzle()) == "RGBA", "setting GL_RGBA8 afterwards gives the identity format swizzle");
    }
}

int main(int, char**)
{
    osg::setNotifyLevel(osg::WARN);

    checkCoreFormats();
    checkCorePixelFormats();
    checkLegacyFormats();
    checkTextureMembers();

    if (s_failures == 0) std::cout << "osgcoreformats: every check holds" << std::endl;
    else std::cout << "osgcoreformats: " << s_failures << " check(s) failed" << std::endl;
    return s_failures == 0 ? 0 : 1;
}
