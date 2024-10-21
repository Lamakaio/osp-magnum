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
#pragma once

#include "game.h"
#include "osp/vehicles/prefabs.h"
#include "render.h"

#include <osp/framework/builder.h>

#include <osp/activescene/basic.h>
#include <osp/drawing/drawing.h>

namespace ospgdext
{
using namespace osp;

struct ACtxEditor
{
    struct PrefabRes {
        ResId m_importer;
        PrefabId m_prefabId;
    };
    const godot::PartInfo* m_selectedPart = nullptr;
    // entt::dense_map< std::string_view, PrefabRes > m_prefabs;
    std::map< std::string_view, PrefabRes > m_prefabs;
    // std::vector<TmpPrefabRequest>               spawnRequest;
    // std::vector< ArrayView<ActiveEnt const> >   spawnedEntsOffset;
    // std::vector<ActiveEnt>                      newEnts;

    // osp::active::ActiveEntSet_t                 roots;
    // KeyedVec<ActiveEnt, PrefabInstanceInfo>     instanceInfo;
};

extern osp::fw::FeatureDef const ftrEditor;

} // namespace ospgdext

