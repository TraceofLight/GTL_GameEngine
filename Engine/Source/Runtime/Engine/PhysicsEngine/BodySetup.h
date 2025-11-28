#pragma once

#include "AggregateGeom.h"
#include "Name.h"
#include "Vector.h"
#include "Source/Runtime/Engine/Collision/AABB.h"

// Mirror UE-style collision trace options (simplified)
enum class ECollisionTraceFlag : uint8
{
    UseDefault = 0,
    UseSimpleAsComplex,
    UseComplexAsSimple,
    UseSimpleAndComplex,
};

class UBodySetup : public UObject
{
public:
	DECLARE_CLASS(UBodySetup, UObject)

    // Optional bone this body setup is associated with (skeletal)
    FName BoneName;

    // Authoring primitives (simple collision)
    FKAggregateGeom AggGeom;

    // Collision trace policy for complex collisions (cooked convex/tri meshes)
    ECollisionTraceFlag CollisionTraceFlag = ECollisionTraceFlag::UseDefault;

    // Aggregate helpers
    int32 GetElementCount() const { return AggGeom.GetElementCount(); }
    int32 GetElementCount(EAggCollisionShape::Type InType) const { return AggGeom.GetElementCount(InType); }

    FKShapeElem* GetElement(EAggCollisionShape::Type InType, int32 Index) { return AggGeom.GetElement(InType, Index); }
    const FKShapeElem* GetElement(EAggCollisionShape::Type InType, int32 Index) const { return AggGeom.GetElement(InType, Index); }

    FKShapeElem* GetElementByName(const FName& InName) { return AggGeom.GetElementByName(InName); }
    const FKShapeElem* GetElementByName(const FName& InName) const { return AggGeom.GetElementByName(InName); }

    // Combined world AABB for all primitives in this setup (BoneTM usually the owning component/bone transform)
    FAABB CalcAABB(const FTransform& BoneTM, float Scale = 1.0f) const { return AggGeom.CalcAABB(BoneTM, Scale); }

    // UE-analogous helpers (stubs for future PhysX cooking integration)
    void ClearPhysicsMeshes();
    bool HasCookedData() const;
};

