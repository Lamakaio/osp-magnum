#pragma once

#include "godot_utils.h"
#include <godot_cpp/classes/class_db_singleton.hpp>
#include <godot_cpp/classes/global_constants.hpp>
#include <godot_cpp/classes/input_event.hpp>
#include <godot_cpp/classes/material.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/resource.hpp>
#include <godot_cpp/classes/thread.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/variant.hpp>
#include <map>
#include <sstream>

#include <adera_app/application.h>
#include <osp/core/resourcetypes.h>
#include <osp/framework/executor.h>
#include <osp/framework/framework.h>
#include <osp/util/UserInputHandler.h>
#include <vector>

namespace godot
{
using namespace osp::input;
class GodotTestScene : public Node3D
{
    GDCLASS(GodotTestScene, Node3D)

private:
    using ExecutorType = osp::fw::SingleThreadedExecutor;

    struct UpdateParams {
        float deltaTimeIn;
        bool update;
        bool sceneUpdate;
        bool resync;
        bool sync;
        bool render;
    };

    std::map<const char*, void*>         m_argmap;
    std::map<const char*, Ref<Resource>> m_resmap;

    RID               m_scenario;
    RID               m_viewport;
    std::vector<RID>  m_mats;
    Node3D*           m_light;

    //TestApp           m_testApp;
    //adera::MainLoopControl  *m_mainLoopCtrl;

    //MainLoopSignals   m_signals;

    std::stringstream m_dbgStream;
    std::stringstream m_errStream;
    std::stringstream m_warnStream;

    ExecutorType      m_executor;
    //UserInputHandler  m_userInput;

    osp::fw::Framework m_framework;
    osp::fw::ContextId m_mainContext;
    //osp::fw::IExecutor *m_pExecutor  { nullptr };
    osp::PkgId         m_defaultPkg  { lgrn::id_null<osp::PkgId>() };

    void              clear_resource_owners();

    // testapp has drive_default_main_loop(), but this is mostly for driving the CLI, but there's
    // no equivalent here

    void drive_scene_cycle(UpdateParams p);
    void run_context_cleanup(osp::fw::ContextId);

    void              load_a_bunch_of_stuff();
    void              setup_app();
    void              draw_event();
    void              destroy_app();

protected:
    static void _bind_methods();

public:
    GodotTestScene();
    ~GodotTestScene();

    // void _process(double delta) override;
    void              _enter_tree() override;
    void              _exit_tree() override;
    void              _ready() override;
    void              _physics_process(double delta) override;
    void              _process(double delta) override;
    void              _input(const Ref<InputEvent> &input) override;

    inline godot::RID get_main_scenario()
    {
        return m_scenario;
    };
    inline godot::RID get_main_viewport()
    {
        return m_viewport;
    };

    inline std::vector<godot::RID> get_godot_mats()
    {
        return m_mats;
    };
    //some template black magic to add arguments easily
    //Constexpr string

    template<class T, StringLiteral Name>
    static void register_arg() {
        constexpr size_t N = Name.size();
        constexpr StringLiteral<N+4> get_name = concat<5, N, "get_", Name>();
        constexpr StringLiteral<N+4> set_name = concat<5, N, "set_", Name>();
        ClassDB::bind_method(D_METHOD(get_name.value), &GodotTestScene::get_<T, Name>);
        ClassDB::bind_method(D_METHOD(set_name.value, Name.value), &GodotTestScene::set_<T, Name>);
        ClassDB::add_property("GodotTestScene", PropertyInfo(get_gd_type<T>(), Name.value),
                                set_name.value, get_name.value);
    }

    template<class T, StringLiteral Name, StringLiteral ResTypeHint>
    static void register_res_arg() {
        constexpr size_t N = Name.size();
        constexpr StringLiteral<N+4> get_name = concat<5, N, "get_", Name>();
        constexpr StringLiteral<N+4> set_name = concat<5, N, "set_", Name>();
        ClassDB::bind_method(D_METHOD(get_name.value), &GodotTestScene::get_res<T, Name>);
        ClassDB::bind_method(D_METHOD(set_name.value, Name.value), &GodotTestScene::set_res<T, Name>);
        ClassDB::add_property("GodotTestScene", PropertyInfo(Variant::OBJECT, Name.value, PROPERTY_HINT_RESOURCE_TYPE, ResTypeHint.value),
                                set_name.value, get_name.value);
    }

    template<class T, StringLiteral S>
    const T &get_() {
        if (!m_argmap.contains(S.value)) 
        {
            m_argmap[S.value] = new T;
        }
        return * (T*) m_argmap[S.value];
    }

    template<class T, StringLiteral S>
    void set_(const T& in) {
        if (!m_argmap.contains(S.value)) 
        {
            m_argmap[S.value] = new T;
        }
        * (T*) m_argmap[S.value] = in;
    }

    template<class T, StringLiteral S>
    Ref<T> get_res() {
        if (!m_resmap.contains(S.value)) 
        {
            return Ref<T>(nullptr);
        }
        return m_resmap[S.value];
    }

    template<class T, StringLiteral S>
    void set_res(Ref<T> in) {
        m_resmap[S.value] = in;
    }

//    inline void set_user_input(UserInputHandler *pUserInput)
//    {
//        m_pUserInput = pUserInput;
//    }
//    inline void set_ctrl(adera::MainLoopControl *mainLoopCtrl)
//    {
//        m_mainLoopCtrl = mainLoopCtrl;
//        //m_signals      = signals;
//    }
};

} // namespace godot
