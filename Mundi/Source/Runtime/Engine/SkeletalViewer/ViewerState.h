#pragma once

class UWorld; class FViewport; class FViewportClient; class ASkeletalMeshActor; class USkeletalMesh; class UAnimSequence;

enum class EViewerMode : uint8
{
    Skeletal,   // Skeleton, Bone 편집 모드
    Animation   // Animation 재생 및 편집 모드
};

class ViewerState
{
public:
    FName Name;
    UWorld* World = nullptr;
    FViewport* Viewport = nullptr;
    FViewportClient* Client = nullptr;

    // Have a pointer to the currently selected mesh to render in the viewer
    ASkeletalMeshActor* PreviewActor = nullptr;
    USkeletalMesh* CurrentMesh = nullptr;
    FString LoadedMeshPath;  // Track loaded mesh path for unloading
    int32 SelectedBoneIndex = -1;
    bool bShowMesh = true;
    bool bShowBones = true;
    // Bone line rebuild control
    bool bBoneLinesDirty = true;      // true면 본 라인 재구성
    int32 LastSelectedBoneIndex = -1; // 색상 갱신을 위한 이전 선택 인덱스
    // UI path buffer per-tab
    char MeshPathBuffer[260] = {0};
    char AnimPathBuffer[260] = {0};
    std::set<int32> ExpandedBoneIndices;

    // 애니메이션 임포트 관련
    int32 SelectedSkeletonIndex = -1;  // ComboBox에서 선택된 Skeleton 인덱스
    USkeletalMesh* SelectedSkeletonMesh = nullptr;  // 선택된 Skeleton을 가진 Mesh

    // 본 트랜스폼 편집 관련
    FVector EditBoneLocation;
    FVector EditBoneRotation;  // Euler angles in degrees
    FVector EditBoneScale;

    bool bBoneTransformChanged = false;
    bool bBoneRotationEditing = false;

    // 편집된 bone transform 캐시 (애니메이션 재생 중에도 유지)
    TMap<int32, FTransform> EditedBoneTransforms;  // BoneIndex -> 편집된 Transform

    // 애니메이션 재생 관련
    int32 SelectedAnimationIndex = -1;  // 선택된 Animation 인덱스
    UAnimSequence* CurrentAnimation = nullptr;  // 현재 재생 중인 Animation
    bool bIsPlaying = false;  // 재생 중인지 여부
    float CurrentAnimationTime = 0.0f;  // 현재 재생 시간 (초)
    bool bLoopAnimation = true;  // 루프 재생 여부
    float PlaybackSpeed = 1.0f;  // 재생 속도 배율

    // 타임라인 뷰 관련
    float TimelineZoom = 1.0f;  // 타임라인 줌 레벨 (1.0 = 기본)
    float TimelineScroll = 0.0f;  // 타임라인 스크롤 오프셋 (초 단위)
    bool bShowFrameNumbers = false;  // false=시간(초), true=프레임 번호
    float PlaybackRangeStart = 0.0f;  // 재생 범위 시작 (초)
    float PlaybackRangeEnd = -1.0f;  // 재생 범위 끝 (초, -1=전체)

    // 이 Viewer에서 임포트한 AnimSequence들 (메모리 관리용)
    TArray<UAnimSequence*> ImportedAnimSequences;

    // View Mode
    EViewerMode ViewMode = EViewerMode::Skeletal;  // 기본값: Skeletal 모드
};
