-- MoveForwardRotated.lua
-- ProjectileMovementComponent를 사용하여 액터의 회전 방향으로 이동시키는 스크립트

-- 이동 속도 (units per second)
local Speed = 500.0

-- ProjectileMovementComponent 참조
local ProjectileComp = nil

-- 벡터 정규화 함수
local function Normalize(V)
    local length = math.sqrt(V.X * V.X + V.Y * V.Y + V.Z * V.Z)
    if length > 0 then
        return Vector(V.X / length, V.Y / length, V.Z / length)
    end
    return Vector(0, 0, 0)
end

-- Yaw(Z축 회전)을 기준으로 Forward 벡터 계산
local function GetForwardVector(yaw)
    -- Yaw는 라디안 단위
    local forward = Vector(
        math.cos(yaw),  -- X
        math.sin(yaw),  -- Y
        0               -- Z (수평 이동)
    )
    return Normalize(forward)
end

function BeginPlay()
    print("========================================")
    print("=== MoveForwardRotated Script Started ===")
    print("========================================")
    print("  Initial Location:", Obj.Location.X, Obj.Location.Y, Obj.Location.Z)
    print("  Initial Rotation:", Obj.Rotation.X, Obj.Rotation.Y, Obj.Rotation.Z)
    print("  Speed:", Speed)

    -- ProjectileMovementComponent 가져오기
    ProjectileComp = GetComponent(Obj, "UProjectileMovementComponent")

    if ProjectileComp then
        print("  >> ProjectileMovementComponent found!")

        -- 중력 비활성화 (수평 이동만 원한다면)
        ProjectileComp.Gravity = 0.0

        -- 초기 속도와 최대 속도 설정
        ProjectileComp.InitialSpeed = Speed
        ProjectileComp.MaxSpeed = Speed

        -- 현재 회전 방향으로 Velocity 설정
        local yaw = math.rad(Obj.Rotation.Z)
        local forward = GetForwardVector(yaw)
        ProjectileComp.Velocity = Vector(
            forward.X * Speed,
            forward.Y * Speed,
            forward.Z * Speed
        )

        -- 컴포넌트 활성화
        ProjectileComp:SetActive(true)

        print("  Forward Direction:", forward.X, forward.Y, forward.Z)
        print("  Velocity:", ProjectileComp.Velocity.X, ProjectileComp.Velocity.Y, ProjectileComp.Velocity.Z)
    else
        print("  >> Warning: No ProjectileMovementComponent found!")
        print("  >> Please add ProjectileMovementComponent to this actor")
    end
end

function Tick(dt)
    -- (선택사항) 회전이 실시간으로 바뀌면 Velocity도 업데이트
    -- 만약 액터가 회전하면서 이동 방향도 바뀌어야 한다면 주석 해제:
    --[[
    if ProjectileComp then
        local yaw = math.rad(Obj.Rotation.Z)
        local forward = GetForwardVector(yaw)
        ProjectileComp.Velocity = Vector(
            forward.X * Speed,
            forward.Y * Speed,
            forward.Z * Speed
        )
    end
    ]]--
end

function EndPlay()
    print("=== MoveForwardRotated Script Ended ===")
    print("  Final Location:", Obj.Location.X, Obj.Location.Y, Obj.Location.Z)
end

function OnBeginOverlap(OtherActor)
    -- 충돌 시 처리
end

function OnEndOverlap(OtherActor)
    -- 충돌 종료 시 처리
end
