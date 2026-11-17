/* osgstatemodes: osg::State's mode table, checked without a window or a GL context (Arenalogic fork test, ctest).
 *
 * The oracle is written out here from the OpenGL 4.6 core and compatibility specifications, independent of the
 * tables in src/osg/State.cpp:
 *  - every glEnable capability of the 4.6 core profile is a CORE_MODE and stays valid;
 *  - every capability only the compatibility profile has, every OSG texture mode (never a glEnable capability on
 *    any profile) and every enable of an extension that configures the fixed-function pipeline is a
 *    FIXED_FUNCTION_MODE. A build without fixed function (OSG_GL_FIXED_FUNCTION_AVAILABLE undefined: GLCORE)
 *    creates such a mode invalid in the global map, on every texture unit and after releaseGLObjects; it cannot be
 *    made valid, never applies, and is warned once per process and then counted;
 *  - any other capability (an extension a core context may expose) is an UNKNOWN_MODE and passes through.
 *
 * No call below reaches GL: only refused modes are applied, so no context is needed.
 * Exit code 0 when every check holds, 1 otherwise; each failed check is printed.
 */

#include <osg/State>
#include <osg/Notify>
#include <osg/ref_ptr>

#include <cstring>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace
{
    struct NamedMode { osg::StateAttribute::GLMode mode; const char* name; };

    // glEnable's capabilities in the OpenGL 4.6 core profile (GL_CLIP_DISTANCEn = the old GL_CLIP_PLANEn,
    // GL_PROGRAM_POINT_SIZE = GL_VERTEX_PROGRAM_POINT_SIZE).
    const NamedMode s_core[] =
    {
        {0x0B20, "GL_LINE_SMOOTH"},
        {0x0B41, "GL_POLYGON_SMOOTH"},
        {0x0B44, "GL_CULL_FACE"},
        {0x0B71, "GL_DEPTH_TEST"},
        {0x0B90, "GL_STENCIL_TEST"},
        {0x0BD0, "GL_DITHER"},
        {0x0BE2, "GL_BLEND"},
        {0x0BF2, "GL_COLOR_LOGIC_OP"},
        {0x0C11, "GL_SCISSOR_TEST"},
        {0x2A01, "GL_POLYGON_OFFSET_POINT"},
        {0x2A02, "GL_POLYGON_OFFSET_LINE"},
        {0x3000, "GL_CLIP_DISTANCE0"},
        {0x3001, "GL_CLIP_DISTANCE1"},
        {0x3002, "GL_CLIP_DISTANCE2"},
        {0x3003, "GL_CLIP_DISTANCE3"},
        {0x3004, "GL_CLIP_DISTANCE4"},
        {0x3005, "GL_CLIP_DISTANCE5"},
        {0x3006, "GL_CLIP_DISTANCE6"},
        {0x3007, "GL_CLIP_DISTANCE7"},
        {0x8037, "GL_POLYGON_OFFSET_FILL"},
        {0x809D, "GL_MULTISAMPLE"},
        {0x809E, "GL_SAMPLE_ALPHA_TO_COVERAGE"},
        {0x809F, "GL_SAMPLE_ALPHA_TO_ONE"},
        {0x80A0, "GL_SAMPLE_COVERAGE"},
        {0x8242, "GL_DEBUG_OUTPUT_SYNCHRONOUS"},
        {0x8642, "GL_PROGRAM_POINT_SIZE"},
        {0x864F, "GL_DEPTH_CLAMP"},
        {0x884F, "GL_TEXTURE_CUBE_MAP_SEAMLESS"},
        {0x8C36, "GL_SAMPLE_SHADING"},
        {0x8C89, "GL_RASTERIZER_DISCARD"},
        {0x8D69, "GL_PRIMITIVE_RESTART_FIXED_INDEX"},
        {0x8DB9, "GL_FRAMEBUFFER_SRGB"},
        {0x8E51, "GL_SAMPLE_MASK"},
        {0x8F9D, "GL_PRIMITIVE_RESTART"},
        {0x92E0, "GL_DEBUG_OUTPUT"},
    };

    // The compatibility profile's other capabilities, OSG's texture modes (TextureGLModeSet in StateSet.cpp) and the
    // fixed-function pipeline extensions' enables.
    const NamedMode s_fixedFunction[] =
    {
        // OpenGL 4.6 compatibility profile only
        {0x0B10, "GL_POINT_SMOOTH"},
        {0x0B24, "GL_LINE_STIPPLE"},
        {0x0B42, "GL_POLYGON_STIPPLE"},
        {0x0B50, "GL_LIGHTING"},
        {0x0B57, "GL_COLOR_MATERIAL"},
        {0x0B60, "GL_FOG"},
        {0x0BA1, "GL_NORMALIZE"},
        {0x0BC0, "GL_ALPHA_TEST"},
        {0x0BF1, "GL_INDEX_LOGIC_OP"},
        {0x0D80, "GL_AUTO_NORMAL"},
        {0x0D90, "GL_MAP1_COLOR_4"},
        {0x0D91, "GL_MAP1_INDEX"},
        {0x0D92, "GL_MAP1_NORMAL"},
        {0x0D93, "GL_MAP1_TEXTURE_COORD_1"},
        {0x0D94, "GL_MAP1_TEXTURE_COORD_2"},
        {0x0D95, "GL_MAP1_TEXTURE_COORD_3"},
        {0x0D96, "GL_MAP1_TEXTURE_COORD_4"},
        {0x0D97, "GL_MAP1_VERTEX_3"},
        {0x0D98, "GL_MAP1_VERTEX_4"},
        {0x0DB0, "GL_MAP2_COLOR_4"},
        {0x0DB1, "GL_MAP2_INDEX"},
        {0x0DB2, "GL_MAP2_NORMAL"},
        {0x0DB3, "GL_MAP2_TEXTURE_COORD_1"},
        {0x0DB4, "GL_MAP2_TEXTURE_COORD_2"},
        {0x0DB5, "GL_MAP2_TEXTURE_COORD_3"},
        {0x0DB6, "GL_MAP2_TEXTURE_COORD_4"},
        {0x0DB7, "GL_MAP2_VERTEX_3"},
        {0x0DB8, "GL_MAP2_VERTEX_4"},
        {0x4000, "GL_LIGHT0"},
        {0x4001, "GL_LIGHT1"},
        {0x4002, "GL_LIGHT2"},
        {0x4003, "GL_LIGHT3"},
        {0x4004, "GL_LIGHT4"},
        {0x4005, "GL_LIGHT5"},
        {0x4006, "GL_LIGHT6"},
        {0x4007, "GL_LIGHT7"},
        {0x8010, "GL_CONVOLUTION_1D"},
        {0x8011, "GL_CONVOLUTION_2D"},
        {0x8012, "GL_SEPARABLE_2D"},
        {0x8024, "GL_HISTOGRAM"},
        {0x802E, "GL_MINMAX"},
        {0x803A, "GL_RESCALE_NORMAL"},
        {0x80D0, "GL_COLOR_TABLE"},
        {0x80D1, "GL_POST_CONVOLUTION_COLOR_TABLE"},
        {0x80D2, "GL_POST_COLOR_MATRIX_COLOR_TABLE"},
        {0x8458, "GL_COLOR_SUM"},
        {0x8620, "GL_VERTEX_PROGRAM_ARB"},
        {0x8643, "GL_VERTEX_PROGRAM_TWO_SIDE"},
        {0x8804, "GL_FRAGMENT_PROGRAM_ARB"},
        {0x8861, "GL_POINT_SPRITE"},
        // texture targets and texture coordinate generation: compatibility enables or never a capability at all
        {0x0C60, "GL_TEXTURE_GEN_S"},
        {0x0C61, "GL_TEXTURE_GEN_T"},
        {0x0C62, "GL_TEXTURE_GEN_R"},
        {0x0C63, "GL_TEXTURE_GEN_Q"},
        {0x0DE0, "GL_TEXTURE_1D"},
        {0x0DE1, "GL_TEXTURE_2D"},
        {0x806F, "GL_TEXTURE_3D"},
        {0x84F5, "GL_TEXTURE_RECTANGLE"},
        {0x8513, "GL_TEXTURE_CUBE_MAP"},
        {0x8C1A, "GL_TEXTURE_2D_ARRAY"},
        {0x8C2A, "GL_TEXTURE_BUFFER"},
        {0x9100, "GL_TEXTURE_2D_MULTISAMPLE"},
        {0x9102, "GL_TEXTURE_2D_MULTISAMPLE_ARRAY"},
        // extensions of the fixed-function pipeline
        {0x81FB, "GL_SHARED_TEXTURE_PALETTE_EXT"},
        {0x8522, "GL_REGISTER_COMBINERS_NV"},
        {0x8535, "GL_PER_STAGE_CONSTANTS_NV"},
        {0x86DE, "GL_TEXTURE_SHADER_NV"},
        {0x8870, "GL_FRAGMENT_PROGRAM_NV"},
        {0x8910, "GL_STENCIL_TEST_TWO_SIDE"},
        {0x8920, "GL_FRAGMENT_SHADER_ATI"},
    };

    // Capabilities of extensions a core context may expose: neither table, passed through.
    const NamedMode s_unknown[] =
    {
        {0x8890, "GL_DEPTH_BOUNDS_TEST_EXT"},
        {0x9346, "GL_CONSERVATIVE_RASTERIZATION_NV"},
    };

    template<size_t N> size_t count(const NamedMode (&)[N]) { return N; }

    unsigned int s_checks = 0;
    unsigned int s_failures = 0;

    void check(bool condition, const std::string& what)
    {
        ++s_checks;
        if (!condition)
        {
            ++s_failures;
            std::cout << "FAILED: " << what << std::endl;
        }
    }

    std::string label(const NamedMode& entry)
    {
        std::ostringstream text;
        text << entry.name << " (0x" << std::hex << entry.mode << ")";
        return text.str();
    }

    // Counts the refusal warnings State writes (one line each), and passes nothing else on.
    class RefusalWarnings : public osg::NotifyHandler
    {
    public:
        RefusalWarnings() : _count(0) {}
        virtual void notify(osg::NotifySeverity, const char* message)
        {
            if (std::strstr(message, "refuses fixed-function mode") != 0) ++_count;
            else std::cout << message;
        }
        unsigned int count() const { return _count; }

    private:
        unsigned int _count;
    };

    const osg::State::FixedFunctionRefusal* findRefusal(const std::vector<osg::State::FixedFunctionRefusal>& refusals,
                                                        const NamedMode& entry)
    {
        const std::string what = "mode " + label(entry);
        for (std::vector<osg::State::FixedFunctionRefusal>::const_iterator itr = refusals.begin(); itr != refusals.end(); ++itr)
        {
            if (itr->what == what) return &(*itr);
        }
        return 0;
    }

    // The tables, whatever the build.
    void checkClassification()
    {
        for (size_t i = 0; i < count(s_core); ++i)
        {
            const NamedMode& entry = s_core[i];
            check(osg::State::classifyMode(entry.mode) == osg::State::CORE_MODE, label(entry) + " is a core mode");
            const char* name = osg::State::getModeName(entry.mode);
            check(name != 0 && std::strcmp(name, entry.name) == 0, label(entry) + " is named");
        }
        for (size_t i = 0; i < count(s_fixedFunction); ++i)
        {
            const NamedMode& entry = s_fixedFunction[i];
            check(osg::State::classifyMode(entry.mode) == osg::State::FIXED_FUNCTION_MODE, label(entry) + " is a fixed-function mode");
            check(osg::State::isFixedFunctionMode(entry.mode), label(entry) + " isFixedFunctionMode");
            const char* name = osg::State::getModeName(entry.mode);
            check(name != 0 && std::strcmp(name, entry.name) == 0, label(entry) + " is named");
        }
        for (size_t i = 0; i < count(s_unknown); ++i)
        {
            const NamedMode& entry = s_unknown[i];
            check(osg::State::classifyMode(entry.mode) == osg::State::UNKNOWN_MODE, label(entry) + " is in neither table");
            check(osg::State::getModeName(entry.mode) == 0, label(entry) + " has no name");
        }
    }

#if !defined(OSG_GL_FIXED_FUNCTION_AVAILABLE)
    const unsigned int s_textureUnit = 3;

    // A build without fixed function: the gate in State::ModeMap.
    void checkGate(RefusalWarnings& warnings)
    {
        osg::State::resetFixedFunctionRefusals();
        osg::ref_ptr<osg::State> state = new osg::State;

        for (size_t i = 0; i < count(s_core); ++i)
        {
            check(state->getModeValidity(s_core[i].mode), label(s_core[i]) + " is valid");
        }
        for (size_t i = 0; i < count(s_unknown); ++i)
        {
            check(state->getModeValidity(s_unknown[i].mode), label(s_unknown[i]) + " passes through");
        }
        check(osg::State::getFixedFunctionRefusals().empty(), "core and unknown modes record no refusal");

        // A core mode stays the application's to mark invalid, as upstream (setModeValidity).
        state->setModeValidity(s_core[0].mode, false);
        check(!state->getModeValidity(s_core[0].mode), label(s_core[0]) + " can be marked invalid");
        state->setModeValidity(s_core[0].mode, true);
        check(state->getModeValidity(s_core[0].mode), label(s_core[0]) + " can be marked valid again");

        for (size_t i = 0; i < count(s_fixedFunction); ++i)
        {
            const NamedMode& entry = s_fixedFunction[i];
            check(!state->getModeValidity(entry.mode), label(entry) + " is created invalid");
            state->setModeValidity(entry.mode, true);
            check(!state->getModeValidity(entry.mode), label(entry) + " cannot be made valid");
            check(!state->applyMode(entry.mode, true), label(entry) + " never applies");
            check(!state->getLastAppliedModeValue(entry.mode), label(entry) + " is never recorded as applied");
            check(!state->applyTextureMode(s_textureUnit, entry.mode, true), label(entry) + " never applies on a texture unit");
            check(!state->getLastAppliedTextureModeValue(s_textureUnit, entry.mode), label(entry) + " is never recorded as applied on a texture unit");
        }

        // One record per mode: the global map and the texture unit's map each created it once.
        std::vector<osg::State::FixedFunctionRefusal> refusals = osg::State::getFixedFunctionRefusals();
        check(refusals.size() == count(s_fixedFunction), "one refusal record per fixed-function mode");
        for (size_t i = 0; i < count(s_fixedFunction); ++i)
        {
            const osg::State::FixedFunctionRefusal* refusal = findRefusal(refusals, s_fixedFunction[i]);
            check(refusal != 0 && refusal->count == 2, label(s_fixedFunction[i]) + " refused twice (global map, texture unit)");
        }

        // Another State, and the first one after a release: refused again, counted, not warned again.
        const NamedMode& lighting = s_fixedFunction[3];
        osg::ref_ptr<osg::State> other = new osg::State;
        check(!other->getModeValidity(lighting.mode), label(lighting) + " is invalid in another State");
        state->releaseGLObjects();
        check(!state->getModeValidity(lighting.mode), label(lighting) + " is invalid after releaseGLObjects");
        check(!state->applyTextureMode(s_textureUnit, lighting.mode, true), label(lighting) + " never applies on a texture unit after releaseGLObjects");

        refusals = osg::State::getFixedFunctionRefusals();
        check(refusals.size() == count(s_fixedFunction), "no new record for a mode already refused");
        const osg::State::FixedFunctionRefusal* refusal = findRefusal(refusals, lighting);
        check(refusal != 0 && refusal->count == 5, label(lighting) + " counted in every map that created it");
        check(warnings.count() == count(s_fixedFunction), "each fixed-function mode warned exactly once");

        osg::State::resetFixedFunctionRefusals();
    }
#else
    // A build with fixed function: nothing is refused.
    void checkNoGate()
    {
        osg::State::resetFixedFunctionRefusals();
        osg::ref_ptr<osg::State> state = new osg::State;
        for (size_t i = 0; i < count(s_fixedFunction); ++i)
        {
            check(state->getModeValidity(s_fixedFunction[i].mode), label(s_fixedFunction[i]) + " is valid with fixed function");
        }
        check(osg::State::getFixedFunctionRefusals().empty(), "a build with fixed function records no refusal");
    }
#endif
}

int main(int, char**)
{
    osg::ref_ptr<RefusalWarnings> warnings = new RefusalWarnings;
    osg::setNotifyLevel(osg::WARN);
    osg::setNotifyHandler(warnings.get());

    checkClassification();
#if !defined(OSG_GL_FIXED_FUNCTION_AVAILABLE)
    checkGate(*warnings);
#else
    checkNoGate();
#endif

    std::cout << "osgstatemodes: " << s_checks << " checks, " << s_failures << " failed ("
              << count(s_core) << " core, " << count(s_fixedFunction) << " fixed-function, "
              << count(s_unknown) << " unknown modes)" << std::endl;
    return s_failures == 0 ? 0 : 1;
}
