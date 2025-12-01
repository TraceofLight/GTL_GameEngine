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
    int32 TabId = 0;  // 고유 탭 ID (ImGui 탭 식별용)
    UWorld* World = nullptr;
    FViewport* Viewport = nullptr;
    FViewportClient* Client = nullptr;

    // Have a pointer to the currently selected mesh to render in the viewer
    ASkeletalMeshActor* PreviewActor = nullptr;
    AStaticMeshActor* FloorActor = nullptr;
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
    std::set<int32> ExpandedBoneIndices;

    // 본 트랜스폼 편집 관련
    FVector EditBoneLocation;
    FVector EditBoneRotation;  // Euler angles in degrees
    FVector EditBoneScale;

    bool bBoneTransformChanged = false;
    bool bBoneRotationEditing = false;
    bool bWasGizmoDragging = false;  // 이전 프레임의 기즈모 드래그 상태 (드래그 종료 감지용)
    int32 DragStartBoneIndex = -1;  // 드래그 시작 시 선택된 본 인덱스

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
    int32 PlaybackRangeStartFrame = 0;  // 재생 범위 시작 (프레임)
    int32 PlaybackRangeEndFrame = -1;  // 재생 범위 끝 (프레임, -1=전체)
    int32 WorkingRangeStartFrame = 0;  // 작업 범위 시작 (프레임)
    int32 WorkingRangeEndFrame = -1;  // 작업 범위 끝 (프레임, -1=전체)
    int32 ViewRangeStartFrame = 0;  // 뷰 범위 시작 (프레임)
    int32 ViewRangeEndFrame = -1;  // 뷰 범위 끝 (프레임, -1=전체)

    // Notify Track 관련
    bool bNotifiesExpanded = false;  // Notifies 드롭다운 펼침 상태
    int32 SelectedNotifyTrackIndex = -1;  // 선택된 Notify Track 인덱스
    int32 SelectedNotifyIndex = -1;  // 선택된 Notify 이벤트 인덱스
    TArray<FString> NotifyTrackNames;  // Notify Track 이름 목록 (예: "Track 1", "Track 2")
    std::set<int32> UsedTrackNumbers;  // 사용 중인 Track 번호 (빈 번호 재사용용)
	bool bDraggingNotify = false;
	float NotifyDragOffsetX = 0.0f;

    // Notify 클립보드 (복사/붙여넣기)
    bool bHasNotifyClipboard = false;  // 클립보드에 Notify가 있는지 여부
    struct FAnimNotifyEvent NotifyClipboard;  // 클립보드에 복사된 Notify

    // Animation Recording
    bool bIsRecording = false;  // 녹화 중인지 여부
    bool bShowRecordDialog = false;  // 녹화 파일명 입력 다이얼로그 표시 여부
    char RecordFileNameBuffer[128] = "";  // 다이얼로그용 파일명 버퍼
    FString RecordedFileName = "";  // 실제 녹화 중인 파일명
    TArray<TMap<int32, FTransform>> RecordedFrames;  // 녹화된 프레임 데이터 (프레임별 본 트랜스폼)
    float RecordStartTime = 0.0f;  // 녹화 시작 시간
    float RecordFrameRate = 30.0f;  // 녹화 프레임레이트

    // View Mode
    EViewerMode ViewMode = EViewerMode::Skeletal;

    // Detail Panel 탭 관련
    int32 DetailPanelTabIndex = 0;  // 0: Asset Details, 1: Details (Bone Transform)
    // Transform 표시 모드 (비트마스크, Shift+클릭으로 다중 선택)
    // Bit 0: Bone, Bit 1: Reference, Bit 2: Mesh Relative
    uint32 BoneTransformModeFlags = 0x7;  // 기본값: 전체 선택 (Bone + Reference + Mesh Relative)

    // Reference Pose 트랜스폼 (편집 UI용 캐시)
    FVector ReferenceBoneLocation;
    FVector ReferenceBoneRotation;
    FVector ReferenceBoneScale;

    // Mesh Relative 트랜스폼 (편집 UI용)
    FVector MeshRelativeBoneLocation;
    FVector MeshRelativeBoneRotation;
    FVector MeshRelativeBoneScale;
};
