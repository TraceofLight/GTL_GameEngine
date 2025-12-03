#pragma once

#include "AggregateGeom.h"
#include "Name.h"
#include "Vector.h"
#include "Source/Runtime/Engine/Collision/AABB.h"

namespace physx { class PxTriangleMesh; class PxConvexMesh; }

// Mirror UE-style collision trace options (simplified)
enum class ECollisionTraceFlag : uint8
{
    UseDefault = 0,          // Use simple collision only (default box/primitives)
    UseSimpleAsComplex,      // Use simple collision for complex queries too
    UseComplexAsSimple,      // Use cooked convex mesh for all collision
    UseSimpleAndComplex,     // Use simple for simple queries, complex for complex
};

class UBodySetup : public UObject
{
public:
	DECLARE_CLASS(UBodySetup, UObject)

    UBodySetup() = default;
    ~UBodySetup();

    // Optional bone this body setup is associated with (skeletal)
    FName BoneName;

    // Authoring primitives (simple collision)
    FKAggregateGeom AggGeom;

    // Collision trace policy for complex collisions (cooked convex/tri meshes)
    ECollisionTraceFlag CollisionTraceFlag = ECollisionTraceFlag::UseDefault;

    // Cooked mesh data (cached after first cook)
    PxTriangleMesh* TriMesh = nullptr;
    PxConvexMesh* ConvexMesh = nullptr;

    // Source mesh data for cooking (set before calling EnsureCooked)
    TArray<FVector> CookSourceVertices;
    TArray<uint32> CookSourceIndices;

    // Aggregate helpers
    int32 GetElementCount() const { return AggGeom.GetElementCount(); }
    int32 GetElementCount(EAggCollisionShape::Type InType) const { return AggGeom.GetElementCount(InType); }

    FKShapeElem* GetElement(EAggCollisionShape::Type InType, int32 Index) { return AggGeom.GetElement(InType, Index); }
    const FKShapeElem* GetElement(EAggCollisionShape::Type InType, int32 Index) const { return AggGeom.GetElement(InType, Index); }

    FKShapeElem* GetElementByName(const FName& InName) { return AggGeom.GetElementByName(InName); }
    const FKShapeElem* GetElementByName(const FName& InName) const { return AggGeom.GetElementByName(InName); }

    // Combined world AABB for all primitives in this setup (BoneTM usually the owning component/bone transform)
    FAABB CalcAABB(const FTransform& BoneTM, float Scale = 1.0f) const { return AggGeom.CalcAABB(BoneTM, Scale); }

    // Cooking interface
    bool EnsureCooked();      // Cook mesh data if needed, returns true if cooked data available
    void ClearPhysicsMeshes(); // Release cooked PhysX meshes
    bool HasCookedData() const;

    // Serialization
    virtual void Serialize(bool bIsLoading, JSON& Json) override;
};

