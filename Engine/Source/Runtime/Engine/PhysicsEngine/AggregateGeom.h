#pragma once
#include "SphereElem.h"
#include "BoxElem.h"

class FKAggregateGeom
{
public:
	TArray<FKSphereElem> SphereElems;
	TArray<FKBoxElem> BoxElems;

};
