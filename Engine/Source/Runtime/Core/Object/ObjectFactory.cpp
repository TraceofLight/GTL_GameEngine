#include "pch.h"
#include "ObjectFactory.h"

TArray<UObject*> GUObjectArray;

namespace
{
    static TMap<UClass*, int>& GetNameCounters()
    {
        static TMap<UClass*, int> NameCounters;
        return NameCounters;
    }
}

namespace ObjectFactory
{
    TMap<UClass*, ConstructFunc>& GetRegistry()
    {
        static TMap<UClass*, ConstructFunc> Registry;
        return Registry;
    }

    void RegisterClassType(UClass* Class, ConstructFunc Func)
    {
        GetRegistry()[Class] = std::move(Func);
    }

    UObject* ConstructObject(UClass* Class)
    {
        auto& reg = GetRegistry();
        auto it = reg.find(Class);
        if (it == reg.end())
        {
            return nullptr;
        }
        return it->second();
    }

    UObject* NewObject(UClass* Class)
    {
        UObject* Obj = ConstructObject(Class);
        if (!Obj)
        {
            return nullptr;
        }

        int32 idx = GUObjectArray.Add(Obj);
        Obj->InternalIndex = static_cast<uint32>(idx);

        int Count = ++GetNameCounters()[Class];

        const std::string base = Class->Name;
        std::string unique;
        unique.reserve(base.size() + 1 + 12);
        unique.append(base);
        unique.push_back('_');
        unique.append(std::to_string(Count));

        Obj->ObjectName = FName(unique);

        return Obj;
    }

    UObject* AddToGUObjectArray(UClass* Class, UObject* Obj)
    {
        if (!Obj)
        {
            return nullptr;
        }

        int32 idx = GUObjectArray.Add(Obj);
        Obj->InternalIndex = static_cast<uint32>(idx);

        int Count = ++GetNameCounters()[Class];

        const std::string base = Class->Name;
        std::string unique;
        unique.reserve(base.size() + 1 + 12);
        unique.append(base);
        unique.push_back('_');
        unique.append(std::to_string(Count));

        Obj->ObjectName = FName(unique);

        return Obj;
    }

    void DeleteObject(UObject* Obj)
    {
        if (!Obj)
        {
            return;
        }

        // Important: DO NOT dereference Obj fields before verifying it is still in GUObjectArray.
        int32 foundIndex = -1;
        for (int32 i = 0; i < GUObjectArray.Num(); ++i)
        {
            if (GUObjectArray[i] == Obj)
            {
                foundIndex = i;
                break;
            }
        }
        if (foundIndex < 0)
        {
            // Not managed or already deleted.
            return;
        }

        GUObjectArray[foundIndex] = nullptr;
        // Safe to delete now; Obj still valid since we found it in GUObjectArray
        Obj->DestroyInternal();
    }

    void DeleteAll(bool bCallBeginDestroy)
    {
        // 역순으로 삭제 (DeleteObject가 배열에서 nullptr 처리)
        for (int32 i = GUObjectArray.Num() - 1; i >= 0; --i)
        {
            if (UObject* Obj = GUObjectArray[i])
            {
                DeleteObject(Obj);
            }
        }
        GUObjectArray.Empty();
        GUObjectArray.Shrink();
    }

    void CompactNullSlots()
    {
        int32 write = 0;
        for (int32 read = 0; read < GUObjectArray.Num(); ++read)
        {
            if (UObject* Obj = GUObjectArray[read])
            {
                if (write != read)
                {
                    GUObjectArray[write] = Obj;
                    Obj->InternalIndex = static_cast<uint32>(write);
                    GUObjectArray[read] = nullptr;
                }
                ++write;
            }
        }
        GUObjectArray.SetNum(write);
    }

    bool IsValidObject(const UObject* Obj)
    {
        if (!Obj)
        {
            return false;
        }
        // GUObjectArray에서 포인터 찾기
        for (int32 i = 0; i < GUObjectArray.Num(); ++i)
        {
            if (GUObjectArray[i] == Obj)
            {
                return true;
            }
        }
        return false;
    }
}
