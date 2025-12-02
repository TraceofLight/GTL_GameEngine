#include "pch.h"
#include "PhysicsConstraintSetup.h"
#include "Source/Runtime/Core/Misc/JsonSerializer.h"

IMPLEMENT_CLASS(UPhysicsConstraintSetup)

void UPhysicsConstraintSetup::Serialize(bool bIsLoading, JSON& Json)
{
	if (bIsLoading)
	{
		// Bone Names
		FString Bone1Str, Bone2Str;
		FJsonSerializer::ReadString(Json, "ConstraintBone1", Bone1Str, "");
		FJsonSerializer::ReadString(Json, "ConstraintBone2", Bone2Str, "");
		ConstraintBone1 = FName(Bone1Str);
		ConstraintBone2 = FName(Bone2Str);

		// Body Indices (will be recalculated from bone names on load)
		FJsonSerializer::ReadInt32(Json, "BodyIndex1", BodyIndex1, -1);
		FJsonSerializer::ReadInt32(Json, "BodyIndex2", BodyIndex2, -1);

		// Constraint Frame
		FJsonSerializer::ReadVector(Json, "ConstraintPositionInBody1", ConstraintPositionInBody1);
		FJsonSerializer::ReadVector(Json, "ConstraintPositionInBody2", ConstraintPositionInBody2);
		FJsonSerializer::ReadVector(Json, "ConstraintRotationInBody1", ConstraintRotationInBody1);
		FJsonSerializer::ReadVector(Json, "ConstraintRotationInBody2", ConstraintRotationInBody2);

		// Linear Limits
		int32 LinearX = 2, LinearY = 2, LinearZ = 2;
		FJsonSerializer::ReadInt32(Json, "LinearXMotion", LinearX, 2);
		FJsonSerializer::ReadInt32(Json, "LinearYMotion", LinearY, 2);
		FJsonSerializer::ReadInt32(Json, "LinearZMotion", LinearZ, 2);
		LinearXMotion = static_cast<ELinearConstraintMotion>(LinearX);
		LinearYMotion = static_cast<ELinearConstraintMotion>(LinearY);
		LinearZMotion = static_cast<ELinearConstraintMotion>(LinearZ);
		FJsonSerializer::ReadFloat(Json, "LinearLimit", LinearLimit, 0.0f);

		// Angular Limits
		int32 Swing1 = 1, Swing2 = 1, Twist = 1;
		FJsonSerializer::ReadInt32(Json, "Swing1Motion", Swing1, 1);
		FJsonSerializer::ReadInt32(Json, "Swing2Motion", Swing2, 1);
		FJsonSerializer::ReadInt32(Json, "TwistMotion", Twist, 1);
		Swing1Motion = static_cast<EAngularConstraintMotion>(Swing1);
		Swing2Motion = static_cast<EAngularConstraintMotion>(Swing2);
		TwistMotion = static_cast<EAngularConstraintMotion>(Twist);

		FJsonSerializer::ReadFloat(Json, "Swing1LimitAngle", Swing1LimitAngle, 45.0f);
		FJsonSerializer::ReadFloat(Json, "Swing2LimitAngle", Swing2LimitAngle, 45.0f);
		FJsonSerializer::ReadFloat(Json, "TwistLimitAngle", TwistLimitAngle, 45.0f);

		// Soft Limits
		FJsonSerializer::ReadBool(Json, "bSoftSwingLimit", bSoftSwingLimit, false);
		FJsonSerializer::ReadBool(Json, "bSoftTwistLimit", bSoftTwistLimit, false);
		FJsonSerializer::ReadFloat(Json, "SwingStiffness", SwingStiffness, 0.0f);
		FJsonSerializer::ReadFloat(Json, "SwingDamping", SwingDamping, 0.0f);
		FJsonSerializer::ReadFloat(Json, "TwistStiffness", TwistStiffness, 0.0f);
		FJsonSerializer::ReadFloat(Json, "TwistDamping", TwistDamping, 0.0f);

		// Break Settings
		FJsonSerializer::ReadBool(Json, "bLinearBreakable", bLinearBreakable, false);
		FJsonSerializer::ReadBool(Json, "bAngularBreakable", bAngularBreakable, false);
		FJsonSerializer::ReadFloat(Json, "LinearBreakThreshold", LinearBreakThreshold, 0.0f);
		FJsonSerializer::ReadFloat(Json, "AngularBreakThreshold", AngularBreakThreshold, 0.0f);
	}
	else
	{
		// Bone Names
		Json["ConstraintBone1"] = ConstraintBone1.ToString();
		Json["ConstraintBone2"] = ConstraintBone2.ToString();

		// Body Indices
		Json["BodyIndex1"] = BodyIndex1;
		Json["BodyIndex2"] = BodyIndex2;

		// Constraint Frame
		Json["ConstraintPositionInBody1"] = FJsonSerializer::VectorToJson(ConstraintPositionInBody1);
		Json["ConstraintPositionInBody2"] = FJsonSerializer::VectorToJson(ConstraintPositionInBody2);
		Json["ConstraintRotationInBody1"] = FJsonSerializer::VectorToJson(ConstraintRotationInBody1);
		Json["ConstraintRotationInBody2"] = FJsonSerializer::VectorToJson(ConstraintRotationInBody2);

		// Linear Limits
		Json["LinearXMotion"] = static_cast<int32>(LinearXMotion);
		Json["LinearYMotion"] = static_cast<int32>(LinearYMotion);
		Json["LinearZMotion"] = static_cast<int32>(LinearZMotion);
		Json["LinearLimit"] = LinearLimit;

		// Angular Limits
		Json["Swing1Motion"] = static_cast<int32>(Swing1Motion);
		Json["Swing2Motion"] = static_cast<int32>(Swing2Motion);
		Json["TwistMotion"] = static_cast<int32>(TwistMotion);
		Json["Swing1LimitAngle"] = Swing1LimitAngle;
		Json["Swing2LimitAngle"] = Swing2LimitAngle;
		Json["TwistLimitAngle"] = TwistLimitAngle;

		// Soft Limits
		Json["bSoftSwingLimit"] = bSoftSwingLimit;
		Json["bSoftTwistLimit"] = bSoftTwistLimit;
		Json["SwingStiffness"] = SwingStiffness;
		Json["SwingDamping"] = SwingDamping;
		Json["TwistStiffness"] = TwistStiffness;
		Json["TwistDamping"] = TwistDamping;

		// Break Settings
		Json["bLinearBreakable"] = bLinearBreakable;
		Json["bAngularBreakable"] = bAngularBreakable;
		Json["LinearBreakThreshold"] = LinearBreakThreshold;
		Json["AngularBreakThreshold"] = AngularBreakThreshold;
	}
}
