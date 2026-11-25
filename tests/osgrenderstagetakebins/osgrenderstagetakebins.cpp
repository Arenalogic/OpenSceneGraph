/* osgrenderstagetakebins: RenderStage::takeBinsFrom splits a culled stage at a bin number into a second stage (Arenalogic
 * fork test, ctest). The split is checked without a context; the draws need a GL context and without one the program
 * exits 77, which ctest reports as skipped (SKIP_RETURN_CODE), after the split's checks.
 *
 * The scene: one Geode per bin 0, 5, 10 and 20, and a bin 2 nested under bin 20 (the bin-20 StateSet nests), under a
 * master StateSet (the SceneView's global StateSet) that sets GL_BLEND ON; the bin-10 leaf's own StateSet sets it OFF.
 * It is culled through an osgUtil::SceneView, then split at 10 into a second RenderStage, a post-render stage of the
 * first (order 1).
 *
 *  1. The table: the source keeps 0 and 5, with itself as their stage; the target holds 10 and 20 with the target as
 *     their parent and stage; the nested bin 2 stays under bin 20 (its parent) with the target as its stage.
 *  2. The draws: every leaf draws once over both stages (an osg::Drawable::DrawCallback counter), 0 and 5 inside the
 *     source, 10, 20 and the nested 2 inside the target; the bin a leaf draws in reports the stage it draws in.
 *  3. Each leaf's own StateGraph chain wins in the target: the bin-10 leaf reads glIsEnabled(GL_BLEND) == GL_FALSE,
 *     the bin-20 leaf drawn after it GL_TRUE (the master's), as do the source's leaves.
 *  4. Three cull/draw cycles of the same SceneView give the same table and the same draws each time.
 *  5. A bin number the target already holds stays in the source and is not counted as moved.
 *
 * Exit code 0 when every check holds, 1 otherwise (each failed check is printed), 77 when the draws are skipped.
 */

#include <osg/FrameStamp>
#include <osg/Geode>
#include <osg/Geometry>
#include <osg/GraphicsContext>
#include <osg/Group>
#include <osg/Notify>
#include <osg/RenderInfo>
#include <osg/StateSet>
#include <osgUtil/RenderStage>
#include <osgUtil/SceneView>
#include <osgViewer/GraphicsWindow>

#include <iostream>
#include <sstream>
#include <string>
#include <vector>

USE_GRAPHICSWINDOW()

namespace
{
    const int kSkip = 77;
    const int kSplit = 10;
    int s_failures = 0;

    void check(bool ok, const std::string& what)
    {
        if (!ok)
        {
            ++s_failures;
            std::cout << "FAILED: " << what << std::endl;
        }
    }

    // One leaf: what its draws saw this cycle.
    struct Leaf
    {
        std::string name;
        int bin;
        bool inTarget;          // expected to draw inside the target stage
        bool expectBlend;       // expected glIsEnabled(GL_BLEND) while it draws
        int draws;
        const osgUtil::RenderBin* drawingStage;     // the outermost bin on the RenderInfo's stack while it drew
        const osgUtil::RenderBin* binStage;         // the stage of the bin it drew in
        int drawnBin;
        GLboolean blend;
    };

    class CountingDraw : public osg::Drawable::DrawCallback
    {
    public:
        explicit CountingDraw(Leaf* leaf) : _leaf(leaf) {}

        virtual void drawImplementation(osg::RenderInfo& renderInfo, const osg::Drawable*) const
        {
            ++_leaf->draws;
            osg::RenderInfo::RenderBinStack& stack = renderInfo.getRenderBinStack();
            _leaf->drawingStage = stack.empty() ? 0 : stack.front();
            _leaf->binStage = stack.empty() ? 0 : stack.back()->getStage();
            _leaf->drawnBin = stack.empty() ? -1 : stack.back()->getBinNum();
            _leaf->blend = glIsEnabled(GL_BLEND);
        }

    private:
        Leaf* _leaf;
    };

    osg::Geode* makeLeaf(Leaf* leaf)
    {
        osg::ref_ptr<osg::Vec3Array> vertices = new osg::Vec3Array;
        vertices->push_back(osg::Vec3(-1.0f, -1.0f, -5.0f));
        vertices->push_back(osg::Vec3(1.0f, -1.0f, -5.0f));
        vertices->push_back(osg::Vec3(0.0f, 1.0f, -5.0f));

        osg::ref_ptr<osg::Geometry> geometry = new osg::Geometry;
        geometry->setVertexArray(vertices.get());
        geometry->setUseDisplayList(false);         // a display list would run the callback once, at compile time
        geometry->setCullingActive(false);
        geometry->setDrawCallback(new CountingDraw(leaf));
        if (!leaf->expectBlend) geometry->getOrCreateStateSet()->setMode(GL_BLEND, osg::StateAttribute::OFF);

        osg::Geode* geode = new osg::Geode;
        geode->setName(leaf->name);
        geode->addDrawable(geometry.get());
        return geode;
    }

    void setBin(osg::Node& node, int bin, bool nest)
    {
        osg::StateSet* stateSet = node.getOrCreateStateSet();
        stateSet->setRenderBinDetails(bin, "RenderBin");
        stateSet->setNestRenderBins(nest);
    }

    std::string binNumbers(const osgUtil::RenderBin& bin)
    {
        std::ostringstream out;
        const osgUtil::RenderBin::RenderBinList& bins = bin.getRenderBinList();
        for (osgUtil::RenderBin::RenderBinList::const_iterator itr = bins.begin(); itr != bins.end(); ++itr)
            out << (itr == bins.begin() ? "" : " ") << itr->first;
        return out.str();
    }

    const osgUtil::RenderBin* child(const osgUtil::RenderBin& bin, int number)
    {
        osgUtil::RenderBin::RenderBinList::const_iterator itr = bin.getRenderBinList().find(number);
        return itr == bin.getRenderBinList().end() ? 0 : itr->second.get();
    }

    // The split's table, as text (so the cycles can be compared), checked against the expectation.
    std::string checkTable(const osgUtil::RenderStage& source, const osgUtil::RenderStage& target, unsigned int moved, int cycle)
    {
        std::ostringstream prefix;
        prefix << "cycle " << cycle << ": ";
        const std::string p = prefix.str();

        std::ostringstream table;
        table << "moved " << moved << "; source [" << binNumbers(source) << "]; target [" << binNumbers(target) << "]";

        check(moved == 2, p + "takeBinsFrom moves two bins (" + table.str() + ")");
        check(binNumbers(source) == "0 5", p + "the source keeps bins 0 and 5 (" + table.str() + ")");
        check(binNumbers(target) == "10 20", p + "the target holds bins 10 and 20 (" + table.str() + ")");

        for (int number = 0; number <= 5; number += 5)
        {
            const osgUtil::RenderBin* bin = child(source, number);
            std::ostringstream what;
            what << p << "source bin " << number << " keeps the source as its parent and stage";
            check(bin && bin->getParent() == &source && bin->getStage() == &source, what.str());
        }
        for (int number = 10; number <= 20; number += 10)
        {
            const osgUtil::RenderBin* bin = child(target, number);
            std::ostringstream what;
            what << p << "moved bin " << number << " has the target as its parent and stage";
            check(bin && bin->getParent() == &target && bin->getStage() == &target, what.str());
        }

        const osgUtil::RenderBin* bin20 = child(target, 20);
        const osgUtil::RenderBin* nested = bin20 ? child(*bin20, 2) : 0;
        check(bin20 && binNumbers(*bin20) == "2", p + "bin 20 still nests bin 2 (bin 20 holds [" + (bin20 ? binNumbers(*bin20) : std::string("-")) + "])");
        check(nested && nested->getParent() == bin20, p + "the nested bin 2 keeps bin 20 as its parent");
        check(nested && nested->getStage() == &target, p + "the nested bin 2 has the target as its stage");
        if (nested) table << "; bin 20 [" << binNumbers(*bin20) << "]";
        return table.str();
    }

    void checkDraws(const std::vector<Leaf>& leaves, const osgUtil::RenderStage& source, const osgUtil::RenderStage& target, int cycle)
    {
        for (size_t i = 0; i < leaves.size(); ++i)
        {
            const Leaf& leaf = leaves[i];
            const osgUtil::RenderStage* expected = leaf.inTarget ? &target : &source;
            std::ostringstream p;
            p << "cycle " << cycle << ": leaf " << leaf.name << " ";
            std::ostringstream draws;
            draws << "draws once (" << leaf.draws << ")";
            check(leaf.draws == 1, p.str() + draws.str());
            check(leaf.drawingStage == expected, p.str() + "draws inside the " + (leaf.inTarget ? "target" : "source"));
            std::ostringstream bin;
            bin << "draws in bin " << leaf.bin << " (" << leaf.drawnBin << ")";
            check(leaf.drawnBin == leaf.bin, p.str() + bin.str());
            check(leaf.binStage == expected, p.str() + "draws in a bin whose stage is the stage drawing it");
            std::ostringstream blend;
            blend << "reads glIsEnabled(GL_BLEND) " << (leaf.expectBlend ? "GL_TRUE" : "GL_FALSE") << " (" << int(leaf.blend) << ")";
            check((leaf.blend != GL_FALSE) == leaf.expectBlend, p.str() + blend.str());
        }
    }

    void checkCollision()
    {
        osg::ref_ptr<osgUtil::RenderStage> source = new osgUtil::RenderStage;
        osg::ref_ptr<osgUtil::RenderStage> target = new osgUtil::RenderStage;
        source->find_or_insert(3, "RenderBin");
        source->find_or_insert(12, "RenderBin");
        source->find_or_insert(14, "RenderBin");
        osgUtil::RenderBin* held = target->find_or_insert(12, "RenderBin");
        osgUtil::RenderBin* sourceTwelve = source->find_or_insert(12, "RenderBin");

        const unsigned int moved = target->takeBinsFrom(*source, 10);
        std::ostringstream what;
        what << "collision: one bin moves (" << moved << "), the source keeps 3 and 12 ([" << binNumbers(*source)
             << "]), the target holds 12 and 14 ([" << binNumbers(*target) << "])";
        check(moved == 1 && binNumbers(*source) == "3 12" && binNumbers(*target) == "12 14", what.str());
        check(child(*target, 12) == held && child(*source, 12) == sourceTwelve && sourceTwelve->getStage() == source.get(),
              "collision: the target keeps its own bin 12 and the source's bin 12 stays the source's");
        check(target->takeBinsFrom(*target, 0) == 0, "a stage takes nothing from itself");
    }
}

int main(int, char**)
{
    osg::setNotifyLevel(osg::WARN);

    checkCollision();

    osg::ref_ptr<osg::GraphicsContext::Traits> traits = new osg::GraphicsContext::Traits;
    traits->x = 0;
    traits->y = 0;
    traits->width = 16;
    traits->height = 16;
    traits->windowDecoration = false;
    traits->doubleBuffer = false;
    traits->pbuffer = true;
    traits->hidden = false;
    traits->windowName = "osgrenderstagetakebins";

    osg::ref_ptr<osg::GraphicsContext> context = osg::GraphicsContext::createGraphicsContext(traits.get());
    bool haveContext = context.valid() && context->realize() && context->makeCurrent();
    if (!haveContext)
    {
        traits->pbuffer = false;
        traits->hidden = true;
        context = osg::GraphicsContext::createGraphicsContext(traits.get());
        haveContext = context.valid() && context->realize() && context->makeCurrent();
    }
    if (haveContext)
    {
        const char* renderer = reinterpret_cast<const char*>(glGetString(GL_RENDERER));
        const char* version = reinterpret_cast<const char*>(glGetString(GL_VERSION));
        std::cout << "osgrenderstagetakebins: " << (renderer ? renderer : "?") << ", GL " << (version ? version : "?") << std::endl;
    }
    else
    {
        std::cout << "osgrenderstagetakebins: no GL context (traits ask for " << traits->glContextVersion << "), the draws are skipped" << std::endl;
    }

    // name, bin, in the target, GL_BLEND expected
    std::vector<Leaf> leaves;
    const struct { const char* name; int bin; bool blend; } rows[] =
        { { "bin0", 0, true }, { "bin5", 5, true }, { "bin10", 10, false }, { "bin20", 20, true }, { "bin20/bin2", 2, true } };
    for (size_t i = 0; i < sizeof(rows) / sizeof(rows[0]); ++i)
    {
        Leaf leaf = { rows[i].name, rows[i].bin, i >= 2, rows[i].blend, 0, 0, 0, -1, GL_FALSE };
        leaves.push_back(leaf);
    }

    osg::ref_ptr<osg::Group> root = new osg::Group;
    for (size_t i = 0; i < 3; ++i)
    {
        osg::Geode* geode = makeLeaf(&leaves[i]);
        setBin(*geode, leaves[i].bin, true);
        root->addChild(geode);
    }
    osg::ref_ptr<osg::Group> group20 = new osg::Group;
    setBin(*group20, 20, true);
    group20->addChild(makeLeaf(&leaves[3]));
    osg::Geode* nested = makeLeaf(&leaves[4]);
    setBin(*nested, 2, true);
    group20->addChild(nested);
    root->addChild(group20.get());

    osg::ref_ptr<osg::StateSet> master = new osg::StateSet;
    master->setMode(GL_BLEND, osg::StateAttribute::ON);

    osg::ref_ptr<osg::FrameStamp> frameStamp = new osg::FrameStamp;
    osg::ref_ptr<osgUtil::SceneView> sceneView = new osgUtil::SceneView;
    sceneView->setDefaults(0);
    sceneView->setGlobalStateSet(master.get());
    sceneView->setFrameStamp(frameStamp.get());
    sceneView->setViewport(0, 0, traits->width, traits->height);
    sceneView->setCullingMode(osg::CullSettings::NO_CULLING);
    sceneView->setComputeNearFarMode(osg::CullSettings::DO_NOT_COMPUTE_NEAR_FAR);
    sceneView->setProjectionMatrixAsPerspective(60.0, 1.0, 1.0, 100.0);
    sceneView->setViewMatrix(osg::Matrixd::identity());
    sceneView->setSceneData(root.get());
    if (haveContext) sceneView->setState(context->getState());

    osg::ref_ptr<osgUtil::RenderStage> target = new osgUtil::RenderStage;
    target->setClearMask(0);

    std::vector<std::string> tables;
    for (int cycle = 0; cycle < 3; ++cycle)
    {
        frameStamp->setFrameNumber(cycle);
        sceneView->cull();

        osgUtil::RenderStage* source = sceneView->getRenderStage();
        target->reset();
        target->setViewport(source->getViewport());
        const unsigned int moved = target->takeBinsFrom(*source, kSplit);
        source->addPostRenderStage(target.get(), 1);
        tables.push_back(checkTable(*source, *target, moved, cycle));

        if (!haveContext) continue;

        for (size_t i = 0; i < leaves.size(); ++i)
        {
            leaves[i].draws = 0;
            leaves[i].drawingStage = 0;
            leaves[i].binStage = 0;
            leaves[i].drawnBin = -1;
            leaves[i].blend = GL_FALSE;
        }
        sceneView->draw();
        checkDraws(leaves, *source, *target, cycle);
    }
    for (size_t i = 1; i < tables.size(); ++i)
        check(tables[i] == tables[0], "the split repeats: cycle " + std::string(1, char('0' + i)) + " gives [" + tables[i] + "], cycle 0 gave [" + tables[0] + "]");
    std::cout << "osgrenderstagetakebins: " << tables[0] << std::endl;

    if (haveContext)
    {
        sceneView->releaseAllGLObjects();
        context->releaseContext();
        context->close();
    }

    if (s_failures != 0)
    {
        std::cout << "osgrenderstagetakebins: " << s_failures << " check(s) failed" << std::endl;
        return 1;
    }
    if (!haveContext)
    {
        std::cout << "osgrenderstagetakebins: the split's checks hold, the draws are skipped" << std::endl;
        return kSkip;
    }
    std::cout << "osgrenderstagetakebins: every check holds" << std::endl;
    return 0;
}
