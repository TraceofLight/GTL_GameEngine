#include "pch.h"
#include "BodySetup.h"
#include "PhysicsCooking.h"
#include "PxPhysicsAPI.h"

using namespace physx;

IMPLEMENT_CLASS(UBodySetup)

UBodySetup::~UBodySetup()
{
    ClearPhysicsMeshes();
}

bool UBodySetup::EnsureCooked()
{
    // Already cooked?
    if (TriMesh || ConvexMesh)
        return true;

    // PhysX not ready yet?
    if (!PHYSICS.GetPhysics() || !PHYSICS.GetCooking())
    {
        UE_LOG("[BodySetup] EnsureCooked: PhysX not initialized yet, deferring");
        return false;
    }

    // No source data to cook?
    if (CookSourceVertices.IsEmpty() || CookSourceIndices.IsEmpty())
        return false;

    // Decide cooking strategy based on CollisionTraceFlag
    // UseComplexAsSimple -> cook convex mesh (dynamic-friendly, approximate)
    // UseSimpleAndComplex -> cook triangle mesh (static only, exact)
    // UseDefault/UseSimpleAsComplex -> don't cook complex, use simple primitives only

    bool bSuccess = false;

    if (CollisionTraceFlag == ECollisionTraceFlag::UseComplexAsSimple)
    {
        // Cook a convex mesh (works for both static and dynamic actors)
        bSuccess = PhysicsCooking::CookConvex(CookSourceVertices, ConvexMesh, true);
        if (bSuccess)
        {
            UE_LOG("[BodySetup] Cooked convex mesh: %u verts -> convex", CookSourceVertices.Num());
        }
    }
    else if (CollisionTraceFlag == ECollisionTraceFlag::UseSimpleAndComplex)
    {
        // Cook a triangle mesh (exact collision, but static actors only!)
        bSuccess = PhysicsCooking::CookTriangleMesh(CookSourceVertices, CookSourceIndices, TriMesh);
        if (bSuccess)
        {
            UE_LOG("[BodySetup] Cooked triangle mesh: %u verts, %u tris",
                   CookSourceVertices.Num(), CookSourceIndices.Num() / 3);
        }
    }

    return bSuccess;
}

void UBodySetup::ClearPhysicsMeshes()
{
    // Safety check: only release if PhysX is still valid
    // (meshes may outlive the physics system during shutdown)
    if (TriMesh)
    {
        if (PHYSICS.GetPhysics())
        {
            TriMesh->release();
        }
        TriMesh = nullptr;
    }
    if (ConvexMesh)
    {
        if (PHYSICS.GetPhysics())
        {
            ConvexMesh->release();
        }
        ConvexMesh = nullptr;
    }
}

bool UBodySetup::HasCookedData() const
{
    return (TriMesh != nullptr) || (ConvexMesh != nullptr);
}
