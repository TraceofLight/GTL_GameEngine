-- ANS_HitDetection.lua
-- Animation Notify State for melee attack hit detection

NotifyStateClass = {
    DisplayName = "Hit Detection",
    Description = "보여주기용 샘플 State 코드, 작동 안함",

    -- Timeline UI에서 편집 가능한 프로퍼티
    Properties = {
        { Name = "BoneName", Type = "String", Default = "RightHand" },
        { Name = "TraceRadius", Type = "Float", Default = 50.0 },
        { Name = "DebugDraw", Type = "Bool", Default = true }
    }
}

function NotifyStateClass:NotifyBegin(MeshComp, Time)
    print(string.format("[HitDetection] NotifyBegin - Time: %.2f", Time))

    -- 피격 추적을 위한 테이블 초기화 (같은 타겟을 여러 번 때리지 않도록)
    self.HitActors = {}

    local BoneName = self.BoneName or "RightHand"
    local TraceRadius = self.TraceRadius or 50.0

    print(string.format("[HitDetection] Started - BoneName: %s, Radius: %.1f", BoneName, TraceRadius))
end

function NotifyStateClass:NotifyTick(MeshComp, Time, DeltaTime)
    -- 매 프레임 충돌 검사
    local Owner = MeshComp:GetOwner()
    if not Owner then
        return
    end

    -- Bone 위치 가져오기 (무기 위치)
    local BoneName = self.BoneName or "RightHand"
    local BoneTransform = MeshComp:GetBoneWorldTransform(BoneName)
    if not BoneTransform then
        return
    end

    local BoneLocation = BoneTransform:GetLocation()
    local TraceRadius = self.TraceRadius or 50.0

    -- World에서 Sphere Overlap 수행 (주변 Actor 검색)
    local World = Owner:GetWorld()
    local OverlappingActors = World:GetActorsInSphere(BoneLocation, TraceRadius)

    -- 충돌한 Actor 처리
    for i = 1, #OverlappingActors do
        local HitActor = OverlappingActors[i]

        -- 자기 자신은 제외
        if HitActor ~= Owner then
            -- 이미 맞은 Actor는 제외 (중복 피격 방지)
            local ActorUUID = HitActor:GetUUID()
            if not self.HitActors[ActorUUID] then
                self.HitActors[ActorUUID] = true

                -- 피격 정보 로그
                local HitLocation = HitActor:GetActorLocation()
                print(string.format("[HitDetection] Hit Actor: %s at (%.1f, %.1f, %.1f)",
                    HitActor:GetName(), HitLocation.X, HitLocation.Y, HitLocation.Z))

                -- TriggerAnimNotify로 Delegate 발생 (피격 정보 전달)
                -- 포맷: "HitActor|ActorName|X|Y|Z"
                local HitData = string.format("%s|%.1f|%.1f|%.1f",
                    HitActor:GetName(), HitLocation.X, HitLocation.Y, HitLocation.Z)
                MeshComp:TriggerAnimNotify(HitData, Time, 0.0)
            end
        end
    end

    -- Debug Draw (옵션)
    if self.DebugDraw then
        World:DrawDebugSphere(BoneLocation, TraceRadius, 0.0, 0.0, 1.0, 0.3, DeltaTime)
    end
end

function NotifyStateClass:NotifyEnd(MeshComp, Time)
    print(string.format("[HitDetection] NotifyEnd - Time: %.2f, Total Hits: %d", Time, self:CountHits()))

    -- 정리
    self.HitActors = nil
end

-- Helper: 피격 횟수 카운트
function NotifyStateClass:CountHits()
    if not self.HitActors then
        return 0
    end

    local Count = 0
    for _ in pairs(self.HitActors) do
        Count = Count + 1
    end
    return Count
end

return NotifyStateClass
