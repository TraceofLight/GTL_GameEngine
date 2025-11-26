#include "pch.h"
#include "ThumbnailManager.h"
#include "Texture.h"
#include "ResourceManager.h"
#include "Base64.h"
#include "JsonSerializer.h"
#include <DirectXTex.h>
#include <algorithm>

void FThumbnailManager::Initialize(ID3D11Device* InDevice, ID3D11DeviceContext* InDeviceContext)
{
	Device = InDevice;
	DeviceContext = InDeviceContext;

	UE_LOG("ThumbnailManager: Initialized");
}

void FThumbnailManager::Shutdown()
{
	// 모든 썸네일 리소스 해제 (Manager가 소유한 것만)
	for (auto& Pair : ThumbnailCache)
	{
		if (Pair.second.bOwnedByManager)
		{
			if (Pair.second.SRV)
			{
				Pair.second.SRV->Release();
			}
			if (Pair.second.Texture)
			{
				Pair.second.Texture->Release();
			}
		}
	}
	ThumbnailCache.clear();

	// 기본 아이콘 해제 (항상 Manager가 소유)
	for (auto& Pair : DefaultIconCache)
	{
		if (Pair.second.SRV)
		{
			Pair.second.SRV->Release();
		}
		if (Pair.second.Texture)
		{
			Pair.second.Texture->Release();
		}
	}
	DefaultIconCache.clear();

	UE_LOG("ThumbnailManager: Shutdown");
}

ID3D11ShaderResourceView* FThumbnailManager::GetThumbnail(const std::string& FilePath)
{
	// 캐시에 있으면 반환
	auto It = ThumbnailCache.find(FilePath);
	if (It != ThumbnailCache.end())
	{
		return It->second.SRV;
	}

	// 확장자 추출
	size_t DotPos = FilePath.find_last_of('.');
	if (DotPos == std::string::npos)
	{
		return nullptr;
	}

	std::string Extension = FilePath.substr(DotPos);
	std::transform(Extension.begin(), Extension.end(), Extension.begin(), ::tolower);

	// 확장자별 썸네일 생성
	FThumbnailData* ThumbnailData = nullptr;

	if (Extension == ".fbx")
	{
		ThumbnailData = CreateFBXThumbnail(FilePath);
	}
	else if (Extension == ".png" || Extension == ".jpg" || Extension == ".jpeg" ||
	         Extension == ".dds" || Extension == ".tga")
	{
		ThumbnailData = CreateImageThumbnail(FilePath);
	}
	else if (Extension == ".psys")
	{
		ThumbnailData = CreatePsysThumbnail(FilePath);
	}
	else
	{
		// 기본 아이콘 사용
		ThumbnailData = CreateDefaultThumbnail(Extension);
	}

	if (ThumbnailData && ThumbnailData->SRV)
	{
		return ThumbnailData->SRV;
	}

	return nullptr;
}

void FThumbnailManager::ClearCache()
{
	// 캐시된 썸네일 모두 해제 (Manager가 소유한 것만)
	for (auto& Pair : ThumbnailCache)
	{
		if (Pair.second.bOwnedByManager)
		{
			if (Pair.second.SRV)
			{
				Pair.second.SRV->Release();
			}
			if (Pair.second.Texture)
			{
				Pair.second.Texture->Release();
			}
		}
	}
	ThumbnailCache.clear();

	UE_LOG("ThumbnailManager: Cache cleared");
}

void FThumbnailManager::InvalidateThumbnail(const std::string& Key)
{
	auto It = ThumbnailCache.find(Key);
	if (It != ThumbnailCache.end())
	{
		// Manager가 소유한 경우에만 Release
		if (It->second.bOwnedByManager)
		{
			if (It->second.SRV)
			{
				It->second.SRV->Release();
			}
			if (It->second.Texture)
			{
				It->second.Texture->Release();
			}
		}

		ThumbnailCache.erase(It);
		UE_LOG("ThumbnailManager: Thumbnail cache invalidated: %s", Key.c_str());
	}
}

FThumbnailData* FThumbnailManager::CreateFBXThumbnail(const std::string& FilePath)
{
	// TODO: 실제 FBX 메시를 렌더타겟에 렌더링하여 썸네일 생성
	// 현재는 기본 아이콘 반환
	UE_LOG("ThumbnailManager: FBX thumbnail generation not yet implemented for %s", FilePath.c_str());
	return CreateDefaultThumbnail(".fbx");
}

FThumbnailData* FThumbnailManager::CreateImageThumbnail(const std::string& FilePath)
{
	if (!Device)
	{
		return nullptr;
	}

	// 이미지 파일은 직접 로드하여 썸네일로 사용
	UTexture* Texture = UResourceManager::GetInstance().Load<UTexture>(FString(FilePath.c_str()), true);
	if (!Texture || !Texture->GetShaderResourceView())
	{
		UE_LOG("ThumbnailManager: Failed to load image thumbnail: %s", FilePath.c_str());
		return CreateDefaultThumbnail(".img");
	}

	// 캐시에 추가 (참조만 저장, ResourceManager가 관리하므로 Release하지 않음)
	FThumbnailData Data;
	Data.SRV = Texture->GetShaderResourceView();
	Data.Texture = Texture->GetTexture2D();
	Data.Width = Texture->GetWidth();
	Data.Height = Texture->GetHeight();
	Data.bOwnedByManager = false;  // ResourceManager가 소유, 우리는 Release 안 함

	ThumbnailCache[FilePath] = Data;
	return &ThumbnailCache[FilePath];
}

FThumbnailData* FThumbnailManager::CreateDefaultThumbnail(const std::string& Extension)
{
	// 이미 생성된 기본 아이콘이 있으면 반환
	auto It = DefaultIconCache.find(Extension);
	if (It != DefaultIconCache.end())
	{
		return &It->second;
	}

	if (!Device)
	{
		return nullptr;
	}

	// 단색 텍스처 생성 (128x128)
	FThumbnailData Data;
	Data.Width = ThumbnailSize;
	Data.Height = ThumbnailSize;

	// 확장자별 색상 결정
	uint32_t Color = 0xFF808080; // 기본 회색
	if (Extension == ".fbx" || Extension == ".obj")
	{
		Color = 0xFF4080FF; // 파란색 (메시)
	}
	else if (Extension == ".prefab")
	{
		Color = 0xFF40FF80; // 초록색 (프리팹)
	}
	else if (Extension == ".mat")
	{
		Color = 0xFFFF8040; // 주황색 (머티리얼)
	}
	else if (Extension == ".psys")
	{
		Color = 0xFFFF40FF; // 마젠타 (파티클 시스템)
	}

	// 텍스처 데이터 생성 (단색)
	std::vector<uint32_t> Pixels(ThumbnailSize * ThumbnailSize, Color);

	// D3D11 텍스처 생성
	D3D11_TEXTURE2D_DESC TexDesc = {};
	TexDesc.Width = ThumbnailSize;
	TexDesc.Height = ThumbnailSize;
	TexDesc.MipLevels = 1;
	TexDesc.ArraySize = 1;
	TexDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	TexDesc.SampleDesc.Count = 1;
	TexDesc.Usage = D3D11_USAGE_IMMUTABLE;
	TexDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

	D3D11_SUBRESOURCE_DATA InitData = {};
	InitData.pSysMem = Pixels.data();
	InitData.SysMemPitch = ThumbnailSize * sizeof(uint32_t);

	HRESULT Hr = Device->CreateTexture2D(&TexDesc, &InitData, &Data.Texture);
	if (FAILED(Hr))
	{
		UE_LOG("ThumbnailManager: Failed to create default thumbnail texture for %s", Extension.c_str());
		return nullptr;
	}

	// ShaderResourceView 생성
	D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc = {};
	SRVDesc.Format = TexDesc.Format;
	SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	SRVDesc.Texture2D.MipLevels = 1;

	Hr = Device->CreateShaderResourceView(Data.Texture, &SRVDesc, &Data.SRV);
	if (FAILED(Hr))
	{
		Data.Texture->Release();
		UE_LOG("ThumbnailManager: Failed to create SRV for default thumbnail: %s", Extension.c_str());
		return nullptr;
	}

	DefaultIconCache[Extension] = Data;
	return &DefaultIconCache[Extension];
}

ID3D11ShaderResourceView* FThumbnailManager::GetThumbnailFromBase64(const std::string& Base64Data, const std::string& CacheKey)
{
	if (Base64Data.empty() || !Device)
	{
		return nullptr;
	}

	// 캐시에 있으면 반환
	auto It = ThumbnailCache.find(CacheKey);
	if (It != ThumbnailCache.end())
	{
		return It->second.SRV;
	}

	// Base64 디코딩
	std::vector<uint8_t> DDSData = FBase64::Decode(Base64Data);
	if (DDSData.empty())
	{
		UE_LOG("ThumbnailManager: Failed to decode Base64 data for %s", CacheKey.c_str());
		return nullptr;
	}

	// DirectXTex를 사용하여 DDS 메모리에서 로드
	DirectX::TexMetadata Metadata;
	DirectX::ScratchImage Image;
	HRESULT Hr = DirectX::LoadFromDDSMemory(
		DDSData.data(),
		DDSData.size(),
		DirectX::DDS_FLAGS_NONE,
		&Metadata,
		Image
	);

	if (FAILED(Hr))
	{
		UE_LOG("ThumbnailManager: Failed to load DDS from memory for %s", CacheKey.c_str());
		return nullptr;
	}

	// D3D11 텍스처 생성
	FThumbnailData Data;
	Hr = DirectX::CreateTexture(
		Device,
		Image.GetImages(),
		Image.GetImageCount(),
		Metadata,
		(ID3D11Resource**)&Data.Texture
	);

	if (FAILED(Hr) || !Data.Texture)
	{
		UE_LOG("ThumbnailManager: Failed to create texture from DDS for %s", CacheKey.c_str());
		return nullptr;
	}

	// ShaderResourceView 생성
	D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc = {};
	SRVDesc.Format = Metadata.format;
	SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	SRVDesc.Texture2D.MipLevels = static_cast<UINT>(Metadata.mipLevels);

	Hr = Device->CreateShaderResourceView(Data.Texture, &SRVDesc, &Data.SRV);
	if (FAILED(Hr))
	{
		Data.Texture->Release();
		UE_LOG("ThumbnailManager: Failed to create SRV from DDS for %s", CacheKey.c_str());
		return nullptr;
	}

	Data.Width = static_cast<int>(Metadata.width);
	Data.Height = static_cast<int>(Metadata.height);
	Data.bOwnedByManager = true;

	ThumbnailCache[CacheKey] = Data;
	return Data.SRV;
}

FThumbnailData* FThumbnailManager::CreatePsysThumbnail(const std::string& FilePath)
{
	if (!Device)
	{
		return nullptr;
	}

	// .psys 파일을 JSON으로 로드
	FWideString WidePath(FilePath.begin(), FilePath.end());
	JSON Json;
	if (!FJsonSerializer::LoadJsonFromFile(Json, WidePath))
	{
		UE_LOG("ThumbnailManager: Failed to load .psys file: %s", FilePath.c_str());
		return CreateDefaultThumbnail(".psys");
	}

	// 0번 이미터(첫 번째 이미터)의 ThumbnailData 확인
	std::string ThumbnailData;
	if (Json.hasKey("Emitters") && Json["Emitters"].JSONType() == JSON::Class::Array && Json["Emitters"].size() > 0)
	{
		JSON& FirstEmitter = Json["Emitters"][0];
		if (FirstEmitter.hasKey("ThumbnailData"))
		{
			ThumbnailData = FirstEmitter["ThumbnailData"].ToString();
		}
	}

	// 0번 이미터에 썸네일이 있으면 사용
	if (!ThumbnailData.empty())
	{
		ID3D11ShaderResourceView* SRV = GetThumbnailFromBase64(ThumbnailData, FilePath);
		if (SRV)
		{
			// 캐시에서 찾아 반환 (GetThumbnailFromBase64가 캐시에 추가함)
			auto It = ThumbnailCache.find(FilePath);
			if (It != ThumbnailCache.end())
			{
				return &It->second;
			}
		}
	}

	// 썸네일이 없으면 파티클 시스템 아이콘 사용 (S_Emitter.PNG)
	std::string IconPath = GDataDir + "/Default/Icon/S_Emitter.PNG";
	return CreateImageThumbnail(IconPath);
}
