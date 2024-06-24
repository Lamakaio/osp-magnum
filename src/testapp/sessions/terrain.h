/**
 * Open Space Program
 * Copyright © 2019-2023 Open Space Program Project
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
#pragma once

#include <osp/tasks/tasks.h>
#include <osp/core/keyed_vector.h>
#include <osp/core/resourcetypes.h>
#include <osp/tasks/top_execute.h>
#include <osp/tasks/top_session.h>
#include <osp/util/logging.h>

#include <entt/core/any.hpp>
#include "planet-a/chunk_utils.h"

#include <osp/core/math_types.h>
#include <osp/drawing/draw_ent.h>

#include <planet-a/skeleton.h>
#include <planet-a/skeleton_subdiv.h>
#include <planet-a/chunk_generate.h>

namespace testapp::scenes
{

/**
 * @brief Scene orientation relative to planet center
 *
 * This is intended to be modified by a system responsible for handling floating origin and
 * the Scene's position within a universe.
 */
struct ACtxTerrainFrame
{
    osp::Vector3l       position;
    osp::Quaterniond    rotation;
    bool                active      {false};
};

struct ACtxTerrain
{
    // 'Skeleton' used for managing instances and relationships between vertices, triangles, and
    // chunks. These are all integer IDs that can be parented, connected, neighboring, etc...
    planeta::SubdivTriangleSkeleton     skeleton;
    planeta::SkeletonVertexData         skData;
    planeta::ChunkSkeleton              skChunks;

    planeta::ChunkMeshBufferInfo        chunkInfo{};
    planeta::BasicChunkMeshGeometry     chunkGeom;

    planeta::ChunkScratchpad            chunkSP;
    planeta::SkeletonSubdivScratchpad   scratchpad;
};

struct ACtxTerrainIco
{
    /// Planet lowest ground level in meters
    double  radius{};

    /// Planet max ground height in meters
    double  height{};

    std::array<planeta::SkVrtxId,     12>   icoVrtx;
    std::array<planeta::SkTriGroupId, 5>    icoGroups;
    std::array<planeta::SkTriId,      20>   icoTri;
};


/**
 * @brief Skeleton, mesh data, and scratchpads to support a single terrain surface within a scene
 */
osp::Session setup_terrain(
        osp::TopTaskBuilder         &rBuilder,
        osp::ArrayView<entt::any>   topData,
        osp::Session          const &scene);

/**
 * @brief Icosahedron-specific data for spherical planet terrains
 */
osp::Session setup_terrain_icosahedron(
        osp::TopTaskBuilder&        rBuilder,
        osp::ArrayView<entt::any>   topData,
        osp::Session          const &terrain);

/**
 * @brief Subdivide-by-distance logic for icosahedron sphere planets
 */
osp::Session setup_terrain_subdiv_dist(
        osp::TopTaskBuilder&        rBuilder,
        osp::ArrayView<entt::any>   topData,
        osp::Session          const &scene,
        osp::Session          const &terrain,
        osp::Session          const &terrainIco);


struct TerrainTestPlanetSpecs
{
    /// Planet lowest ground level in meters
    double          radius              {};

    /// Planet max ground height in meters
    double          height              {};

    /// Skeleton Vector3l precision (2^precision units = 1 meter)
    int             skelPrecision       {};

    /// Skeleton max subdivision levels. 0 for no subdivision. Max is 9.
    std::uint8_t    skelMaxSubdivLevels {};

    /// Number of times an initial triangle is subdivided to form a chunk.
    /// Due to bugs (LOL XD): Minimum is 2, Maximum is 8.
    std::uint8_t    chunkSubdivLevels   {};
};


/**
 * @brief Allocate and set parameters for a icosahedron planet, given specifications
 */
void initialize_ico_terrain(
        osp::ArrayView<entt::any>   topData,
        osp::Session          const &terrain,
        osp::Session          const &terrainIco,
        TerrainTestPlanetSpecs      params);


/**
 * @brief Uses camera target as position relative to planet, and visualizes terrain skeleton.
 */
osp::Session setup_terrain_debug_draw(
        osp::TopTaskBuilder&        rBuilder,
        osp::ArrayView<entt::any>   topData,
        osp::Session          const &windowApp,
        osp::Session          const &sceneRenderer,
        osp::Session          const &cameraCtrl,
        osp::Session          const &commonScene,
        osp::Session          const &terrain,
        osp::Session          const &terrainIco,
        osp::draw::MaterialId       mat);

} // namespace testapp::scenes
