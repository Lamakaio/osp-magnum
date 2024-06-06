/**
 * Open Space Program
 * Copyright © 2019-2024 Open Space Program Project
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

#include "forcefactors.h"

#include <osp/activescene/basic.h>
#include <osp/core/id_map.h>

#include <Jolt/Jolt.h>
#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Physics/PhysicsSettings.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/Shape/CylinderShape.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyActivationListener.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/Collision/Shape/CompoundShape.h>
#include <Jolt/Physics/Collision/Shape/MutableCompoundShape.h>
#include <Jolt/Physics/PhysicsStepListener.h>
#include <Jolt/Core/JobSystemSingleThreaded.h>

#include <longeron/id_management/registry_stl.hpp>
#include <longeron/id_management/id_set_stl.hpp>

#include <entt/core/any.hpp>

#include <iostream>
#include <cstdarg>
#include <thread>

// Callback for traces, connect this to your own trace function if you have one
static void TraceImpl(const char *inFMT, ...)
{
	// Format the message
	va_list list;
	va_start(list, inFMT);
	char buffer[1024];
	vsnprintf(buffer, sizeof(buffer), inFMT, list);
	va_end(list);

	// Print to the TTY
	std::cout << buffer << std::endl;
}

#ifdef JPH_ENABLE_ASSERTS

// Callback for asserts, connect this to your own assert handler if you have one
static bool AssertFailedImpl(const char *inExpression, const char *inMessage, const char *inFile, uint inLine)
{
	// Print to the TTY
	std::cout << inFile << ":" << inLine << ": (" << inExpression << ") " << (inMessage != nullptr? inMessage : "") << std::endl;

	// Breakpoint
	return true;
};

#endif // JPH_ENABLE_ASSERTS



//disable common warning triggered by Jolt
JPH_SUPPRESS_WARNINGS

namespace ospjolt
{
using namespace JPH;

using JoltBodyPtr_t = std::unique_ptr< Body >;

using OspBodyId = uint32_t;
using JoltBodyId = BodyID;

/**
 * @brief The different physics layers for the simulation
 */
namespace Layers
{
	static constexpr ObjectLayer NON_MOVING = 0;
	static constexpr ObjectLayer MOVING = 1;
	static constexpr ObjectLayer NUM_LAYERS = 2;
};
/**
 * @brief Class that determines if two object layers can collide
 */
class ObjectLayerPairFilterImpl : public ObjectLayerPairFilter
{
public:
	virtual bool ShouldCollide(ObjectLayer inObject1, ObjectLayer inObject2) const override
	{
		switch (inObject1)
		{
		case Layers::NON_MOVING:
			return inObject2 == Layers::MOVING; // Non moving only collides with moving
		case Layers::MOVING:
			return true; // Moving collides with everything
		default:
			JPH_ASSERT(false);
			return false;
		}
	}
};
/**
 * @brief The different broad phase layers (currently identical to physics Layers, but might change in the future)
 */
namespace BroadPhaseLayers
{
	static constexpr BroadPhaseLayer NON_MOVING(0);
	static constexpr BroadPhaseLayer MOVING(1);
	static constexpr uint NUM_LAYERS(2);
};

/**
 * @brief Class that defines a mapping between object and broadphase layers.
 */
class BPLayerInterfaceImpl final : public BroadPhaseLayerInterface
{
public:
									BPLayerInterfaceImpl()
	{
		// Create a mapping table from object to broad phase layer
		mObjectToBroadPhase[Layers::NON_MOVING] = BroadPhaseLayers::NON_MOVING;
		mObjectToBroadPhase[Layers::MOVING] = BroadPhaseLayers::MOVING;
	}

	virtual uint					GetNumBroadPhaseLayers() const override
	{
		return BroadPhaseLayers::NUM_LAYERS;
	}

	virtual BroadPhaseLayer			GetBroadPhaseLayer(ObjectLayer inLayer) const override
	{
		JPH_ASSERT(inLayer < Layers::NUM_LAYERS);
		return mObjectToBroadPhase[inLayer];
	}

#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
	/// Get the user readable name of a broadphase layer (debugging purposes)
	virtual const char *			GetBroadPhaseLayerName(BroadPhaseLayer inLayer) const override
	{
		if (inLayer == BroadPhaseLayers::NON_MOVING) {
			return "NON_MOVING";
		}
		else {
			return "MOVING";
		}
	}
#endif // JPH_EXTERNAL_PROFILE || JPH_PROFILE_ENABLED

private:
	BroadPhaseLayer					mObjectToBroadPhase[Layers::NUM_LAYERS];
};
/**
 * @brief Class that determines if an object layer can collide with a broadphase layer
 */
class ObjectVsBroadPhaseLayerFilterImpl : public ObjectVsBroadPhaseLayerFilter
{
public:
	virtual bool				ShouldCollide(ObjectLayer inLayer1, BroadPhaseLayer inLayer2) const override
	{
		switch (inLayer1)
		{
		case Layers::NON_MOVING:
			return inLayer2 == BroadPhaseLayers::MOVING;
		case Layers::MOVING:
			return true;
		default:
			JPH_ASSERT(false);
			return false;
		}
	}
};

//Forward declaration
struct ACtxJoltWorld;

class PhysicsStepListenerImpl : public PhysicsStepListener
{
public:
    PhysicsStepListenerImpl(ACtxJoltWorld* pContext) : m_context(pContext) {};
    virtual void OnStep(float inDeltaTime, PhysicsSystem& inPhysicsSystem) override;

private:
    ACtxJoltWorld* m_context;
};

using TransformedShapePtr_t = std::unique_ptr<TransformedShape>;
using ShapeStorage_t = osp::Storage_t<osp::active::ActiveEnt, TransformedShapePtr_t>;

/**
 * @brief Represents an instance of a Jolt physics world in the scene
 */
struct ACtxJoltWorld
{

    struct ForceFactorFunc
    {
        using UserData_t = std::array<void*, 6u>;
        using Func_t = void (*)(JoltBodyId joltBodyId, OspBodyId ospBodyId, ACtxJoltWorld const&, UserData_t, osp::Vector3&, osp::Vector3&) noexcept;

        Func_t      m_func;
        UserData_t  m_userData;
    };
    // The default values are the one suggested in the Jolt hello world exemple for a "real" project.
    // It might be overkill here.
	//TODO temp allocator
    ACtxJoltWorld(  int threadCount = 2,
                    uint maxBodies = 65536, 
                    uint numBodyMutexes = 0, 
                    uint maxBodyPairs = 65536, 
                    uint maxContactConstraints = 10240
                ) : m_world(std::make_unique<PhysicsSystem>()), 
				    m_joltJobSystem(std::make_unique<JobSystemSingleThreaded>(cMaxPhysicsJobs)) //TODO multithreading
    {
        m_world->Init(maxBodies, 
                    numBodyMutexes, 
                    maxBodyPairs, 
                    maxContactConstraints, 
                    m_bPLInterface, 	
                    m_objectVsBPLFilter, 
                    m_objectLayerFilter);
		m_world->SetGravity(Vec3Arg::sZero());
		m_listener = std::make_unique<PhysicsStepListenerImpl>(this);
		m_world->AddStepListener(m_listener.get());
    }

	static void initJoltGlobal() 
	{
	
		RegisterDefaultAllocator();

		// Install trace and assert callbacks
		Trace = TraceImpl;
		JPH_IF_ENABLE_ASSERTS(AssertFailed = AssertFailedImpl;)

		// Create a factory, this class is responsible for creating instances of classes based on their name or hash and is mainly used for deserialization of saved data.
		// It is not directly used in this example but still required.
		Factory::sInstance = new Factory();

		// Register all physics types with the factory and install their collision handlers with the CollisionDispatch class.
		// If you have your own custom shape types you probably need to register their handlers with the CollisionDispatch before calling this function.
		// If you implement your own default material (PhysicsMaterial::sDefault) make sure to initialize it before this function or else this function will create one for you.
		RegisterTypes();

	}


    TempAllocatorMalloc                             m_temp_allocator;
    ObjectLayerPairFilterImpl                       m_objectLayerFilter;
    BPLayerInterfaceImpl                            m_bPLInterface;
    ObjectVsBroadPhaseLayerFilterImpl               m_objectVsBPLFilter;
    std::unique_ptr<JobSystem>                      m_joltJobSystem;

    std::unique_ptr<PhysicsSystem>                  m_world;

	std::unique_ptr<PhysicsStepListenerImpl>		m_listener;

    lgrn::IdRegistryStl<OspBodyId>                  m_bodyIds;
    //std::vector<JoltBodyPtr_t>                      m_bodyPtrs;
    std::vector<ForceFactors_t>                     m_bodyFactors;
    lgrn::IdSetStl<OspBodyId>                       m_bodyDirty;

    std::vector<JoltBodyId>                         m_ospToJoltBodyId;
    std::vector<osp::active::ActiveEnt>             m_bodyToEnt;
    osp::IdMap_t<osp::active::ActiveEnt, OspBodyId> m_entToBody;

    std::vector<ForceFactorFunc>                    m_factors;
	ShapeStorage_t                                  m_shapes;


    osp::active::ACompTransformStorage_t            *m_pTransform;
};



}
