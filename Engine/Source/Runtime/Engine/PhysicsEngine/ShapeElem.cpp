#include "pch.h"
#include "ShapeElem.h"
#include "Source/Runtime/Core/Misc/JsonSerializer.h"

void FKShapeElem::Serialize(bool bIsLoading, JSON& Json)
{
	if (bIsLoading)
	{
		// Name
		FString NameStr;
		FJsonSerializer::ReadString(Json, "Name", NameStr, "");
		Name = FName(NameStr);

		// ShapeType (read but don't overwrite - set by constructor)
		int32 TypeInt;
		FJsonSerializer::ReadInt32(Json, "ShapeType", TypeInt, static_cast<int32>(ShapeType));
		// ShapeType is set by derived class constructor, so we just validate

		// Mass & Collision
		FJsonSerializer::ReadBool(Json, "bContributeToMass", bContributeToMass, true);
		FJsonSerializer::ReadBool(Json, "bCollisionEnabled", bCollisionEnabled, true);
	}
	else
	{
		// Name
		Json["Name"] = Name.ToString();

		// ShapeType
		Json["ShapeType"] = static_cast<int32>(ShapeType);

		// Mass & Collision
		Json["bContributeToMass"] = bContributeToMass;
		Json["bCollisionEnabled"] = bCollisionEnabled;
	}
}
