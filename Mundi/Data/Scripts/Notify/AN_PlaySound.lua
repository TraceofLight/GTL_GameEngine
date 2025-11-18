-- Notify_PlaySound.lua
-- Plays a sound at the actor's location

NotifyClass = {
    DisplayName = "Play Sound",
    Description = "Plays a sound at actor location",

    -- Properties that can be edited in the Timeline UI
    Properties = {
        { Name = "SoundPath", Type = "String", Default = "Data/Audio/CGC2.wav" },
        { Name = "Volume", Type = "Float", Default = 1.0 },
        { Name = "Pitch", Type = "Float", Default = 1.0 }
    }
}

function NotifyClass:Notify(MeshComp, Time)
    print(string.format("[Notify_PlaySound] Triggered at time %.2f", Time))

    -- SoundPath가 설정되지 않았으면 기본값 사용
    local SoundPath = self.SoundPath
    if not SoundPath or SoundPath == "" or SoundPath == "None" then
        SoundPath = "Data/Audio/CGC2.wav"  -- 기본 사운드 (Properties의 Default와 동일)
    end

    -- Volume 파라미터 (Properties에서 설정 가능)
    local Volume = self.Volume or 1.0

    print(string.format("[Notify_PlaySound] Playing SoundPath: %s, Volume: %.2f", SoundPath, Volume))

    -- TriggerAnimNotify를 통해 Delegate로 사운드 재생 (SkeletalMeshActor가 구독)
    MeshComp:TriggerAnimNotify(SoundPath, Time, 0.0)
end

return NotifyClass
