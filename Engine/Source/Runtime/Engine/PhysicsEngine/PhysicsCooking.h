#pragma once

#include "Vector.h"
#include "UEContainer.h"

namespace physx { class PxTriangleMesh; class PxConvexMesh; }

class PhysicsCooking
{
public:
    static bool CookTriangleMesh(const TArray<FVector>& Vertices,
                                 const TArray<uint32>& Indices,
                                 physx::PxTriangleMesh*& OutTriMesh);

    static bool CookConvex(const TArray<FVector>& Vertices,
                           physx::PxConvexMesh*& OutConvex,
                           bool bInflate = false);
};
