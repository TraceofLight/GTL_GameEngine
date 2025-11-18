#pragma once
#include "LuaManager.h"
#include "LuaComponentProxy.h"

// 클래스별 바인더 등록 매크로(컴포넌트 cpp에서 사용)
#define LUA_BIND_BEGIN(ClassType) \
static void BuildLua_##ClassType(sol::state_view L, sol::table& T); \
struct FLuaBinder_##ClassType { \
FLuaBinder_##ClassType() { \
FLuaBindRegistry::Get().Register(ClassType::StaticClass(), &BuildLua_##ClassType); \
} \
}; \
static FLuaBinder_##ClassType G_LuaBinder_##ClassType; \
static void BuildLua_##ClassType(sol::state_view L, sol::table& T) \
/**/

#define LUA_BIND_END() /* nothing */

// 멤버 함수 포인터를 Lua 함수로 감싸는 헬퍼(리턴 void 버전)
template<typename C, typename... P>
static void AddMethod(sol::table& T, const char* Name, void(C::*Method)(P...))
{
    T.set_function(Name, [Method](LuaComponentProxy& Proxy, P... Args)
    {
        if (!Proxy.Instance || Proxy.Class != C::StaticClass()) return;
        (static_cast<C*>(Proxy.Instance)->*Method)(std::forward<P>(Args)...);
    });
}

// 리턴값이 있는 멤버 함수용 버전
template<typename R, typename C, typename... P>
static void AddMethodR(sol::table& T, const char* Name, R(C::*Method)(P...))
{
    // Check if R is a pointer type
    if constexpr (std::is_pointer_v<R>)
    {
        // For pointer return types, wrap in LuaComponentProxy
        using PointeeType = std::remove_pointer_t<R>;
        T.set_function(Name, [Method](sol::this_state s, LuaComponentProxy& Proxy, P... Args) -> sol::object
        {
            if (!Proxy.Instance || Proxy.Class != C::StaticClass())
            {
                sol::state_view L(s);
                return sol::make_object(L, sol::nil);
            }
            R result = (static_cast<C*>(Proxy.Instance)->*Method)(std::forward<P>(Args)...);
            if (!result)
            {
                sol::state_view L(s);
                return sol::make_object(L, sol::nil);
            }
            // Wrap the returned object pointer in a LuaComponentProxy
            sol::state_view L(s);
            return MakeCompProxy(L, result, PointeeType::StaticClass());
        });
    }
    else
    {
        // Non-pointer return types - use original implementation
        T.set_function(Name, [Method](LuaComponentProxy& Proxy, P... Args) -> R
        {
            if (!Proxy.Instance || Proxy.Class != C::StaticClass())
            {
                if constexpr (!std::is_void_v<R>) return R{};
            }
            return (static_cast<C*>(Proxy.Instance)->*Method)(std::forward<P>(Args)...);
        });
    }
}

// const 멤버 함수용 오버로드
template<typename R, typename C, typename... P>
static void AddMethodR(sol::table& T, const char* Name, R(C::*Method)(P...) const)
{
    // Check if R is a pointer type
    if constexpr (std::is_pointer_v<R>)
    {
        // For pointer return types, wrap in LuaComponentProxy
        using PointeeType = std::remove_pointer_t<R>;
        T.set_function(Name, [Method](sol::this_state s, LuaComponentProxy& Proxy, P... Args) -> sol::object
        {
            if (!Proxy.Instance || Proxy.Class != C::StaticClass())
            {
                sol::state_view L(s);
                return sol::make_object(L, sol::nil);
            }
            R result = (static_cast<const C*>(Proxy.Instance)->*Method)(std::forward<P>(Args)...);
            if (!result)
            {
                sol::state_view L(s);
                return sol::make_object(L, sol::nil);
            }
            // Wrap the returned object pointer in a LuaComponentProxy
            sol::state_view L(s);
            return MakeCompProxy(L, result, PointeeType::StaticClass());
        });
    }
    else
    {
        // Non-pointer return types - use original implementation
        T.set_function(Name, [Method](LuaComponentProxy& Proxy, P... Args) -> R
        {
            if (!Proxy.Instance || Proxy.Class != C::StaticClass())
            {
                if constexpr (!std::is_void_v<R>) return R{};
            }
            return (static_cast<const C*>(Proxy.Instance)->*Method)(std::forward<P>(Args)...);
        });
    }
}

// 친절한 별칭 부여용
template<typename C, typename... P>
static void AddAlias(sol::table& T, const char* Alias, void(C::*Method)(P...))
{
    AddMethod<C, P...>(T, Alias, Method);
}

// ===== 프로퍼티 자동 바인딩 헬퍼 =====

// 멤버변수를 Lua 프로퍼티로 바인딩 (Read/Write)
template<typename C, typename T>
static void AddProperty(sol::table& Table, const char* Name, T C::*Member)
{
    // Create a property descriptor table
    sol::state_view lua = Table.lua_state();
    sol::table propDesc = lua.create_table();

    propDesc.set_function("get", [Member](LuaComponentProxy& Proxy) -> T {
        if (!Proxy.Instance || Proxy.Class != C::StaticClass()) return T{};
        return static_cast<C*>(Proxy.Instance)->*Member;
    });

    propDesc.set_function("set", [Member](LuaComponentProxy& Proxy, T Value) {
        if (!Proxy.Instance || Proxy.Class != C::StaticClass()) return;
        static_cast<C*>(Proxy.Instance)->*Member = Value;
    });

    propDesc["is_property"] = true;
    Table[Name] = propDesc;
}

// 읽기 전용 프로퍼티 (EditAnywhere가 false인 경우)
template<typename C, typename T>
static void AddReadOnlyProperty(sol::table& Table, const char* Name, T C::*Member)
{
    sol::state_view lua = Table.lua_state();
    sol::table propDesc = lua.create_table();

    propDesc.set_function("get", [Member](LuaComponentProxy& Proxy) -> T {
        if (!Proxy.Instance || Proxy.Class != C::StaticClass()) return T{};
        return static_cast<C*>(Proxy.Instance)->*Member;
    });

    propDesc["is_property"] = true;
    propDesc["read_only"] = true;
    Table[Name] = propDesc;
}

// 포인터 타입 프로퍼티 (UTexture*, UMaterial* 등)
template<typename C, typename T>
static void AddPropertyPtr(sol::table& Table, const char* Name, T* C::*Member)
{
    sol::state_view lua = Table.lua_state();
    sol::table propDesc = lua.create_table();

    propDesc.set_function("get", [Member](LuaComponentProxy& Proxy) -> T* {
        if (!Proxy.Instance || Proxy.Class != C::StaticClass()) return nullptr;
        return static_cast<C*>(Proxy.Instance)->*Member;
    });

    propDesc.set_function("set", [Member](LuaComponentProxy& Proxy, T* Value) {
        if (!Proxy.Instance || Proxy.Class != C::StaticClass()) return;
        static_cast<C*>(Proxy.Instance)->*Member = Value;
    });

    propDesc["is_property"] = true;
    Table[Name] = propDesc;
}

// TArray<T*> 타입 프로퍼티 (TArray<UMaterial*>, TArray<USound*> 등)
template<typename C, typename T>
static void AddPropertyArrayPtr(sol::table& Table, const char* Name, TArray<T*> C::*Member)
{
    sol::state_view lua = Table.lua_state();
    sol::table propDesc = lua.create_table();

    propDesc.set_function("get", [Member](sol::this_state L, LuaComponentProxy& Proxy) -> sol::table {
        sol::state_view lua(L);
        sol::table result = lua.create_table();
        if (!Proxy.Instance || Proxy.Class != C::StaticClass()) return result;

        TArray<T*>& arr = static_cast<C*>(Proxy.Instance)->*Member;
        for (size_t i = 0; i < arr.size(); ++i) {
            result[i + 1] = arr[i];  // Lua는 1-based 인덱싱
        }
        return result;
    });

    propDesc.set_function("set", [Member](LuaComponentProxy& Proxy, sol::table luaTable) {
        if (!Proxy.Instance || Proxy.Class != C::StaticClass()) return;

        TArray<T*>& arr = static_cast<C*>(Proxy.Instance)->*Member;
        arr.clear();

        for (size_t i = 1; i <= luaTable.size(); ++i) {
            sol::optional<T*> elem = luaTable[i];
            if (elem) {
                arr.push_back(*elem);
            }
        }
    });

    propDesc["is_property"] = true;
    Table[Name] = propDesc;
}
