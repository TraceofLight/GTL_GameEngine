#pragma once

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
     * Creates a new BodySetup for a specific bone and adds it to the PhysicsAsset.
     *
     * @param PhysicsAsset  The physics asset to add the body to
     * @param BoneName      Name of the bone this body is for
     * @return The newly created BodySetup, or nullptr on failure
     */
    UBodySetup* CreateBodySetupForBone(UPhysicsAsset* PhysicsAsset, FName BoneName);

	/**
	 * @brief 본 계층구조 기반 래그돌 Constraint 생성
	 * @details Body가 있는 각 본에 대해 가장 가까운 조상 Body와 연결하는 D6 Joint를 생성.
	 *          Linear는 Locked, Angular는 Limited로 설정하여 기본 래그돌 동작 구현.
	 *
	 * @param PhysicsAsset Constraint를 추가할 PhysicsAsset
	 * @param SkeletalMesh 본 계층구조를 가져올 SkeletalMesh
	 * @return Constraint가 하나 이상 생성되면 true
	 */
	bool CreateConstraintsForRagdoll(UPhysicsAsset* PhysicsAsset, const USkeletalMesh* SkeletalMesh);
}
