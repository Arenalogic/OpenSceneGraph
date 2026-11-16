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
#include <osg/GL>
#include <osg/LineWidth>
#include <osg/State>
#include <osg/Notify>

using namespace osg;


LineWidth::LineWidth(float width)
{
    _width = width;
}


LineWidth::~LineWidth()
{
}

void LineWidth::setWidth( float width )
{
    _width = width;
}

void LineWidth::apply(State& state) const
{
    // A forward-compatible context raises GL_INVALID_VALUE for a width above 1 (wide lines are deprecated) and keeps the
    // previous width: draw 1 and record the refusal. Wide strokes are drawn by geometry (dtRender's stroke shaders).
    if (_width>1.0f && state.isForwardCompatible())
    {
        glLineWidth(1.0f);
        State::noteRefusedAttribute(libraryName(), className(), "a width above 1 on a forward-compatible context");
        return;
    }
    glLineWidth(_width);
}

