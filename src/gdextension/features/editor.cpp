#include "editor.h"

#include "../feature_interfaces.h"
#include "adera_app/feature_interfaces.h"
#include "osp/activescene/prefab_fn.h"
#include "osp/core/Resources.h"
#include "osp/vehicles/prefabs.h"
#include "osp/vehicles/ImporterData.h"
#include <utility>

using namespace osp::active;
using namespace osp::draw;
using namespace osp::fw;
using namespace ftr_inter;
using namespace ftr_inter::stages;

using namespace osp;
using namespace godot;
using namespace osp::restypes;
namespace ospgdext
{
osp::fw::FeatureDef const ftrEditor = feature_def("Editor", [] (
        FeatureBuilder              &rFB,
        Implement<FIEditor>         editor,
        DependOn<FICleanupContext>  cleanup,
        DependOn<FIPrefabs>         prefabs,
        DependOn<FIMainApp>         mainApp)
{

    rFB.pipeline(editor.pl.spawn).parent(mainApp.pl.mainLoop);
    ACtxEditor& rEditor = rFB.data_emplace<ACtxEditor>(editor.di.ctx);
    Resources&  rResources = rFB.data_get<Resources>(mainApp.di.resources);

    for (unsigned int i = 0; i < rResources.ids(gc_importer).capacity(); ++i)
    {
        auto const resId = ResId(i);
        if ( ! rResources.ids(gc_importer).exists(resId))
        {
            continue;
        }

        auto const *pPrefabData = rResources.data_try_get<osp::Prefabs>(gc_importer, resId);
        if (pPrefabData == nullptr)
        {
            continue; // No prefab data
        }

        for (osp::PrefabId j = 0; j < pPrefabData->m_prefabNames.size(); ++j)
        {
            rEditor.m_prefabs.emplace(pPrefabData->m_prefabNames[j], ACtxEditor::PrefabRes {resId, j});
        }
    }

    // rFB.pipeline(godot.pl.mesh).parent(windowApp.pl.sync);
    // rFB.pipeline(godot.pl.texture).parent(windowApp.pl.sync);
    // rFB.pipeline(godot.pl.entMesh).parent(windowApp.pl.sync);
    // rFB.pipeline(godot.pl.entTexture).parent(windowApp.pl.sync);
    // Order-dependent; MagnumApplication construction starts OpenGL context, needed by RenderGL
    /* unused */ // rFB.data_emplace<MagnumApplication>(idActiveApp, args, rUserInput);
    rFB.task()
        .name("Handle new selected part")
        .run_on({ editor.pl.spawn(Run) })
        .args({ editor.di.ctx, prefabs.di.prefabs })
        .func([](ACtxEditor &rEditor, ACtxPrefabs &rPrefabs) noexcept {
            const PartInfo* selectedPart = rEditor.m_selectedPart;
            if (selectedPart == nullptr) { return; }

            rEditor.m_selectedPart = nullptr;
            auto& rPrefabPair = rEditor.m_prefabs[selectedPart->prefab];
            rPrefabs.spawnRequest.push_back(
                TmpPrefabRequest {
                rPrefabPair.m_importer, 
                rPrefabPair.m_prefabId, 
                new Matrix4()
                });
        });
}); // ftrGodot

}