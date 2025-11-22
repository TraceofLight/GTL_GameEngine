#pragma once
#include "Vector.h"
#include "Color.h"
/**
 * Base particle structure containing fundamental particle data
 * This represents a single particle in the system
 */
struct FBaseParticle
{
	/** Current location of the particle in world space */
	FVector Location;

	/** Previous frame location for interpolation */
	FVector OldLocation;

	/** Current velocity of the particle */
	FVector Velocity;

	/** Relative time (0.0 to 1.0) within the particle's lifetime */
	float RelativeTime;

	/** Total lifetime of the particle in seconds */
	float Lifetime;

	/** Base velocity set at spawn time */
	FVector BaseVelocity;

	/** Current rotation of the particle in radians */
	float Rotation;

	/** Rate of rotation change per second */
	float RotationRate;

	/** Current size of the particle */
	FVector Size;

	/** Current color of the particle */
	FLinearColor Color;

	// TODO: Add additional particle properties as needed

	FBaseParticle()
		: Location(FVector::Zero())
		, OldLocation(FVector::Zero())
		, Velocity(FVector::Zero())
		, RelativeTime(0.0f)
		, Lifetime(0.0f)
		, BaseVelocity(FVector::Zero())
		, Rotation(0.0f)
		, RotationRate(0.0f)
		, Size(FVector::One())
		, Color(FLinearColor(1.0f, 1.0f, 1.0f, 1.0f))
	{
	}
};
