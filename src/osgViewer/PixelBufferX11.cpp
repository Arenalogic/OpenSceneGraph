/* -*-c++-*- OpenSceneGraph - Copyright (C) 1998-2006 Robert Osfield
 *
 * This library is open source and may be redistributed and/or modified under
 * the terms of the OpenSceneGraph Public License (OSGPL) version 0.0 or
 * (at your option) any later version.  The full license is in LICENSE file
 * included with this distribution, and on the openscenegraph.org website.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * OpenSceneGraph Public License for more details.
*/

/* Note, elements of PixelBufferX11 have used Prodcer/RenderSurface_X11.cpp as both
 * a guide to use of X11/GLX and copiying directly in the case of setBorder().
 * These elements are license under OSGPL as above, with Copyright (C) 2001-2004  Don Burns.
 */

#include <osgViewer/api/X11/PixelBufferX11>
#include <osgViewer/api/X11/GraphicsWindowX11>
#include <osg/GLExtensions>

#include <X11/Xlib.h>

#include <unistd.h>

#ifndef GLX_ARB_create_context
#define GLX_CONTEXT_MAJOR_VERSION_ARB       0x2091
#define GLX_CONTEXT_MINOR_VERSION_ARB       0x2092
#define GLX_CONTEXT_FLAGS_ARB               0x2094
#endif

#ifndef GLX_CONTEXT_PROFILE_MASK_ARB
#define GLX_CONTEXT_PROFILE_MASK_ARB        0x9126
#endif

using namespace osgViewer;

// Return true if the traits explicitly request a context version (3.0 or later), a profile or context flags.
// Explicit requests are created with glXCreateContextAttribsARB and fail rather than fall back to glXCreateContext.
static bool isExplicitContextRequest(const osg::GraphicsContext::Traits* traits)
{
    if (!traits) return false;

    unsigned int major = 1, minor = 0;
    if (traits->getContextVersion(major, minor) && major>=3) return true;

    return traits->glContextFlags!=0 || traits->glContextProfileMask!=0;
}

#ifdef GLX_VERSION_1_3
// Create the context of an explicit request with the GLXFBConfig of visualInfo, the config the pbuffer is created with.
static GLXContext createContextAttribs(Display* display, int screen, XVisualInfo* visualInfo, GLXContext sharedContext, const osg::GraphicsContext::Traits* traits)
{
    typedef GLXContext (*glXCreateContextAttribsARBProc)(Display*, GLXFBConfig, GLXContext, Bool, const int*);

    const char* extensions = glXQueryExtensionsString(display, screen);
    glXCreateContextAttribsARBProc glXCreateContextAttribsARB = 0;
    if (extensions && osg::isExtensionInExtensionString("GLX_ARB_create_context", extensions))
    {
        glXCreateContextAttribsARB = (glXCreateContextAttribsARBProc) glXGetProcAddress((const GLubyte*)"glXCreateContextAttribsARB");
    }

    if (!glXCreateContextAttribsARB)
    {
        OSG_WARN<<"Error: PixelBufferX11, glXCreateContextAttribsARB not available."<<std::endl;
        return 0;
    }

    GLXFBConfig fbconfig = 0;
    int nelements = 0;
    GLXFBConfig *fbconfigs = glXGetFBConfigs( display, screen, &nelements );
    for ( int i = 0; i < nelements && !fbconfig; ++i )
    {
        int visual_id;
        if ( glXGetFBConfigAttrib( display, fbconfigs[i], GLX_VISUAL_ID, &visual_id ) == 0 &&
             (unsigned int)visual_id == visualInfo->visualid )
        {
            fbconfig = fbconfigs[i];
        }
    }
    if (fbconfigs) XFree( fbconfigs );

    if (!fbconfig)
    {
        OSG_WARN<<"Error: PixelBufferX11, no GLXFBConfig matches the chosen visual."<<std::endl;
        return 0;
    }

    unsigned int major = 1, minor = 0;
    traits->getContextVersion(major, minor);

    std::vector<int> attributes;
    attributes.push_back(GLX_CONTEXT_MAJOR_VERSION_ARB);
    attributes.push_back(major);
    attributes.push_back(GLX_CONTEXT_MINOR_VERSION_ARB);
    attributes.push_back(minor);
    if (traits->glContextProfileMask != 0)
    {
        attributes.push_back(GLX_CONTEXT_PROFILE_MASK_ARB);
        attributes.push_back(traits->glContextProfileMask);
    }
    if (traits->glContextFlags != 0)
    {
        attributes.push_back(GLX_CONTEXT_FLAGS_ARB);
        attributes.push_back(traits->glContextFlags);
    }
    attributes.push_back(None);

    GLXContext context = glXCreateContextAttribsARB( display, fbconfig, sharedContext, True, &attributes.front() );
    XSync( display, False );

    if (!context)
    {
        OSG_WARN<<"Error: PixelBufferX11, glXCreateContextAttribsARB failed for version "<<traits->glContextVersion
                <<", profile mask 0x"<<std::hex<<traits->glContextProfileMask
                <<", flags 0x"<<traits->glContextFlags<<std::dec
                <<"; no fallback to a legacy context."<<std::endl;
    }
    return context;
}
#endif

PixelBufferX11::PixelBufferX11(osg::GraphicsContext::Traits* traits)
  : _valid(false),
    _pbuffer(0),
    _visualInfo(0),
    _initialized(false),
    _realized(false),
    _useGLX1_3(false),
    _useSGIX(false)
#ifdef GLX_SGIX_pbuffer
    ,_glXCreateGLXPbufferSGIX(NULL),
    _glXDestroyGLXPbufferSGIX(NULL),
    _glXQueryGLXPbufferSGIX(NULL),
    _glXGetFBConfigFromVisualSGIX(NULL)
#endif
    {
    _traits = traits;

    init();

    if (valid())
    {
        setState( new osg::State );
        getState()->setGraphicsContext(this);

        if (_traits.valid() && _traits->sharedContext.valid() && !_traits->uniqueContextId)
        {
            getState()->setContextID( _traits->sharedContext->getState()->getContextID() );
            incrementContextIDUsageCount( getState()->getContextID() );
        }
        else
        {
            getState()->setContextID( osg::GraphicsContext::createNewContextID() );
        }

    }
}

PixelBufferX11::~PixelBufferX11()
{
    close(true);
}

#if defined(GLX_VERSION_1_3) || defined(GLX_SGIX_pbuffer)
bool PixelBufferX11::createVisualInfo()
{
    typedef std::vector<int> Attributes;
    Attributes attributes;

    attributes.push_back(GLX_USE_GL);

    attributes.push_back(GLX_RGBA);

    if (_traits->doubleBuffer) attributes.push_back(GLX_DOUBLEBUFFER);

    attributes.push_back(GLX_RED_SIZE); attributes.push_back(_traits->red);
    attributes.push_back(GLX_GREEN_SIZE); attributes.push_back(_traits->green);
    attributes.push_back(GLX_BLUE_SIZE); attributes.push_back(_traits->blue);
    attributes.push_back(GLX_DEPTH_SIZE); attributes.push_back(_traits->depth);

    if (_traits->alpha) { attributes.push_back(GLX_ALPHA_SIZE); attributes.push_back(_traits->alpha); }

    if (_traits->stencil) { attributes.push_back(GLX_STENCIL_SIZE); attributes.push_back(_traits->stencil); }

#if defined(GLX_SAMPLE_BUFFERS) && defined (GLX_SAMPLES)

    if (_traits->sampleBuffers) { attributes.push_back(GLX_SAMPLE_BUFFERS); attributes.push_back(_traits->sampleBuffers); }
    if (_traits->sampleBuffers) { attributes.push_back(GLX_SAMPLES); attributes.push_back(_traits->samples); }

#endif
    // TODO
    //  GLX_AUX_BUFFERS
    //  GLX_ACCUM_RED_SIZE
    //  GLX_ACCUM_GREEN_SIZE

    attributes.push_back(None);

    _visualInfo = glXChooseVisual( _display, _traits->screenNum, &(attributes.front()) );

    return _visualInfo != 0;
}

void PixelBufferX11::init()
{
    if (_initialized) return;

    if (!_traits)
    {
        _valid = false;
        return;
    }

    if (_traits->target != 0)
    {
        // we don't support Pbuffer render to texture under GLX.
        _valid = false;
        return;
    }


    _display = XOpenDisplay(_traits->displayName().c_str());

    unsigned int screen = _traits->screenNum;

    if (!_display)
    {
        OSG_NOTICE<<"Error: Unable to open display \"" << XDisplayName(_traits->displayName().c_str()) << "\"."<<std::endl;
        _valid = false;
        return;
    }

    // Query for GLX extension
    int errorBase, eventBase;
    if( glXQueryExtension( _display, &errorBase, &eventBase)  == False )
    {
        OSG_NOTICE<<"Error: " << XDisplayName(_traits->displayName().c_str()) <<" has no GLX extension." << std::endl;

        XCloseDisplay( _display );
        _display = 0;
        _valid = false;
        return;
    }

    // OSG_NOTICE<<"GLX extension, errorBase="<<errorBase<<" eventBase="<<eventBase<<std::endl;

    int major, minor;
    if (glXQueryVersion(_display, &major, &minor) == False)
    {
        OSG_NOTICE << "Error: " << XDisplayName(_traits->displayName().c_str())
                                 << " can not query GLX version." << std::endl;
        XCloseDisplay( _display );
        _display = 0;
        _valid = false;
        return;
    }

    // Just be paranoid, if we are older than 1.1, we cannot even call glxQueryExtensionString
    if (major < 1 || (1 == major && minor < 1))
    {
        OSG_NOTICE << "Error: " << XDisplayName(_traits->displayName().c_str())
                                 << " GLX version " << major << "." << minor << " is too old." << std::endl;
        XCloseDisplay( _display );
        _display = 0;
        _valid = false;
        return;
    }

    bool haveGLX1_3 = false;
    bool haveSGIX_pbuffer = false;

    // We need to have at least GLX 1.3 to use getFBConfigFromVisual and glXCreatePbuffer
    if (1 < major || (1 == major && 3 <= minor))
    {
        haveGLX1_3 = true;
    }

#if defined(GLX_VERSION_1_1) && defined(GLX_SGIX_pbuffer)
    // We need at least GLX 1.1 for glXQueryExtensionsString
    if (!haveGLX1_3 && 1 <= minor)
    {
        const char *extensions = glXQueryExtensionsString(_display, screen);
        haveSGIX_pbuffer = osg::isExtensionInExtensionString("GLX_SGIX_pbuffer", extensions)
           && osg::isExtensionInExtensionString("GLX_SGIX_fbconfig", extensions);

        if (haveSGIX_pbuffer)
        {
            osg::setGLExtensionFuncPtr(_glXCreateGLXPbufferSGIX, "glXDestroyGLXPbufferSGIX");
            osg::setGLExtensionFuncPtr(_glXDestroyGLXPbufferSGIX, "glXDestroyGLXPbufferSGIX");
            osg::setGLExtensionFuncPtr(_glXQueryGLXPbufferSGIX, "glXDestroyGLXPbufferSGIX");
            osg::setGLExtensionFuncPtr(_glXGetFBConfigFromVisualSGIX, "glXGetFBConfigFromVisualSGIX");
            if (_glXCreateGLXPbufferSGIX == NULL ||
                _glXDestroyGLXPbufferSGIX == NULL ||
                _glXQueryGLXPbufferSGIX == NULL ||
                _glXGetFBConfigFromVisualSGIX == NULL) {
                haveSGIX_pbuffer = false;
            }
        }
    }
#endif

    if (!haveGLX1_3 && !haveSGIX_pbuffer)
    {
        OSG_NOTICE << "Error: " << XDisplayName(_traits->displayName().c_str())
                                 << " no Pbuffer support in GLX available." << std::endl;
        XCloseDisplay( _display );
        _display = 0;
        _valid = false;
        return;
    }


    if (!createVisualInfo())
    {
        _traits->red /= 2;
        _traits->green /= 2;
        _traits->blue /= 2;
        _traits->alpha /= 2;
        _traits->depth /= 2;

        OSG_INFO<<"Relaxing traits"<<std::endl;

        if (!createVisualInfo())
        {
            OSG_NOTICE<<"Error: Not able to create requested visual." << std::endl;
            XCloseDisplay( _display );
            _display = 0;
            _valid = false;
            return;
        }
    }

    // get any shared GLX contexts
    GraphicsHandleX11* graphicsHandleX11 = dynamic_cast<GraphicsHandleX11*>(_traits->sharedContext.get());
    Context sharedContext = graphicsHandleX11 ? graphicsHandleX11->getContext() : 0;

    if (isExplicitContextRequest(_traits.get()))
    {
#ifdef GLX_VERSION_1_3
        if (_traits->sharedContext.valid() && !sharedContext)
        {
            OSG_WARN<<"Error: PixelBufferX11, the shared context is not a realized GLX context."<<std::endl;
        }
        else if (!haveGLX1_3)
        {
            OSG_WARN<<"Error: PixelBufferX11, explicit context requests need GLX 1.3."<<std::endl;
        }
        else
        {
            _context = createContextAttribs( _display, screen, _visualInfo, sharedContext, _traits.get() );
        }
#else
        OSG_WARN<<"Error: PixelBufferX11, explicit context requests need GLX 1.3."<<std::endl;
#endif
    }
    else
    {
        _context = glXCreateContext( _display, _visualInfo, sharedContext, True );
    }

    if (!_context)
    {
        OSG_NOTICE<<"Error: Unable to create OpenGL graphics context."<<std::endl;
        XCloseDisplay( _display );
        _display = 0;
        _valid = false;
        return;
    }

#ifdef GLX_VERSION_1_3
    // First try the regular glx extension if we have a new enough version available.
    if (haveGLX1_3)
    {
        int nelements;
        GLXFBConfig *fbconfigs = glXGetFBConfigs( _display, screen, &nelements );
        for ( int i = 0; i < nelements; ++i )
        {
            int visual_id;
            if ( glXGetFBConfigAttrib( _display, fbconfigs[i], GLX_VISUAL_ID, &visual_id ) == 0 )
            {
                if ( !_pbuffer && (unsigned int)visual_id == _visualInfo->visualid )
                {
                    typedef std::vector <int> AttributeList;

                    AttributeList attributes;
                    attributes.push_back( GLX_PBUFFER_WIDTH );
                    attributes.push_back( _traits->width );
                    attributes.push_back( GLX_PBUFFER_HEIGHT );
                    attributes.push_back( _traits->height );
                    attributes.push_back( GLX_LARGEST_PBUFFER );
                    attributes.push_back( GL_TRUE );
                    attributes.push_back( 0L );

                    _pbuffer = glXCreatePbuffer(_display, fbconfigs[i], &attributes.front() );
                    _useGLX1_3 = true;
                }
            }
        }
        if (_pbuffer)
        {
            int iWidth = 0;
            int iHeight = 0;
            glXQueryDrawable(_display, _pbuffer, GLX_WIDTH  , (unsigned int *)&iWidth);
            glXQueryDrawable(_display, _pbuffer, GLX_HEIGHT , (unsigned int *)&iHeight);

            if (_traits->width != iWidth || _traits->height != iHeight)
            {
                OSG_NOTICE << "PixelBufferX11::init(), pbuffer created with different size then requested" << std::endl;
                OSG_NOTICE << "\tRequested size (" << _traits->width << "," << _traits->height << ")" << std::endl;
                OSG_NOTICE << "\tPbuffer size (" << iWidth << "," << iHeight << ")" << std::endl;
                _traits->width  = iWidth;
                _traits->height = iHeight;
            }
        }
        XFree( fbconfigs );
    }
#endif

#ifdef GLX_SGIX_pbuffer
    // If we still have no pbuffer but a capable display with the SGIX extension, try to use that
    if (!_pbuffer && haveSGIX_pbuffer)
    {
        GLXFBConfigSGIX fbconfig = _glXGetFBConfigFromVisualSGIX( _display, _visualInfo );
        typedef std::vector <int> AttributeList;

        AttributeList attributes;
        attributes.push_back( GLX_LARGEST_PBUFFER_SGIX );
        attributes.push_back( GL_TRUE );
        attributes.push_back( 0L );

        _pbuffer = _glXCreateGLXPbufferSGIX(_display, fbconfig, _traits->width, _traits->height,  &attributes.front() );
        if (_pbuffer)
        {
            _useSGIX = true;
            int iWidth = 0;
            int iHeight = 0;
            _glXQueryGLXPbufferSGIX(_display, _pbuffer, GLX_WIDTH_SGIX , (unsigned int *)&iWidth);
            _glXQueryGLXPbufferSGIX(_display, _pbuffer, GLX_HEIGHT_SGIX, (unsigned int *)&iHeight);
                                                                                        
            if (_traits->width != iWidth || _traits->height != iHeight)
            {
                OSG_NOTICE << "PixelBufferX11::init(), SGIX_pbuffer created with different size then requested" << std::endl;
                OSG_NOTICE << "\tRequested size (" << _traits->width << "," << _traits->height << ")" << std::endl;
                OSG_NOTICE << "\tPbuffer size (" << iWidth << "," << iHeight << ")" << std::endl;
                _traits->width =  iWidth;
                _traits->height = iHeight;
            }
        }
        XFree( fbconfig );
    }
#endif

    if (!_pbuffer)
    {
        OSG_NOTICE<<"Error: Unable to create pbuffer."<<std::endl;
        XCloseDisplay( _display );
        _display = 0;
        _context = 0;
        _valid = false;
        return;
    }


    XFlush( _display );
    XSync( _display, 0 );

    _valid = true;
    _initialized = true;
}

void PixelBufferX11::closeImplementation()
{
    // OSG_NOTICE<<"Closing PixelBufferX11"<<std::endl;
    if (_display)
    {
        if (_context)
        {
            glXDestroyContext(_display, _context );
        }

        if (_pbuffer)
        {
            if (_useGLX1_3)
            {
#ifdef GLX_VERSION_1_3
                glXDestroyPbuffer(_display, _pbuffer);
#endif
            }
            else
            {
#ifdef GLX_SGIX_pbuffer
                if (_useSGIX)
                {
                    _glXDestroyGLXPbufferSGIX(_display, _pbuffer);
                }
#endif
            }
        }

        XFlush( _display );
        XSync( _display,0 );
    }

    _pbuffer = 0;
    _context = 0;

    if (_visualInfo)
    {
        XFree(_visualInfo);
        _visualInfo = 0;
    }

    if (_display)
    {
        XCloseDisplay( _display );
        _display = 0;
    }

    _initialized = false;
    _realized = false;
    _valid = false;
}

#else

// fallback for non GLX1.3 versions where pbuffers are not supported.
// note, this makes the rest of the pbuffer code a non op as init is false;
bool PixelBufferX11::createVisualInfo()
{
    return false;
}

void PixelBufferX11::init()
{
}

void PixelBufferX11::closeImplementation()
{
    // OSG_NOTICE<<"Closing PixelBufferX11"<<std::endl;
    _pbuffer = 0;
    _context = 0;
    _initialized = false;
    _realized = false;
    _valid = false;
}

#endif

bool PixelBufferX11::realizeImplementation()
{
    if (_realized)
    {
        OSG_NOTICE<<"PixelBufferX11::realizeImplementation() Already realized"<<std::endl;
        return true;
    }

    if (!_initialized) init();

    if (!_initialized) return false;

    _realized = true;

    return true;
}

bool PixelBufferX11::makeCurrentImplementation()
{
    if (!_realized)
    {
        OSG_NOTICE<<"Warning: GraphicsWindow not realized, cannot do makeCurrent."<<std::endl;
        return false;
    }

    // OSG_NOTICE<<"PixelBufferX11::makeCurrentImplementation "<<this<<" "<<OpenThreads::Thread::CurrentThread()<<std::endl;

    #ifdef OSG_USE_EGL
        return eglMakeCurrent(_display, _pbuffer, _pbuffer, _context)==EGL_TRUE;
    #else
        return glXMakeCurrent( _display, _pbuffer, _context )==True;
    #endif
}

bool PixelBufferX11::makeContextCurrentImplementation(osg::GraphicsContext* /*readContext*/)
{
    // OSG_NOTICE<<"PixelBufferX11::makeContextCurrentImplementation() not implementation yet."<<std::endl;
    return makeCurrentImplementation();
}


bool PixelBufferX11::releaseContextImplementation()
{
    if (!_realized)
    {
        OSG_NOTICE<<"Warning: GraphicsWindow not realized, cannot do makeCurrent."<<std::endl;
        return false;
    }

    // OSG_NOTICE<<"PixelBufferX11::releaseContextImplementation() "<<this<<" "<<OpenThreads::Thread::CurrentThread()<<std::endl;

    #ifdef OSG_USE_EGL
        return eglMakeCurrent( _display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT )==EGL_TRUE;
    #else
        return glXMakeCurrent( _display, None, NULL )==True;
    #endif
}


void PixelBufferX11::bindPBufferToTextureImplementation(GLenum /*buffer*/)
{
    OSG_NOTICE<<"PixelBufferX11::bindPBufferToTextureImplementation() not implementation yet."<<std::endl;
}

void PixelBufferX11::swapBuffersImplementation()
{
    if (!_realized) return;

    // OSG_NOTICE<<"PixelBufferX11::swapBuffersImplementation "<<this<<" "<<OpenThreads::Thread::CurrentThread()<<std::endl;

    #ifdef OSG_USE_EGL
        eglSwapBuffers( _display, _pbuffer );
    #else
        glXSwapBuffers( _display, _pbuffer );
    #endif
}
