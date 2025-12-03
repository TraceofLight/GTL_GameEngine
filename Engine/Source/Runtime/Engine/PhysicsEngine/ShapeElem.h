#pragma once

// Forward declaration for JSON type (defined in pch.h via nlohmann/json.hpp)
namespace json { class JSON; }
using JSON = json::JSON;

namespace EAggCollisionShape
{
	enum Type : int
	{
		Sphere,
		Box,
		Capsule,
		Convex,
		TaperedCapsule,
		LevelSet,
		SkinnedLevelSet,
		MLLevelSet,
		SkinnedTriangleMesh,

		Unknown
	};
}

// Used to create the actual physics PxShapes during runtime
struct FKShapeElem
{
public:
	FKShapeElem(EAggCollisionShape::Type InShapeType)
	: ShapeType(InShapeType)
	, bContributeToMass(true)
	, bCollisionEnabled(true)
	{}
	/** Get the user-defined name for this shape */
	const FName& GetName() const { return Name; }

	/** Set the user-defined name for this shape */
	void SetName(const FName& InName) { Name = InName; }

	/** Get whether this shape contributes to the mass of the body */
	bool GetContributeToMass() const { return bContributeToMass; }

	/** Set whether this shape will contribute to the mass of the body */
	void SetContributeToMass(bool bInContributeToMass) { bContributeToMass = bInContributeToMass; }

	/** Set whether this shape should be considered for query or sim collision */
	void SetCollisionEnabled(bool CollisionEnabled) { bCollisionEnabled = CollisionEnabled; }

	/** Get whether this shape should be considered for query or sim collision */
	bool GetCollisionEnabled() const { return bCollisionEnabled; }

	/** Get the shape type */
	EAggCollisionShape::Type GetShapeType() const { return ShapeType; }

	virtual FTransform GetTransform() const
	{
		return FTransform();
	}

	/** Serialize this shape element to/from JSON */
	virtual void Serialize(bool bIsLoading, JSON& Json);

protected:
	FName Name;
	EAggCollisionShape::Type ShapeType;
	bool bContributeToMass = true;
	bool bCollisionEnabled = true;
};
