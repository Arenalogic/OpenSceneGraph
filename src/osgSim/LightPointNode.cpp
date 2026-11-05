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

#include <osgSim/LightPointNode>
#include <osgSim/LightPointSystem>

#include <osg/Notify>

#include <OpenThreads/Atomic>

#include <float.h>

namespace osgSim
{

LightPointNode::LightPointNode():
    _minPixelSize(0.0f),
    _maxPixelSize(30.0f),
    _maxVisibleDistance2(FLT_MAX),
    _lightSystem(0),
    _pointSprites(false)
{
}

/** Copy constructor using CopyOp to manage deep vs shallow copy.*/
LightPointNode::LightPointNode(const LightPointNode& lpn,const osg::CopyOp& copyop):
    osg::Node(lpn,copyop),
    _bbox(lpn._bbox),
    _lightPointList(lpn._lightPointList),
    _minPixelSize(lpn._minPixelSize),
    _maxPixelSize(lpn._maxPixelSize),
    _maxVisibleDistance2(lpn._maxVisibleDistance2),
    _lightSystem(lpn._lightSystem),
    _pointSprites(lpn._pointSprites)
{
}

unsigned int LightPointNode::addLightPoint(const LightPoint& lp)
{
    unsigned int num = _lightPointList.size();
    _lightPointList.push_back(lp);
    dirtyBound();
    return num;
}

void LightPointNode::removeLightPoint(unsigned int pos)
{
    if (pos<_lightPointList.size())
    {
        _lightPointList.erase(_lightPointList.begin()+pos);
        dirtyBound();
    }
    dirtyBound();
}

osg::BoundingSphere LightPointNode::computeBound() const
{
    osg::BoundingSphere bsphere;
    bsphere.init();
    _bbox.init();

    if (_lightPointList.empty())
    {
        return bsphere;
    }


    LightPointList::const_iterator itr;
    for(itr=_lightPointList.begin();
        itr!=_lightPointList.end();
        ++itr)
    {
        _bbox.expandBy(itr->_position);
    }


    bsphere.set(_bbox.center(),0.0f);

    for(itr=_lightPointList.begin();
        itr!=_lightPointList.end();
        ++itr)
    {
        osg::Vec3 dv(itr->_position-bsphere.center());
        float radius = dv.length()+itr->_radius;
        if (bsphere.radius()<radius) bsphere.radius()=radius;
    }

    bsphere.radius()+=1.0f;
    return bsphere;
}


void LightPointNode::traverse(osg::NodeVisitor& nv)
{
    // The light points are data only: the node draws nothing (its drawables and its shared state set are gone, with
    // their client-array GL_POINTS drawing). An application draws them from the light point list with a drawable of
    // its own (ACT: dtRender::LightPointImport converts every node when a file is read). A node that is still culled
    // is reported once per process.
    if (_lightPointList.empty() || nv.getVisitorType() != osg::NodeVisitor::CULL_VISITOR) return;

    static OpenThreads::Atomic s_reported;
    if (s_reported.exchange(1) == 0)
    {
        OSG_WARN << "osgSim::LightPointNode \"" << getName() << "\" (" << _lightPointList.size()
                 << " light points) is culled but draws nothing: osgSim light points are data only in this build "
                 << "(OSGSIM_LIGHTPOINT_NO_DRAW); convert them to a drawable. Further nodes are not reported." << std::endl;
    }
}


} // end of namespace
