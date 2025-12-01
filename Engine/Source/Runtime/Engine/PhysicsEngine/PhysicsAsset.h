#pragma once
#include "BodySetup.h"
#include "PhysicsConstraintSetup.h"
#include "Name.h"
#include "Vector.h"
#include "Source/Runtime/Engine/Collision/AABB.h"

class USkeletalMesh;

class UPhysicsAsset : public UObject
{
public:
    DECLARE_CLASS(UPhysicsAsset, UObject)

	UPROPERTY(EditAnywhere)
	TArray<UBodySetup*> BodySetups;

	UPROPERTY(EditAnywhere)
	TArray<UPhysicsConstraintSetup*> ConstraintSetups;

	UPROPERTY(EditAnywhere)
	TMap<FName, int32> BoneNameToBodyIndex;

	UPROPERTY(EditAnywhere)
	FString SourceSkeletalPath;

    int32 FindBodyIndexByBoneName(FName BoneName) const;
    UBodySetup* FindBodySetupByBoneName(FName BoneName);
    const UBodySetup* FindBodySetupByBoneName(FName BoneName) const;

    void RebuildNameToIndexMap();
    int32 AddBodySetup(UBodySetup* NewSetup);
    bool RemoveBodyByBoneName(FName BoneName);

    FAABB CalcAABB(const FTransform& ComponentTM, float Scale = 1.0f) const;
    FAABB CalcAABB(const TMap<FName, FTransform>& BoneWorldTMs, float Scale = 1.0f) const;

    // Serialization
    bool SaveToFile(const FString& FilePath) const;
    bool LoadFromFile(const FString& FilePath);
};
