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

local tickCounter = 0

function Tick(dt)
    -- AnimInstance가 유효할 때만 로직 수행
    if AnimInstance then
        local bIsJumping = CharacterMoveComp:IsJumping()
        local velocity = CharacterMoveComp:GetVelocity()

        -- V = sqrt(x^2 + y^2 + z^2)
        local speed = math.sqrt(velocity.X * velocity.X + velocity.Y * velocity.Y + velocity.Z * velocity.Z)

        -- 캐릭터의 Forward, Right 벡터 가져오기 (CharacterMoveComp에서 직접 호출)
        local forward = CharacterMoveComp:GetActorForwardVector()
        local right = CharacterMoveComp:GetActorRightVector()

        -- 속도 벡터 정규화 (C++의 LocalVelocity.Normalize()와 동일)
        local normalizedVelX = 0
        local normalizedVelY = 0
        local normalizedVelZ = 0

        if speed > 0.01 then
            normalizedVelX = velocity.X / speed
            normalizedVelY = velocity.Y / speed
            normalizedVelZ = velocity.Z / speed
        end

        -- 내적 계산 (C++의 Dot Product와 동일)
        local forwardDot = normalizedVelX * forward.X + normalizedVelY * forward.Y + normalizedVelZ * forward.Z
        local rightDot = normalizedVelX * right.X + normalizedVelY * right.Y + normalizedVelZ * right.Z

        -- Signed Speed 계산 (후진 시 음수)
        local signedSpeed = speed
        if forwardDot < 0 then
            signedSpeed = -speed
        end

        -- Direction 계산 (C++과 동일: atan2(RightDot, ForwardDot))
        local direction = math.atan(rightDot, forwardDot) * (180.0 / math.pi)

        -- AnimInstance에 값 설정
        AnimInstance:SetFloat("Speed", speed)
        AnimInstance:SetFloat("AbsSpeed", speed)  -- 절대값 (항상 양수)
        AnimInstance:SetBool("bIsJumping", bIsJumping)
        AnimInstance:SetFloat("Direction", direction)

        -- 디버깅용 (60프레임마다 출력)
        if tickCounter % 60 == 0 then
            print(string.format("[AnimBinding] Tick %d: Speed=%.1f (Raw=%.1f), Direction=%.1f",
                tickCounter, signedSpeed, speed, direction))
            print(string.format("  ForwardDot=%.3f, RightDot=%.3f", forwardDot, rightDot))
        end

        tickCounter = tickCounter + 1
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
