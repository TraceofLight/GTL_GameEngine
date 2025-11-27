-- MoveForward.lua
-- ProjectileMovementComponent를 사용하여 액터를 앞으로 이동시키는 스크립트

-- 이동 속도 (units per second)
local Speed = 500.0

-- Forward 방향 (기본값: X축 양의 방향)
local ForwardDirection = Vector(1, 0, 0)

-- ProjectileMovementComponent 참조
local ProjectileComp = nil

function BeginPlay()
    print("========================================")
    print("=== MoveForward Script Started ===")
    print("========================================")
    print("  Initial Location:", Obj.Location.X, Obj.Location.Y, Obj.Location.Z)

    -- ProjectileMovementComponent 가져오기
    ProjectileComp = GetComponent(Obj, "UProjectileMovementComponent")

    if ProjectileComp then
        print("  >> ProjectileMovementComponent found!")

        -- 중력 비활성화 (수평 이동만 원한다면)
        ProjectileComp.Gravity = 0.0

        -- Velocity 설정 (앞으로 향하는 속도)
        ProjectileComp.Velocity = Vector(
            ForwardDirection.X * Speed,
            ForwardDirection.Y * Speed,
            ForwardDirection.Z * Speed
        )

        -- 초기 속도와 최대 속도 설정
        ProjectileComp.InitialSpeed = Speed
        ProjectileComp.MaxSpeed = Speed

        -- 컴포넌트 활성화
        ProjectileComp:SetActive(true)

        print("  Speed:", Speed)
        print("  Velocity:", ProjectileComp.Velocity.X, ProjectileComp.Velocity.Y, ProjectileComp.Velocity.Z)
        print("  Gravity:", ProjectileComp.Gravity)
    else
        print("  >> Warning: No ProjectileMovementComponent found!")
        print("  >> Please add ProjectileMovementComponent to this actor")
    end
end

function Tick(dt)
    -- ProjectileMovementComponent가 자동으로 이동을 처리하므로
    -- 여기서는 아무것도 하지 않아도 됩니다

    -- (선택사항) 실시간으로 방향을 바꾸고 싶다면:
    -- if ProjectileComp then
    --     ProjectileComp.Velocity = Vector(ForwardDirection.X * Speed, ForwardDirection.Y * Speed, ForwardDirection.Z * Speed)
    -- end
end

function EndPlay()
    print("=== MoveForward Script Ended ===")
    print("  Final Location:", Obj.Location.X, Obj.Location.Y, Obj.Location.Z)
end

function OnBeginOverlap(OtherActor)
    -- 충돌 시 처리 (필요하면 추가)
end

function OnEndOverlap(OtherActor)
    -- 충돌 종료 시 처리 (필요하면 추가)
end
