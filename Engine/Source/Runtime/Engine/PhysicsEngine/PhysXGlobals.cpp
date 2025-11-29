//
// PhysX bootstrap and globals implementation
//
// Creates/owns the PhysX SDK, dispatcher, scene, default material, and cooking interface.
//

#include "pch.h"
#include "PhysXGlobals.h"

using namespace physx;

// Define global objects declared in pch.h (extern)
PxDefaultAllocator gAllocator;
PxDefaultErrorCallback gErrorCallback;
PxFoundation* gFoundation = nullptr;
PxPhysics* gPhysics = nullptr;
PxScene* gScene = nullptr;
PxMaterial* gMaterial = nullptr;
PxDefaultCpuDispatcher* gDispatcher = nullptr;
PxCooking* gCooking = nullptr;

// Local PVD pointers (kept internal to this translation unit)
static PxPvd* sPvd = nullptr;
static PxPvdTransport* sPvdTransport = nullptr;

namespace PhysXGlobals
{
    static void CreateDefaultScene()
    {
        PxTolerancesScale scale;
        PxSceneDesc sceneDesc(scale);
        sceneDesc.gravity = PxVec3(0.0f, 0.0f, -9.81f);

        gDispatcher = PxDefaultCpuDispatcherCreate(2);
        sceneDesc.cpuDispatcher = gDispatcher;
        sceneDesc.filterShader = PxDefaultSimulationFilterShader;

        gScene = gPhysics->createScene(sceneDesc);
    }

    static void CreateDefaultMaterial()
    {
        // static friction, dynamic friction, restitution
        gMaterial = gPhysics->createMaterial(0.5f, 0.5f, 0.1f);
    }

    bool InitializePhysX(bool bEnablePvd)
    {
        if (gPhysics) return true; // already initialized

        gFoundation = PxCreateFoundation(PX_PHYSICS_VERSION, gAllocator, gErrorCallback);
        if (!gFoundation)
        {
            UE_LOG("[PhysX] ERROR: Failed to create PxFoundation");
            return false;
        }

        // Optional: PhysX Visual Debugger
        PxPvd* pvdForCreate = nullptr;
        if (bEnablePvd)
        {
            sPvd = PxCreatePvd(*gFoundation);
            sPvdTransport = PxDefaultPvdSocketTransportCreate("127.0.0.1", 5425, 10);
            if (sPvd && sPvdTransport)
            {
                sPvd->connect(*sPvdTransport, PxPvdInstrumentationFlag::eALL);
                pvdForCreate = sPvd;
            }
        }

        gPhysics = PxCreatePhysics(PX_PHYSICS_VERSION, *gFoundation, PxTolerancesScale(), true, pvdForCreate);
        if (!gPhysics)
        {
            UE_LOG("[PhysX] ERROR: Failed to create PxPhysics");
            return false;
        }

        // Cooking interface
        PxCookingParams cookParams = PxCookingParams(PxTolerancesScale());
        gCooking = PxCreateCooking(PX_PHYSICS_VERSION, *gFoundation, cookParams);
        if (!gCooking)
        {
            UE_LOG("[PhysX] ERROR: Failed to create PxCooking");
            return false;
        }

        // Scene + default material
        CreateDefaultScene();
        if (!gScene)
        {
            UE_LOG("[PhysX] ERROR: Failed to create PxScene");
            return false;
        }
        CreateDefaultMaterial();
        if (!gMaterial)
        {
            UE_LOG("[PhysX] ERROR: Failed to create default PxMaterial");
            return false;
        }

        UE_LOG("[PhysX] Initialized (PVD=%s)", bEnablePvd ? "ON" : "OFF");
        return true;
    }

    void ShutdownPhysX()
    {
        // Release scene and dispatcher
        if (gScene) { gScene->release(); gScene = nullptr; }
        if (gDispatcher) { gDispatcher->release(); gDispatcher = nullptr; }

        // Default material
        if (gMaterial) { gMaterial->release(); gMaterial = nullptr; }

        // Cooking interface
        if (gCooking) { gCooking->release(); gCooking = nullptr; }

        // Physics SDK
        if (gPhysics) { gPhysics->release(); gPhysics = nullptr; }

        // PVD (local)
        if (sPvd)
        {
            if (sPvd->isConnected()) sPvd->disconnect();
            sPvd->release();
            sPvd = nullptr;
        }
        if (sPvdTransport)
        {
            sPvdTransport->release();
            sPvdTransport = nullptr;
        }

        // Foundation
        if (gFoundation) { gFoundation->release(); gFoundation = nullptr; }

        UE_LOG("[PhysX] Shutdown complete");
    }
}

