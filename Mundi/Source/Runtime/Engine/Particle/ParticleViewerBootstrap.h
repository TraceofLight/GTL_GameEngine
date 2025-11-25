#pragma once

class ParticleViewerState;
class UWorld;
struct ID3D11Device;

/**
 * @brief 파티클 에디터 프리뷰 월드 부트스트랩
 * @details 파티클 에디터 탭별 상태 생성/파괴 헬퍼
 */
class ParticleViewerBootstrap
{
public:
	static ParticleViewerState* CreateViewerState(const char* Name, UWorld* InWorld, ID3D11Device* InDevice);
	static void DestroyViewerState(ParticleViewerState*& State);
};
