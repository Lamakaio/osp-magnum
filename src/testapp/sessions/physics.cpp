/**
 * Open Space Program
 * Copyright © 2019-2022 Open Space Program Project
 *
 * MIT License
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#include "physics.h"
#include "common.h"

#include <osp/activescene/basic.h>
#include <osp/activescene/physics_fn.h>
#include <osp/activescene/prefab_fn.h>
#include <osp/core/Resources.h>
#include <osp/core/Resources.h>
#include <osp/drawing/drawing_fn.h>
#include <osp/vehicles/ImporterData.h>

#include <Magnum/Trade/Trade.h>
#include <Magnum/Trade/PbrMetallicRoughnessMaterialData.h>


using namespace osp;
using namespace osp::active;
using namespace osp::draw;
using osp::restypes::gc_importer;
using Corrade::Containers::arrayView;

namespace testapp::scenes
{



Session setup_physics(
        TopTaskBuilder&             rBuilder,
        ArrayView<entt::any> const  topData,
        Session const&              scene,
        Session const&              commonScene)
{
    OSP_DECLARE_GET_DATA_IDS(commonScene,  TESTAPP_DATA_COMMON_SCENE);
    auto const tgScn = scene      .get_pipelines<PlScene>();
    auto const tgCS  = commonScene.get_pipelines<PlCommonScene>();

    Session out;
    OSP_DECLARE_CREATE_DATA_IDS(out, topData, TESTAPP_DATA_PHYSICS);
    auto const tgPhy = out.create_pipelines<PlPhysics>(rBuilder);

    rBuilder.pipeline(tgPhy.physBody)  .parent(tgScn.update);
    rBuilder.pipeline(tgPhy.physUpdate).parent(tgScn.update);

    top_emplace< ACtxPhysics >  (topData, idPhys);

    rBuilder.task()
        .name       ("Delete Physics components")
        .run_on     ({tgCS.activeEntDelete(UseOrRun)})
        .sync_with  ({tgPhy.physBody(Delete)})
        .push_to    (out.m_tasks)
        .args       ({        idPhys,                      idActiveEntDel })
        .func([] (ACtxPhysics& rPhys, ActiveEntVec_t const& rActiveEntDel) noexcept
    {
        SysPhysics::update_delete_phys(rPhys, rActiveEntDel.cbegin(), rActiveEntDel.cend());
    });

    return out;
} // setup_physics


//-----------------------------------------------------------------------------


Session setup_phys_shapes(
        TopTaskBuilder&             rBuilder,
        ArrayView<entt::any> const  topData,
        Session const&              scene,
        Session const&              commonScene,
        Session const&              physics,
        MaterialId const            materialId)
{
    OSP_DECLARE_GET_DATA_IDS(commonScene,   TESTAPP_DATA_COMMON_SCENE);
    OSP_DECLARE_GET_DATA_IDS(physics,       TESTAPP_DATA_PHYSICS);
    auto const tgScn    = scene         .get_pipelines<PlScene>();
    auto const tgCS     = commonScene   .get_pipelines<PlCommonScene>();
    auto const tgPhy    = physics       .get_pipelines<PlPhysics>();

    Session out;
    OSP_DECLARE_CREATE_DATA_IDS(out, topData, TESTAPP_DATA_PHYS_SHAPES);
    auto const tgShSp = out.create_pipelines<PlPhysShapes>(rBuilder);

    rBuilder.pipeline(tgShSp.spawnRequest)  .parent(tgScn.update);
    rBuilder.pipeline(tgShSp.spawnedEnts)   .parent(tgScn.update);
    rBuilder.pipeline(tgShSp.ownedEnts)     .parent(tgScn.update);

    top_emplace< ACtxPhysShapes > (topData, idPhysShapes, ACtxPhysShapes{ .m_materialId = materialId });

    rBuilder.task()
        .name       ("Schedule Shape spawn")
        .schedules  ({tgShSp.spawnRequest(Schedule_)})
        .push_to    (out.m_tasks)
        .args       ({           idPhysShapes })
        .func([] (ACtxPhysShapes& rPhysShapes) noexcept -> TaskActions
    {
        return rPhysShapes.m_spawnRequest.empty() ? TaskAction::Cancel : TaskActions{};
    });

    rBuilder.task()
        .name       ("Create ActiveEnts for requested shapes to spawn")
        .run_on     ({tgShSp.spawnRequest(UseOrRun)})
        .sync_with  ({tgCS.activeEnt(New), tgCS.activeEntResized(Schedule), tgShSp.spawnedEnts(Resize)})
        .push_to    (out.m_tasks)
        .args       ({      idBasic,                idPhysShapes})
        .func([] (ACtxBasic& rBasic, ACtxPhysShapes& rPhysShapes) noexcept
    {
        LGRN_ASSERTM(!rPhysShapes.m_spawnRequest.empty(), "spawnRequest Use_ shouldn't run if rPhysShapes.m_spawnRequest is empty!");

        rPhysShapes.m_ents.resize(rPhysShapes.m_spawnRequest.size() * 2);
        rBasic.m_activeIds.create(rPhysShapes.m_ents.begin(), rPhysShapes.m_ents.end());
    });

    rBuilder.task()
        .name       ("Add hierarchy and transform to spawned shapes")
        .run_on     ({tgShSp.spawnRequest(UseOrRun)})
        .sync_with  ({tgShSp.spawnedEnts(UseOrRun), tgShSp.ownedEnts(Modify__), tgCS.hierarchy(New), tgCS.transform(New)})
        .push_to    (out.m_tasks)
        .args       ({      idBasic,                idPhysShapes })
        .func([] (ACtxBasic& rBasic, ACtxPhysShapes& rPhysShapes) noexcept
    {
        osp::bitvector_resize(rPhysShapes.ownedEnts, rBasic.m_activeIds.capacity());
        rBasic.m_scnGraph.resize(rBasic.m_activeIds.capacity());

        SubtreeBuilder bldScnRoot = SysSceneGraph::add_descendants(rBasic.m_scnGraph, rPhysShapes.m_spawnRequest.size() * 2);

        for (std::size_t i = 0; i < rPhysShapes.m_spawnRequest.size(); ++i)
        {
            SpawnShape const &spawn = rPhysShapes.m_spawnRequest[i];
            ActiveEnt const root    = rPhysShapes.m_ents[i * 2];
            ActiveEnt const child   = rPhysShapes.m_ents[i * 2 + 1];

            rPhysShapes.ownedEnts.set(std::size_t(root));

            rBasic.m_transform.emplace(root, ACompTransform{osp::Matrix4::translation(spawn.m_position)});
            rBasic.m_transform.emplace(child, ACompTransform{Matrix4::scaling(spawn.m_size)});
            SubtreeBuilder bldRoot = bldScnRoot.add_child(root, 1);
            bldRoot.add_child(child);
        }
    });

    rBuilder.task()
        .name       ("Add physics to spawned shapes")
        .run_on     ({tgShSp.spawnRequest(UseOrRun)})
        .sync_with  ({tgShSp.spawnedEnts(UseOrRun), tgPhy.physBody(Modify), tgPhy.physUpdate(Done)})
        .push_to    (out.m_tasks)
        .args       ({            idBasic,                idPhysShapes,             idPhys })
        .func([] (ACtxBasic const& rBasic, ACtxPhysShapes& rPhysShapes, ACtxPhysics& rPhys) noexcept
    {
        rPhys.m_hasColliders.ints().resize(rBasic.m_activeIds.vec().capacity());
        rPhys.m_shape.resize(rBasic.m_activeIds.capacity());

        for (std::size_t i = 0; i < rPhysShapes.m_spawnRequest.size(); ++i)
        {
            SpawnShape const &spawn = rPhysShapes.m_spawnRequest[i];
            ActiveEnt const root    = rPhysShapes.m_ents[i * 2];
            ActiveEnt const child   = rPhysShapes.m_ents[i * 2 + 1];

            rPhys.m_hasColliders.set(std::size_t(root));
            if (spawn.m_mass != 0.0f)
            {
                rPhys.m_setVelocity.emplace_back(root, spawn.m_velocity);
                Vector3 const inertia = collider_inertia_tensor(spawn.m_shape, spawn.m_size, spawn.m_mass);
                Vector3 const offset{0.0f, 0.0f, 0.0f};
                rPhys.m_mass.emplace( child, ACompMass{ inertia, offset, spawn.m_mass } );
            }

            rPhys.m_shape[child] = spawn.m_shape;
            rPhys.m_colliderDirty.push_back(child);
        }
    });

    //TODO
    rBuilder.task()
        .name       ("Delete basic components")
        .run_on     ({tgCS.activeEntDelete(UseOrRun)})
        .sync_with  ({tgShSp.ownedEnts(Modify__)})
        .push_to    (out.m_tasks)
        .args       ({      idBasic,                      idActiveEntDel })
        .func([] (ACtxBasic& rBasic, ActiveEntVec_t const& rActiveEntDel) noexcept
    {
        update_delete_basic(rBasic, rActiveEntDel.cbegin(), rActiveEntDel.cend());
    });

    rBuilder.task()
        .name       ("Clear Shape Spawning vector after use")
        .run_on     ({tgShSp.spawnRequest(Clear)})
        .push_to    (out.m_tasks)
        .args       ({           idPhysShapes })
        .func([] (ACtxPhysShapes& rPhysShapes) noexcept
    {
        rPhysShapes.m_spawnRequest.clear();
    });


    return out;
} // setup_phys_shapes




Session setup_phys_shapes_draw(
        TopTaskBuilder&             rBuilder,
        ArrayView<entt::any> const  topData,
        Session const&              windowApp,
        Session const&              sceneRenderer,
        Session const&              commonScene,
        Session const&              physics,
        Session const&              physShapes)
{
    OSP_DECLARE_GET_DATA_IDS(sceneRenderer, TESTAPP_DATA_SCENE_RENDERER);
    OSP_DECLARE_GET_DATA_IDS(commonScene,   TESTAPP_DATA_COMMON_SCENE);
    OSP_DECLARE_GET_DATA_IDS(physics,       TESTAPP_DATA_PHYSICS);
    OSP_DECLARE_GET_DATA_IDS(physShapes,    TESTAPP_DATA_PHYS_SHAPES);
    auto const tgWin    = windowApp     .get_pipelines< PlWindowApp >();
    auto const tgScnRdr = sceneRenderer .get_pipelines< PlSceneRenderer >();
    auto const tgCS     = commonScene   .get_pipelines< PlCommonScene >();
    auto const tgShSp   = physShapes    .get_pipelines< PlPhysShapes >();

    Session out;

    rBuilder.task()
        .name       ("Create DrawEnts for spawned shapes")
        .run_on     ({tgShSp.spawnRequest(UseOrRun)})
        .sync_with  ({tgShSp.spawnedEnts(UseOrRun), tgCS.activeEntResized(Done), tgScnRdr.drawEntResized(ModifyOrSignal)})
        .push_to    (out.m_tasks)
        .args       ({               idBasic,             idDrawing,                 idScnRender,                idPhysShapes,             idNMesh })
        .func([]    (ACtxBasic const& rBasic, ACtxDrawing& rDrawing, ACtxSceneRender& rScnRender, ACtxPhysShapes& rPhysShapes, NamedMeshes& rNMesh) noexcept
    {
        for (std::size_t i = 0; i < rPhysShapes.m_spawnRequest.size(); ++i)
        {
            ActiveEnt const child            = rPhysShapes.m_ents[i * 2 + 1];
            rScnRender.m_activeToDraw[child] = rScnRender.m_drawIds.create();
        }
    });

    rBuilder.task()
        .name       ("Add mesh and material to spawned shapes")
        .run_on     ({tgShSp.spawnRequest(UseOrRun)})
        .sync_with  ({tgShSp.spawnedEnts(UseOrRun),
                      tgScnRdr.entMesh(New), tgScnRdr.material(New), tgScnRdr.drawEnt(New), tgScnRdr.drawEntResized(Done),
                      tgScnRdr.materialDirty(Modify_), tgScnRdr.entMeshDirty(Modify_)})
        .push_to    (out.m_tasks)
        .args       ({            idBasic,             idDrawing,                 idScnRender,                idPhysShapes,             idNMesh })
        .func([] (ACtxBasic const& rBasic, ACtxDrawing& rDrawing, ACtxSceneRender& rScnRender, ACtxPhysShapes& rPhysShapes, NamedMeshes& rNMesh) noexcept
    {
        Material &rMat = rScnRender.m_materials[rPhysShapes.m_materialId];

        for (std::size_t i = 0; i < rPhysShapes.m_spawnRequest.size(); ++i)
        {
            SpawnShape const &spawn = rPhysShapes.m_spawnRequest[i];
            ActiveEnt const root    = rPhysShapes.m_ents[i * 2];
            ActiveEnt const child   = rPhysShapes.m_ents[i * 2 + 1];
            DrawEnt const drawEnt   = rScnRender.m_activeToDraw[child];

            rScnRender.m_needDrawTf.set(std::size_t(root));
            rScnRender.m_needDrawTf.set(std::size_t(child));

            rScnRender.m_mesh[drawEnt] = rDrawing.m_meshRefCounts.ref_add(rNMesh.m_shapeToMesh.at(spawn.m_shape));
            rScnRender.m_meshDirty.push_back(drawEnt);

            rMat.m_ents.set(std::size_t(drawEnt));
            rMat.m_dirty.push_back(drawEnt);

            rScnRender.m_visible.set(std::size_t(drawEnt));
            rScnRender.m_opaque.set(std::size_t(drawEnt));
        }
    });

    // When does resync run relative to deletes?

    rBuilder.task()
        .name       ("Resync spawned shapes DrawEnts")
        .run_on     ({tgWin.resync(Run)})
        .sync_with  ({tgShSp.ownedEnts(UseOrRun_), tgCS.hierarchy(Ready), tgCS.activeEntResized(Done), tgScnRdr.drawEntResized(ModifyOrSignal)})
        .push_to    (out.m_tasks)
        .args       ({               idBasic,             idDrawing,                 idScnRender,                idPhysShapes,             idNMesh })
        .func([]    (ACtxBasic const& rBasic, ACtxDrawing& rDrawing, ACtxSceneRender& rScnRender, ACtxPhysShapes& rPhysShapes, NamedMeshes& rNMesh) noexcept
    {
        for (std::size_t entInt : rPhysShapes.ownedEnts.ones())
        {
            ActiveEnt const root = ActiveEnt(entInt);
            ActiveEnt const child = *SysSceneGraph::children(rBasic.m_scnGraph, root).begin();

            rScnRender.m_activeToDraw[child] = rScnRender.m_drawIds.create();
        }
    });

    rBuilder.task()
        .name       ("Resync spawned shapes mesh and material")
        .run_on     ({tgWin.resync(Run)})
        .sync_with  ({tgShSp.ownedEnts(UseOrRun_), tgScnRdr.entMesh(New), tgScnRdr.material(New), tgScnRdr.drawEnt(New), tgScnRdr.drawEntResized(Done),
                      tgScnRdr.materialDirty(Modify_), tgScnRdr.entMeshDirty(Modify_)})
        .push_to    (out.m_tasks)
        .args       ({            idBasic,             idDrawing,             idPhys,                idPhysShapes,                 idScnRender,             idNMesh })
        .func([] (ACtxBasic const& rBasic, ACtxDrawing& rDrawing, ACtxPhysics& rPhys, ACtxPhysShapes& rPhysShapes, ACtxSceneRender& rScnRender, NamedMeshes& rNMesh) noexcept
    {
        Material &rMat = rScnRender.m_materials[rPhysShapes.m_materialId];

        for (std::size_t entInt : rPhysShapes.ownedEnts.ones())
        {
            ActiveEnt const root = ActiveEnt(entInt);
            ActiveEnt const child = *SysSceneGraph::children(rBasic.m_scnGraph, root).begin();

            //SpawnShape const &spawn = rPhysShapes.m_spawnRequest[i];
            DrawEnt const drawEnt   = rScnRender.m_activeToDraw[child];

            rScnRender.m_needDrawTf.set(std::size_t(root));
            rScnRender.m_needDrawTf.set(std::size_t(child));

            EShape const shape = rPhys.m_shape.at(child);
            rScnRender.m_mesh[drawEnt] = rDrawing.m_meshRefCounts.ref_add(rNMesh.m_shapeToMesh.at(shape));
            rScnRender.m_meshDirty.push_back(drawEnt);

            rMat.m_ents.set(std::size_t(drawEnt));
            rMat.m_dirty.push_back(drawEnt);

            rScnRender.m_visible.set(std::size_t(drawEnt));
            rScnRender.m_opaque.set(std::size_t(drawEnt));
        }
    });

    rBuilder.task()
        .name       ("Remove deleted ActiveEnts from ACtxPhysShapeser")
        .run_on     ({tgCS.activeEntDelete(UseOrRun)})
        .sync_with  ({tgShSp.ownedEnts(Modify__)})
        .push_to    (out.m_tasks)
        .args       ({           idPhysShapes,                      idActiveEntDel })
        .func([] (ACtxPhysShapes& rPhysShapes, ActiveEntVec_t const& rActiveEntDel) noexcept
    {
        for (ActiveEnt const deleted : rActiveEntDel)
        {
            rPhysShapes.ownedEnts.reset(std::size_t(deleted));
        }
    });

    return out;
} // setup_phys_shapes_draw


//-----------------------------------------------------------------------------


Session setup_prefabs(
        TopTaskBuilder&             rBuilder,
        ArrayView<entt::any> const  topData,
        Session const&              application,
        Session const&              scene,
        Session const&              commonScene,
        Session const&              physics)
{
    OSP_DECLARE_GET_DATA_IDS(application,   TESTAPP_DATA_APPLICATION);
    OSP_DECLARE_GET_DATA_IDS(commonScene,   TESTAPP_DATA_COMMON_SCENE);
    OSP_DECLARE_GET_DATA_IDS(physics,       TESTAPP_DATA_PHYSICS);
    auto const tgScn    = scene         .get_pipelines<PlScene>();
    auto const tgCS     = commonScene   .get_pipelines<PlCommonScene>();
    auto const tgPhy    = physics       .get_pipelines<PlPhysics>();

    Session out;
    OSP_DECLARE_CREATE_DATA_IDS(out, topData,  TESTAPP_DATA_PREFABS);
    auto const tgPf = out.create_pipelines<PlPrefabs>(rBuilder);

    rBuilder.pipeline(tgPf.spawnRequest).parent(tgScn.update);
    rBuilder.pipeline(tgPf.spawnedEnts) .parent(tgScn.update);
    rBuilder.pipeline(tgPf.ownedEnts)   .parent(tgScn.update);

    top_emplace< ACtxPrefabInit > (topData, idPrefabInit);

    rBuilder.task()
        .name       ("Schedule Prefab spawn")
        .schedules  ({tgPf.spawnRequest(Schedule_)})
        .push_to    (out.m_tasks)
        .args       ({                 idPrefabInit })
        .func([] (ACtxPrefabInit const& rPrefabInit) noexcept -> TaskActions
    {
        return rPrefabInit.spawnRequest.empty() ? TaskAction::Cancel : TaskActions{};
    });

    rBuilder.task()
        .name       ("Create Prefab entities")
        .run_on     ({tgPf.spawnRequest(UseOrRun)})
        .sync_with  ({tgCS.activeEnt(New), tgCS.activeEntResized(Schedule), tgPf.spawnedEnts(Resize)})
        .push_to    (out.m_tasks)
        .args       ({      idBasic,                idPrefabInit,           idResources})
        .func([] (ACtxBasic& rBasic, ACtxPrefabInit& rPrefabInit, Resources& rResources) noexcept
    {
        // Count number of entities needed to be created
        std::size_t totalEnts = 0;
        for (TmpPrefabInitBasic const& rPfBasic : rPrefabInit.spawnRequest)
        {
            auto const& rPrefabData = rResources.data_get<Prefabs>(gc_importer, rPfBasic.m_importerRes);
            auto const& objects     = rPrefabData.m_prefabs[rPfBasic.m_prefabId];

            totalEnts += objects.size();
        }

        // Create entities
        rPrefabInit.newEnts.resize(totalEnts);
        rBasic.m_activeIds.create(std::begin(rPrefabInit.newEnts), std::end(rPrefabInit.newEnts));

        // Assign new entities to each prefab to create
        rPrefabInit.spawnedEntsOffset.resize(rPrefabInit.spawnRequest.size());
        auto itEntAvailable = std::begin(rPrefabInit.newEnts);
        auto itPfEntSpanOut = std::begin(rPrefabInit.spawnedEntsOffset);
        for (TmpPrefabInitBasic& rPfBasic : rPrefabInit.spawnRequest)
        {
            auto const& rPrefabData = rResources.data_get<Prefabs>(gc_importer, rPfBasic.m_importerRes);
            auto const& objects     = rPrefabData.m_prefabs[rPfBasic.m_prefabId];

            (*itPfEntSpanOut) = { &(*itEntAvailable), objects.size() };

            std::advance(itEntAvailable, objects.size());
            std::advance(itPfEntSpanOut, 1);
        }

        assert(itEntAvailable == std::end(rPrefabInit.newEnts));
    });

    rBuilder.task()
        .name       ("Init Prefab transforms")
        .run_on     ({tgPf.spawnRequest(UseOrRun)})
        .sync_with  ({tgPf.spawnedEnts(UseOrRun), tgCS.transform(New)})
        .push_to    (out.m_tasks)
        .args       ({      idBasic,           idResources,                idPrefabInit})
        .func([] (ACtxBasic& rBasic, Resources& rResources, ACtxPrefabInit& rPrefabInit) noexcept
    {
        SysPrefabInit::init_transforms(rPrefabInit, rResources, rBasic.m_transform);
    });


    rBuilder.task()
        .name       ("Init Prefab physics")
        .run_on     ({tgPf.spawnRequest(UseOrRun)})
        .sync_with  ({tgPf.spawnedEnts(UseOrRun), tgPhy.physBody(Modify), tgPhy.physUpdate(Done)})
        .push_to    (out.m_tasks)
        .args       ({      idBasic,           idResources,             idPhys,                idPrefabInit})
        .func([] (ACtxBasic& rBasic, Resources& rResources, ACtxPhysics& rPhys, ACtxPrefabInit& rPrefabInit) noexcept
    {
        rPhys.m_hasColliders.ints().resize(rBasic.m_activeIds.vec().capacity());
        //rPhys.m_massDirty.ints().resize(rActiveIds.vec().capacity());
        rPhys.m_shape.resize(rBasic.m_activeIds.capacity());
        SysPrefabInit::init_physics(rPrefabInit, rResources, rPhys);
    });

    rBuilder.task()
        .name       ("Clear Prefab vector")
        .run_on     ({tgPf.spawnRequest(Clear)})
        .push_to    (out.m_tasks)
        .args       ({           idPrefabInit})
        .func([] (ACtxPrefabInit& rPrefabInit) noexcept
    {
        rPrefabInit.spawnRequest.clear();
    });


    return out;
}

Session setup_prefab_draw(
        TopTaskBuilder&             rBuilder,
        ArrayView<entt::any> const  topData,
        Session const&              application,
        Session const&              windowApp,
        Session const&              sceneRenderer,
        Session const&              commonScene,
        Session const&              prefabs)
{
    OSP_DECLARE_GET_DATA_IDS(application,   TESTAPP_DATA_APPLICATION);
    OSP_DECLARE_GET_DATA_IDS(sceneRenderer, TESTAPP_DATA_SCENE_RENDERER);
    OSP_DECLARE_GET_DATA_IDS(commonScene,   TESTAPP_DATA_COMMON_SCENE);
    OSP_DECLARE_GET_DATA_IDS(prefabs,       TESTAPP_DATA_PREFABS);
    auto const tgWin    = windowApp     .get_pipelines< PlWindowApp >();
    auto const tgScnRdr = sceneRenderer .get_pipelines< PlSceneRenderer >();
    auto const tgCS     = commonScene   .get_pipelines< PlCommonScene >();
    auto const tgPf     = prefabs       .get_pipelines< PlPrefabs >();

    Session out;

    rBuilder.task()
        .name       ("Create DrawEnts for prefabs")
        .run_on     ({tgPf.spawnRequest(UseOrRun)})
        .sync_with  ({tgPf.spawnedEnts(UseOrRun), tgCS.activeEntResized(Done), tgScnRdr.drawEntResized(ModifyOrSignal)})
        .push_to    (out.m_tasks)
        .args       ({         idResources,                 idBasic,             idDrawing,                 idScnRender,                idPrefabInit })
        .func([]    (Resources& rResources, ACtxBasic const& rBasic, ACtxDrawing& rDrawing, ACtxSceneRender& rScnRender, ACtxPrefabInit& rPrefabInit) noexcept
    {
        auto itPfEnts = rPrefabInit.spawnedEntsOffset.begin();

        for (TmpPrefabInitBasic const& request : rPrefabInit.spawnRequest)
        {
            auto const &rImportData = rResources.data_get<osp::ImporterData const>(
                    gc_importer, request.m_importerRes);
            auto const &rPrefabData = rResources.data_get<osp::Prefabs const>(
                    gc_importer, request.m_importerRes);

            auto const objects = rPrefabData.m_prefabs[request.m_prefabId];

            for (std::size_t i = 0; i < objects.size(); ++i)
            {
                int const meshImportId = rImportData.m_objMeshes[objects[i]];
                if (meshImportId == -1)
                {
                    continue;
                }

                ActiveEnt const ent = (*itPfEnts)[i];
                rScnRender.m_activeToDraw[ent] = rScnRender.m_drawIds.create();
            }

            ++itPfEnts;
        }
    });

    rBuilder.task()
        .name       ("Add mesh and material to prefabs")
        .run_on     ({tgPf.spawnRequest(UseOrRun)})
        .sync_with  ({tgPf.spawnedEnts(UseOrRun),
                      tgScnRdr.entMesh(New), tgScnRdr.material(New), tgScnRdr.drawEnt(New), tgScnRdr.drawEntResized(Done),
                      tgScnRdr.materialDirty(Modify_), tgScnRdr.entMeshDirty(Modify_)})
        .push_to    (out.m_tasks)
        .args       ({      idResources,                 idBasic,             idDrawing,                idDrawingRes,                 idScnRender,                idPrefabInit})
        .func([] (Resources& rResources, ACtxBasic const& rBasic, ACtxDrawing& rDrawing, ACtxDrawingRes& rDrawingRes, ACtxSceneRender& rScnRender, ACtxPrefabInit& rPrefabInit) noexcept
    {
        auto itPfEnts = rPrefabInit.spawnedEntsOffset.begin();

        for (TmpPrefabInitBasic const& rPfBasic : rPrefabInit.spawnRequest)
        {
            auto const &rImportData = rResources.data_get<osp::ImporterData const>(
                    gc_importer, rPfBasic.m_importerRes);
            auto const &rPrefabData = rResources.data_get<osp::Prefabs const>(
                    gc_importer, rPfBasic.m_importerRes);

            auto const ents     = ArrayView<ActiveEnt const>{*itPfEnts};
            auto const objects  = lgrn::Span<int const>{rPrefabData.m_prefabs[rPfBasic.m_prefabId]};
            auto const parents  = lgrn::Span<int const>{rPrefabData.m_prefabParents[rPfBasic.m_prefabId]};

            // All ancestors of  each entity that has a mesh
            auto const needs_draw_transform
                = [&parents, &ents, &rDrawing, &needDrawTf = rScnRender.m_needDrawTf]
                  (auto && self, int const object, ActiveEnt const ent) noexcept -> void
            {
                needDrawTf.set(std::size_t(ent));

                int const parentObj = parents[object];

                if (parentObj != -1)
                {
                    self(self, parentObj, ents[parentObj]);
                }
            };

            for (std::size_t i = 0; i < objects.size(); ++i)
            {
                ActiveEnt const ent = (*itPfEnts)[i];

                // Check if object has mesh
                int const meshImportId = rImportData.m_objMeshes[objects[i]];
                if (meshImportId == -1)
                {
                    continue;
                }

                needs_draw_transform(needs_draw_transform, objects[i], ent);

                DrawEnt const drawEnt = rScnRender.m_activeToDraw[ent];

                osp::ResId const meshRes = rImportData.m_meshes[meshImportId];
                MeshId const meshId = SysRender::own_mesh_resource(rDrawing, rDrawingRes, rResources, meshRes);
                rScnRender.m_mesh[drawEnt] = rDrawing.m_meshRefCounts.ref_add(meshId);
                rScnRender.m_meshDirty.push_back(drawEnt);

                int const matImportId = rImportData.m_objMaterials[objects[i]];

                if (Magnum::Trade::MaterialData const &mat = *rImportData.m_materials.at(matImportId);
                    mat.types() & Magnum::Trade::MaterialType::PbrMetallicRoughness)
                {
                    auto const& matPbr = mat.as<Magnum::Trade::PbrMetallicRoughnessMaterialData>();
                    if (int const baseColor = matPbr.baseColorTexture();
                        baseColor != -1)
                    {
                        osp::ResId const texRes = rImportData.m_textures[baseColor];
                        TexId const texId = SysRender::own_texture_resource(rDrawing, rDrawingRes, rResources, texRes);
                        rScnRender.m_diffuseTex[drawEnt] = rDrawing.m_texRefCounts.ref_add(texId);
                        rScnRender.m_diffuseDirty.push_back(drawEnt);
                    }
                }

                rScnRender.m_opaque.set(std::size_t(drawEnt));
                rScnRender.m_visible.set(std::size_t(drawEnt));

                // TODO: assign proper material
                rScnRender.m_materials[MaterialId{2}].m_dirty.push_back(drawEnt);
                rScnRender.m_materials[MaterialId{2}].m_ents.set(std::size_t(drawEnt));

            }

            ++itPfEnts;
        }
    });


//    rBuilder.task()
//        .name       ("Resync spawned shapes DrawEnts")
//        .run_on     ({tgWin.resync(Run)})
//        .sync_with  ({tgShSp.ownedEnts(UseOrRun_), tgCS.hierarchy(Ready), tgCS.activeEntResized(Done), tgScnRdr.drawEntResized(ModifyOrSignal)})
//        .push_to    (out.m_tasks)
//        .args       ({               idBasic,             idDrawing,                 idScnRender,                idPhysShapes,             idNMesh })
//        .func([]    (ACtxBasic const& rBasic, ACtxDrawing& rDrawing, ACtxSceneRender& rScnRender, ACtxPhysShapes& rPhysShapes, NamedMeshes& rNMesh) noexcept
//    {
//        for (std::size_t entInt : rPhysShapes.ownedEnts.ones())
//        {
//            ActiveEnt const root = ActiveEnt(entInt);
//            ActiveEnt const child = *SysSceneGraph::children(rBasic.m_scnGraph, root).begin();

//            rScnRender.m_activeToDraw[child] = rScnRender.m_drawIds.create();
//        }
//    });

//    rBuilder.task()
//        .name       ("Resync spawned shapes mesh and material")
//        .run_on     ({tgWin.resync(Run)})
//        .sync_with  ({tgShSp.ownedEnts(UseOrRun_), tgScnRdr.entMesh(New), tgScnRdr.material(New), tgScnRdr.drawEnt(New), tgScnRdr.drawEntResized(Done),
//                      tgScnRdr.materialDirty(Modify_), tgScnRdr.entMeshDirty(Modify_)})
//        .push_to    (out.m_tasks)
//        .args       ({            idBasic,             idDrawing,             idPhys,                idPhysShapes,                 idScnRender,             idNMesh })
//        .func([] (ACtxBasic const& rBasic, ACtxDrawing& rDrawing, ACtxPhysics& rPhys, ACtxPhysShapes& rPhysShapes, ACtxSceneRender& rScnRender, NamedMeshes& rNMesh) noexcept
//    {
//        Material &rMat = rScnRender.m_materials[rPhysShapes.m_materialId];

//        for (std::size_t entInt : rPhysShapes.ownedEnts.ones())
//        {
//            ActiveEnt const root = ActiveEnt(entInt);
//            ActiveEnt const child = *SysSceneGraph::children(rBasic.m_scnGraph, root).begin();

//            //SpawnShape const &spawn = rPhysShapes.m_spawnRequest[i];
//            DrawEnt const drawEnt   = rScnRender.m_activeToDraw[child];

//            rScnRender.m_needDrawTf.set(std::size_t(root));
//            rScnRender.m_needDrawTf.set(std::size_t(child));

//            EShape const shape = rPhys.m_shape.at(child);
//            rScnRender.m_mesh[drawEnt] = rDrawing.m_meshRefCounts.ref_add(rNMesh.m_shapeToMesh.at(shape));
//            rScnRender.m_meshDirty.push_back(drawEnt);

//            rMat.m_ents.set(std::size_t(drawEnt));
//            rMat.m_dirty.push_back(drawEnt);

//            rScnRender.m_visible.set(std::size_t(drawEnt));
//            rScnRender.m_opaque.set(std::size_t(drawEnt));
//        }
//    });


    return out;
} // setup_phys_shapes_draw

//    if (material.m_dataIds.empty())
//    {
//        prefabs.task() = rBuilder.task().assign({tgSceneEvt, tgPrefabReq, tgPrefabEntReq, tgDrawMod, tgMeshMod}).data(
//                "Init Prefab drawables (no material)",
//                TopDataIds_t{                idPrefabInit,           idResources,             idDrawing,                idDrawingRes },
//                wrap_args([] (ACtxPrefabInit& rPrefabInit, Resources& rResources, ACtxDrawing& rDrawing, ACtxDrawingRes& rDrawingRes) noexcept
//        {
//            SysPrefabInit::init_drawing(rPrefabInit, rResources, rDrawing, rDrawingRes, {});
//        }));
//    }
//    else
//    {
//        OSP_SESSION_UNPACK_DATA(material,   TESTAPP_MATERIAL);
//        OSP_SESSION_UNPACK_TAGS(material,   TESTAPP_MATERIAL);

//        prefabs.task() = rBuilder.task().assign({tgSceneEvt, tgPrefabReq, tgPrefabEntReq, tgDrawMod, tgMeshMod, tgMatMod}).data(
//                "Init Prefab drawables (single material)",
//                TopDataIds_t{                idPrefabInit,           idResources,             idDrawing,                idDrawingRes,          idMatEnts,                      idMatDirty,                   idActiveIds },
//                wrap_args([] (ACtxPrefabInit& rPrefabInit, Resources& rResources, ACtxDrawing& rDrawing, ACtxDrawingRes& rDrawingRes, EntSet_t& rMatEnts, std::vector<DrawEnt>& rMatDirty, ActiveReg_t const& rActiveIds) noexcept
//        {
//            rDrawing.resize_active(rActiveIds.capacity());
//            rMatEnts.ints().resize(rActiveIds.vec().capacity());
//            SysPrefabInit::init_drawing(rPrefabInit, rResources, rDrawing, rDrawingRes, {{rMatEnts, rMatDirty}});
//        }));
//    }


} // namespace testapp::scenes
