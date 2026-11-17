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
#include <osg/GLExtensions>
#include <osg/Image>
#include <osg/Texture>
#include <osg/State>
#include <osg/Notify>
#include <osg/GLU>
#include <osg/Timer>
#include <osg/ApplicationUsage>
#include <osg/FrameBufferObject>
#include <osg/TextureRectangle>
#include <osg/Texture1D>
#include <osg/ContextData>

#include <OpenThreads/ScopedLock>
#include <OpenThreads/Mutex>

#ifndef GL_TEXTURE_WRAP_R
#define GL_TEXTURE_WRAP_R                 0x8072
#endif

#ifndef GL_TEXTURE_MAX_LEVEL
#define GL_TEXTURE_MAX_LEVEL              0x813D
#endif


#ifndef GL_UNPACK_CLIENT_STORAGE_APPLE
#define GL_UNPACK_CLIENT_STORAGE_APPLE    0x85B2
#endif

#ifndef GL_APPLE_vertex_array_range
#define GL_VERTEX_ARRAY_RANGE_APPLE       0x851D
#define GL_VERTEX_ARRAY_RANGE_LENGTH_APPLE 0x851E
#define GL_VERTEX_ARRAY_STORAGE_HINT_APPLE 0x851F
#define GL_VERTEX_ARRAY_RANGE_POINTER_APPLE 0x8521
#define GL_STORAGE_CACHED_APPLE           0x85BE
#define GL_STORAGE_SHARED_APPLE           0x85BF
#endif

#ifndef GL_RGB565
#define GL_RGB565                         0x8D62
#endif

#ifndef GL_COMPRESSED_RED
#define GL_COMPRESSED_RED                 0x8225
#define GL_COMPRESSED_RG                  0x8226
#endif

#if 0
    #define CHECK_CONSISTENCY checkConsistency();
#else
    #define CHECK_CONSISTENCY
#endif

namespace osg {

ApplicationUsageProxy Texture_e0(ApplicationUsage::ENVIRONMENTAL_VARIABLE,"OSG_MAX_TEXTURE_SIZE","Set the maximum size of textures.");
ApplicationUsageProxy Texture_e1(ApplicationUsage::ENVIRONMENTAL_VARIABLE,"OSG_GL_TEXTURE_STORAGE","ON|OFF or ENABLE|DISABLE, Enables/disables usage of glTexStorage for textures where supported, default is ENABLED.");

struct InternalPixelRelations
{
    GLenum sizedInternalFormat;
    GLint internalFormat;
    GLenum type;
};


InternalPixelRelations sizedInternalFormats[] = {

      { GL_R8UI                                , GL_RED_INTEGER_EXT  , GL_UNSIGNED_BYTE                             }
    , { GL_R8I                                 , GL_RED_INTEGER_EXT  , GL_BYTE                                      }
    , { GL_R16UI                               , GL_RED_INTEGER_EXT  , GL_UNSIGNED_SHORT                            }
    , { GL_R16I                                , GL_RED_INTEGER_EXT  , GL_SHORT                                     }
    , { GL_R32UI                               , GL_RED_INTEGER_EXT  , GL_UNSIGNED_INT                              }
    , { GL_R32I                                , GL_RED_INTEGER_EXT  , GL_INT                                       }

    , { GL_RG8UI                               , GL_RG_INTEGER       , GL_UNSIGNED_BYTE                             }
    , { GL_RG8I                                , GL_RG_INTEGER       , GL_BYTE                                      }
    , { GL_RG16UI                              , GL_RG_INTEGER       , GL_UNSIGNED_SHORT                            }
    , { GL_RG16I                               , GL_RG_INTEGER       , GL_SHORT                                     }
    , { GL_RG32UI                              , GL_RG_INTEGER       , GL_UNSIGNED_INT                              }
    , { GL_RG32I                               , GL_RG_INTEGER       , GL_INT                                       }

    , { GL_RGB8UI_EXT                          , GL_RGB_INTEGER_EXT  , GL_UNSIGNED_BYTE                             }
    , { GL_RGB8I_EXT                           , GL_RGB_INTEGER_EXT  , GL_BYTE                                      }
    , { GL_RGB16UI_EXT                         , GL_RGB_INTEGER_EXT  , GL_UNSIGNED_SHORT                            }
    , { GL_RGB16I_EXT                          , GL_RGB_INTEGER_EXT  , GL_SHORT                                     }
    , { GL_RGB32UI_EXT                         , GL_RGB_INTEGER_EXT  , GL_UNSIGNED_INT                              }
    , { GL_RGB32I_EXT                          , GL_RGB_INTEGER_EXT  , GL_INT                                       }

    , { GL_RGBA8UI_EXT                         , GL_RGBA_INTEGER_EXT , GL_UNSIGNED_BYTE                             }
    , { GL_RGBA8I_EXT                          , GL_RGBA_INTEGER_EXT , GL_BYTE                                      }
 // , { GL_RGB10_A2UI_EXT                      , GL_RGBA_INTEGER_EXT , GL_UNSIGNED_INT_2_10_10_10_REV               }
    , { GL_RGBA16UI_EXT                        , GL_RGBA_INTEGER_EXT , GL_UNSIGNED_SHORT                            }
    , { GL_RGBA16I_EXT                         , GL_RGBA_INTEGER_EXT , GL_SHORT                                     }
    , { GL_RGBA32I_EXT                         , GL_RGBA_INTEGER_EXT , GL_INT                                       }
    , { GL_RGBA32UI_EXT                        , GL_RGBA_INTEGER_EXT , GL_UNSIGNED_INT                              }

    , { GL_R8                                  , GL_RED              , GL_UNSIGNED_BYTE                             }
    , { GL_R16F                                , GL_RED              , GL_HALF_FLOAT                                }
    , { GL_R32F                                , GL_RED              , GL_FLOAT                                     }
    , { GL_R16F                                , GL_RED              , GL_FLOAT                                     }
    , { GL_RG8                                 , GL_RG               , GL_UNSIGNED_BYTE                             }
    , { GL_RG16F                               , GL_RG               , GL_HALF_FLOAT                                }
    , { GL_RG16F                               , GL_RG               , GL_FLOAT                                     }
    , { GL_RG32F                               , GL_RG               , GL_FLOAT                                     }
 // , ( GL_RGBA2                               , GL_RGB              , UNKNOWN                                      )
    , { GL_R3_G3_B2                            , GL_RGB              , GL_UNSIGNED_BYTE_3_3_2                       }
    , { GL_R3_G3_B2                            , GL_RGB              , GL_UNSIGNED_BYTE_2_3_3_REV                   }
    , { GL_RGB4                                , GL_RGB              , GL_UNSIGNED_SHORT_4_4_4_4                    }
    , { GL_RGB4                                , GL_RGB              , GL_UNSIGNED_SHORT_4_4_4_4_REV                }
    , { GL_RGB5                                , GL_RGB              , GL_UNSIGNED_SHORT_5_5_5_1                    }
    , { GL_RGB5                                , GL_RGB              , GL_UNSIGNED_SHORT_1_5_5_5_REV                }
    , { GL_RGB8                                , GL_RGB              , GL_UNSIGNED_BYTE                             }
    , { GL_RGB565                              , GL_RGB              , GL_UNSIGNED_BYTE                             }
    , { GL_RGB565                              , GL_RGB              , GL_UNSIGNED_SHORT_5_6_5                      }
    , { GL_RGB565                              , GL_RGB              , GL_UNSIGNED_SHORT_5_6_5_REV                  }
 // , { GL_RGB9_E5                             , GL_RGB              , GL_UNSIGNED_INT_9_9_9_5,                     }
 // , { GL_RGB9_E5                             , GL_RGB              , GL_UNSIGNED_INT_5_9_9_9_REV,                 }
 // , { GL_RGB9_E5                             , GL_RGB              , GL_HALF_FLOAT                                }
 // , { GL_RGB9_E5                             , GL_RGB              , GL_FLOAT                                     }
 // , { GL_R11F_G11F_B10F                      , GL_RGB              , GL_UNSIGNED_INT_10F_11F_11F_REV              }
 // , { GL_R11F_G11F_B10F                      , GL_RGB              , GL_HALF_FLOAT                                }
 // , { GL_R11F_G11F_B10F                      , GL_RGB              , GL_FLOAT                                     }
    , { GL_RGB10                               , GL_RGB              , GL_UNSIGNED_INT_2_10_10_10_REV               }
    , { GL_RGB10                               , GL_RGB              , GL_UNSIGNED_INT_10_10_10_2                   }
    , { GL_RGB12                               , GL_RGB              , GL_UNSIGNED_SHORT                            }
    , { GL_RGB16F_ARB                          , GL_RGB              , GL_HALF_FLOAT                                }
    , { GL_RGB32F_ARB                          , GL_RGB              , GL_FLOAT                                     }
    , { GL_RGB16F_ARB                          , GL_RGB              , GL_FLOAT                                     }


    , { GL_RGBA8                               , GL_RGBA             , GL_UNSIGNED_BYTE                             }
    , { GL_RGBA16                              , GL_RGBA             , GL_UNSIGNED_SHORT                            }
    , { GL_RGB10_A2                            , GL_RGBA             , GL_UNSIGNED_INT_10_10_10_2                   }
    , { GL_RGB10_A2                            , GL_RGBA             , GL_UNSIGNED_INT_2_10_10_10_REV               }
    , { GL_RGBA12                              , GL_RGBA             , GL_UNSIGNED_SHORT                            }
    , { GL_RGBA4                               , GL_RGBA             , GL_UNSIGNED_BYTE                             }
    , { GL_RGBA4                               , GL_RGBA             , GL_UNSIGNED_SHORT_4_4_4_4                    }
    , { GL_RGBA4                               , GL_RGBA             , GL_UNSIGNED_SHORT_4_4_4_4_REV                }
    , { GL_RGB5_A1                             , GL_RGBA             , GL_UNSIGNED_BYTE                             }
    , { GL_RGB5_A1                             , GL_RGBA             , GL_UNSIGNED_SHORT_5_5_5_1                    }
    , { GL_RGB5_A1                             , GL_RGBA             , GL_UNSIGNED_SHORT_1_5_5_5_REV                }
    , { GL_RGB5_A1                             , GL_RGBA             , GL_UNSIGNED_INT_10_10_10_2                   }
    , { GL_RGB5_A1                             , GL_RGBA             , GL_UNSIGNED_INT_2_10_10_10_REV               }
 // , { GL_RGBA16F                             , GL_RGBA             , GL_HALF_FLOAT                                }
    , { GL_RGBA16F_ARB                         , GL_RGBA             , GL_FLOAT                                     }
    , { GL_RGBA32F_ARB                         , GL_RGBA             , GL_FLOAT                                     }

    , { GL_SRGB8                               , GL_RGB              , GL_UNSIGNED_BYTE                             }
    , { GL_SRGB8_ALPHA8                        , GL_RGBA             , GL_UNSIGNED_BYTE                             }

    , { GL_R8_SNORM                            , GL_RED              , GL_BYTE                                      }
    , { GL_R16_SNORM                           , GL_RED              , GL_SHORT                                     }
    , { GL_RG8_SNORM                           , GL_RG               , GL_BYTE                                      }
    , { GL_RG16_SNORM                          , GL_RG               , GL_SHORT                                     }
    , { GL_RGB8_SNORM                          , GL_RGB              , GL_BYTE                                      }
    , { GL_RGB16_SNORM                         , GL_RGB              , GL_SHORT                                     }
    , { GL_RGBA8_SNORM                         , GL_RGBA             , GL_BYTE                                      }
};

InternalPixelRelations sizedDepthAndStencilInternalFormats[] = {
      { GL_DEPTH_COMPONENT16                   , GL_DEPTH_COMPONENT  , GL_UNSIGNED_SHORT                            }
    , { GL_DEPTH_COMPONENT16                   , GL_DEPTH_COMPONENT  , GL_UNSIGNED_INT                              }
    , { GL_DEPTH_COMPONENT24                   , GL_DEPTH_COMPONENT  , GL_UNSIGNED_INT                              }
    , { GL_DEPTH_COMPONENT32                   , GL_DEPTH_COMPONENT  , GL_UNSIGNED_INT                              }
    , { GL_DEPTH_COMPONENT32F                  , GL_DEPTH_COMPONENT  , GL_FLOAT                                     }
 // , { GL_DEPTH24_STENCIL8                    , GL_DEPTH_STENCIL    , GL_UNSIGNED_INT_24_8                         }
 // , { GL_DEPTH32F_STENCIL8                   , GL_DEPTH_STENCIL    , GL_FLOAT_32_UNSIGNED_INT_24_8_REV            }
};

InternalPixelRelations compressedInternalFormats[] = {
 // , { GL_COMPRESSED_RED                          , GL_RED          , GL_COMPRESSED_RED                            }
 // , { GL_COMPRESSED_RG                           , GL_RG           , GL_COMPRESSED_RG                             }
      { GL_COMPRESSED_RGB                          , GL_RGB          , GL_COMPRESSED_RGB                            }
    , { GL_COMPRESSED_RGBA                         , GL_RGBA         , GL_COMPRESSED_RGBA                           }
    , { GL_COMPRESSED_SRGB                         , GL_RGB          , GL_COMPRESSED_SRGB                           }
    , { GL_COMPRESSED_SRGB_ALPHA                   , GL_RGBA         , GL_COMPRESSED_SRGB_ALPHA                     }
    , { GL_COMPRESSED_RED_RGTC1_EXT                , GL_RED          , GL_COMPRESSED_RED_RGTC1_EXT                  }
    , { GL_COMPRESSED_SIGNED_RED_RGTC1_EXT         , GL_RED          , GL_COMPRESSED_SIGNED_RED_RGTC1_EXT           }
 // , { GL_COMPRESSED_RG_RGTC2                     , GL_RG           , GL_COMPRESSED_RG_RGTC2                       }
 // , { GL_COMPRESSED_SIGNED_RG_RGTC2              , GL_RG           , GL_COMPRESSED_SIGNED_RG_RGTC2                }
 // , { GL_COMPRESSED_RGBA_BPTC_UNORM              , GL_RGBA         , GL_COMPRESSED_RGBA_BPTC_UNORM                }
 // , { GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM        , GL_RGBA         , GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM          }
 // , { GL_COMPRESSED_RGB_BPTC_SIGNED_FLOAT        , GL_RGB          , GL_COMPRESSED_RGB_BPTC_SIGNED_FLOAT          }
 // , { GL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT      , GL_RGB          , GL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT        }

    , { GL_COMPRESSED_RGB_S3TC_DXT1_EXT            , GL_RGB          , GL_COMPRESSED_RGB_S3TC_DXT1_EXT              }
    , { GL_COMPRESSED_RGBA_S3TC_DXT1_EXT           , GL_RGBA         , GL_COMPRESSED_RGBA_S3TC_DXT1_EXT             }
    , { GL_COMPRESSED_RGBA_S3TC_DXT3_EXT           , GL_RGBA         , GL_COMPRESSED_RGBA_S3TC_DXT3_EXT             }
    , { GL_COMPRESSED_RGBA_S3TC_DXT5_EXT           , GL_RGBA         , GL_COMPRESSED_RGBA_S3TC_DXT5_EXT             }

    , { GL_COMPRESSED_SRGB_S3TC_DXT1_EXT           , GL_RGB          , GL_COMPRESSED_SRGB_S3TC_DXT1_EXT             }
    , { GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT     , GL_RGBA         , GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT       }
    , { GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT     , GL_RGBA         , GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT       }
    , { GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT     , GL_RGBA         , GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT       }

    , { GL_COMPRESSED_RGBA_ASTC_4x4_KHR            , GL_RGBA         , GL_COMPRESSED_RGBA_ASTC_4x4_KHR              }
    , { GL_COMPRESSED_RGBA_ASTC_5x4_KHR            , GL_RGBA         , GL_COMPRESSED_RGBA_ASTC_5x4_KHR              }
    , { GL_COMPRESSED_RGBA_ASTC_5x5_KHR            , GL_RGBA         , GL_COMPRESSED_RGBA_ASTC_5x5_KHR              }
    , { GL_COMPRESSED_RGBA_ASTC_6x5_KHR            , GL_RGBA         , GL_COMPRESSED_RGBA_ASTC_6x5_KHR              }
    , { GL_COMPRESSED_RGBA_ASTC_6x6_KHR            , GL_RGBA         , GL_COMPRESSED_RGBA_ASTC_6x6_KHR              }
    , { GL_COMPRESSED_RGBA_ASTC_8x5_KHR            , GL_RGBA         , GL_COMPRESSED_RGBA_ASTC_8x5_KHR              }
    , { GL_COMPRESSED_RGBA_ASTC_8x6_KHR            , GL_RGBA         , GL_COMPRESSED_RGBA_ASTC_8x6_KHR              }
    , { GL_COMPRESSED_RGBA_ASTC_8x8_KHR            , GL_RGBA         , GL_COMPRESSED_RGBA_ASTC_8x8_KHR              }
    , { GL_COMPRESSED_RGBA_ASTC_10x5_KHR           , GL_RGBA         , GL_COMPRESSED_RGBA_ASTC_10x5_KHR             }
    , { GL_COMPRESSED_RGBA_ASTC_10x6_KHR           , GL_RGBA         , GL_COMPRESSED_RGBA_ASTC_10x6_KHR             }
    , { GL_COMPRESSED_RGBA_ASTC_10x8_KHR           , GL_RGBA         , GL_COMPRESSED_RGBA_ASTC_10x8_KHR             }
    , { GL_COMPRESSED_RGBA_ASTC_10x10_KHR          , GL_RGBA         , GL_COMPRESSED_RGBA_ASTC_10x10_KHR            }
    , { GL_COMPRESSED_RGBA_ASTC_12x10_KHR          , GL_RGBA         , GL_COMPRESSED_RGBA_ASTC_12x10_KHR            }
    , { GL_COMPRESSED_RGBA_ASTC_12x12_KHR          , GL_RGBA         , GL_COMPRESSED_RGBA_ASTC_12x12_KHR            }

    , { GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4_KHR    , GL_RGBA         , GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4_KHR      }
    , { GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x4_KHR    , GL_RGBA         , GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x4_KHR      }
    , { GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x5_KHR    , GL_RGBA         , GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x5_KHR      }
    , { GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x5_KHR    , GL_RGBA         , GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x5_KHR      }
    , { GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x6_KHR    , GL_RGBA         , GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x6_KHR      }
    , { GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x5_KHR    , GL_RGBA         , GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x5_KHR      }
    , { GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x6_KHR    , GL_RGBA         , GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x6_KHR      }
    , { GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x8_KHR    , GL_RGBA         , GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x8_KHR      }
    , { GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x5_KHR   , GL_RGBA         , GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x5_KHR     }
    , { GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x6_KHR   , GL_RGBA         , GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x6_KHR     }
    , { GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x8_KHR   , GL_RGBA         , GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x8_KHR     }
    , { GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x10_KHR  , GL_RGBA         , GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x10_KHR    }
    , { GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x10_KHR  , GL_RGBA         , GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x10_KHR    }
    , { GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x12_KHR  , GL_RGBA         , GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x12_KHR    }
};

bool isSizedInternalFormat(GLint internalFormat)
{
    const size_t formatsCount = sizeof(sizedInternalFormats) / sizeof(sizedInternalFormats[0]);

    for (size_t i=0; i < formatsCount; ++i)
    {
        if((GLenum)internalFormat == sizedInternalFormats[i].sizedInternalFormat)
            return true;
    }

    return false;
}

GLenum assumeSizedInternalFormat(GLint internalFormat, GLenum type)
{
    const size_t formatsCount = sizeof(sizedInternalFormats) / sizeof(sizedInternalFormats[0]);

    for (size_t i=0; i < formatsCount; ++i)
    {
        if(internalFormat == sizedInternalFormats[i].internalFormat && type == sizedInternalFormats[i].type)
            return sizedInternalFormats[i].sizedInternalFormat;
    }

    return 0;
}

bool isCompressedInternalFormatSupportedByTexStorage(GLint internalFormat)
{
    const size_t formatsCount = sizeof(compressedInternalFormats) / sizeof(compressedInternalFormats[0]);

    for (size_t i=0; i < formatsCount; ++i)
    {
        if((GLenum)internalFormat == compressedInternalFormats[i].sizedInternalFormat)
            return true;
    }

    return false;
}

Texture::TextureObject::~TextureObject()
{
    // OSG_NOTICE<<"Texture::TextureObject::~TextureObject() "<<this<<std::endl;
}

void Texture::TextureObject::bind()
{
    glBindTexture( _profile._target, _id);
    if (_set) _set->moveToBack(this);
}

void Texture::TextureObject::release()
{
    if (_set) _set->orphan(this);
}

void Texture::TextureObject::setAllocated(GLint     numMipmapLevels,
                                          GLenum    internalFormat,
                                          GLsizei   width,
                                          GLsizei   height,
                                          GLsizei   depth,
                                          GLint     border)
{
    _allocated=true;
    if (!match(_profile._target,numMipmapLevels,internalFormat,width,height,depth,border))
    {
        // keep previous size
        unsigned int previousSize = _profile._size;

        _profile.set(numMipmapLevels,internalFormat,width,height,depth,border);

        if (_set)
        {
            _set->moveToSet(this, _set->getParent()->getTextureObjectSet(_profile));

            // Update texture pool size
            _set->getParent()->getCurrTexturePoolSize() -= previousSize;
            _set->getParent()->getCurrTexturePoolSize() += _profile._size;
        }
    }
}

void Texture::TextureProfile::computeSize()
{
    unsigned int numBitsPerTexel = 32;

    switch(_internalFormat)
    {
        case 1:
        case GL_ALPHA:
        case GL_LUMINANCE:
        case GL_INTENSITY:
        case GL_RED:
            numBitsPerTexel = 8;
            break;

        case 2:
        case GL_LUMINANCE_ALPHA:
        case GL_RG:
            numBitsPerTexel = 16;
            break;

        case 3:
        case GL_RGB:
        case GL_BGR:
            numBitsPerTexel = 24;
            break;

        case 4:
        case GL_RGBA:
        case GL_BGRA:
            numBitsPerTexel = 32;
            break;

        case GL_COMPRESSED_ALPHA_ARB:                      numBitsPerTexel = 4;  break;
        case GL_COMPRESSED_INTENSITY_ARB:                  numBitsPerTexel = 4;  break;
        case GL_COMPRESSED_LUMINANCE_ALPHA_ARB:            numBitsPerTexel = 4;  break;
        case GL_COMPRESSED_RGB_S3TC_DXT1_EXT:              numBitsPerTexel = 4;  break;
        case GL_COMPRESSED_SRGB_S3TC_DXT1_EXT:             numBitsPerTexel = 4;  break;
        case GL_COMPRESSED_RGBA_S3TC_DXT1_EXT:             numBitsPerTexel = 4;  break;
        case GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT:       numBitsPerTexel = 4;  break;

        case GL_COMPRESSED_RGB_ARB:                        numBitsPerTexel = 8;  break;
        case GL_COMPRESSED_RGBA_S3TC_DXT3_EXT:             numBitsPerTexel = 8;  break;
        case GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT:       numBitsPerTexel = 8;  break;
        case GL_COMPRESSED_RGBA_S3TC_DXT5_EXT:             numBitsPerTexel = 8;  break;
        case GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT:       numBitsPerTexel = 8;  break;

        case GL_COMPRESSED_SIGNED_RED_RGTC1_EXT:           numBitsPerTexel = 4;  break;
        case GL_COMPRESSED_RED_RGTC1_EXT:                  numBitsPerTexel = 4;  break;
        case GL_COMPRESSED_SIGNED_RED_GREEN_RGTC2_EXT:     numBitsPerTexel = 8;  break;
        case GL_COMPRESSED_RED_GREEN_RGTC2_EXT:            numBitsPerTexel = 8;  break;

        case GL_COMPRESSED_RGB_PVRTC_2BPPV1_IMG:           numBitsPerTexel = 2;  break;
        case GL_COMPRESSED_RGBA_PVRTC_2BPPV1_IMG:          numBitsPerTexel = 2;  break;
        case GL_COMPRESSED_RGB_PVRTC_4BPPV1_IMG:           numBitsPerTexel = 4;  break;
        case GL_COMPRESSED_RGBA_PVRTC_4BPPV1_IMG:          numBitsPerTexel = 4;  break;

        case GL_ETC1_RGB8_OES:                             numBitsPerTexel = 4;  break;

        case GL_COMPRESSED_RGB8_ETC2:                      numBitsPerTexel = 4;  break;
        case GL_COMPRESSED_SRGB8_ETC2:                     numBitsPerTexel = 4;  break;
        case GL_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2:  numBitsPerTexel = 8;  break;
        case GL_COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2: numBitsPerTexel = 8;  break;
        case GL_COMPRESSED_RGBA8_ETC2_EAC:                 numBitsPerTexel = 8;  break;
        case GL_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC:          numBitsPerTexel = 8;  break;
        case GL_COMPRESSED_R11_EAC:                        numBitsPerTexel = 4;  break;
        case GL_COMPRESSED_SIGNED_R11_EAC:                 numBitsPerTexel = 4;  break;
        case GL_COMPRESSED_RG11_EAC:                       numBitsPerTexel = 8;  break;
        case GL_COMPRESSED_SIGNED_RG11_EAC:                numBitsPerTexel = 8;  break;

        // ASTC
        case GL_COMPRESSED_RGBA_ASTC_4x4_KHR:
        case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4_KHR:
        case GL_COMPRESSED_RGBA_ASTC_5x4_KHR:
        case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x4_KHR:
        case GL_COMPRESSED_RGBA_ASTC_5x5_KHR:
        case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x5_KHR:
        case GL_COMPRESSED_RGBA_ASTC_6x5_KHR:
        case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x5_KHR:
        case GL_COMPRESSED_RGBA_ASTC_6x6_KHR:
        case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x6_KHR:
        case GL_COMPRESSED_RGBA_ASTC_8x5_KHR:
        case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x5_KHR:
        case GL_COMPRESSED_RGBA_ASTC_8x6_KHR:
        case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x6_KHR:
        case GL_COMPRESSED_RGBA_ASTC_8x8_KHR:
        case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x8_KHR:
        case GL_COMPRESSED_RGBA_ASTC_10x5_KHR:
        case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x5_KHR:
        case GL_COMPRESSED_RGBA_ASTC_10x6_KHR:
        case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x6_KHR:
        case GL_COMPRESSED_RGBA_ASTC_10x8_KHR:
        case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x8_KHR:
        case GL_COMPRESSED_RGBA_ASTC_10x10_KHR:
        case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x10_KHR:
        case GL_COMPRESSED_RGBA_ASTC_12x10_KHR:
        case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x10_KHR:
        case GL_COMPRESSED_RGBA_ASTC_12x12_KHR:
        case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x12_KHR:
        {
            _size = 0;
            for (int i = 0; i < std::max(_numMipmapLevels, 1); ++i)
            {
                GLint blockSize;
                GLint size;
                getCompressedSize(_internalFormat, _width/(1 << i), _height/(1 << i), _depth, blockSize, size);
                _size += size;
            }
            return;
        }
    }

    _size = (unsigned int)(ceil(double(_width * _height * _depth * numBitsPerTexel)/8.0));

    if (_numMipmapLevels>1)
    {
        unsigned int mipmapSize = _size / 4;
        for(GLint i=0; i<_numMipmapLevels && mipmapSize!=0; ++i)
        {
            _size += mipmapSize;
            mipmapSize /= 4;
        }
    }

    // OSG_NOTICE<<"TO ("<<_width<<", "<<_height<<", "<<_depth<<") size="<<_size<<" numBitsPerTexel="<<numBitsPerTexel<<std::endl;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//  New texture object manager
//
TextureObjectSet::TextureObjectSet(TextureObjectManager* parent, const Texture::TextureProfile& profile):
    _parent(parent),
    _contextID(parent->getContextID()),
    _profile(profile),
    _numOfTextureObjects(0),
    _head(0),
    _tail(0)
{
}

TextureObjectSet::~TextureObjectSet()
{
#if 0
    OSG_NOTICE<<"TextureObjectSet::~TextureObjectSet(), _numOfTextureObjects="<<_numOfTextureObjects<<std::endl;
    OSG_NOTICE<<"     _orphanedTextureObjects = "<<_orphanedTextureObjects.size()<<std::endl;
    OSG_NOTICE<<"     _head = "<<_head<<std::endl;
    OSG_NOTICE<<"     _tail = "<<_tail<<std::endl;
#endif
}

bool TextureObjectSet::checkConsistency() const
{
    OSG_NOTICE<<"TextureObjectSet::checkConsistency()"<<std::endl;
    // check consistency of linked list.
    unsigned int numInList = 0;
    Texture::TextureObject* to = _head;
    while(to!=0)
    {
        ++numInList;

        if (to->_next)
        {
            if ((to->_next)->_previous != to)
            {
                OSG_NOTICE<<"TextureObjectSet::checkConsistency() : Error (to->_next)->_previous != to "<<std::endl;
                return false;
            }
        }
        else
        {
            if (_tail != to)
            {
                OSG_NOTICE<<"TextureObjectSet::checkConsistency() : Error _tail != to"<<std::endl;
                return false;
            }
        }

        to = to->_next;
    }

    unsigned int totalNumber = numInList + _orphanedTextureObjects.size();
    if (totalNumber != _numOfTextureObjects)
    {
        OSG_NOTICE<<"Error numInList + _orphanedTextureObjects.size() != _numOfTextureObjects"<<std::endl;
        OSG_NOTICE<<"    numInList = "<<numInList<<std::endl;
        OSG_NOTICE<<"    _orphanedTextureObjects.size() = "<<_orphanedTextureObjects.size()<<std::endl;
        OSG_NOTICE<<"    _pendingOrphanedTextureObjects.size() = "<<_pendingOrphanedTextureObjects.size()<<std::endl;
        OSG_NOTICE<<"    _numOfTextureObjects = "<<_numOfTextureObjects<<std::endl;
        return false;
    }

    _parent->checkConsistency();

    return true;
}

void TextureObjectSet::handlePendingOrphandedTextureObjects()
{
    // OSG_NOTICE<<"handlePendingOrphandedTextureObjects()"<<_pendingOrphanedTextureObjects.size()<<std::endl;

    if (_pendingOrphanedTextureObjects.empty()) return;

    unsigned int numOrphaned = _pendingOrphanedTextureObjects.size();

    for(Texture::TextureObjectList::iterator itr = _pendingOrphanedTextureObjects.begin();
        itr != _pendingOrphanedTextureObjects.end();
        ++itr)
    {
        Texture::TextureObject* to = itr->get();

        _orphanedTextureObjects.push_back(to);

        remove(to);
    }


    // update the TextureObjectManager's running total of active + orphaned TextureObjects
    _parent->getNumberOrphanedTextureObjects() += numOrphaned;
    _parent->getNumberActiveTextureObjects() -= numOrphaned;

    _pendingOrphanedTextureObjects.clear();

    CHECK_CONSISTENCY
}


void TextureObjectSet::deleteAllTextureObjects()
{
    // OSG_NOTICE<<"TextureObjectSet::deleteAllTextureObjects()"<<std::endl;

    {
        OpenThreads::ScopedLock<OpenThreads::Mutex> lock(_mutex);
        if (!_pendingOrphanedTextureObjects.empty())
        {
            // OSG_NOTICE<<"TextureObjectSet::flushDeletedTextureObjects(..) handling orphans"<<std::endl;
            handlePendingOrphandedTextureObjects();
        }
    }

    CHECK_CONSISTENCY

    // detect all the active texture objects from their Textures
    unsigned int numOrphaned = 0;
    Texture::TextureObject* to = _head;
    while(to!=0)
    {
        ref_ptr<Texture::TextureObject> glto = to;

        to = to->_next;

        _orphanedTextureObjects.push_back(glto.get());

        remove(glto.get());

        ++numOrphaned;

        ref_ptr<Texture> original_texture = glto->getTexture();
        if (original_texture.valid())
        {
            original_texture->setTextureObject(_contextID,0);
        }
    }

    _parent->getNumberOrphanedTextureObjects() += numOrphaned;
    _parent->getNumberActiveTextureObjects() -= numOrphaned;

    // now do the actual delete.
    flushAllDeletedTextureObjects();

    // OSG_NOTICE<<"done GLBufferObjectSet::deleteAllGLBufferObjects()"<<std::endl;
}

void TextureObjectSet::discardAllTextureObjects()
{
    // OSG_NOTICE<<"TextureObjectSet::discardAllTextureObjects()."<<std::endl;

    Texture::TextureObject* to = _head;
    while(to!=0)
    {
        ref_ptr<Texture::TextureObject> glto = to;

        to = to->_next;

        ref_ptr<Texture> original_texture = glto->getTexture();

        if (original_texture.valid())
        {
            original_texture->setTextureObject(_contextID,0);
        }
    }

    // the linked list should now be empty
    _head = 0;
    _tail = 0;

    _pendingOrphanedTextureObjects.clear();
    _orphanedTextureObjects.clear();

    unsigned int numDeleted = _numOfTextureObjects;
    _numOfTextureObjects = 0;

    // update the TextureObjectManager's running total of current pool size
    _parent->getCurrTexturePoolSize() -= numDeleted*_profile._size;
    _parent->getNumberOrphanedTextureObjects() -= numDeleted;
    _parent->getNumberDeleted() += numDeleted;
}

void TextureObjectSet::flushAllDeletedTextureObjects()
{
    // OSG_NOTICE<<"TextureObjectSet::flushAllDeletedTextureObjects()"<<std::endl;
    {
        OpenThreads::ScopedLock<OpenThreads::Mutex> lock(_mutex);
        if (!_pendingOrphanedTextureObjects.empty())
        {
            // OSG_NOTICE<<"TextureObjectSet::flushDeletedTextureObjects(..) handling orphans"<<std::endl;
            handlePendingOrphandedTextureObjects();
        }
    }

    for(Texture::TextureObjectList::iterator itr = _orphanedTextureObjects.begin();
        itr != _orphanedTextureObjects.end();
        ++itr)
    {

        GLuint id = (*itr)->id();

        // OSG_NOTICE<<"    Deleting textureobject ptr="<<itr->get()<<" id="<<id<<std::endl;
        glDeleteTextures( 1L, &id);
    }

    unsigned int numDeleted = _orphanedTextureObjects.size();
    _numOfTextureObjects -= numDeleted;

    // update the TextureObjectManager's running total of current pool size
    _parent->getCurrTexturePoolSize() -= numDeleted*_profile._size;
    _parent->getNumberOrphanedTextureObjects() -= numDeleted;
    _parent->getNumberDeleted() += numDeleted;

    _orphanedTextureObjects.clear();
}

void TextureObjectSet::discardAllDeletedTextureObjects()
{
    // OSG_NOTICE<<"TextureObjectSet::discardAllDeletedTextureObjects()"<<std::endl;

    // clean up the pending orphans.
    {
        OpenThreads::ScopedLock<OpenThreads::Mutex> lock(_mutex);
        if (!_pendingOrphanedTextureObjects.empty())
        {
            // OSG_NOTICE<<"TextureObjectSet::flushDeletedTextureObjects(..) handling orphans"<<std::endl;
            handlePendingOrphandedTextureObjects();
        }
    }

    unsigned int numDiscarded = _orphanedTextureObjects.size();

    _numOfTextureObjects -= numDiscarded;

    // update the TextureObjectManager's running total of current pool size
    _parent->getCurrTexturePoolSize() -= numDiscarded*_profile._size;

    // update the number of active and orphaned TextureObjects
    _parent->getNumberOrphanedTextureObjects() -= numDiscarded;
    _parent->getNumberDeleted() += numDiscarded;

    // just clear the list as there is nothing else we can do with them when discarding them
    _orphanedTextureObjects.clear();
}

void TextureObjectSet::flushDeletedTextureObjects(double /*currentTime*/, double& availableTime)
{
    // OSG_NOTICE<<"TextureObjectSet::flushDeletedTextureObjects(..)"<<std::endl;

    {
        OpenThreads::ScopedLock<OpenThreads::Mutex> lock(_mutex);
        if (!_pendingOrphanedTextureObjects.empty())
        {
            // OSG_NOTICE<<"TextureObjectSet::flushDeletedTextureObjects(..) handling orphans"<<std::endl;
            handlePendingOrphandedTextureObjects();
        }
    }

    if (_profile._size!=0 && _parent->getCurrTexturePoolSize()<=_parent->getMaxTexturePoolSize())
    {
        // OSG_NOTICE<<"Plenty of space in TextureObject pool"<<std::endl;
        return;
    }

    // if nothing to delete return
    if (_orphanedTextureObjects.empty())
    {
        return;
    }

    // if no time available don't try to flush objects.
    if (availableTime<=0.0) return;

    unsigned int numDeleted = 0;
    unsigned int sizeRequired = _parent->getCurrTexturePoolSize() - _parent->getMaxTexturePoolSize();

    unsigned int maxNumObjectsToDelete = _profile._size!=0 ?
        static_cast<unsigned int>(ceil(double(sizeRequired) / double(_profile._size))):
        _orphanedTextureObjects.size();

    OSG_INFO<<"_parent->getCurrTexturePoolSize()="<<_parent->getCurrTexturePoolSize() <<" _parent->getMaxTexturePoolSize()="<< _parent->getMaxTexturePoolSize()<<std::endl;
    OSG_INFO<<"Looking to reclaim "<<sizeRequired<<", going to look to remove "<<maxNumObjectsToDelete<<" from "<<_orphanedTextureObjects.size()<<" orphans"<<std::endl;

    ElapsedTime timer;

    Texture::TextureObjectList::iterator itr = _orphanedTextureObjects.begin();
    for(;
        itr != _orphanedTextureObjects.end() && timer.elapsedTime()<availableTime && numDeleted<maxNumObjectsToDelete;
        ++itr)
    {

        GLuint id = (*itr)->id();

        // OSG_NOTICE<<"    Deleting textureobject ptr="<<itr->get()<<" id="<<id<<std::endl;
        glDeleteTextures( 1L, &id);

        ++numDeleted;
    }

    // OSG_NOTICE<<"Size before = "<<_orphanedTextureObjects.size();
    _orphanedTextureObjects.erase(_orphanedTextureObjects.begin(), itr);
    // OSG_NOTICE<<", after = "<<_orphanedTextureObjects.size()<<" numDeleted = "<<numDeleted<<std::endl;

    // update the number of TO's in this TextureObjectSet
    _numOfTextureObjects -= numDeleted;

    _parent->getCurrTexturePoolSize() -= numDeleted*_profile._size;

    // update the number of active and orphaned TextureObjects
    _parent->getNumberOrphanedTextureObjects() -= numDeleted;
    _parent->getNumberDeleted() += numDeleted;

    availableTime -= timer.elapsedTime();
}

bool TextureObjectSet::makeSpace(unsigned int& size)
{
    {
        OpenThreads::ScopedLock<OpenThreads::Mutex> lock(_mutex);
        if (!_pendingOrphanedTextureObjects.empty())
        {
            // OSG_NOTICE<<"TextureObjectSet::TextureObjectSet::makeSpace(..) handling orphans"<<std::endl;
            handlePendingOrphandedTextureObjects();
        }
    }

    if (!_orphanedTextureObjects.empty())
    {
        unsigned int sizeAvailable = _orphanedTextureObjects.size() * _profile._size;
        if (size>sizeAvailable) size -= sizeAvailable;
        else size = 0;

        flushAllDeletedTextureObjects();
    }

    return size==0;
}

osg::ref_ptr<Texture::TextureObject> TextureObjectSet::takeFromOrphans(Texture* texture)
{
    // take front of orphaned list.
    ref_ptr<Texture::TextureObject> to = _orphanedTextureObjects.front();

    // remove from orphan list.
    _orphanedTextureObjects.pop_front();

    // assign to new texture
    to->setTexture(texture);

    // update the number of active and orphaned TextureObjects
    _parent->getNumberOrphanedTextureObjects() -= 1;
    _parent->getNumberActiveTextureObjects() += 1;

    // place at back of active list
    addToBack(to.get());

    OSG_INFO<<"Reusing orphaned TextureObject, _numOfTextureObjects="<<_numOfTextureObjects<<std::endl;

    return to;
}


osg::ref_ptr<Texture::TextureObject> TextureObjectSet::takeOrGenerate(Texture* texture)
{
    // see if we can recycle TextureObject from the orphan list
    {
        OpenThreads::ScopedLock<OpenThreads::Mutex> lock(_mutex);
        if (!_pendingOrphanedTextureObjects.empty())
        {
            handlePendingOrphandedTextureObjects();
            return takeFromOrphans(texture);
        }
    }

    if (!_orphanedTextureObjects.empty())
    {
        return takeFromOrphans(texture);
    }

    unsigned int minFrameNumber = _parent->getFrameNumber();

    // see if we can reuse TextureObject by taking the least recently used active TextureObject
    if ((_parent->getMaxTexturePoolSize()!=0) &&
        (!_parent->hasSpace(_profile._size)) &&
        (_numOfTextureObjects>1) &&
        (_head != 0) &&
        (_head->_frameLastUsed<minFrameNumber))
    {

        OpenThreads::ScopedLock<OpenThreads::Mutex> lock(_mutex);

        ref_ptr<Texture::TextureObject> to = _head;

        ref_ptr<Texture> original_texture = to->getTexture();

        if (original_texture.valid())
        {
            original_texture->setTextureObject(_contextID,0);
            OSG_INFO<<"TextureObjectSet="<<this<<": Reusing an active TextureObject "<<to.get()<<" _numOfTextureObjects="<<_numOfTextureObjects<<" width="<<_profile._width<<" height="<<_profile._height<<std::endl;
        }
        else
        {
            OSG_INFO<<"Reusing a recently orphaned active TextureObject "<<to.get()<<std::endl;
        }

        moveToBack(to.get());

        // assign to new texture
        to->setTexture(texture);

        return to;
    }

    //
    // no TextureObjects available to recycle so have to create one from scratch
    //
    GLuint id;
    glGenTextures( 1L, &id );

    osg::ref_ptr<Texture::TextureObject> to = new Texture::TextureObject(const_cast<Texture*>(texture),id,_profile);
    to->_set = this;
    ++_numOfTextureObjects;

    // update the current texture pool size
    _parent->getCurrTexturePoolSize() += _profile._size;
    _parent->getNumberActiveTextureObjects() += 1;

    addToBack(to.get());

    OSG_INFO<<"Created new " << this << " TextureObject, _numOfTextureObjects "<<_numOfTextureObjects<<std::endl;

    return to;
}

void TextureObjectSet::moveToBack(Texture::TextureObject* to)
{
#if 0
    OSG_NOTICE<<"TextureObjectSet::moveToBack("<<to<<")"<<std::endl;
    OSG_NOTICE<<"    before _head = "<<_head<<std::endl;
    OSG_NOTICE<<"    before _tail = "<<_tail<<std::endl;
    OSG_NOTICE<<"    before to->_previous = "<<to->_previous<<std::endl;
    OSG_NOTICE<<"    before to->_next = "<<to->_next<<std::endl;
#endif

    to->_frameLastUsed = _parent->getFrameNumber();

    // nothing to do if we are already tail
    if (to==_tail) return;

    // if no tail exists then assign 'to' as tail and head
    if (_tail==0)
    {
        OSG_NOTICE<<"Error ***************** Should not get here !!!!!!!!!"<<std::endl;
        _head = to;
        _tail = to;
        return;
    }

    if (to->_next==0)
    {
        OSG_NOTICE<<"Error ***************** Should not get here either !!!!!!!!!"<<std::endl;
        return;
    }


    if (to->_previous)
    {
        (to->_previous)->_next = to->_next;
    }
    else
    {
        // 'to' is the head, so moving it to the back will mean we need a new head
        if (to->_next)
        {
            _head = to->_next;
        }
    }

    (to->_next)->_previous = to->_previous;

    _tail->_next = to;

    to->_previous = _tail;
    to->_next = 0;

    _tail = to;

#if 0
    OSG_NOTICE<<"  m2B   after  _head = "<<_head<<std::endl;
    OSG_NOTICE<<"  m2B   after _tail = "<<_tail<<std::endl;
    OSG_NOTICE<<"  m2B   after to->_previous = "<<to->_previous<<std::endl;
    OSG_NOTICE<<"  m2B   after to->_next = "<<to->_next<<std::endl;
#endif
    CHECK_CONSISTENCY
}

void TextureObjectSet::addToBack(Texture::TextureObject* to)
{
#if 0
    OSG_NOTICE<<"TextureObjectSet::addToBack("<<to<<")"<<std::endl;
    OSG_NOTICE<<"    before _head = "<<_head<<std::endl;
    OSG_NOTICE<<"    before _tail = "<<_tail<<std::endl;
    OSG_NOTICE<<"    before to->_previous = "<<to->_previous<<std::endl;
    OSG_NOTICE<<"    before to->_next = "<<to->_next<<std::endl;
#endif

    if (to->_previous !=0 || to->_next !=0)
    {
        moveToBack(to);
    }
    else
    {
        to->_frameLastUsed = _parent->getFrameNumber();

        if (_tail) _tail->_next = to;
        to->_previous = _tail;

        if (!_head) _head = to;
        _tail = to;
    }
#if 0
    OSG_NOTICE<<"  a2B  after  _head = "<<_head<<std::endl;
    OSG_NOTICE<<"  a2B   after _tail = "<<_tail<<std::endl;
    OSG_NOTICE<<"  a2B   after to->_previous = "<<to->_previous<<std::endl;
    OSG_NOTICE<<"  a2B   after to->_next = "<<to->_next<<std::endl;
#endif
    CHECK_CONSISTENCY
}

void TextureObjectSet::orphan(Texture::TextureObject* to)
{
    // OSG_NOTICE<<"TextureObjectSet::orphan("<<to<<")"<<std::endl;

    OpenThreads::ScopedLock<OpenThreads::Mutex> lock(_mutex);

    // disconnect from original texture
    to->setTexture(0);

    // add orphan 'to' to the pending list of orphans, these will then be
    // handled in the handlePendingOrphandedTextureObjects() where the TO's
    // will be removed from the active list, and then placed in the orhpanTextureObject
    // list.  This double buffered approach to handling orphaned TO's is used
    // to avoid having to mutex the process of appling active TO's.
    _pendingOrphanedTextureObjects.push_back(to);

#if 0
    OSG_NOTICE<<"TextureObjectSet::orphan("<<to<<")  _pendingOrphanedTextureObjects.size()="<<_pendingOrphanedTextureObjects.size()<<std::endl;
    OSG_NOTICE<<"                                    _orphanedTextureObjects.size()="<<_orphanedTextureObjects.size()<<std::endl;
#endif
}

void TextureObjectSet::remove(Texture::TextureObject* to)
{
    if (to->_previous!=0)
    {
        (to->_previous)->_next = to->_next;
    }
    else
    {
        // 'to' was head so assign _head to the next in list
        _head = to->_next;
    }

    if (to->_next!=0)
    {
        (to->_next)->_previous = to->_previous;
    }
    else
    {
        // 'to' was tail so assign tail to the previous in list
        _tail = to->_previous;
    }

    // reset the 'to' list pointers as it's no longer in the active list.
    to->_next = 0;
    to->_previous = 0;
}

void TextureObjectSet::moveToSet(Texture::TextureObject* to, TextureObjectSet* set)
{
    if (set==this) return;
    if (!set) return;

    // remove 'to' from original set
    --_numOfTextureObjects;
    remove(to);

    // register 'to' with new set.
    to->_set = set;
    ++set->_numOfTextureObjects;
    set->addToBack(to);
}

unsigned int TextureObjectSet::computeNumTextureObjectsInList() const
{
    unsigned int num=0;
    Texture::TextureObject* obj = _head;
    while(obj!=NULL)
    {
        ++num;
        obj = obj->_next;
    }
    return num;
}


TextureObjectManager::TextureObjectManager(unsigned int contextID):
    GraphicsObjectManager("TextureObjectManager", contextID),
    _numActiveTextureObjects(0),
    _numOrphanedTextureObjects(0),
    _currTexturePoolSize(0),
    _maxTexturePoolSize(0),
    _frameNumber(0),
    _numFrames(0),
    _numDeleted(0),
    _deleteTime(0.0),
    _numGenerated(0),
    _generateTime(0.0)
{
}

TextureObjectManager::~TextureObjectManager()
{
}


void TextureObjectManager::setMaxTexturePoolSize(unsigned int size)
{
    if (_maxTexturePoolSize == size) return;

    if (size<_currTexturePoolSize)
    {
        OSG_NOTICE<<"Warning: new MaxTexturePoolSize="<<size<<" is smaller than current TexturePoolSize="<<_currTexturePoolSize<<std::endl;
    }

    _maxTexturePoolSize = size;
}

bool TextureObjectManager::makeSpace(unsigned int size)
{
    for(TextureSetMap::iterator itr = _textureSetMap.begin();
        itr != _textureSetMap.end() && size>0;
        ++itr)
    {
        if ((*itr).second->makeSpace(size)) return true;
    }

    return size==0;
}


osg::ref_ptr<Texture::TextureObject> TextureObjectManager::generateTextureObject(const Texture* texture, GLenum target)
{
    return generateTextureObject(texture, target, 0, 0, 0, 0, 0, 0);
}

osg::ref_ptr<Texture::TextureObject> TextureObjectManager::generateTextureObject(const Texture* texture,
                                             GLenum    target,
                                             GLint     numMipmapLevels,
                                             GLenum    internalFormat,
                                             GLsizei   width,
                                             GLsizei   height,
                                             GLsizei   depth,
                                             GLint     border)
{
    ElapsedTime elapsedTime(&(getGenerateTime()));
    ++getNumberGenerated();

    Texture::TextureProfile profile(target,numMipmapLevels,internalFormat,width,height,depth,border);
    TextureObjectSet* tos = getTextureObjectSet(profile);
    return tos->takeOrGenerate(const_cast<Texture*>(texture));
}

Texture::TextureObject* Texture::generateAndAssignTextureObject(unsigned int contextID, GLenum target) const
{
    _textureObjectBuffer[contextID] = generateTextureObject(this, contextID, target);
    return _textureObjectBuffer[contextID].get();
}

Texture::TextureObject* Texture::generateAndAssignTextureObject(
                                             unsigned int contextID,
                                             GLenum    target,
                                             GLint     numMipmapLevels,
                                             GLenum    internalFormat,
                                             GLsizei   width,
                                             GLsizei   height,
                                             GLsizei   depth,
                                             GLint     border) const
{
    _textureObjectBuffer[contextID] = generateTextureObject(this, contextID, target, numMipmapLevels, internalFormat, width, height, depth, border);
    return _textureObjectBuffer[contextID].get();
}

TextureObjectSet* TextureObjectManager::getTextureObjectSet(const Texture::TextureProfile& profile)
{
    osg::ref_ptr<TextureObjectSet>& tos = _textureSetMap[profile];
    if (!tos) tos = new TextureObjectSet(this, profile);
    return tos.get();
}

void TextureObjectManager::handlePendingOrphandedTextureObjects()
{
    for(TextureSetMap::iterator itr = _textureSetMap.begin();
        itr != _textureSetMap.end();
        ++itr)
    {
        (*itr).second->handlePendingOrphandedTextureObjects();
    }
}

void TextureObjectManager::deleteAllGLObjects()
{
    for(TextureSetMap::iterator itr = _textureSetMap.begin();
        itr != _textureSetMap.end();
        ++itr)
    {
        (*itr).second->deleteAllTextureObjects();
    }
}

void TextureObjectManager::discardAllGLObjects()
{
    for(TextureSetMap::iterator itr = _textureSetMap.begin();
        itr != _textureSetMap.end();
        ++itr)
    {
        (*itr).second->discardAllTextureObjects();
    }
}

void TextureObjectManager::flushAllDeletedGLObjects()
{
    for(TextureSetMap::iterator itr = _textureSetMap.begin();
        itr != _textureSetMap.end();
        ++itr)
    {
        (*itr).second->flushAllDeletedTextureObjects();
    }
}

void TextureObjectManager::discardAllDeletedGLObjects()
{
    for(TextureSetMap::iterator itr = _textureSetMap.begin();
        itr != _textureSetMap.end();
        ++itr)
    {
        (*itr).second->discardAllDeletedTextureObjects();
    }
}

void TextureObjectManager::flushDeletedGLObjects(double currentTime, double& availableTime)
{
    for(TextureSetMap::iterator itr = _textureSetMap.begin();
        (itr != _textureSetMap.end()) && (availableTime > 0.0);
        ++itr)
    {
        (*itr).second->flushDeletedTextureObjects(currentTime, availableTime);
    }
}

void TextureObjectManager::newFrame(osg::FrameStamp* fs)
{
    if (fs) _frameNumber = fs->getFrameNumber();
    else ++_frameNumber;

    ++_numFrames;
}

void TextureObjectManager::reportStats(std::ostream& out)
{
    double numFrames(_numFrames==0 ? 1.0 : _numFrames);
    out<<"TextureObjectMananger::reportStats()"<<std::endl;
    out<<"   total _numOfTextureObjects="<<_numActiveTextureObjects<<", _numOrphanedTextureObjects="<<_numOrphanedTextureObjects<<" _currTexturePoolSize="<<_currTexturePoolSize<<std::endl;
    out<<"   total _numGenerated="<<_numGenerated<<", _generateTime="<<_generateTime<<", averagePerFrame="<<_generateTime/numFrames*1000.0<<"ms"<<std::endl;
    out<<"   total _numDeleted="<<_numDeleted<<", _deleteTime="<<_deleteTime<<", averagePerFrame="<<_deleteTime/numFrames*1000.0<<"ms"<<std::endl;
    out<<"   getMaxTexturePoolSize()="<<getMaxTexturePoolSize()<<" current/max size = "<<double(_currTexturePoolSize)/double(getMaxTexturePoolSize())<<std::endl;
    recomputeStats(out);
}

void TextureObjectManager::resetStats()
{
    _numFrames = 0;
    _numDeleted = 0;
    _deleteTime = 0;

    _numGenerated = 0;
    _generateTime = 0;
}


void TextureObjectManager::recomputeStats(std::ostream& out) const
{
    out<<"TextureObjectManager::recomputeStats()"<<std::endl;
    unsigned int numObjectsInLists = 0;
    unsigned int numActive = 0;
    unsigned int numOrphans = 0;
    unsigned int numPendingOrphans = 0;
    unsigned int currentSize = 0;
    for(TextureSetMap::const_iterator itr = _textureSetMap.begin();
        itr != _textureSetMap.end();
        ++itr)
    {
         const TextureObjectSet* os = itr->second.get();
         numObjectsInLists += os->computeNumTextureObjectsInList();
         numActive += os->getNumOfTextureObjects();
         numOrphans += os->getNumOrphans();
         numPendingOrphans += os->getNumPendingOrphans();
         currentSize += os->getProfile()._size * (os->computeNumTextureObjectsInList()+os->getNumOrphans());
         out<<"   size="<<os->getProfile()._size
           <<", os->computeNumTextureObjectsInList()"<<os->computeNumTextureObjectsInList()
           <<", os->getNumOfTextureObjects()"<<os->getNumOfTextureObjects()
           <<", os->getNumOrphans()"<<os->getNumOrphans()
           <<", os->getNumPendingOrphans()"<<os->getNumPendingOrphans()
           <<std::endl;
    }
    out<<"   numObjectsInLists="<<numObjectsInLists<<", numActive="<<numActive<<", numOrphans="<<numOrphans<<" currentSize="<<currentSize<<std::endl;
    out<<"   getMaxTexturePoolSize()="<<getMaxTexturePoolSize()<<" current/max size = "<<double(currentSize)/double(getMaxTexturePoolSize())<<std::endl;
    if (currentSize != _currTexturePoolSize) out<<"   WARNING: _currTexturePoolSize("<<_currTexturePoolSize<<") != currentSize, delta = "<<int(_currTexturePoolSize)-int(currentSize)<<std::endl;
}

bool TextureObjectManager::checkConsistency() const
{
    unsigned int numObjectsInLists = 0;
    unsigned int numActive = 0;
    unsigned int numOrphans = 0;
    unsigned int numPendingOrphans = 0;
    unsigned int currentSize = 0;
    for(TextureSetMap::const_iterator itr = _textureSetMap.begin();
        itr != _textureSetMap.end();
        ++itr)
    {
         const TextureObjectSet* os = itr->second.get();
         numObjectsInLists += os->computeNumTextureObjectsInList();
         numActive += os->getNumOfTextureObjects();
         numOrphans += os->getNumOrphans();
         numPendingOrphans += os->getNumPendingOrphans();
         currentSize += os->getProfile()._size * (os->computeNumTextureObjectsInList()+os->getNumOrphans());
    }

    if (currentSize != _currTexturePoolSize)
    {
        recomputeStats(osg::notify(osg::NOTICE));

        throw "TextureObjectManager::checkConsistency()  sizes inconsistent";
        return false;
    }
    return true;
}

osg::ref_ptr<Texture::TextureObject> Texture::generateTextureObject(const Texture* texture, unsigned int contextID, GLenum target)
{
    return osg::get<TextureObjectManager>(contextID)->generateTextureObject(texture, target);
}

osg::ref_ptr<Texture::TextureObject> Texture::generateTextureObject(const Texture* texture, unsigned int contextID,
                                             GLenum    target,
                                             GLint     numMipmapLevels,
                                             GLenum    internalFormat,
                                             GLsizei   width,
                                             GLsizei   height,
                                             GLsizei   depth,
                                             GLint     border)
{
    return osg::get<TextureObjectManager>(contextID)->generateTextureObject(texture,target,numMipmapLevels,internalFormat,width,height,depth,border);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// GL core native: legacy texture formats on a core profile (Texture::selectCoreFormat)
//
namespace
{
    // Where a sampled channel comes from: a channel of the texel as the upload stores it, or a constant.
    enum Source { SRC_R = 0, SRC_G = 1, SRC_B = 2, SRC_A = 3, SRC_ZERO = 4, SRC_ONE = 5 };

    struct Sources
    {
        Sources(int r, int g, int b, int a) { s[0] = r; s[1] = g; s[2] = b; s[3] = a; }
        int s[4];
    };

    // The precision of a legacy format, the row of its core stand-ins in kCoreFormats.
    enum Precision { P_UNSIZED, P_UNORM8, P_UNORM16, P_FLOAT16, P_FLOAT32, P_UINT8, P_UINT16, P_UINT32, P_INT8, P_INT16, P_INT32, P_COMPRESSED, P_COUNT };

    // The core formats of one to four channels at each precision.
    const GLint kCoreFormats[P_COUNT][4] = {
        { GL_RED,             GL_RG,             GL_RGB,             GL_RGBA },
        { GL_R8,              GL_RG8,            GL_RGB8,            GL_RGBA8 },
        { GL_R16,             GL_RG16,           GL_RGB16,           GL_RGBA16 },
        { GL_R16F,            GL_RG16F,          GL_RGB16F_ARB,      GL_RGBA16F_ARB },
        { GL_R32F,            GL_RG32F,          GL_RGB32F_ARB,      GL_RGBA32F_ARB },
        { GL_R8UI,            GL_RG8UI,          GL_RGB8UI_EXT,      GL_RGBA8UI_EXT },
        { GL_R16UI,           GL_RG16UI,         GL_RGB16UI_EXT,     GL_RGBA16UI_EXT },
        { GL_R32UI,           GL_RG32UI,         GL_RGB32UI_EXT,     GL_RGBA32UI_EXT },
        { GL_R8I,             GL_RG8I,           GL_RGB8I_EXT,       GL_RGBA8I_EXT },
        { GL_R16I,            GL_RG16I,          GL_RGB16I_EXT,      GL_RGBA16I_EXT },
        { GL_R32I,            GL_RG32I,          GL_RGB32I_EXT,      GL_RGBA32I_EXT },
        { GL_COMPRESSED_RED,  GL_COMPRESSED_RG,  GL_COMPRESSED_RGB,  GL_COMPRESSED_RGBA }
    };

    struct LegacyFormat
    {
        GLint format;
        GLenum base;            // GL_LUMINANCE, GL_ALPHA, GL_LUMINANCE_ALPHA or GL_INTENSITY
        Precision precision;
    };

    // Every legacy internal format a core profile lacks. The first of a base and precision is the one a readback
    // labels its image with (selectLegacyFormat). 1 and 2 are the GL 1.0 component counts, luminance and
    // luminance-alpha. The sRGB luminance formats are left out: core has no one- or two-channel sRGB format.
    const LegacyFormat kLegacyFormats[] = {
        { GL_LUMINANCE,                 GL_LUMINANCE,       P_UNSIZED },
        { 1,                            GL_LUMINANCE,       P_UNSIZED },
        { GL_LUMINANCE8,                GL_LUMINANCE,       P_UNORM8 },
        { GL_LUMINANCE4,                GL_LUMINANCE,       P_UNORM8 },
        { GL_LUMINANCE16,               GL_LUMINANCE,       P_UNORM16 },
        { GL_LUMINANCE12,               GL_LUMINANCE,       P_UNORM16 },
        { GL_LUMINANCE16F_ARB,          GL_LUMINANCE,       P_FLOAT16 },
        { GL_LUMINANCE32F_ARB,          GL_LUMINANCE,       P_FLOAT32 },
        { GL_LUMINANCE8UI_EXT,          GL_LUMINANCE,       P_UINT8 },
        { GL_LUMINANCE16UI_EXT,         GL_LUMINANCE,       P_UINT16 },
        { GL_LUMINANCE32UI_EXT,         GL_LUMINANCE,       P_UINT32 },
        { GL_LUMINANCE8I_EXT,           GL_LUMINANCE,       P_INT8 },
        { GL_LUMINANCE16I_EXT,          GL_LUMINANCE,       P_INT16 },
        { GL_LUMINANCE32I_EXT,          GL_LUMINANCE,       P_INT32 },
        { GL_COMPRESSED_LUMINANCE,      GL_LUMINANCE,       P_COMPRESSED },

        { GL_ALPHA,                     GL_ALPHA,           P_UNSIZED },
        { GL_ALPHA8,                    GL_ALPHA,           P_UNORM8 },
        { GL_ALPHA4,                    GL_ALPHA,           P_UNORM8 },
        { GL_ALPHA16,                   GL_ALPHA,           P_UNORM16 },
        { GL_ALPHA12,                   GL_ALPHA,           P_UNORM16 },
        { GL_ALPHA16F_ARB,              GL_ALPHA,           P_FLOAT16 },
        { GL_ALPHA32F_ARB,              GL_ALPHA,           P_FLOAT32 },
        { GL_ALPHA8UI_EXT,              GL_ALPHA,           P_UINT8 },
        { GL_ALPHA16UI_EXT,             GL_ALPHA,           P_UINT16 },
        { GL_ALPHA32UI_EXT,             GL_ALPHA,           P_UINT32 },
        { GL_ALPHA8I_EXT,               GL_ALPHA,           P_INT8 },
        { GL_ALPHA16I_EXT,              GL_ALPHA,           P_INT16 },
        { GL_ALPHA32I_EXT,              GL_ALPHA,           P_INT32 },
        { GL_COMPRESSED_ALPHA,          GL_ALPHA,           P_COMPRESSED },

        { GL_LUMINANCE_ALPHA,           GL_LUMINANCE_ALPHA, P_UNSIZED },
        { 2,                            GL_LUMINANCE_ALPHA, P_UNSIZED },
        { GL_LUMINANCE8_ALPHA8,         GL_LUMINANCE_ALPHA, P_UNORM8 },
        { GL_LUMINANCE4_ALPHA4,         GL_LUMINANCE_ALPHA, P_UNORM8 },
        { GL_LUMINANCE6_ALPHA2,         GL_LUMINANCE_ALPHA, P_UNORM8 },
        { GL_LUMINANCE16_ALPHA16,       GL_LUMINANCE_ALPHA, P_UNORM16 },
        { GL_LUMINANCE12_ALPHA4,        GL_LUMINANCE_ALPHA, P_UNORM16 },
        { GL_LUMINANCE12_ALPHA12,       GL_LUMINANCE_ALPHA, P_UNORM16 },
        { GL_LUMINANCE_ALPHA16F_ARB,    GL_LUMINANCE_ALPHA, P_FLOAT16 },
        { GL_LUMINANCE_ALPHA32F_ARB,    GL_LUMINANCE_ALPHA, P_FLOAT32 },
        { GL_LUMINANCE_ALPHA8UI_EXT,    GL_LUMINANCE_ALPHA, P_UINT8 },
        { GL_LUMINANCE_ALPHA16UI_EXT,   GL_LUMINANCE_ALPHA, P_UINT16 },
        { GL_LUMINANCE_ALPHA32UI_EXT,   GL_LUMINANCE_ALPHA, P_UINT32 },
        { GL_LUMINANCE_ALPHA8I_EXT,     GL_LUMINANCE_ALPHA, P_INT8 },
        { GL_LUMINANCE_ALPHA16I_EXT,    GL_LUMINANCE_ALPHA, P_INT16 },
        { GL_LUMINANCE_ALPHA32I_EXT,    GL_LUMINANCE_ALPHA, P_INT32 },
        { GL_COMPRESSED_LUMINANCE_ALPHA, GL_LUMINANCE_ALPHA, P_COMPRESSED },

        { GL_INTENSITY,                 GL_INTENSITY,       P_UNSIZED },
        { GL_INTENSITY8,                GL_INTENSITY,       P_UNORM8 },
        { GL_INTENSITY4,                GL_INTENSITY,       P_UNORM8 },
        { GL_INTENSITY16,               GL_INTENSITY,       P_UNORM16 },
        { GL_INTENSITY12,               GL_INTENSITY,       P_UNORM16 },
        { GL_INTENSITY16F_ARB,          GL_INTENSITY,       P_FLOAT16 },
        { GL_INTENSITY32F_ARB,          GL_INTENSITY,       P_FLOAT32 },
        { GL_INTENSITY8UI_EXT,          GL_INTENSITY,       P_UINT8 },
        { GL_INTENSITY16UI_EXT,         GL_INTENSITY,       P_UINT16 },
        { GL_INTENSITY32UI_EXT,         GL_INTENSITY,       P_UINT32 },
        { GL_INTENSITY8I_EXT,           GL_INTENSITY,       P_INT8 },
        { GL_INTENSITY16I_EXT,          GL_INTENSITY,       P_INT16 },
        { GL_INTENSITY32I_EXT,          GL_INTENSITY,       P_INT32 },
        { GL_COMPRESSED_INTENSITY,      GL_INTENSITY,       P_COMPRESSED }
    };

    const LegacyFormat* findLegacyFormat(GLint internalFormat)
    {
        for (size_t i=0; i<sizeof(kLegacyFormats)/sizeof(kLegacyFormats[0]); ++i)
        {
            if (kLegacyFormats[i].format==internalFormat) return &kLegacyFormats[i];
        }
        return 0;
    }

    // The channels a non-legacy internal format samples (1 red, 2 red-green, 3 RGB, 4 RGBA and anything else).
    int coreChannelsOf(GLint internalFormat)
    {
        switch (internalFormat)
        {
            case GL_RED: case GL_R8: case GL_R8_SNORM: case GL_R16: case GL_R16_SNORM: case GL_R16F: case GL_R32F:
            case GL_R8UI: case GL_R16UI: case GL_R32UI: case GL_R8I: case GL_R16I: case GL_R32I:
            case GL_COMPRESSED_RED: case GL_COMPRESSED_RED_RGTC1_EXT: case GL_COMPRESSED_SIGNED_RED_RGTC1_EXT:
            case GL_COMPRESSED_R11_EAC: case GL_COMPRESSED_SIGNED_R11_EAC:
                return 1;
            case GL_RG: case GL_RG8: case GL_RG8_SNORM: case GL_RG16: case GL_RG16_SNORM: case GL_RG16F: case GL_RG32F:
            case GL_RG8UI: case GL_RG16UI: case GL_RG32UI: case GL_RG8I: case GL_RG16I: case GL_RG32I:
            case GL_COMPRESSED_RG: case GL_COMPRESSED_RED_GREEN_RGTC2_EXT: case GL_COMPRESSED_SIGNED_RED_GREEN_RGTC2_EXT:
            case GL_COMPRESSED_RG11_EAC: case GL_COMPRESSED_SIGNED_RG11_EAC:
                return 2;
            case 3: case GL_RGB: case GL_RGB4: case GL_RGB5: case GL_RGB8: case GL_RGB10: case GL_RGB12: case GL_RGB16:
            case GL_R3_G3_B2: case GL_RGB565: case GL_SRGB8: case GL_RGB8_SNORM: case GL_RGB16_SNORM:
            case GL_RGB16F_ARB: case GL_RGB32F_ARB:
            case GL_RGB8UI_EXT: case GL_RGB16UI_EXT: case GL_RGB32UI_EXT: case GL_RGB8I_EXT: case GL_RGB16I_EXT: case GL_RGB32I_EXT:
            case GL_COMPRESSED_RGB: case GL_COMPRESSED_SRGB: case GL_COMPRESSED_RGB_S3TC_DXT1_EXT: case GL_COMPRESSED_SRGB_S3TC_DXT1_EXT:
            case GL_COMPRESSED_RGB8_ETC2: case GL_COMPRESSED_SRGB8_ETC2: case GL_ETC1_RGB8_OES:
                return 3;
            default:
                return 4;
        }
    }

    // What a texture of `channels` channels stores of an upload in a core pixel format, as sampled through an
    // RGBA format: the client channels in order, the missing colour channels 0 and a missing alpha 1.
    Sources uploadedSources(GLenum corePixelFormat)
    {
        switch (corePixelFormat)
        {
            case GL_RED: case GL_RED_INTEGER_EXT:
                return Sources(SRC_R, SRC_ZERO, SRC_ZERO, SRC_ONE);
            case GL_RG: case GL_RG_INTEGER:
                return Sources(SRC_R, SRC_G, SRC_ZERO, SRC_ONE);
            case GL_RGB: case GL_BGR: case GL_RGB_INTEGER_EXT: case GL_BGR_INTEGER_EXT:
                return Sources(SRC_R, SRC_G, SRC_B, SRC_ONE);
            default:    // RGBA, BGRA, no client data
                return Sources(SRC_R, SRC_G, SRC_B, SRC_A);
        }
    }

    GLint swizzleOf(int source)
    {
        switch (source)
        {
            case SRC_R: return GL_RED;
            case SRC_G: return GL_GREEN;
            case SRC_B: return GL_BLUE;
            case SRC_A: return GL_ALPHA;
            case SRC_ZERO: return GL_ZERO;
            default: return GL_ONE;
        }
    }
}

GLint Texture::selectCoreFormat(GLint internalFormat, GLenum pixelFormat, Vec4i& swizzle)
{
    swizzle.set(GL_RED, GL_GREEN, GL_BLUE, GL_ALPHA);

    const LegacyFormat* legacy = findLegacyFormat(internalFormat);
    const GLenum corePixelFormat = selectCorePixelFormat(pixelFormat);

    // The GL 1.0 component counts with no legacy meaning: core has no counts at all.
    if (!legacy && internalFormat==3) internalFormat = GL_RGB;
    if (!legacy && internalFormat==4) internalFormat = GL_RGBA;
    if (!legacy && corePixelFormat==pixelFormat) return internalFormat;

    // The legacy pipeline (GL 2.1 sec. 3.6.4, 3.8.1, table 3.20): the client group becomes an RGBA group (luminance
    // L -> (L, L, L, 1), alpha A -> (0, 0, 0, A), luminance-alpha -> (L, L, L, A)), the internal format keeps its
    // components of it and sampling expands them again. `group` is that RGBA group in terms of what the core
    // upload stores, `sampled` what the legacy texture sampled.
    const Sources stored = uploadedSources(corePixelFormat);
    Sources group = stored;
    switch (pixelFormat)
    {
        case GL_LUMINANCE: case GL_INTENSITY: case GL_LUMINANCE_INTEGER_EXT:
            group = Sources(stored.s[0], stored.s[0], stored.s[0], SRC_ONE); break;
        case GL_ALPHA: case GL_ALPHA_INTEGER_EXT:
            group = Sources(SRC_ZERO, SRC_ZERO, SRC_ZERO, stored.s[0]); break;
        case GL_LUMINANCE_ALPHA: case GL_LUMINANCE_ALPHA_INTEGER_EXT:
            group = Sources(stored.s[0], stored.s[0], stored.s[0], stored.s[1]); break;
        default: break;
    }

    const int r = group.s[0], g = group.s[1], b = group.s[2], a = group.s[3];
    Sources sampled = group;
    if (legacy)
    {
        switch (legacy->base)
        {
            case GL_LUMINANCE:          sampled = Sources(r, r, r, SRC_ONE); break;
            case GL_ALPHA:              sampled = Sources(SRC_ZERO, SRC_ZERO, SRC_ZERO, a); break;
            case GL_LUMINANCE_ALPHA:    sampled = Sources(r, r, r, a); break;
            default:                    sampled = Sources(r, r, r, r); break;   // GL_INTENSITY
        }
    }
    else
    {
        switch (coreChannelsOf(internalFormat))
        {
            case 1:  sampled = Sources(r, SRC_ZERO, SRC_ZERO, SRC_ONE); break;
            case 2:  sampled = Sources(r, g, SRC_ZERO, SRC_ONE); break;
            case 3:  sampled = Sources(r, g, b, SRC_ONE); break;
            default: break;
        }
    }

    for (int i=0; i<4; ++i) swizzle[i] = swizzleOf(sampled.s[i]);

    if (!legacy) return internalFormat;

    // A legacy internal format stands in as the fewest core channels that hold what it samples, at its precision.
    int channels = 1;
    for (int i=0; i<4; ++i)
    {
        if (sampled.s[i]<=SRC_A && sampled.s[i]+1>channels) channels = sampled.s[i]+1;
    }
    return kCoreFormats[legacy->precision][channels-1];
}

GLenum Texture::selectCorePixelFormat(GLenum pixelFormat)
{
    switch (pixelFormat)
    {
        case GL_LUMINANCE: case GL_ALPHA: case GL_INTENSITY: return GL_RED;
        case GL_LUMINANCE_ALPHA: return GL_RG;
        case GL_LUMINANCE_INTEGER_EXT: case GL_ALPHA_INTEGER_EXT: return GL_RED_INTEGER_EXT;
        case GL_LUMINANCE_ALPHA_INTEGER_EXT: return GL_RG_INTEGER;
        default: return pixelFormat;
    }
}

bool Texture::selectLegacyFormat(GLint coreInternalFormat, const Vec4i& swizzle, GLint& legacyInternalFormat, GLenum& legacyPixelFormat)
{
    for (int precision=0; precision<P_COUNT; ++precision)
    {
        for (int channels=1; channels<=2; ++channels)
        {
            if (kCoreFormats[precision][channels-1]!=coreInternalFormat) continue;

            GLenum base = 0;
            if (channels==1 && swizzle==Vec4i(GL_RED, GL_RED, GL_RED, GL_ONE)) base = GL_LUMINANCE;
            else if (channels==1 && swizzle==Vec4i(GL_ZERO, GL_ZERO, GL_ZERO, GL_RED)) base = GL_ALPHA;
            else if (channels==1 && swizzle==Vec4i(GL_RED, GL_RED, GL_RED, GL_RED)) base = GL_INTENSITY;
            else if (channels==2 && swizzle==Vec4i(GL_RED, GL_RED, GL_RED, GL_GREEN)) base = GL_LUMINANCE_ALPHA;
            if (base==0) return false;

            for (size_t i=0; i<sizeof(kLegacyFormats)/sizeof(kLegacyFormats[0]); ++i)
            {
                if (kLegacyFormats[i].base!=base || kLegacyFormats[i].precision!=precision) continue;

                const bool integer = precision>=P_UINT8 && precision<=P_INT32;
                legacyInternalFormat = kLegacyFormats[i].format;
                switch (base)
                {
                    case GL_ALPHA: legacyPixelFormat = integer ? GL_ALPHA_INTEGER_EXT : GL_ALPHA; break;
                    case GL_LUMINANCE_ALPHA: legacyPixelFormat = integer ? GL_LUMINANCE_ALPHA_INTEGER_EXT : GL_LUMINANCE_ALPHA; break;
                    default: legacyPixelFormat = integer ? GL_LUMINANCE_INTEGER_EXT : GL_LUMINANCE; break;   // luminance, intensity
                }
                return true;
            }
            return false;
        }
    }
    return false;
}

GLenum Texture::selectUploadPixelFormat(GLenum pixelFormat)
{
#if defined(OSG_GL3_AVAILABLE)
    return selectCorePixelFormat(pixelFormat);
#else
    return pixelFormat;
#endif
}

GLint Texture::selectUploadInternalFormat(const Image& image)
{
#if defined(OSG_GL3_AVAILABLE)
    Vec4i swizzle;
    return selectCoreFormat(image.getInternalTextureFormat(), image.getPixelFormat(), swizzle);
#else
    return image.getInternalTextureFormat();
#endif
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Texture class implementation
//
Texture::Texture():
            _wrap_s(CLAMP),
            _wrap_t(CLAMP),
            _wrap_r(CLAMP),
            _min_filter(LINEAR_MIPMAP_LINEAR), // trilinear
            _mag_filter(LINEAR),
            _maxAnisotropy(1.0f),
            _minlod(0.0f),
            _maxlod(-1.0f),
            _lodbias(0.0f),
            _swizzle(GL_RED, GL_GREEN, GL_BLUE, GL_ALPHA),
            _useHardwareMipMapGeneration(true),
            _unrefImageDataAfterApply(false),
            _clientStorageHint(false),
            _resizeNonPowerOfTwoHint(true),
            _borderColor(0.0, 0.0, 0.0, 0.0),
            _borderWidth(0),
            _internalFormatMode(USE_IMAGE_DATA_FORMAT),
            _internalFormatType(NORMALIZED),
            _internalFormat(0),
            _sourceFormat(0),
            _sourceType(0),
            _legacyInternalFormat(0),
            _formatSwizzle(GL_RED, GL_GREEN, GL_BLUE, GL_ALPHA),
            _use_shadow_comparison(false),
            _shadow_compare_func(LEQUAL),
            _shadow_texture_mode(LUMINANCE),
            _shadow_ambient(0)
{
}

Texture::Texture(const Texture& text,const CopyOp& copyop):
            StateAttribute(text,copyop),
            _wrap_s(text._wrap_s),
            _wrap_t(text._wrap_t),
            _wrap_r(text._wrap_r),
            _min_filter(text._min_filter),
            _mag_filter(text._mag_filter),
            _maxAnisotropy(text._maxAnisotropy),
            _minlod(text._minlod),
            _maxlod(text._maxlod),
            _lodbias(text._lodbias),
            _swizzle(text._swizzle),
            _useHardwareMipMapGeneration(text._useHardwareMipMapGeneration),
            _unrefImageDataAfterApply(text._unrefImageDataAfterApply),
            _clientStorageHint(text._clientStorageHint),
            _resizeNonPowerOfTwoHint(text._resizeNonPowerOfTwoHint),
            _borderColor(text._borderColor),
            _borderWidth(text._borderWidth),
            _internalFormatMode(text._internalFormatMode),
            _internalFormatType(text._internalFormatType),
            _internalFormat(text._internalFormat),
            _sourceFormat(text._sourceFormat),
            _sourceType(text._sourceType),
            _legacyInternalFormat(text._legacyInternalFormat),
            _formatSwizzle(text._formatSwizzle),
            _use_shadow_comparison(text._use_shadow_comparison),
            _shadow_compare_func(text._shadow_compare_func),
            _shadow_texture_mode(text._shadow_texture_mode),
            _shadow_ambient(text._shadow_ambient)
{
}

Texture::~Texture()
{
    // delete old texture objects.
    dirtyTextureObject();
}

int Texture::compareTexture(const Texture& rhs) const
{
    COMPARE_StateAttribute_Parameter(_wrap_s)
    COMPARE_StateAttribute_Parameter(_wrap_t)
    COMPARE_StateAttribute_Parameter(_wrap_r)
    COMPARE_StateAttribute_Parameter(_min_filter)
    COMPARE_StateAttribute_Parameter(_mag_filter)
    COMPARE_StateAttribute_Parameter(_maxAnisotropy)
    COMPARE_StateAttribute_Parameter(_minlod)
    COMPARE_StateAttribute_Parameter(_maxlod)
    COMPARE_StateAttribute_Parameter(_lodbias)
    COMPARE_StateAttribute_Parameter(_swizzle)
    COMPARE_StateAttribute_Parameter(_useHardwareMipMapGeneration)
    COMPARE_StateAttribute_Parameter(_internalFormatMode)

    // only compare _internalFomat is it has already been set in both lhs, and rhs
    if (_internalFormat!=0 && rhs._internalFormat!=0)
    {
        COMPARE_StateAttribute_Parameter(_internalFormat)
        // GL core native: GL_LUMINANCE8 and GL_ALPHA8 both stand in as GL_R8 and differ in their swizzle.
        COMPARE_StateAttribute_Parameter(_legacyInternalFormat)
        COMPARE_StateAttribute_Parameter(_formatSwizzle)
    }

    COMPARE_StateAttribute_Parameter(_sourceFormat)
    COMPARE_StateAttribute_Parameter(_sourceType)

    COMPARE_StateAttribute_Parameter(_use_shadow_comparison)
    COMPARE_StateAttribute_Parameter(_shadow_compare_func)
    COMPARE_StateAttribute_Parameter(_shadow_texture_mode)
    COMPARE_StateAttribute_Parameter(_shadow_ambient)

    COMPARE_StateAttribute_Parameter(_unrefImageDataAfterApply)
    COMPARE_StateAttribute_Parameter(_clientStorageHint)
    COMPARE_StateAttribute_Parameter(_resizeNonPowerOfTwoHint)

    COMPARE_StateAttribute_Parameter(_internalFormatType);

    return 0;
}

int Texture::compareTextureObjects(const Texture& rhs) const
{
    if (_textureObjectBuffer.size()<rhs._textureObjectBuffer.size()) return -1;
    if (rhs._textureObjectBuffer.size()<_textureObjectBuffer.size()) return 1;
    for(unsigned int i=0; i<_textureObjectBuffer.size(); ++i)
    {
        if (_textureObjectBuffer[i] < rhs._textureObjectBuffer[i]) return -1;
        else if (rhs._textureObjectBuffer[i] < _textureObjectBuffer[i]) return 1;
    }
    return 0;
}

void Texture::setWrap(WrapParameter which, WrapMode wrap)
{
    switch( which )
    {
        case WRAP_S : _wrap_s = wrap; dirtyTextureParameters(); break;
        case WRAP_T : _wrap_t = wrap; dirtyTextureParameters(); break;
        case WRAP_R : _wrap_r = wrap; dirtyTextureParameters(); break;
        default : OSG_WARN<<"Error: invalid 'which' passed Texture::setWrap("<<(unsigned int)which<<","<<(unsigned int)wrap<<")"<<std::endl; break;
    }
}


Texture::WrapMode Texture::getWrap(WrapParameter which) const
{
    switch( which )
    {
        case WRAP_S : return _wrap_s;
        case WRAP_T : return _wrap_t;
        case WRAP_R : return _wrap_r;
        default : OSG_WARN<<"Error: invalid 'which' passed Texture::getWrap(which)"<<std::endl; return _wrap_s;
    }
}


void Texture::setFilter(FilterParameter which, FilterMode filter)
{
    switch( which )
    {
        case MIN_FILTER : _min_filter = filter; dirtyTextureParameters(); break;
        case MAG_FILTER : _mag_filter = filter; dirtyTextureParameters(); break;
        default : OSG_WARN<<"Error: invalid 'which' passed Texture::setFilter("<<(unsigned int)which<<","<<(unsigned int)filter<<")"<<std::endl; break;
    }
}


Texture::FilterMode Texture::getFilter(FilterParameter which) const
{
    switch( which )
    {
        case MIN_FILTER : return _min_filter;
        case MAG_FILTER : return _mag_filter;
        default : OSG_WARN<<"Error: invalid 'which' passed Texture::getFilter(which)"<< std::endl; return _min_filter;
    }
}

void Texture::setMaxAnisotropy(float anis)
{
    if (_maxAnisotropy!=anis)
    {
        _maxAnisotropy = anis;
        dirtyTextureParameters();
    }
}

void Texture::setMinLOD(float anis)
{
    if (_minlod!=anis)
    {
        _minlod = anis;
        dirtyTextureParameters();
    }
}

void Texture::setMaxLOD(float anis)
{
    if (_maxlod!=anis)
    {
        _maxlod = anis;
        dirtyTextureParameters();
    }
}

void Texture::setLODBias(float anis)
{
    if (_lodbias!=anis)
    {
        _lodbias = anis;
        dirtyTextureParameters();
    }
}

/** Force a recompile on next apply() of associated OpenGL texture objects.*/
void Texture::dirtyTextureObject()
{
    for(unsigned int i=0; i<_textureObjectBuffer.size();++i)
    {
        if (_textureObjectBuffer[i].valid())
        {
            _textureObjectBuffer[i]->release();
            _textureObjectBuffer[i] = 0;
        }
    }
}

void Texture::dirtyTextureParameters()
{
    _texParametersDirtyList.setAllElementsTo(1);
}

void Texture::allocateMipmapLevels()
{
    _texMipmapGenerationDirtyList.setAllElementsTo(1);
}

void Texture::computeInternalFormatWithImage(const osg::Image& image) const
{
    GLint internalFormat = image.getInternalTextureFormat();

    if (_internalFormatMode==USE_IMAGE_DATA_FORMAT)
    {
        internalFormat = image.getInternalTextureFormat();
    }
    else if (_internalFormatMode==USE_USER_DEFINED_FORMAT)
    {
        internalFormat = _internalFormat;
    }
    else
    {

        const unsigned int contextID = 0; // state.getContextID();  // set to 0 right now, assume same parameters for each graphics context...
        const GLExtensions* extensions = GLExtensions::Get(contextID,true);

        switch(_internalFormatMode)
        {
        case(USE_ARB_COMPRESSION):
            if (extensions->isTextureCompressionARBSupported)
            {
                switch(image.getPixelFormat())
                {
                    case(1): internalFormat = GL_COMPRESSED_ALPHA_ARB; break;
                    case(2): internalFormat = GL_COMPRESSED_LUMINANCE_ALPHA_ARB; break;
                    case(3): internalFormat = GL_COMPRESSED_RGB_ARB; break;
                    case(4): internalFormat = GL_COMPRESSED_RGBA_ARB; break;
                    case(GL_RGB): internalFormat = GL_COMPRESSED_RGB_ARB; break;
                    case(GL_RGBA): internalFormat = GL_COMPRESSED_RGBA_ARB; break;
                    case(GL_ALPHA): internalFormat = GL_COMPRESSED_ALPHA_ARB; break;
                    case(GL_LUMINANCE): internalFormat = GL_COMPRESSED_LUMINANCE_ARB; break;
                    case(GL_LUMINANCE_ALPHA): internalFormat = GL_COMPRESSED_LUMINANCE_ALPHA_ARB; break;
                    case(GL_INTENSITY): internalFormat = GL_COMPRESSED_INTENSITY_ARB; break;
                }
            }
            break;

        case(USE_S3TC_DXT1_COMPRESSION):
            if (extensions->isTextureCompressionS3TCSupported)
            {
                switch(image.getPixelFormat())
                {
                    case(3):        internalFormat = GL_COMPRESSED_RGB_S3TC_DXT1_EXT; break;
                    case(4):        internalFormat = GL_COMPRESSED_RGBA_S3TC_DXT1_EXT; break;
                    case(GL_RGB):   internalFormat = GL_COMPRESSED_RGB_S3TC_DXT1_EXT; break;
                    case(GL_RGBA):  internalFormat = GL_COMPRESSED_RGBA_S3TC_DXT1_EXT; break;
                    default:        internalFormat = image.getInternalTextureFormat(); break;
                }
            }
            break;

        case(USE_S3TC_DXT1c_COMPRESSION):
            if (extensions->isTextureCompressionS3TCSupported)
            {
                switch(image.getPixelFormat())
                {
                    case(3):
                    case(4):
                    case(GL_RGB):
                    case(GL_RGBA):  internalFormat = GL_COMPRESSED_RGB_S3TC_DXT1_EXT; break;
                    default:        internalFormat = image.getInternalTextureFormat(); break;
                }
            }
            break;

        case(USE_S3TC_DXT1a_COMPRESSION):
            if (extensions->isTextureCompressionS3TCSupported)
            {
                switch(image.getPixelFormat())
                {
                    case(3):
                    case(4):
                    case(GL_RGB):
                    case(GL_RGBA):  internalFormat = GL_COMPRESSED_RGBA_S3TC_DXT1_EXT; break;
                    default:        internalFormat = image.getInternalTextureFormat(); break;
                }
            }
            break;

        case(USE_S3TC_DXT3_COMPRESSION):
            if (extensions->isTextureCompressionS3TCSupported)
            {
                switch(image.getPixelFormat())
                {
                    case(3):
                    case(GL_RGB):   internalFormat = GL_COMPRESSED_RGB_S3TC_DXT1_EXT; break;
                    case(4):
                    case(GL_RGBA):  internalFormat = GL_COMPRESSED_RGBA_S3TC_DXT3_EXT; break;
                    default:        internalFormat = image.getInternalTextureFormat(); break;
                }
            }
            break;

        case(USE_S3TC_DXT5_COMPRESSION):
            if (extensions->isTextureCompressionS3TCSupported)
            {
                switch(image.getPixelFormat())
                {
                    case(3):
                    case(GL_RGB):   internalFormat = GL_COMPRESSED_RGB_S3TC_DXT1_EXT; break;
                    case(4):
                    case(GL_RGBA):  internalFormat = GL_COMPRESSED_RGBA_S3TC_DXT5_EXT; break;
                    default:        internalFormat = image.getInternalTextureFormat(); break;
                }
            }
            break;

        case(USE_PVRTC_2BPP_COMPRESSION):
            if (extensions->isTextureCompressionPVRTC2BPPSupported)
            {
                switch(image.getPixelFormat())
                {
                case(3):
                case(GL_RGB):   internalFormat = GL_COMPRESSED_RGB_PVRTC_2BPPV1_IMG; break;
                case(4):
                case(GL_RGBA):  internalFormat = GL_COMPRESSED_RGBA_PVRTC_2BPPV1_IMG; break;
                default:        internalFormat = image.getInternalTextureFormat(); break;
                }
            }
            break;

        case(USE_PVRTC_4BPP_COMPRESSION):
            if (extensions->isTextureCompressionPVRTC4BPPSupported)
            {
                switch(image.getPixelFormat())
                {
                case(3):
                case(GL_RGB):   internalFormat = GL_COMPRESSED_RGB_PVRTC_4BPPV1_IMG; break;
                case(4):
                case(GL_RGBA):  internalFormat = GL_COMPRESSED_RGBA_PVRTC_4BPPV1_IMG; break;
                default:        internalFormat = image.getInternalTextureFormat(); break;
                }
            }
            break;

        case(USE_ETC_COMPRESSION):
            if (extensions->isTextureCompressionETCSupported)
            {
                switch(image.getPixelFormat())
                {
                case(3):
                case(GL_RGB):   internalFormat = GL_ETC1_RGB8_OES; break;
                default:        internalFormat = image.getInternalTextureFormat(); break;
                }
            }
            break;

        case(USE_ETC2_COMPRESSION):
            if (extensions->isTextureCompressionETC2Supported)
            {
                switch(image.getPixelFormat())
                {
                    case(1):
                    case(GL_RED):   internalFormat = GL_COMPRESSED_R11_EAC; break;
                    case(2):
                    case(GL_RG):   internalFormat = GL_COMPRESSED_RG11_EAC; break;
                    case(3):
                    case(GL_RGB):   internalFormat = GL_COMPRESSED_RGB8_ETC2; break;
                    case(4):
                    case(GL_RGBA):  internalFormat = GL_COMPRESSED_RGBA8_ETC2_EAC; break;
                    default:        internalFormat = image.getInternalTextureFormat(); break;
                }
            }
            break;

        case(USE_RGTC1_COMPRESSION):
            if (extensions->isTextureCompressionRGTCSupported)
            {
                switch(image.getPixelFormat())
                {
                    case(3):
                    case(GL_RGB):   internalFormat = GL_COMPRESSED_RED_RGTC1_EXT; break;
                    case(4):
                    case(GL_RGBA):  internalFormat = GL_COMPRESSED_RED_RGTC1_EXT; break;
                    default:        internalFormat = image.getInternalTextureFormat(); break;
                }
            }
            break;

        case(USE_RGTC2_COMPRESSION):
            if (extensions->isTextureCompressionRGTCSupported)
            {
                switch(image.getPixelFormat())
                {
                    case(3):
                    case(GL_RGB):   internalFormat = GL_COMPRESSED_RED_GREEN_RGTC2_EXT; break;
                    case(4):
                    case(GL_RGBA):  internalFormat = GL_COMPRESSED_RED_GREEN_RGTC2_EXT; break;
                    default:        internalFormat = image.getInternalTextureFormat(); break;
                }
            }
            break;

        default:
            break;
        }
    }

#if defined (OSG_GLES1_AVAILABLE) || defined (OSG_GLES2_AVAILABLE) || defined (OSG_GLES3_AVAILABLE)
    // GLES doesn't cope with internal formats of 1,2,3 and 4 and glTexImage doesn't
    // handle the _OES pixel formats so map them to the appropriate equivilants.
    switch(internalFormat)
    {
        case(1) : internalFormat = GL_LUMINANCE; break;
        case(2) : internalFormat = GL_LUMINANCE_ALPHA; break;
        case(3) : internalFormat = GL_RGB; break;
        case(4) : internalFormat = GL_RGBA; break;
        case(GL_RGB8_OES) : internalFormat = GL_RGB; break;
        case(GL_RGBA8_OES) : internalFormat = GL_RGBA; break;
        default: break;
    }
#endif

    // GL core native: a user-defined legacy format was mapped when it was set (setInternalFormat); map the request
    // again, now with this image's pixel format. On a build with legacy formats this sets internalFormat as is.
    if (_internalFormatMode==USE_USER_DEFINED_FORMAT && _legacyInternalFormat!=0) internalFormat = _legacyInternalFormat;
    applyCoreFormat(internalFormat, image.getPixelFormat());

    _internalFormatType = internalFormatTypeOf(_internalFormat);

    //OSG_NOTICE<<"Internal format="<<std::hex<<internalFormat<<std::dec<<std::endl;
}

void Texture::computeInternalFormatType() const
{
    // GL core native: no image, so the source format is the pixel format of any data a subload brings.
    applyCoreFormat(_legacyInternalFormat!=0 ? _legacyInternalFormat : _internalFormat, _sourceFormat);

    _internalFormatType = internalFormatTypeOf(_internalFormat);
}

void Texture::applyCoreFormat(GLint requestedInternalFormat, GLenum pixelFormat) const
{
#if defined(OSG_GL3_AVAILABLE)
    Vec4i swizzle;
    _internalFormat = selectCoreFormat(requestedInternalFormat, pixelFormat, swizzle);
    _legacyInternalFormat = findLegacyFormat(requestedInternalFormat)!=0 ? requestedInternalFormat : 0;
    if (swizzle!=_formatSwizzle)
    {
        _formatSwizzle = swizzle;
        _texParametersDirtyList.setAllElementsTo(1);
    }
#else
    (void)pixelFormat;
    _internalFormat = requestedInternalFormat;
#endif
}

Texture::InternalFormatType Texture::internalFormatTypeOf(GLint internalFormat)
{
    // Here we could also precompute the _sourceFormat if it is not set,
    // since it is different for different internal formats
    // (i.e. rgba integer texture --> _sourceFormat = GL_RGBA_INTEGER_EXT)
    // Should we do this?  ( Art, 09. Sept. 2007)

    // compute internal format type based on the internal format
    switch(internalFormat)
    {
        case GL_RGBA32UI_EXT:
        case GL_RGBA16UI_EXT:
        case GL_RGBA8UI_EXT:

        case GL_RGB32UI_EXT:
        case GL_RGB16UI_EXT:
        case GL_RGB8UI_EXT:

        case GL_RG32UI:
        case GL_RG16UI:
        case GL_RG8UI:

        case GL_R32UI:
        case GL_R16UI:
        case GL_R8UI:

        case GL_LUMINANCE32UI_EXT:
        case GL_LUMINANCE16UI_EXT:
        case GL_LUMINANCE8UI_EXT:

        case GL_INTENSITY32UI_EXT:
        case GL_INTENSITY16UI_EXT:
        case GL_INTENSITY8UI_EXT:

        case GL_LUMINANCE_ALPHA32UI_EXT:
        case GL_LUMINANCE_ALPHA16UI_EXT:
        case GL_LUMINANCE_ALPHA8UI_EXT :
            return UNSIGNED_INTEGER;

        case GL_RGBA32I_EXT:
        case GL_RGBA16I_EXT:
        case GL_RGBA8I_EXT:

        case GL_RGB32I_EXT:
        case GL_RGB16I_EXT:
        case GL_RGB8I_EXT:

        case GL_RG32I:
        case GL_RG16I:
        case GL_RG8I:

        case GL_R32I:
        case GL_R16I:
        case GL_R8I:

        case GL_LUMINANCE32I_EXT:
        case GL_LUMINANCE16I_EXT:
        case GL_LUMINANCE8I_EXT:

        case GL_INTENSITY32I_EXT:
        case GL_INTENSITY16I_EXT:
        case GL_INTENSITY8I_EXT:

        case GL_LUMINANCE_ALPHA32I_EXT:
        case GL_LUMINANCE_ALPHA16I_EXT:
        case GL_LUMINANCE_ALPHA8I_EXT:
            return SIGNED_INTEGER;

        case GL_RGBA32F_ARB:
        case GL_RGBA16F_ARB:

        case GL_RGB32F_ARB:
        case GL_RGB16F_ARB:

        case GL_RG32F:
        case GL_RG16F:

        case GL_R32F:
        case GL_R16F:

        case GL_LUMINANCE32F_ARB:
        case GL_LUMINANCE16F_ARB:

        case GL_INTENSITY32F_ARB:
        case GL_INTENSITY16F_ARB:

        case GL_LUMINANCE_ALPHA32F_ARB:
        case GL_LUMINANCE_ALPHA16F_ARB:
            return FLOAT;

        default:
            return NORMALIZED;
    };
}

bool Texture::isCompressedInternalFormat() const
{
    return isCompressedInternalFormat(getInternalFormat());
}

bool Texture::isCompressedInternalFormat(GLint internalFormat)
{
    switch (internalFormat)
    {
        case (GL_COMPRESSED_ALPHA_ARB):
        case (GL_COMPRESSED_INTENSITY_ARB):
        case (GL_COMPRESSED_LUMINANCE_ALPHA_ARB):
        case (GL_COMPRESSED_LUMINANCE_ARB):
        case (GL_COMPRESSED_RGBA_ARB):
        case (GL_COMPRESSED_RGB_ARB):
        case (GL_COMPRESSED_RGB_S3TC_DXT1_EXT):
        case (GL_COMPRESSED_RGBA_S3TC_DXT1_EXT):
        case (GL_COMPRESSED_RGBA_S3TC_DXT3_EXT):
        case (GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT):
        case (GL_COMPRESSED_RGBA_S3TC_DXT5_EXT):
        case (GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT):
        case (GL_COMPRESSED_SIGNED_RED_RGTC1_EXT):
        case (GL_COMPRESSED_RED_RGTC1_EXT):
        case (GL_COMPRESSED_SIGNED_RED_GREEN_RGTC2_EXT):
        case (GL_COMPRESSED_RED_GREEN_RGTC2_EXT):
        case (GL_ETC1_RGB8_OES):
        case (GL_COMPRESSED_RGB8_ETC2):
        case (GL_COMPRESSED_SRGB8_ETC2):
        case (GL_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2):
        case (GL_COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2):
        case (GL_COMPRESSED_RGBA8_ETC2_EAC):
        case (GL_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC):
        case (GL_COMPRESSED_R11_EAC):
        case (GL_COMPRESSED_SIGNED_R11_EAC):
        case (GL_COMPRESSED_RG11_EAC):
        case (GL_COMPRESSED_SIGNED_RG11_EAC):
        case (GL_COMPRESSED_RGB_PVRTC_4BPPV1_IMG):
        case (GL_COMPRESSED_RGB_PVRTC_2BPPV1_IMG):
        case (GL_COMPRESSED_RGBA_PVRTC_4BPPV1_IMG):
        case (GL_COMPRESSED_RGBA_PVRTC_2BPPV1_IMG):
        case (GL_COMPRESSED_RGBA_ASTC_4x4_KHR):
        case (GL_COMPRESSED_RGBA_ASTC_5x4_KHR):
        case (GL_COMPRESSED_RGBA_ASTC_5x5_KHR):
        case (GL_COMPRESSED_RGBA_ASTC_6x5_KHR):
        case (GL_COMPRESSED_RGBA_ASTC_6x6_KHR):
        case (GL_COMPRESSED_RGBA_ASTC_8x5_KHR):
        case (GL_COMPRESSED_RGBA_ASTC_8x6_KHR):
        case (GL_COMPRESSED_RGBA_ASTC_8x8_KHR):
        case (GL_COMPRESSED_RGBA_ASTC_10x5_KHR):
        case (GL_COMPRESSED_RGBA_ASTC_10x6_KHR):
        case (GL_COMPRESSED_RGBA_ASTC_10x8_KHR):
        case (GL_COMPRESSED_RGBA_ASTC_10x10_KHR):
        case (GL_COMPRESSED_RGBA_ASTC_12x10_KHR):
        case (GL_COMPRESSED_RGBA_ASTC_12x12_KHR):
        case (GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4_KHR):
        case (GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x4_KHR):
        case (GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x5_KHR):
        case (GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x5_KHR):
        case (GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x6_KHR):
        case (GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x5_KHR):
        case (GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x6_KHR):
        case (GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x8_KHR):
        case (GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x5_KHR):
        case (GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x6_KHR):
        case (GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x8_KHR):
        case (GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x10_KHR):
        case (GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x10_KHR):
        case (GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x12_KHR):
            return true;
        default:
            return false;
    }
}

void Texture::getCompressedSize(GLenum internalFormat, GLint width, GLint height, GLint depth, GLint& blockSize, GLint& size)
{
    if (internalFormat == GL_COMPRESSED_RGB_S3TC_DXT1_EXT || internalFormat == GL_COMPRESSED_RGBA_S3TC_DXT1_EXT)
        blockSize = 8;
    else if (internalFormat == GL_COMPRESSED_RGBA_S3TC_DXT3_EXT || internalFormat == GL_COMPRESSED_RGBA_S3TC_DXT5_EXT)
        blockSize = 16;
    else if (internalFormat == GL_ETC1_RGB8_OES)
        blockSize = 8;
    else if (internalFormat == GL_COMPRESSED_RGB8_ETC2 || internalFormat == GL_COMPRESSED_SRGB8_ETC2)
        blockSize = 8;
    else if (internalFormat == GL_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2 || internalFormat == GL_COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2)
        blockSize = 8;
    else if (internalFormat == GL_COMPRESSED_RGBA8_ETC2_EAC || internalFormat == GL_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC)
        blockSize = 16;
    else if (internalFormat == GL_COMPRESSED_R11_EAC || internalFormat == GL_COMPRESSED_SIGNED_R11_EAC)
        blockSize = 8;
    else if (internalFormat == GL_COMPRESSED_RG11_EAC || internalFormat == GL_COMPRESSED_SIGNED_RG11_EAC)
        blockSize = 16;
    else if (internalFormat == GL_COMPRESSED_RED_RGTC1_EXT || internalFormat == GL_COMPRESSED_SIGNED_RED_RGTC1_EXT)
        blockSize = 8;
    else if (internalFormat == GL_COMPRESSED_RED_GREEN_RGTC2_EXT || internalFormat == GL_COMPRESSED_SIGNED_RED_GREEN_RGTC2_EXT)
        blockSize = 16;
    else if (internalFormat == GL_COMPRESSED_RGBA_PVRTC_2BPPV1_IMG || internalFormat == GL_COMPRESSED_RGB_PVRTC_2BPPV1_IMG)
    {
         blockSize = 8 * 4; // Pixel by pixel block size for 2bpp
         GLint widthBlocks = width / 8;
         GLint heightBlocks = height / 4;
         GLint bpp = 2;

         // Clamp to minimum number of blocks
         if(widthBlocks < 2)
             widthBlocks = 2;
         if(heightBlocks < 2)
             heightBlocks = 2;

         size = widthBlocks * heightBlocks * ((blockSize  * bpp) / 8);
         return;
    }
    else if (internalFormat == GL_COMPRESSED_RGBA_PVRTC_4BPPV1_IMG || internalFormat == GL_COMPRESSED_RGB_PVRTC_4BPPV1_IMG)
    {
         blockSize = 4 * 4; // Pixel by pixel block size for 4bpp
         GLint widthBlocks = width / 4;
         GLint heightBlocks = height / 4;
         GLint bpp = 4;

         // Clamp to minimum number of blocks
         if(widthBlocks < 2)
             widthBlocks = 2;
         if(heightBlocks < 2)
             heightBlocks = 2;

         size = widthBlocks * heightBlocks * ((blockSize  * bpp) / 8);
         return;
    }
    // ASTC compression (block size is always equal to 16)
    else if (internalFormat == GL_COMPRESSED_RGBA_ASTC_4x4_KHR || internalFormat == GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4_KHR)
    {
        blockSize = 16;
        size = ceil(width/4.0)*ceil(height/4.0)*blockSize;
        return;
    }
    else if (internalFormat == GL_COMPRESSED_RGBA_ASTC_5x4_KHR || internalFormat == GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x4_KHR)
    {
        blockSize = 16;
        size = ceil(width/5.0)*ceil(height/4.0)*blockSize;
        return;
    }
    else if (internalFormat == GL_COMPRESSED_RGBA_ASTC_5x5_KHR || internalFormat == GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x5_KHR)
    {
        blockSize = 16;
        size = ceil(width/5.0)*ceil(height/5.0)*blockSize;
        return;
    }
    else if (internalFormat == GL_COMPRESSED_RGBA_ASTC_6x5_KHR || internalFormat == GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x5_KHR)
    {
        blockSize = 16;
        size = ceil(width/6.0)*ceil(height/5.0)*blockSize;
        return;
    }
    else if (internalFormat == GL_COMPRESSED_RGBA_ASTC_6x6_KHR || internalFormat == GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x6_KHR)
    {
        blockSize = 16;
        size = ceil(width/6.0)*ceil(height/6.0)*blockSize;
        return;
    }
    else if (internalFormat == GL_COMPRESSED_RGBA_ASTC_8x5_KHR || internalFormat == GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x5_KHR)
    {
        blockSize = 16;
        size = ceil(width/8.0)*ceil(height/5.0)*blockSize;
        return;
    }
    else if (internalFormat == GL_COMPRESSED_RGBA_ASTC_8x6_KHR || internalFormat == GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x6_KHR)
    {
        blockSize = 16;
        size = ceil(width/8.0)*ceil(height/6.0)*blockSize;
        return;
    }
    else if (internalFormat == GL_COMPRESSED_RGBA_ASTC_8x8_KHR || internalFormat == GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x8_KHR)
    {
        blockSize = 16;
        size = ceil(width/8.0)*ceil(height/8.0)*blockSize;
        return;
    }
    else if (internalFormat == GL_COMPRESSED_RGBA_ASTC_10x5_KHR || internalFormat == GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x5_KHR)
    {
        blockSize = 16;
        size = ceil(width/10.0)*ceil(height/5.0)*blockSize;
        return;
    }
    else if (internalFormat == GL_COMPRESSED_RGBA_ASTC_10x6_KHR || internalFormat == GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x6_KHR)
    {
        blockSize = 16;
        size = ceil(width/10.0)*ceil(height/6.0)*blockSize;
        return;
    }
    else if (internalFormat == GL_COMPRESSED_RGBA_ASTC_10x8_KHR || internalFormat == GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x8_KHR)
    {
        blockSize = 16;
        size = ceil(width/10.0)*ceil(height/8.0)*blockSize;
        return;
    }
    else if (internalFormat == GL_COMPRESSED_RGBA_ASTC_10x10_KHR || internalFormat == GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x10_KHR)
    {
        blockSize = 16;
        size = ceil(width/10.0)*ceil(height/10.0)*blockSize;
        return;
    }
    else if (internalFormat == GL_COMPRESSED_RGBA_ASTC_12x10_KHR || internalFormat == GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x10_KHR)
    {
        blockSize = 16;
        size = ceil(width/12.0)*ceil(height/10.0)*blockSize;
        return;
    }
    else if (internalFormat == GL_COMPRESSED_RGBA_ASTC_12x12_KHR || internalFormat == GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x12_KHR)
    {
        blockSize = 16;
        size = ceil(width/12.0)*ceil(height/12.0)*blockSize;
        return;
    }
    else
    {
        OSG_WARN<<"Texture::getCompressedSize(...) : cannot compute correct size of compressed format ("<<internalFormat<<") returning 0."<<std::endl;
        blockSize = 0;
    }

    size = ((width+3)/4)*((height+3)/4)*depth*blockSize;
}

void Texture::applyTexParameters(GLenum target, State& state) const
{
    // get the contextID (user defined ID of 0 upwards) for the
    // current OpenGL context.
    const unsigned int contextID = state.getContextID();
    const GLExtensions* extensions = state.get<GLExtensions>();

    TextureObject* to = getTextureObject(contextID);
    if (to)
    {
        extensions->debugObjectLabel(GL_TEXTURE, to->id(), getName());
    }


    WrapMode ws = _wrap_s, wt = _wrap_t, wr = _wrap_r;

    // GL_IBM_texture_mirrored_repeat, fall-back REPEAT
    if (!extensions->isTextureMirroredRepeatSupported)
    {
        if (ws == MIRROR)
            ws = REPEAT;
        if (wt == MIRROR)
            wt = REPEAT;
        if (wr == MIRROR)
            wr = REPEAT;
    }

    // GL_EXT_texture_edge_clamp, fall-back CLAMP
    if (!extensions->isTextureEdgeClampSupported)
    {
        if (ws == CLAMP_TO_EDGE)
            ws = CLAMP;
        if (wt == CLAMP_TO_EDGE)
            wt = CLAMP;
        if (wr == CLAMP_TO_EDGE)
            wr = CLAMP;
    }

    if(!extensions->isTextureBorderClampSupported)
    {
        if(ws == CLAMP_TO_BORDER)
            ws = CLAMP;
        if(wt == CLAMP_TO_BORDER)
            wt = CLAMP;
        if(wr == CLAMP_TO_BORDER)
            wr = CLAMP;
    }

    #if defined(OSG_GLES1_AVAILABLE) || defined(OSG_GLES2_AVAILABLE) || defined(OSG_GLES3_AVAILABLE) || defined(OSG_GL3_AVAILABLE)
        if (ws == CLAMP) ws = CLAMP_TO_EDGE;
        if (wt == CLAMP) wt = CLAMP_TO_EDGE;
        if (wr == CLAMP) wr = CLAMP_TO_EDGE;
    #endif

    const Image * image = getImage(0);
    if( image &&
        image->isMipmap() &&
        extensions->isTextureMaxLevelSupported &&
        int( image->getNumMipmapLevels() ) <
            Image::computeNumberOfMipmapLevels( image->s(), image->t(), image->r() ) )
            glTexParameteri( target, GL_TEXTURE_MAX_LEVEL, image->getNumMipmapLevels() - 1 );


    glTexParameteri( target, GL_TEXTURE_WRAP_S, ws );

    if (target!=GL_TEXTURE_1D) glTexParameteri( target, GL_TEXTURE_WRAP_T, wt );

    if (target==GL_TEXTURE_3D) glTexParameteri( target, GL_TEXTURE_WRAP_R, wr );


    glTexParameteri( target, GL_TEXTURE_MIN_FILTER, _min_filter);
    glTexParameteri( target, GL_TEXTURE_MAG_FILTER, _mag_filter);

    // Art: I think anisotropic filtering is not supported by the integer textures
    if (extensions->isTextureFilterAnisotropicSupported &&
        _internalFormatType != SIGNED_INTEGER && _internalFormatType != UNSIGNED_INTEGER)
    {
        // note, GL_TEXTURE_MAX_ANISOTROPY_EXT will either be defined
        // by gl.h (or via glext.h) or by include/osg/Texture.
        glTexParameterf(target, GL_TEXTURE_MAX_ANISOTROPY_EXT, _maxAnisotropy);
    }

    if (extensions->isTextureSwizzleSupported)
    {
        // note, GL_TEXTURE_SWIZZLE_RGBA will either be defined
        // by gl.h (or via glext.h) or by include/osg/Texture.
        // GL core native: the user's swizzle reads the channels as the legacy format had them (_formatSwizzle).
        Vec4i swizzle = _swizzle;
        for (int i=0; i<4; ++i)
        {
            if (swizzle[i]>=GL_RED && swizzle[i]<=GL_ALPHA) swizzle[i] = _formatSwizzle[swizzle[i]-GL_RED];
        }
        glTexParameteriv(target, GL_TEXTURE_SWIZZLE_RGBA, swizzle.ptr());
    }
    else if (_legacyInternalFormat!=0)
    {
        OSG_NOTICE<<"Warning: Texture::applyTexParameters(..) no texture swizzle on this context: a legacy format texture ("
                  <<std::hex<<_legacyInternalFormat<<std::dec<<") samples as its core stand-in."<<std::endl;
    }

    if (extensions->isTextureBorderClampSupported)
    {

        #ifndef GL_TEXTURE_BORDER_COLOR
            #define GL_TEXTURE_BORDER_COLOR     0x1004
        #endif


        if (_internalFormatType == SIGNED_INTEGER)
        {
            GLint color[4] = {(GLint)_borderColor.r(), (GLint)_borderColor.g(), (GLint)_borderColor.b(), (GLint)_borderColor.a()};
            extensions->glTexParameterIiv(target, GL_TEXTURE_BORDER_COLOR, color);
        }else if (_internalFormatType == UNSIGNED_INTEGER)
        {
            GLuint color[4] = {(GLuint)_borderColor.r(), (GLuint)_borderColor.g(), (GLuint)_borderColor.b(), (GLuint)_borderColor.a()};
            extensions->glTexParameterIuiv(target, GL_TEXTURE_BORDER_COLOR, color);
        }else{
            GLfloat color[4] = {(GLfloat)_borderColor.r(), (GLfloat)_borderColor.g(), (GLfloat)_borderColor.b(), (GLfloat)_borderColor.a()};
            glTexParameterfv(target, GL_TEXTURE_BORDER_COLOR, color);
        }
    }

    // integer textures are not supported by the shadow
    // GL_TEXTURE_1D_ARRAY_EXT could be included in the check below but its not yet implemented in OSG
    if (extensions->isShadowSupported &&
        (target == GL_TEXTURE_2D || target == GL_TEXTURE_1D || target == GL_TEXTURE_RECTANGLE || target == GL_TEXTURE_CUBE_MAP || target == GL_TEXTURE_2D_ARRAY ) &&
        _internalFormatType != SIGNED_INTEGER && _internalFormatType != UNSIGNED_INTEGER)
    {
        if (_use_shadow_comparison)
        {
            glTexParameteri(target, GL_TEXTURE_COMPARE_MODE_ARB, GL_COMPARE_R_TO_TEXTURE_ARB);
            glTexParameteri(target, GL_TEXTURE_COMPARE_FUNC_ARB, _shadow_compare_func);
            #if defined(OSG_GL1_AVAILABLE) || defined(OSG_GL2_AVAILABLE)
                glTexParameteri(target, GL_DEPTH_TEXTURE_MODE_ARB, _shadow_texture_mode);
            #endif

            // if ambient value is 0 - it is default behaviour of GL_ARB_shadow
            // no need for GL_ARB_shadow_ambient in this case
            if (extensions->isShadowAmbientSupported && _shadow_ambient > 0)
            {
                glTexParameterf(target, TEXTURE_COMPARE_FAIL_VALUE_ARB, _shadow_ambient);
            }
        }
        else
        {
            glTexParameteri(target, GL_TEXTURE_COMPARE_MODE_ARB, GL_NONE);
        }
    }
    // if range is valid
    if( _maxlod - _minlod >= 0)
    {
        glTexParameterf(target, GL_TEXTURE_MIN_LOD, _minlod);
        glTexParameterf(target, GL_TEXTURE_MAX_LOD, _maxlod);
    }

    if (extensions->isTextureLODBiasSupported)
        glTexParameterf(target, GL_TEXTURE_LOD_BIAS, _lodbias);

    getTextureParameterDirty(state.getContextID()) = false;

}

void Texture::computeRequiredTextureDimensions(State& state, const osg::Image& image,GLsizei& inwidth, GLsizei& inheight,GLsizei& numMipmapLevels) const
{
    const GLExtensions* extensions = state.get<GLExtensions>();

    int width,height;

    if( !_resizeNonPowerOfTwoHint && extensions->isNonPowerOfTwoTextureSupported(_min_filter) )
    {
        width = image.s();
        height = image.t();
    }
    else
    {
        width = Image::computeNearestPowerOfTwo(image.s()-2*_borderWidth)+2*_borderWidth;
        height = Image::computeNearestPowerOfTwo(image.t()-2*_borderWidth)+2*_borderWidth;
    }

    // cap the size to what the graphics hardware can handle.
    if (width>extensions->maxTextureSize) width = extensions->maxTextureSize;
    if (height>extensions->maxTextureSize) height = extensions->maxTextureSize;

    inwidth = width;
    inheight = height;

    if( _min_filter == LINEAR || _min_filter == NEAREST)
    {
        numMipmapLevels = 1;
    }
    else if( image.isMipmap() )
    {
        numMipmapLevels = image.getNumMipmapLevels();
    }
    else
    {
        numMipmapLevels = 1;
        for(int s=1; s<width || s<height; s <<= 1, ++numMipmapLevels) {}
    }

    // OSG_NOTICE<<"Texture::computeRequiredTextureDimensions() image.s() "<<image.s()<<" image.t()="<<image.t()<<" width="<<width<<" height="<<height<<" numMipmapLevels="<<numMipmapLevels<<std::endl;
    // OSG_NOTICE<<"  _resizeNonPowerOfTwoHint="<<_resizeNonPowerOfTwoHint<<" extensions->isNonPowerOfTwoTextureSupported(_min_filter)="<<extensions->isNonPowerOfTwoTextureSupported(_min_filter) <<std::endl;
}

bool Texture::areAllTextureObjectsLoaded() const
{
    for(unsigned int i=0;i<DisplaySettings::instance()->getMaxNumberOfGraphicsContexts();++i)
    {
        if (_textureObjectBuffer[i]==0) return false;
    }
    return true;
}

GLenum Texture::selectSizedInternalFormat(const osg::Image* image) const
{
    if (image)
    {
        bool compressed_image = isCompressedInternalFormat((GLenum)image->getPixelFormat());

        //calculate sized internal format
        if(compressed_image)
        {
            if(isCompressedInternalFormatSupportedByTexStorage(_internalFormat))
            {
                return _internalFormat;
            }
            else
            {
                return 0;
            }
        }
        else
        {
            if(isSizedInternalFormat(_internalFormat))
            {
                return _internalFormat;
            }
            else
            {
                return assumeSizedInternalFormat((GLenum)image->getInternalTextureFormat(), (GLenum)image->getDataType());
            }
        }
    }
    else
    {
        if (isSizedInternalFormat(_internalFormat)) return _internalFormat;

        return assumeSizedInternalFormat(_internalFormat, (_sourceType!=0) ? _sourceType : GL_UNSIGNED_BYTE);
    }
}

void Texture::applyTexImage2D_load(State& state, GLenum target, const Image* image, GLsizei inwidth, GLsizei inheight,GLsizei numMipmapLevels) const
{
    // if we don't have a valid image we can't create a texture!
    if (!image || !image->data())
        return;

#ifdef DO_TIMING
    osg::Timer_t start_tick = osg::Timer::instance()->tick();
    OSG_NOTICE<<"glTexImage2D pixelFormat = "<<std::hex<<image->getPixelFormat()<<std::dec<<std::endl;
#endif

    // get extensions object
    const GLExtensions* extensions = state.get<GLExtensions>();

    // select the internalFormat required for the texture.
    bool compressed_image = isCompressedInternalFormat((GLenum)image->getPixelFormat());

    // If the texture's internal format is a compressed type, then the
    // user is requesting that the graphics card compress the image if it's
    // not already compressed. However, if the image is not a multiple of
    // four in each dimension the subsequent calls to glTexSubImage* will
    // fail. Revert to uncompressed format in this case.
    if (isCompressedInternalFormat(_internalFormat) &&
        (((inwidth >> 2) << 2) != inwidth ||
         ((inheight >> 2) << 2) != inheight))
    {
        OSG_NOTICE<<"Received a request to compress an image, but image size is not a multiple of four ("<<inwidth<<"x"<<inheight<<"). Reverting to uncompressed.\n";
        switch(_internalFormat)
        {
            case GL_COMPRESSED_RGB_S3TC_DXT1_EXT:
            case GL_COMPRESSED_SRGB_S3TC_DXT1_EXT:
            case GL_COMPRESSED_RGB_PVRTC_2BPPV1_IMG:
            case GL_COMPRESSED_RGB_PVRTC_4BPPV1_IMG:
            case GL_ETC1_RGB8_OES:
            case(GL_COMPRESSED_RGB8_ETC2):
            case(GL_COMPRESSED_SRGB8_ETC2):
            case GL_COMPRESSED_RGB: _internalFormat = GL_RGB; break;
            case GL_COMPRESSED_RGBA_S3TC_DXT1_EXT:
            case GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT:
            case GL_COMPRESSED_RGBA_S3TC_DXT3_EXT:
            case GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT:
            case GL_COMPRESSED_RGBA_S3TC_DXT5_EXT:
            case GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT:
            case GL_COMPRESSED_RGBA_PVRTC_2BPPV1_IMG:
            case GL_COMPRESSED_RGBA_PVRTC_4BPPV1_IMG:
            case(GL_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2):
            case(GL_COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2):
            case(GL_COMPRESSED_RGBA8_ETC2_EAC):
            case(GL_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC):
            case GL_COMPRESSED_RGBA: _internalFormat = GL_RGBA; break;
            case GL_COMPRESSED_ALPHA: _internalFormat = GL_ALPHA; break;
            case GL_COMPRESSED_LUMINANCE: _internalFormat = GL_LUMINANCE; break;
            case GL_COMPRESSED_LUMINANCE_ALPHA: _internalFormat = GL_LUMINANCE_ALPHA; break;
            case GL_COMPRESSED_INTENSITY: _internalFormat = GL_INTENSITY; break;
            case(GL_COMPRESSED_R11_EAC):
            case(GL_COMPRESSED_SIGNED_R11_EAC):
            case GL_COMPRESSED_SIGNED_RED_RGTC1_EXT:
            case GL_COMPRESSED_RED_RGTC1_EXT: _internalFormat = GL_RED; break;
            case(GL_COMPRESSED_RG11_EAC):
            case(GL_COMPRESSED_SIGNED_RG11_EAC):
            case GL_COMPRESSED_SIGNED_RED_GREEN_RGTC2_EXT:
            case GL_COMPRESSED_RED_GREEN_RGTC2_EXT: _internalFormat = GL_RG; break;
        }
    }

    glPixelStorei(GL_UNPACK_ALIGNMENT,image->getPacking());
    unsigned int rowLength = image->getRowLength();

    bool useClientStorage = extensions->isClientStorageSupported && getClientStorageHint();
    if (useClientStorage)
    {
        glPixelStorei(GL_UNPACK_CLIENT_STORAGE_APPLE,GL_TRUE);

        #if !defined(OSG_GLES1_AVAILABLE) && !defined(OSG_GLES2_AVAILABLE) && !defined(OSG_GLES3_AVAILABLE) && !defined(OSG_GL3_AVAILABLE)
            glTexParameterf(GL_TEXTURE_2D,GL_TEXTURE_PRIORITY,0.0f);
        #endif

        #ifdef GL_TEXTURE_STORAGE_HINT_APPLE
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_STORAGE_HINT_APPLE , GL_STORAGE_CACHED_APPLE);
        #endif
    }

    unsigned char* dataPtr = (unsigned char*)image->data();

    // OSG_NOTICE<<"inwidth="<<inwidth<<" inheight="<<inheight<<" image->getFileName()"<<image->getFileName()<<std::endl;

    bool needImageRescale = inwidth!=image->s() || inheight!=image->t();
    if (needImageRescale)
    {
        // resize the image to power of two.

        if (image->isMipmap())
        {
            OSG_WARN<<"Warning:: Mipmapped osg::Image not a power of two, cannot apply to texture."<<std::endl;
            return;
        }
        else if (compressed_image)
        {
            OSG_WARN<<"Warning:: Compressed osg::Image not a power of two, cannot apply to texture."<<std::endl;
            return;
        }

        unsigned int newTotalSize = osg::Image::computeRowWidthInBytes(inwidth,image->getPixelFormat(),image->getDataType(),image->getPacking())*inheight;
        dataPtr = new unsigned char [newTotalSize];

        if (!dataPtr)
        {
            OSG_WARN<<"Warning:: Not enough memory to resize image, cannot apply to texture."<<std::endl;
            return;
        }

        if (!image->getFileName().empty()) { OSG_NOTICE << "Scaling image '"<<image->getFileName()<<"' from ("<<image->s()<<","<<image->t()<<") to ("<<inwidth<<","<<inheight<<")"<<std::endl; }
        else { OSG_NOTICE << "Scaling image from ("<<image->s()<<","<<image->t()<<") to ("<<inwidth<<","<<inheight<<")"<<std::endl; }

        PixelStorageModes psm;
        psm.pack_alignment = image->getPacking();
        psm.pack_row_length = image->getRowLength();
        psm.unpack_alignment = image->getPacking();

        // rescale the image to the correct size.
        gluScaleImage(&psm, image->getPixelFormat(),
                        image->s(),image->t(),image->getDataType(),image->data(),
                        inwidth,inheight,image->getDataType(),
                        dataPtr);

        rowLength = 0;
    }

    bool mipmappingRequired = _min_filter != LINEAR && _min_filter != NEAREST;
    bool useHardwareMipMapGeneration = mipmappingRequired && (!image->isMipmap() && isHardwareMipmapGenerationEnabled(state));
    bool useGluBuildMipMaps = mipmappingRequired && (!useHardwareMipMapGeneration && !image->isMipmap());

    GLBufferObject* pbo = image->getOrCreateGLBufferObject(state.getContextID());
    if (pbo && !needImageRescale && !useGluBuildMipMaps)
    {
        state.bindPixelBufferObject(pbo);
        dataPtr = reinterpret_cast<unsigned char*>(pbo->getOffset(image->getBufferIndex()));
        rowLength = 0;
#ifdef DO_TIMING
        OSG_NOTICE<<"after PBO "<<osg::Timer::instance()->delta_m(start_tick,osg::Timer::instance()->tick())<<"ms"<<std::endl;
#endif
    }
    else
    {
        pbo = 0;
    }
#if !defined(OSG_GLES1_AVAILABLE) && !defined(OSG_GLES2_AVAILABLE) && !defined(OSG_GLES3_AVAILABLE)
    glPixelStorei(GL_UNPACK_ROW_LENGTH,rowLength);
#endif
    if( !mipmappingRequired || useHardwareMipMapGeneration)
    {

        GenerateMipmapMode mipmapResult = mipmapBeforeTexImage(state, useHardwareMipMapGeneration);

        if ( !compressed_image)
        {
            numMipmapLevels = 1;

            glTexImage2D( target, 0, _internalFormat,
                inwidth, inheight, _borderWidth,
                selectUploadPixelFormat(image->getPixelFormat()),
                (GLenum)image->getDataType(),
                dataPtr);

        }
        else if (extensions->isCompressedTexImage2DSupported())
        {
            numMipmapLevels = 1;

            GLint blockSize, size;
            getCompressedSize(_internalFormat, inwidth, inheight, 1, blockSize,size);

            extensions->glCompressedTexImage2D(target, 0, _internalFormat,
                inwidth, inheight,0,
                size,
                dataPtr);
        }

        // One face of a cube map is not the texture: glGenerateMipmap on a cube before its last face is defined is
        // GL_INVALID_OPERATION (not cube complete), so a face leaves the chain to TextureCubeMap::apply, which
        // generates it once for all six faces after the last one.
        if (mipmapResult==GENERATE_MIPMAP && getTextureTarget()==GL_TEXTURE_CUBE_MAP && target!=GL_TEXTURE_CUBE_MAP)
            _texMipmapGenerationDirtyList[state.getContextID()] = 1;
        else
            mipmapAfterTexImage(state, mipmapResult);
    }
    else
    {
        // we require mip mapping.
        if(image->isMipmap())
        {

            // image is mip mapped so we take the mip map levels from the image.

            numMipmapLevels = image->getNumMipmapLevels();

            int width  = inwidth;
            int height = inheight;

            GLenum texStoragesizedInternalFormat = extensions->isTextureStorageEnabled && extensions->isTexStorage2DSupported() && (_borderWidth==0) ? selectSizedInternalFormat(image) : 0;

            if (texStoragesizedInternalFormat!=0)
            {
                if (getTextureTarget()==GL_TEXTURE_CUBE_MAP)
                {
                    if (target==GL_TEXTURE_CUBE_MAP_POSITIVE_X)
                    {
                        // only allocate on first face image
                        extensions->glTexStorage2D(GL_TEXTURE_CUBE_MAP, numMipmapLevels, texStoragesizedInternalFormat, width, height);
                    }
                }
                else
                {
                    extensions->glTexStorage2D(target, numMipmapLevels, texStoragesizedInternalFormat, width, height);
                }

                if( !compressed_image )
                {
                    for( GLsizei k = 0 ; k < numMipmapLevels  && (width || height) ;k++)
                    {

                        if (width == 0)
                            width = 1;
                        if (height == 0)
                            height = 1;

                        glTexSubImage2D( target, k,
                            0, 0,
                            width, height,
                            selectUploadPixelFormat(image->getPixelFormat()),
                            (GLenum)image->getDataType(),
                            dataPtr + image->getMipmapOffset(k));

                        width >>= 1;
                        height >>= 1;
                    }
                }
                else if (extensions->isCompressedTexImage2DSupported())
                {
                    GLint blockSize,size;
                    for( GLsizei k = 0 ; k < numMipmapLevels  && (width || height) ;k++)
                    {
                        if (width == 0)
                            width = 1;
                        if (height == 0)
                            height = 1;

                        getCompressedSize(image->getInternalTextureFormat(), width, height, 1, blockSize,size);

                        //state.checkGLErrors("before extensions->glCompressedTexSubImage2D(");

                        extensions->glCompressedTexSubImage2D(target, k,
                            0, 0,
                            width, height,
                            (GLenum)image->getPixelFormat(),
                            size,
                            dataPtr + image->getMipmapOffset(k));

                        //state.checkGLErrors("after extensions->glCompressedTexSubImage2D(");

                        width >>= 1;
                        height >>= 1;
                    }
                }
            }
            else
            {
                if( !compressed_image )
                {
                    for( GLsizei k = 0 ; k < numMipmapLevels  && (width || height) ;k++)
                    {

                        if (width == 0)
                            width = 1;
                        if (height == 0)
                            height = 1;

                        glTexImage2D( target, k, _internalFormat,
                             width, height, _borderWidth,
                            selectUploadPixelFormat(image->getPixelFormat()),
                            (GLenum)image->getDataType(),
                            dataPtr + image->getMipmapOffset(k));

                        width >>= 1;
                        height >>= 1;
                    }
                }
                else if (extensions->isCompressedTexImage2DSupported())
                {
                    GLint blockSize, size;

                    for( GLsizei k = 0 ; k < numMipmapLevels  && (width || height) ;k++)
                    {
                        if (width == 0)
                            width = 1;
                        if (height == 0)
                            height = 1;

                        getCompressedSize(_internalFormat, width, height, 1, blockSize,size);

                        extensions->glCompressedTexImage2D(target, k, _internalFormat,
                                                           width, height, _borderWidth,
                                                           size, dataPtr + image->getMipmapOffset(k));

                        width >>= 1;
                        height >>= 1;
                    }
                }
            }
        }
        else
        {
            if ( !compressed_image)
            {
                numMipmapLevels = 0;

                gluBuild2DMipmaps( target, _internalFormat,
                    inwidth,inheight,
                    selectUploadPixelFormat(image->getPixelFormat()), (GLenum)image->getDataType(),
                    dataPtr);

                int width  = image->s();
                int height = image->t();
                for( numMipmapLevels = 0 ; (width || height) ; ++numMipmapLevels)
                {
                    width >>= 1;
                    height >>= 1;
                }
            }
            else
            {
                OSG_WARN<<"Warning:: Compressed image cannot be mip mapped"<<std::endl;
            }

        }

    }

    if (pbo)
    {
        state.unbindPixelBufferObject();

        const BufferObject* bo = image->getBufferObject();
        if (bo->getCopyDataAndReleaseGLBufferObject())
        {
            pbo->setBufferDataHasBeenRead(image);
            if (pbo->hasAllBufferDataBeenRead())
            {
                //OSG_NOTICE<<"Release PBO"<<std::endl;
                bo->releaseGLObjects(&state);
            }
        }
    }

#ifdef DO_TIMING
    static double s_total_time = 0.0;
    double delta_time = osg::Timer::instance()->delta_m(start_tick,osg::Timer::instance()->tick());
    s_total_time += delta_time;
    OSG_NOTICE<<"glTexImage2D "<<delta_time<<"ms  total "<<s_total_time<<"ms"<<std::endl;
#endif

    if (needImageRescale)
    {
        // clean up the resized image.
        delete [] dataPtr;
    }

    if (useClientStorage)
    {
        glPixelStorei(GL_UNPACK_CLIENT_STORAGE_APPLE,GL_FALSE);
    }
}



void Texture::applyTexImage2D_subload(State& state, GLenum target, const Image* image, GLsizei inwidth, GLsizei inheight, GLint inInternalFormat, GLint numMipmapLevels) const
{
    // if we don't have a valid image we can't create a texture!
    if (!image || !image->data())
        return;

    // image size has changed so we have to re-load the image from scratch.
    if (image->s()!=inwidth || image->t()!=inheight || selectUploadInternalFormat(*image)!=inInternalFormat )
    {
        applyTexImage2D_load(state, target, image, inwidth, inheight,numMipmapLevels);
        return;
    }
    // else image size the same as when loaded so we can go ahead and subload

    // If the texture's internal format is a compressed type, then the
    // user is requesting that the graphics card compress the image if it's
    // not already compressed. However, if the image is not a multiple of
    // four in each dimension the subsequent calls to glTexSubImage* will
    // fail. Revert to uncompressed format in this case.
    if (isCompressedInternalFormat(_internalFormat) &&
        (((inwidth >> 2) << 2) != inwidth ||
         ((inheight >> 2) << 2) != inheight))
    {
        applyTexImage2D_load(state, target, image, inwidth, inheight, numMipmapLevels);
        return;
    }

#ifdef DO_TIMING
    osg::Timer_t start_tick = osg::Timer::instance()->tick();
    OSG_NOTICE<<"glTexSubImage2D pixelFormat = "<<std::hex<<image->getPixelFormat()<<std::dec<<std::endl;
#endif


    // get the contextID (user defined ID of 0 upwards) for the
    // current OpenGL context.
    const unsigned int contextID = state.getContextID();
    const GLExtensions* extensions = state.get<GLExtensions>();

    // select the internalFormat required for the texture.
    bool compressed_image = isCompressedInternalFormat((GLenum)image->getPixelFormat());

    glPixelStorei(GL_UNPACK_ALIGNMENT,image->getPacking());
    unsigned int rowLength = image->getRowLength();

    unsigned char* dataPtr = (unsigned char*)image->data();

    bool needImageRescale = inwidth!=image->s() || inheight!=image->t();
    if (needImageRescale)
    {
        // resize the image to power of two.
        if (image->isMipmap())
        {
            OSG_WARN<<"Warning:: Mipmapped osg::Image not a power of two, cannot apply to texture."<<std::endl;
            return;
        }
        else if (compressed_image)
        {
            OSG_WARN<<"Warning:: Compressed osg::Image not a power of two, cannot apply to texture."<<std::endl;
            return;
        }

        unsigned int newTotalSize = osg::Image::computeRowWidthInBytes(inwidth,image->getPixelFormat(),image->getDataType(),image->getPacking())*inheight;
        dataPtr = new unsigned char [newTotalSize];

        if (!dataPtr)
        {
            OSG_WARN<<"Warning:: Not enough memory to resize image, cannot apply to texture."<<std::endl;
            return;
        }

        if (!image->getFileName().empty()) { OSG_NOTICE << "Scaling image '"<<image->getFileName()<<"' from ("<<image->s()<<","<<image->t()<<") to ("<<inwidth<<","<<inheight<<")"<<std::endl; }
        else { OSG_NOTICE << "Scaling image from ("<<image->s()<<","<<image->t()<<") to ("<<inwidth<<","<<inheight<<")"<<std::endl; }

        // rescale the image to the correct size.
        PixelStorageModes psm;
        psm.pack_alignment = image->getPacking();
        psm.unpack_alignment = image->getPacking();

        gluScaleImage(&psm, image->getPixelFormat(),
                      image->s(),image->t(),image->getDataType(),image->data(),
                      inwidth,inheight,image->getDataType(),
                      dataPtr);

        rowLength = 0;
    }


    bool mipmappingRequired = _min_filter != LINEAR && _min_filter != NEAREST;
    bool useHardwareMipMapGeneration = mipmappingRequired && (!image->isMipmap() && isHardwareMipmapGenerationEnabled(state));
    bool useGluBuildMipMaps = mipmappingRequired && (!useHardwareMipMapGeneration && !image->isMipmap());

    GLBufferObject* pbo = image->getOrCreateGLBufferObject(contextID);
    if (pbo && !needImageRescale && !useGluBuildMipMaps)
    {
        state.bindPixelBufferObject(pbo);
        dataPtr = reinterpret_cast<unsigned char*>(pbo->getOffset(image->getBufferIndex()));
        rowLength = 0;
#ifdef DO_TIMING
        OSG_NOTICE<<"after PBO "<<osg::Timer::instance()->delta_m(start_tick,osg::Timer::instance()->tick())<<"ms"<<std::endl;
#endif
    }
    else
    {
        pbo = 0;
    }
#if !defined(OSG_GLES1_AVAILABLE) && !defined(OSG_GLES2_AVAILABLE) && !defined(OSG_GLES3_AVAILABLE)
    glPixelStorei(GL_UNPACK_ROW_LENGTH,rowLength);
#endif
    if( !mipmappingRequired || useHardwareMipMapGeneration)
    {

        GenerateMipmapMode mipmapResult = mipmapBeforeTexImage(state, useHardwareMipMapGeneration);

        if (!compressed_image)
        {
            glTexSubImage2D( target, 0,
                0, 0,
                inwidth, inheight,
                selectUploadPixelFormat(image->getPixelFormat()),
                (GLenum)image->getDataType(),
                dataPtr);
        }
        else if (extensions->isCompressedTexImage2DSupported())
        {
            GLint blockSize,size;
            getCompressedSize(image->getInternalTextureFormat(), inwidth, inheight, 1, blockSize,size);

            extensions->glCompressedTexSubImage2D(target, 0,
                0,0,
                inwidth, inheight,
                (GLenum)image->getPixelFormat(),
                size,
                dataPtr);
        }

        // A cube face leaves its chain to TextureCubeMap::apply, as in applyTexImage2D_load.
        if (mipmapResult==GENERATE_MIPMAP && getTextureTarget()==GL_TEXTURE_CUBE_MAP && target!=GL_TEXTURE_CUBE_MAP)
            _texMipmapGenerationDirtyList[contextID] = 1;
        else
            mipmapAfterTexImage(state, mipmapResult);
    }
    else
    {
        if (image->isMipmap())
        {
            numMipmapLevels = image->getNumMipmapLevels();

            int width  = inwidth;
            int height = inheight;

            if( !compressed_image )
            {
                for( GLsizei k = 0 ; k < numMipmapLevels  && (width || height) ;k++)
                {

                    if (width == 0)
                        width = 1;
                    if (height == 0)
                        height = 1;

                    glTexSubImage2D( target, k,
                        0, 0,
                        width, height,
                        selectUploadPixelFormat(image->getPixelFormat()),
                        (GLenum)image->getDataType(),
                        dataPtr + image->getMipmapOffset(k));

                    width >>= 1;
                    height >>= 1;
                }
            }
            else if (extensions->isCompressedTexImage2DSupported())
            {
                GLint blockSize,size;
                for( GLsizei k = 0 ; k < numMipmapLevels  && (width || height) ;k++)
                {
                    if (width == 0)
                        width = 1;
                    if (height == 0)
                        height = 1;

                    getCompressedSize(image->getInternalTextureFormat(), width, height, 1, blockSize,size);

                    //state.checkGLErrors("before extensions->glCompressedTexSubImage2D(");

                    extensions->glCompressedTexSubImage2D(target, k,
                                                       0, 0,
                                                       width, height,
                                                       (GLenum)image->getPixelFormat(),
                                                       size,
                                                       dataPtr + image->getMipmapOffset(k));

                    //state.checkGLErrors("after extensions->glCompressedTexSubImage2D(");

                    width >>= 1;
                    height >>= 1;
                }

            }
        }
        else
        {
            //OSG_WARN<<"Warning:: cannot subload mip mapped texture from non mipmapped image."<<std::endl;
            applyTexImage2D_load(state, target, image, inwidth, inheight,numMipmapLevels);
        }
    }

    if (pbo)
    {
        state.unbindPixelBufferObject();
    }
#ifdef DO_TIMING
    OSG_NOTICE<<"glTexSubImage2D "<<osg::Timer::instance()->delta_m(start_tick,osg::Timer::instance()->tick())<<"ms"<<std::endl;
#endif

    if (needImageRescale)
    {
        // clean up the resized image.
        delete [] dataPtr;
    }
}

bool Texture::isHardwareMipmapGenerationEnabled(const State& state) const
{
    if (_useHardwareMipMapGeneration)
    {
        const GLExtensions* extensions = state.get<GLExtensions>();
        if (extensions->isGenerateMipMapSupported)
        {
            return true;
        }

        // FrameBufferObjects are required for glGenerateMipmap
        if (extensions->isFrameBufferObjectSupported && extensions->glGenerateMipmap)
        {
            return true;
        }
    }

    return false;
}

Texture::GenerateMipmapMode Texture::mipmapBeforeTexImage(const State& state, bool hardwareMipmapOn) const
{
    if (hardwareMipmapOn)
    {
#if defined( OSG_GLES2_AVAILABLE ) || defined( OSG_GLES3_AVAILABLE ) || defined( OSG_GL3_AVAILABLE )
        return GENERATE_MIPMAP;
#else

        const GLExtensions* extensions = state.get<GLExtensions>();
        bool useGenerateMipMap = extensions->isFrameBufferObjectSupported && extensions->glGenerateMipmap;

        if (useGenerateMipMap)
        {
            if (extensions->preferGenerateMipmapSGISForPowerOfTwo)
            {
                int width = getTextureWidth();
                int height = getTextureHeight();
                useGenerateMipMap = ((width & (width - 1)) || (height & (height - 1)));
            }

            if (useGenerateMipMap)
            {
                useGenerateMipMap = (_internalFormatType != SIGNED_INTEGER && _internalFormatType != UNSIGNED_INTEGER);
            }

            if (useGenerateMipMap) return GENERATE_MIPMAP;
        }

        glTexParameteri(getTextureTarget(), GL_GENERATE_MIPMAP_SGIS, GL_TRUE);
        return GENERATE_MIPMAP_TEX_PARAMETER;
#endif
    }
    return GENERATE_MIPMAP_NONE;
}

void Texture::mipmapAfterTexImage(State& state, GenerateMipmapMode beforeResult) const
{
    switch (beforeResult)
    {
        case GENERATE_MIPMAP:
        {
            unsigned int contextID = state.getContextID();
            TextureObject* textureObject = getTextureObject(contextID);
            if (textureObject)
            {
                osg::GLExtensions* ext = state.get<GLExtensions>();
                ext->glGenerateMipmap(textureObject->target());
            }
            break;
        }
        case GENERATE_MIPMAP_TEX_PARAMETER:
            glTexParameteri(getTextureTarget(), GL_GENERATE_MIPMAP_SGIS, GL_FALSE);
            break;
        case GENERATE_MIPMAP_NONE:
            break;
    }
}

void Texture::generateMipmap(State& state) const
{
    const unsigned int contextID = state.getContextID();

    // get the texture object for the current contextID.
    TextureObject* textureObject = getTextureObject(contextID);

    // if not initialized before, then do nothing
    if (textureObject == NULL) return;

    _texMipmapGenerationDirtyList[contextID] = 0;

    // if internal format does not provide automatic mipmap generation, then do manual allocation
    if (_internalFormatType == SIGNED_INTEGER || _internalFormatType == UNSIGNED_INTEGER)
    {
        allocateMipmap(state);
        return;
    }

    // get fbo extension which provides us with the glGenerateMipmapEXT function
    osg::GLExtensions* ext = state.get<GLExtensions>();

    // FrameBufferObjects are required for glGenerateMipmap
    if (ext->isFrameBufferObjectSupported && ext->glGenerateMipmap)
    {
        textureObject->bind();
        ext->glGenerateMipmap(textureObject->target());

        // inform state that this texture is the current one bound.
        state.haveAppliedTextureAttribute(state.getActiveTextureUnit(), this);

    // if the function is not supported, then do manual allocation
    }else
    {
        allocateMipmap(state);
    }

}

void Texture::compileGLObjects(State& state) const
{
    apply(state);
}

void Texture::resizeGLObjectBuffers(unsigned int maxSize)
{
    _textureObjectBuffer.resize(maxSize);
    _texParametersDirtyList.resize(maxSize);
    _texMipmapGenerationDirtyList.resize(maxSize);
}

void Texture::releaseGLObjects(State* state) const
{
//    if (state) OSG_NOTICE<<"Texture::releaseGLObjects contextID="<<state->getContextID()<<std::endl;
//    else OSG_NOTICE<<"Texture::releaseGLObjects no State "<<std::endl;

    if (!state) const_cast<Texture*>(this)->dirtyTextureObject();
    else
    {
        unsigned int contextID = state->getContextID();
        if (_textureObjectBuffer[contextID].valid())
        {
            _textureObjectBuffer[contextID]->release();

            _textureObjectBuffer[contextID] = 0;
        }
    }
}

}
