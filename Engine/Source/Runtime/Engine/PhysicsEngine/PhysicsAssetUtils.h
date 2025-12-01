#pragma once

struct FBoneVertInfo;
class UPhysicsAsset;
class USkeletalMesh;
class UBodySetup;
struct FName;

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
     *
     * @param PhysicsAsset  The physics asset to populate (will be cleared first)
     * @param SkeletalMesh  The skeletal mesh to generate bodies from
     * @return true if any bodies were created
     */
    bool CreateFromSkeletalMesh(UPhysicsAsset* PhysicsAsset, const USkeletalMesh* SkeletalMesh);

    /**
     * Creates collision geometry (capsule) for a single bone using vertex data.
     * Uses covariance matrix and eigenvector to determine optimal capsule orientation.
     *
     * @param BodySetup     The body setup to add collision to
     * @param Info          Vertex positions/normals associated with this bone
     * @param BoneName      Name of the bone (for logging)
     * @return true if collision was created successfully
     */
    bool CreateCollisionFromBoneInternal(UBodySetup* BodySetup, const FBoneVertInfo& Info, const std::string& BoneName);

    /**
     * Creates a new BodySetup for a specific bone and adds it to the PhysicsAsset.
     *
     * @param PhysicsAsset  The physics asset to add the body to
     * @param BoneName      Name of the bone this body is for
     * @return The newly created BodySetup, or nullptr on failure
     */
    UBodySetup* CreateBodySetupForBone(UPhysicsAsset* PhysicsAsset, FName BoneName);
}
