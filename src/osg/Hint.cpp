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

#include <osg/Hint>
#include <osg/State>
#include <osg/StateSet>

using namespace osg;

void Hint::apply(State& /*state*/) const
{
    if (_target==GL_NONE || _mode==GL_NONE) return;

#if !defined(OSG_GL_FIXED_FUNCTION_AVAILABLE)
    // The core profile's targets are GL_LINE_SMOOTH_HINT, GL_POLYGON_SMOOTH_HINT, GL_TEXTURE_COMPRESSION_HINT and
    // GL_FRAGMENT_SHADER_DERIVATIVE_HINT; the fixed-function ones are GL_INVALID_ENUM there and are refused.
    const char* refused = 0;
    switch (_target)
    {
        case 0x0C50: refused = "target GL_PERSPECTIVE_CORRECTION_HINT"; break;
        case 0x0C51: refused = "target GL_POINT_SMOOTH_HINT"; break;
        case 0x0C54: refused = "target GL_FOG_HINT"; break;
        case 0x8192: refused = "target GL_GENERATE_MIPMAP_HINT"; break;
        default: break;
    }
    if (refused)
    {
        State::noteRefusedAttribute(libraryName(), className(), refused);
        return;
    }
#endif

    glHint(_target, _mode);
}

void Hint::setTarget(GLenum target)
{
    if (_target==target) return;

    ReassignToParents needToReassingToParentsWhenMemberValueChanges(this);

    _target = target;
}

