#include "pch.h"
#include "BodySetup.h"
#include "PhysicsCooking.h"
#include "PxPhysicsAPI.h"
#include "Source/Runtime/Core/Misc/JsonSerializer.h"

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
        UE_LOG("BodySetup: EnsureCooked: PhysX not initialized");
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
            UE_LOG("BodySetup: Cooked convex: %u verts", CookSourceVertices.Num());
        }
    }
    else if (CollisionTraceFlag == ECollisionTraceFlag::UseSimpleAndComplex)
    {
        // Cook a triangle mesh (exact collision, but static actors only!)
        bSuccess = PhysicsCooking::CookTriangleMesh(CookSourceVertices, CookSourceIndices, TriMesh);
        if (bSuccess)
        {
            UE_LOG("BodySetup: Cooked trimesh: %u verts, %u tris",
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

void UBodySetup::Serialize(bool bIsLoading, JSON& Json)
{
    if (bIsLoading)
    {
        // BoneName
        FString BoneNameStr;
        FJsonSerializer::ReadString(Json, "BoneName", BoneNameStr, "");
        BoneName = FName(BoneNameStr);

        // CollisionTraceFlag
        int32 TraceFlagInt;
        FJsonSerializer::ReadInt32(Json, "CollisionTraceFlag", TraceFlagInt, 0);
        CollisionTraceFlag = static_cast<ECollisionTraceFlag>(TraceFlagInt);

        // AggGeom (shape data)
        if (Json.hasKey("AggGeom") && Json["AggGeom"].JSONType() == JSON::Class::Object)
        {
            JSON AggGeomJson = Json["AggGeom"];
            AggGeom.Serialize(true, AggGeomJson);
        }

        // Note: CookSourceVertices/Indices are not serialized - they're runtime-generated
        // Note: TriMesh/ConvexMesh are not serialized - they're cooked at runtime
    }
    else
    {
        // BoneName
        Json["BoneName"] = BoneName.ToString();

        // CollisionTraceFlag
        Json["CollisionTraceFlag"] = static_cast<int32>(CollisionTraceFlag);

        // AggGeom (shape data)
        JSON AggGeomJson;
        AggGeom.Serialize(false, AggGeomJson);
        Json["AggGeom"] = AggGeomJson;
    }
}
