-- AN_Template.lua
-- Animation Notify (즉시 이벤트)
-- 새로운 Notify 스크립트를 만들기 위한 템플릿 파일입니다

NotifyClass = {
    DisplayName = "Notify Template",
    Description = "Custom Notify Template",

    -- Timeline UI에서 편집 가능한 프로퍼티
    Properties = {
        -- 예시 프로퍼티 (필요에 따라 주석 해제 및 수정):
        -- { Name = "Volume", Type = "Float", Default = 1.0 },
        -- { Name = "SoundPath", Type = "String", Default = "Data/Audio/CGC1.wav" },
    }
}

function NotifyClass:Notify(meshComp, time)
    print(string.format("[Animation Notify] %.2f초에 트리거됨", time))

    -- 여기에 커스텀 로직을 추가하세요
    -- 예시: 사운드 재생, 파티클 효과 생성 등

    -- 프로퍼티 접근 예시:
    -- local volume = self.Volume or 1.0
    -- local soundPath = self.SoundPath or "Data/Audio/CGC1.wav"
end

return NotifyClass
