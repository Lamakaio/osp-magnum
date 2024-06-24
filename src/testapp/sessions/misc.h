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

#include <osp/tasks/tasks.h>
#include <osp/core/keyed_vector.h>
#include <osp/core/resourcetypes.h>
#include <osp/tasks/top_execute.h>
#include <osp/tasks/top_session.h>
#include <osp/util/logging.h>

#include <entt/core/any.hpp>

#include <osp/activescene/basic.h>
#include <osp/drawing/drawing.h>

namespace testapp::scenes
{

void create_materials(
        osp::ArrayView<entt::any>   topData,
        osp::Session const&         sceneRenderer,
        int                         count);

/**
 * @brief Create CameraController connected to an app's UserInputHandler
 */
osp::Session setup_camera_ctrl(
        osp::TopTaskBuilder&        rBuilder,
        osp::ArrayView<entt::any>   topData,
        osp::Session const&         windowApp,
        osp::Session const&         sceneRenderer,
        osp::Session const&         magnumScene);

/**
 * @brief Adds free cam controls to a CameraController
 */
osp::Session setup_camera_free(
        osp::TopTaskBuilder&        rBuilder,
        osp::ArrayView<entt::any>   topData,
        osp::Session const&         windowApp,
        osp::Session const&         scene,
        osp::Session const&         camera);

/**
 * @brief Wireframe cube over the camera controller's target
 */
osp::Session setup_cursor(
        osp::TopTaskBuilder&        rBuilder,
        osp::ArrayView<entt::any>   topData,
        osp::Session const&         application,
        osp::Session const&         sceneRenderer,
        osp::Session const&         cameraCtrl,
        osp::Session const&         commonScene,
        osp::draw::MaterialId const material,
        osp::PkgId const            pkg);


}
