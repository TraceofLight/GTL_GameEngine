-- ANS_Template.lua
-- Animation Notify State (지속 시간 있음)
-- 새로운 Notify State 스크립트를 만들기 위한 템플릿 파일입니다

NotifyStateClass = {
    DisplayName = "Notify State Template",
    Description = "Custom Notify State Template",

    -- Timeline UI에서 편집 가능한 프로퍼티
    Properties = {
        -- 예시 프로퍼티 (필요에 따라 주석 해제 및 수정):
        -- { Name = "Intensity", Type = "Float", Default = 1.0 },
        -- { Name = "EffectPath", Type = "String", Default = "Data/Effects/Trail.vfx" },
    }
}

function NotifyStateClass:NotifyBegin(meshComp, time)
    print(string.format("[Animation Notify State] NotifyBegin - %.2f초", time))

    -- 초기화 로직을 여기에 추가하세요
    -- Notify State가 시작될 때 한 번 호출됩니다

    -- 프로퍼티 접근 예시:
    -- local intensity = self.Intensity or 1.0
end

function NotifyStateClass:NotifyTick(meshComp, time, deltaTime)
    -- Notify State가 활성화된 동안 매 프레임 호출됩니다
    -- 프레임마다 실행할 로직을 여기에 추가하세요 (예: 파티클 효과 업데이트)
end

function NotifyStateClass:NotifyEnd(meshComp, time)
    print(string.format("[Animation Notify State] NotifyEnd - %.2f초", time))

    -- 정리 로직을 여기에 추가하세요
    -- Notify State가 종료될 때 한 번 호출됩니다
end

return NotifyStateClass
