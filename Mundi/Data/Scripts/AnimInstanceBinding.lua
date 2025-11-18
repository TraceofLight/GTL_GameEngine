-- ===================================================================
-- Character Animation Binding Script
-- 캐릭터의 속도를 AnimInstance의 "Speed" 파라미터에 바인딩
-- ===================================================================

-- 성능을 위해 매 틱마다 GetMesh 등을 호출하지 않고 변수에 캐싱합니다.
local CharacterMoveComp = nil
local SkeletalMesh = nil
local AnimInstance = nil

function BeginPlay()
    print("=== Animation Binding Started ===")

    -- 1. Obj를 Character로 간주 (C++에서 ACharacter로 바인딩 되었다고 가정)
    CharacterMoveComp = GetComponent(Obj, "UCharacterMovementComponent")
    SkeletalMesh = GetComponent(Obj, "USkeletalMeshComponent")

    if SkeletalMesh then
        print(" -> SkeletalMesh found.")

        -- C++: GetAnimInstance() -> Lua: :GetAnimInstance()
        if SkeletalMesh.GetAnimInstance then
            AnimInstance = SkeletalMesh:GetAnimInstance()
        else
            print("[Error] SkeletalMesh does not have GetAnimInstance function.")
        end
    end

    if AnimInstance then
        print(" -> AnimInstance found and cached successfully.")
    else
        print("[Warning] AnimInstance is nil. Check if Animation Blueprint is assigned.")
    end
end

function Tick(dt)
    -- AnimInstance가 유효할 때만 로직 수행
    if AnimInstance then
        local velocity = CharacterMoveComp:GetVelocity()

        -- V = sqrt(x^2 + y^2 + z^2)
        local speed = math.sqrt(velocity.X * velocity.X + velocity.Y * velocity.Y + velocity.Z * velocity.Z)

        -- 디버깅용 (필요시 주석 해제)
        print("Current Speed:", speed)

        AnimInstance:SetFloat("Speed", speed)
    end
end

function EndPlay()
    print("=== Animation Binding Ended ===")
    -- 참조 해제
    CharacterMoveComp = nil
    SkeletalMesh = nil
    AnimInstance = nil
end

function OnBeginOverlap(OtherActor)
end

function OnEndOverlap(OtherActor)
end