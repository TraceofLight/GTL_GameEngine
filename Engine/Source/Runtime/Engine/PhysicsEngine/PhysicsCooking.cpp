#include "pch.h"
#include "PhysicsCooking.h"

#include "PhysX/PxPhysicsAPI.h"

using namespace physx;

static inline PxVec3 ToPx(const FVector& v)
{
    return PxVec3(v.X, v.Y, v.Z);
}

static void FillBoundedDataPoints(const TArray<FVector>& Vertices, PxBoundedData& Out)
{
    Out.count = static_cast<uint32>(Vertices.Num());
    Out.stride = sizeof(FVector);
    Out.data = reinterpret_cast<const PxU8*>(Vertices.GetData());
}

static void FillBoundedDataTriangles(const TArray<uint32>& Indices, PxBoundedData& Out)
{
    Out.count = static_cast<uint32>(Indices.Num() / 3);
    Out.stride = sizeof(uint32) * 3;
    Out.data = reinterpret_cast<const PxU8*>(Indices.GetData());
}

bool PhysicsCooking::CookTriangleMesh(const TArray<FVector>& Vertices,
                                      const TArray<uint32>& Indices,
                                      PxTriangleMesh*& OutTriMesh)
{
    OutTriMesh = nullptr;

    PxCooking* Cooking = PHYSICS.GetCooking();
    PxPhysics* Physics = PHYSICS.GetPhysics();
    if (!Cooking || !Physics) return false;
    if (Vertices.IsEmpty() || Indices.IsEmpty() || (Indices.Num() % 3) != 0) return false;

    PxTriangleMeshDesc desc;
    FillBoundedDataPoints(Vertices, desc.points);
    FillBoundedDataTriangles(Indices, desc.triangles);

    PxDefaultMemoryOutputStream cooked;
    if (!Cooking->cookTriangleMesh(desc, cooked))
    {
        UE_LOG("PhysX: CookTriMesh: Failed");
        return false;
    }

    PxDefaultMemoryInputData input(cooked.getData(), cooked.getSize());
    PxTriangleMesh* mesh = Physics->createTriangleMesh(input);
    if (!mesh)
    {
        UE_LOG("PhysX: CreateTriMesh: Failed");
        return false;
    }

    OutTriMesh = mesh;
    return true;
}

bool PhysicsCooking::CookConvex(const TArray<FVector>& Vertices,
                                PxConvexMesh*& OutConvex,
                                bool bInflate)
{
    OutConvex = nullptr;

    PxCooking* Cooking = PHYSICS.GetCooking();
    PxPhysics* Physics = PHYSICS.GetPhysics();
    if (!Cooking || !Physics) return false;
    if (Vertices.IsEmpty()) return false;

    PxConvexMeshDesc desc;
    FillBoundedDataPoints(Vertices, desc.points);
    desc.vertexLimit = 255; // PhysX typical limit
    desc.flags = PxConvexFlag::eCOMPUTE_CONVEX;
    if (bInflate)
    {
        // Use stability/robustness-oriented flags available in this PhysX version
        desc.flags |= PxConvexFlag::eCHECK_ZERO_AREA_TRIANGLES;
        desc.flags |= PxConvexFlag::eSHIFT_VERTICES;
    }

    PxDefaultMemoryOutputStream cooked;
    if (!Cooking->cookConvexMesh(desc, cooked))
    {
        UE_LOG("PhysX: CookConvex: Failed");
        return false;
    }

    PxDefaultMemoryInputData input(cooked.getData(), cooked.getSize());
    PxConvexMesh* mesh = Physics->createConvexMesh(input);
    if (!mesh)
    {
        UE_LOG("PhysX: CreateConvex: Failed");
        return false;
    }

    OutConvex = mesh;
    return true;
}
