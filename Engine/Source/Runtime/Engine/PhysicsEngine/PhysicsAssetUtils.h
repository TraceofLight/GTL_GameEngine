#pragma once

class UPhysicsAsset;
class USkeletalMesh;
class UBodySetup;
struct FName;

/**
 * Parameters for physics asset creation.
 * Similar to UE's FPhysAssetCreateParams.
 */
struct FPhysAssetCreateParams
{
    /** Minimum bone size to create a body for. Bones smaller than this will be merged into parent. */
    float MinBoneSize = 0.1f;

    /** Minimum bone size to even consider for welding. Bones smaller than this are ignored entirely. */
    float MinWeldSize = 0.01f;

    /** If true, create a body for every bone regardless of size. */
    bool bBodyForAll = false;
};

/**
 * Static utility functions for building and manipulating PhysicsAssets.
 * Follows UE pattern where UPhysicsAsset is a pure data container
 * and utilities handle the creation/modification logic.
 */
namespace FPhysicsAssetUtils
{
    /**
     * Creates physics bodies from a skeletal mesh's bone hierarchy.
     * Generates capsules for each non-leaf bone based on bone length and heuristic radius.
     * Small bones are merged into their parents based on MinBoneSize parameter.
     * This is the public entry point that handles validation and setup.
     *
     * @param PhysicsAsset  The physics asset to populate (will be cleared first)
     * @param SkeletalMesh  The skeletal mesh to generate bodies from
     * @param Params        Creation parameters (MinBoneSize, MinWeldSize, etc.)
     * @return true if any bodies were created
     */
    bool CreateFromSkeletalMesh(UPhysicsAsset* PhysicsAsset, const USkeletalMesh* SkeletalMesh, const FPhysAssetCreateParams& Params = FPhysAssetCreateParams());

    /**
     * Internal implementation of physics asset creation.
     * Called by CreateFromSkeletalMesh after validation.
     *
     * @param PhysicsAsset  The physics asset to populate
     * @param SkeletalMesh  The skeletal mesh to generate bodies from
     * @param Params        Creation parameters (MinBoneSize, MinWeldSize, etc.)
     * @return true if any bodies were created
     */
    bool CreateFromSkeletalMeshInternal(UPhysicsAsset* PhysicsAsset, const USkeletalMesh* SkeletalMesh, const FPhysAssetCreateParams& Params);

    /**
     * Creates a new BodySetup for a specific bone and adds it to the PhysicsAsset.
     *
     * @param PhysicsAsset  The physics asset to add the body to
     * @param BoneName      Name of the bone this body is for
     * @return The newly created BodySetup, or nullptr on failure
     */
    UBodySetup* CreateBodySetupForBone(UPhysicsAsset* PhysicsAsset, FName BoneName);
}
