#include "pch.h"
#include "ObjectFactory.h"
#include "FbxLoader.h"
#include "fbxsdk/fileio/fbxiosettings.h"
#include "fbxsdk/scene/geometry/fbxcluster.h"
#include "ObjectIterator.h"
#include "WindowsBinReader.h"
#include "WindowsBinWriter.h"
#include "PathUtils.h"
#include "Source/Runtime/Engine/Animation/AnimSequence.h"
#include "Source/Runtime/Engine/Animation/AnimDataModel.h"
#include "Source/Runtime/Engine/Animation/AnimationTypes.h"
#include "Source/Runtime/Engine/Animation/MixamoChainMapper.h"
#include "Source/Runtime/AssetManagement/ResourceManager.h"
#include <filesystem>
#include <cmath>
#include <cfloat>
#include <algorithm>
#include <mutex>

IMPLEMENT_CLASS(UFbxLoader)

// FBX SDK는 스레드 안전하지 않으므로 mutex로 보호
static std::mutex GFbxSdkMutex;

UFbxLoader::UFbxLoader()
{
	// 메인 스레드에서 FBX SDK 초기화
	SdkManager = FbxManager::Create();
	FbxIOSettings* IOSettings = FbxIOSettings::Create(SdkManager, IOSROOT);
	SdkManager->SetIOSettings(IOSettings);
}

void UFbxLoader::PreLoad()
{
    UFbxLoader& FbxLoader = GetInstance();

    FWideString WDataDir = UTF8ToWide(GDataDir);
    const fs::path DataDir(WDataDir);

    if (!fs::exists(DataDir) || !fs::is_directory(DataDir))
    {
       UE_LOG("FbxLoader: PreLoad: Data directory not found: %s", GDataDir.c_str());
       return;
    }

    size_t LoadedCount = 0;
    std::unordered_set<FWideString> ProcessedFiles;

    for (const auto& Entry : fs::recursive_directory_iterator(DataDir))
    {
       if (!Entry.is_regular_file())
          continue;

       const fs::path& Path = Entry.path();

       FWideString Extension = Path.extension().wstring();
       std::transform(Extension.begin(), Extension.end(), Extension.begin(), ::towlower);

       if (Extension == L".fbx")
       {
          // 변경 (wstring -> WideToUTF8 -> FString)
          FWideString WPathStr = Path.wstring();
          FString PathStr = NormalizePath(WideToUTF8(WPathStr));

          if (ProcessedFiles.find(WPathStr) == ProcessedFiles.end())
          {
             ProcessedFiles.insert(WPathStr);

             UResourceManager::GetInstance().AsyncLoad<USkeletalMesh>(PathStr, nullptr, EAssetLoadPriority::Low);
             ++LoadedCount;
          }
       }
       else if (Extension == L".dds" || Extension == L".jpg" || Extension == L".png")
       {
          // 텍스처도 비동기 로딩으로 변경
          UResourceManager::GetInstance().AsyncLoad<UTexture>(WideToUTF8(Path.wstring()), nullptr, EAssetLoadPriority::Low);
       }
    }

    // 모든 로드 완료 시 후처리 콜백 등록
    RESOURCE.RegisterOnAllLoadsComplete([]()
    {
       auto& RM = UResourceManager::GetInstance();
       RM.SetSkeletalMeshes();
       RM.SetAnimSequences();

       TArray<USkeletalMesh*> AllSkeletalMeshes = RM.GetAll<USkeletalMesh>();
       TArray<UAnimSequence*> AllAnimSequences = RM.GetAll<UAnimSequence>();

       UE_LOG("FbxLoader: Adding %d animations to %d skeletal meshes", AllAnimSequences.Num(), AllSkeletalMeshes.Num());

       for (UAnimSequence* AnimSeq : AllAnimSequences)
       {
          if (!AnimSeq || AnimSeq->Name.empty())
          {
             continue;
          }

          for (USkeletalMesh* Mesh : AllSkeletalMeshes)
          {
             if (Mesh)
             {
                Mesh->AddAnimation(AnimSeq);
             }
          }
       }

       SaveAllAnimSequencesToAnimFiles();
       PrintAllSkeletonHierarchies();
    });

    UE_LOG("FbxLoader: PreLoad: Queued %zu .fbx files from %s", LoadedCount, GDataDir.c_str());
}

UFbxLoader::~UFbxLoader()
{
	// thread_local SdkManager는 각 스레드 종료 시 자동 정리됨
	// 여기서 Destroy 호출하면 다른 스레드의 SdkManager에 접근할 수 있어 위험
}
UFbxLoader& UFbxLoader::GetInstance()
{
	static UFbxLoader* FbxLoader = nullptr;
	if (!FbxLoader)
	{
		FbxLoader = ObjectFactory::NewObject<UFbxLoader>();
	}
	return *FbxLoader;
}

USkeletalMesh* UFbxLoader::LoadFbxMesh(const FString& FilePath)
{
	// 0) 경로
	FString NormalizedPathStr = NormalizePath(FilePath);

	// 1) 이미 로드된 UStaticMesh가 있는지 전체 검색 (정규화된 경로로 비교)
	for (TObjectIterator<USkeletalMesh> It; It; ++It)
	{
		USkeletalMesh* SkeletalMesh = *It;

		if (SkeletalMesh->GetFilePath() == NormalizedPathStr)
		{
			return SkeletalMesh;
		}
	}

	// 2) 없으면 새로 로드 (정규화된 경로 사용)
	USkeletalMesh* SkeletalMesh = UResourceManager::GetInstance().Load<USkeletalMesh>(NormalizedPathStr);

	UE_LOG("USkeletalMesh(filename: \'%s\') is successfully crated!", NormalizedPathStr.c_str());
	return SkeletalMesh;
}
FSkeletalMeshData* UFbxLoader::LoadFbxMeshAsset(const FString& FilePath)
{
    MaterialInfos.clear();

    // 1) 내부에서는 항상 UTF-8 정규화 경로를 사용
    FString NormalizedPath = NormalizePath(FilePath);

    // 2) 파일 시스템/FBX SDK용 보조 경로
    // - FbxPath: std::filesystem 용 wide path
    // - FbxPathAcp: FBX SDK Initialize()에 넘길 ANSI(MBCS) 경로
    FWideString WNormalizedPath = UTF8ToWide(NormalizedPath);
    std::filesystem::path FbxPath(WNormalizedPath);
    FString FbxPathAcp = UTF8ToACP(NormalizedPath);
	CurrentFbxBaseDir = NormalizePath(WideToUTF8(FbxPath.parent_path().wstring()));

    FSkeletalMeshData* MeshData = nullptr;

#ifdef USE_OBJ_CACHE
    // 3. 캐시 파일 경로 설정 (여기는 UTF-8 문자열로 관리)
    FString CachePathStr = ConvertDataPathToCachePath(NormalizedPath);
    const FString BinPathFileName = CachePathStr + ".bin";

    // 파일 시스템용 wide path
    FWideString WBinPath = UTF8ToWide(BinPathFileName);
    std::filesystem::path BinPath(WBinPath);

    // 캐시를 저장할 디렉토리가 없으면 생성
    std::filesystem::path CacheFileDirPath = BinPath;
    if (CacheFileDirPath.has_parent_path())
    {
        std::filesystem::create_directories(CacheFileDirPath.parent_path());
    }

    bool bLoadedFromCache = false;

    // 4. 캐시 유효성 검사 (FBX 원본 vs 캐시 타임스탬프)
    bool bShouldRegenerate = true;
    if (std::filesystem::exists(BinPath))
    {
        try
        {
            auto binTime = std::filesystem::last_write_time(BinPath);
            auto fbxTime = std::filesystem::last_write_time(FbxPath);

            // FBX 파일이 캐시보다 오래되었으면 캐시 사용
            if (fbxTime <= binTime)
            {
                bShouldRegenerate = false;
            }
        }
        catch (const std::filesystem::filesystem_error& e)
        {
            UE_LOG("Filesystem error during cache validation: %s. Forcing regeneration.", e.what());
            bShouldRegenerate = true;
        }
    }

    // 5. 캐시에서 로드 시도
    if (!bShouldRegenerate)
    {
        UE_LOG("Attempting to load FBX '%s' from cache.", NormalizedPath.c_str());
        try
        {
            MeshData = new FSkeletalMeshData();
            MeshData->PathFileName = NormalizedPath;

            FWindowsBinReader Reader(BinPathFileName);
            if (!Reader.IsOpen())
            {
                throw std::runtime_error("Failed to open bin file for reading.");
            }
            Reader << *MeshData;
            Reader.Close();

            // GroupInfos에 들어있는 InitialMaterialName 기준으로 각 머티리얼을 개별 bin에서 로드
            for (int Index = 0; Index < MeshData->GroupInfos.Num(); Index++)
            {
                if (MeshData->GroupInfos[Index].InitialMaterialName.empty())
                    continue;

                const FString& MaterialName = MeshData->GroupInfos[Index].InitialMaterialName;
                const FString MaterialFilePath = ConvertDataPathToCachePath(MaterialName + ".mat.bin");

                FWindowsBinReader MatReader(MaterialFilePath);
                if (!MatReader.IsOpen())
                {
                    throw std::runtime_error("Failed to open material bin file for reading.");
                }

                FMaterialInfo MaterialInfo{};
                Serialization::ReadAsset<FMaterialInfo>(MatReader, &MaterialInfo);

                UMaterial* NewMaterial = NewObject<UMaterial>();
                UMaterial* Default = UResourceManager::GetInstance().GetDefaultMaterial();

                NewMaterial->SetMaterialInfo(MaterialInfo);
                NewMaterial->SetShader(Default->GetShader());
                NewMaterial->SetShaderMacros(Default->GetShaderMacros());

                // 캐시에서 로드 시 누락되던 부분 해결
                NewMaterial->ResolveTextures();

                UResourceManager::GetInstance().Add<UMaterial>(MaterialInfo.MaterialName, NewMaterial);
            }

            MeshData->CacheFilePath = BinPathFileName;
            bLoadedFromCache = true;

            UE_LOG("Successfully loaded FBX '%s' from cache.", NormalizedPath.c_str());
            return MeshData;
        }
        catch (const std::exception& e)
        {
            UE_LOG("Error loading FBX from cache: %s. Cache might be corrupt or incompatible.", e.what());
            UE_LOG("Deleting corrupt cache and forcing regeneration for '%s'.", NormalizedPath.c_str());

            std::filesystem::remove(BinPath);
            if (MeshData)
            {
                delete MeshData;
                MeshData = nullptr;
            }
            bLoadedFromCache = false;
        }
    }

    // 6. 캐시 로드 실패 시 FBX 파싱
    UE_LOG("Regenerating cache for FBX '%s'...", NormalizedPath.c_str());
#endif // USE_OBJ_CACHE

    // FBX SDK는 스레드 안전하지 않으므로 mutex로 보호
    std::lock_guard<std::mutex> FbxLock(GFbxSdkMutex);

    // 7. FBX 임포터 생성
    FbxImporter* Importer = FbxImporter::Create(SdkManager, "");

    // 8. FBX Importer Initialize
    //    - FBX SDK는 wchar_t 버전이 없으므로,
    //      UTF-8(FString) → UTF-16(FWideString) → ANSI(MBCS, CP_ACP) 문자열로 변환한 경로를 사용
    if (!Importer->Initialize(FbxPathAcp.c_str(), -1, SdkManager->GetIOSettings()))
    {
        UE_LOG("Call to FbxImporter::Initialize() Failed\n");
        UE_LOG("[FbxImporter::Initialize()] Error Reports: %s\n\n", Importer->GetStatus().GetErrorString());
        return nullptr;
    }

    // 9. 씬 생성 및 임포트
    FbxScene* Scene = FbxScene::Create(SdkManager, "My Scene");
    Importer->Import(Scene);
    Importer->Destroy();

    // 10. 좌표계/단위 변환
    FbxAxisSystem UnrealImportAxis(FbxAxisSystem::eZAxis, FbxAxisSystem::eParityEven, FbxAxisSystem::eLeftHanded);
    FbxAxisSystem SourceSetup = Scene->GetGlobalSettings().GetAxisSystem();

    // 단위 변환: FBX의 원본 단위를 확인하고 엔진 단위(m)로 변환
    FbxSystemUnit SceneSystemUnit = Scene->GetGlobalSettings().GetSystemUnit();
    if (SceneSystemUnit != FbxSystemUnit::m)
    {
        // 원본 단위를 미터로 변환
        FbxSystemUnit::m.ConvertScene(Scene);
        UE_LOG("FBX 단위 변환: %s -> m", SceneSystemUnit.GetScaleFactorAsString().Buffer());
    }

    if (SourceSetup != UnrealImportAxis)
    {
        UE_LOG("Fbx 축 변환 완료!\n");
        UnrealImportAxis.DeepConvertScene(Scene);
    }

    // 11. 삼각화
    FbxGeometryConverter IGeometryConverter(SdkManager);
    if (IGeometryConverter.Triangulate(Scene, true))
    {
        UE_LOG("Fbx 씬 삼각화 완료\n");
    }
    else
    {
        UE_LOG("Fbx 씬 삼각화가 실패했습니다, 매시가 깨질 수 있습니다\n");
    }

    // 12. 스켈레탈 메쉬 데이터 생성
    MeshData = new FSkeletalMeshData();
    MeshData->PathFileName = NormalizedPath;

    // Skeleton Name을 FBX 파일명에서 추출
    FWideString WPath = UTF8ToWide(NormalizedPath);
    std::filesystem::path FbxFilePath(WPath);
    MeshData->Skeleton.Name = WideToUTF8(FbxFilePath.stem().wstring());

    FbxNode* RootNode = Scene->GetRootNode();

    TMap<FbxNode*, int32> BoneToIndex;
    TMap<FbxSurfaceMaterial*, int32> MaterialToIndex;
    TMap<int32, TArray<uint32>> MaterialGroupIndexList;

    // 머티리얼이 없는 경우용 기본 그룹/인덱스 준비
    MaterialGroupIndexList.Add(0, TArray<uint32>());
    MaterialToIndex.Add(nullptr, 0);
    MeshData->GroupInfos.Add(FGroupInfo());

    if (RootNode)
    {
        // 1패스: 스켈레톤(뼈 계층) 로드
        for (int Index = 0; Index < RootNode->GetChildCount(); Index++)
        {
            LoadSkeletonFromNode(RootNode->GetChild(Index), *MeshData, -1, BoneToIndex);
        }

        // 2패스: 실제 메쉬와 스킨 정보 로드
        for (int Index = 0; Index < RootNode->GetChildCount(); Index++)
        {
            LoadMeshFromNode(RootNode->GetChild(Index), *MeshData, MaterialGroupIndexList, BoneToIndex, MaterialToIndex);
        }

        // 여러 루트 본이 있으면 가상 루트 생성
        EnsureSingleRootBone(*MeshData);
    }

    // 머티리얼 플래그 설정
    if (MeshData->GroupInfos.Num() > 1)
    {
        MeshData->bHasMaterial = true;
    }

    // 머티리얼 그룹별 인덱스를 최종 인덱스 버퍼로 합치기
    UE_LOG("[FBXLoader] MaterialGroupIndexList has %d entries, GroupInfos has %d entries\n",
           (int)MaterialGroupIndexList.Num(), MeshData->GroupInfos.Num());

    uint32 Count = 0;
    for (auto& Element : MaterialGroupIndexList)
    {
        int32 MaterialIndex = Element.first;
        const TArray<uint32>& IndexList = Element.second;

        UE_LOG("[FBXLoader] Processing MaterialIndex=%d, IndexList.Num()=%d, bEnableCloth=%d\n",
               MaterialIndex, IndexList.Num(),
               (MaterialIndex < MeshData->GroupInfos.Num()) ? (MeshData->GroupInfos[MaterialIndex].bEnableCloth ? 1 : 0) : -1);

        MeshData->Indices.Append(IndexList);

        MeshData->GroupInfos[MaterialIndex].StartIndex = Count;
        MeshData->GroupInfos[MaterialIndex].IndexCount = IndexList.Num();
        Count += IndexList.Num();
    }

    // 최종 GroupInfos 상태 출력
    for (int32 i = 0; i < MeshData->GroupInfos.Num(); ++i)
    {
        const auto& G = MeshData->GroupInfos[i];
        UE_LOG("[FBXLoader] Final GroupInfos[%d]: bEnableCloth=%d, StartIndex=%u, IndexCount=%u, Name=%s\n",
               i, G.bEnableCloth ? 1 : 0, G.StartIndex, G.IndexCount, G.InitialMaterialName.c_str());
    }

#ifdef USE_OBJ_CACHE
    // 13. 캐시 저장
    try
    {
        FWindowsBinWriter Writer(BinPathFileName);
        Writer << *MeshData;
        Writer.Close();

        for (FMaterialInfo& MaterialInfo : MaterialInfos)
        {
            const FString MaterialFilePath = ConvertDataPathToCachePath(MaterialInfo.MaterialName + ".mat.bin");
            FWindowsBinWriter MatWriter(MaterialFilePath);
            Serialization::WriteAsset<FMaterialInfo>(MatWriter, &MaterialInfo);
            MatWriter.Close();
        }

        MeshData->CacheFilePath = BinPathFileName;

        UE_LOG("Cache regeneration complete for FBX '%s'.", NormalizedPath.c_str());
    }
    catch (const std::exception& e)
    {
        UE_LOG("Failed to save FBX cache: %s", e.what());
    }
#endif // USE_OBJ_CACHE

    return MeshData;
}


void UFbxLoader::LoadMeshFromNode(FbxNode* InNode,
	FSkeletalMeshData& MeshData,
	TMap<int32, TArray<uint32>>& MaterialGroupIndexList,
	TMap<FbxNode*, int32>& BoneToIndex,
	TMap<FbxSurfaceMaterial*, int32>& MaterialToIndex)
{

	// 부모노드로부터 상대좌표 리턴
	/*FbxDouble3 Translation = InNode->LclTranslation.Get();
	FbxDouble3 Rotation = InNode->LclRotation.Get();
	FbxDouble3 Scaling  = InNode->LclScaling.Get();*/

	// 최적화, 메시 로드 전에 미리 머티리얼로부터 인덱스를 해시맵을 이용해서 얻고 그걸 TArray에 저장하면 됨.
	// 노드의 머티리얼 리스트는 슬롯으로 참조함(내가 정한 MaterialIndex는 슬롯과 다름), 슬롯에 대응하는 머티리얼 인덱스를 캐싱하는 것
	// 그럼 폴리곤 순회하면서 해싱할 필요가 없음
	TArray<int32> MaterialSlotToIndex;
	// Attribute 참조 함수
	for (int Index = 0; Index < InNode->GetNodeAttributeCount(); Index++)
	{
		FbxNodeAttribute* Attribute = InNode->GetNodeAttributeByIndex(Index);
		if (!Attribute)
		{
			continue;
		}

		if (Attribute->GetAttributeType() == FbxNodeAttribute::eMesh)
		{
			FbxMesh* Mesh = (FbxMesh*)Attribute;
			// 위의 MaterialSlotToIndex는 MaterialToIndex 해싱을 안 하기 위함이고, MaterialGroupIndexList도 머티리얼이 없거나 1개만 쓰는 경우 해싱을 피할 수 있음.
			// 이를 위한 최적화 코드를 작성함.
			FString NodeName = InNode->GetName();
			std::transform(NodeName.begin(), NodeName.end(), NodeName.begin(), ::tolower);

			bool bNodeIsCloth =
				(NodeName.find("cloak") != std::string::npos) ||
				(NodeName.find("skirt") != std::string::npos) ||
				(NodeName.find("cape") != std::string::npos) ||
				(NodeName.find("cloth") != std::string::npos);

			// Cloth는 따로 Grouping
			if (bNodeIsCloth)
			{
				int32 ClothGroupIndex = MeshData.GroupInfos.Num();

				FGroupInfo NewClothGroup;
				NewClothGroup.InitialMaterialName = "Cloth_" + NodeName;
				NewClothGroup.bEnableCloth = true;
				NewClothGroup.StartIndex = 0;
				NewClothGroup.IndexCount = 0;

				UE_LOG("[FBXLoader] Detected cloth node '%s' at GroupIndex %d\n", NodeName.c_str(), ClothGroupIndex);

				// Cloth 노드에서도 머티리얼 파싱 (텍스처 추출을 위해)
				for (int MaterialSlot = 0; MaterialSlot < InNode->GetMaterialCount(); MaterialSlot++)
				{
					FbxSurfaceMaterial* Material = InNode->GetMaterial(MaterialSlot);
					if (Material && !MaterialToIndex.Contains(Material))
					{
						FMaterialInfo MaterialInfo{};
						ParseMaterial(Material, MaterialInfo);
						// MaterialInfos에 추가 (캐시 저장 시 텍스처 정보도 저장됨)
						MaterialInfos.push_back(MaterialInfo);
						MaterialToIndex.Add(Material, MaterialToIndex.Num());
						UE_LOG("[FBXLoader] Parsed cloth material: %s\n", MaterialInfo.MaterialName.c_str());
					}
				}

				MeshData.GroupInfos.Add(NewClothGroup);
				LoadMesh(Mesh, MeshData, MaterialGroupIndexList, BoneToIndex, MaterialSlotToIndex, ClothGroupIndex);

				// 로드 후 IndexCount 확인
				UE_LOG("[FBXLoader] After LoadMesh - ClothGroup[%d]: StartIndex=%u, IndexCount=%u\n",
					   ClothGroupIndex, MeshData.GroupInfos[ClothGroupIndex].StartIndex, MeshData.GroupInfos[ClothGroupIndex].IndexCount);

				continue;
			}

			// 0번이 기본 머티리얼이고 1번 이상은 블렌딩 머티리얼이라고 함. 근데 엄청 고급 기능이라서 일반적인 로더는 0번만 쓴다고 함.
			if (Mesh->GetElementMaterialCount() > 0)
			{
				// 머티리얼 슬롯 인덱싱 해주는 클래스 (ex. materialElement->GetIndexArray() : 폴리곤마다 어떤 머티리얼 슬롯을 쓰는지 Array로 표현)
				FbxGeometryElementMaterial* MaterialElement = Mesh->GetElementMaterial(0);
				// 머티리얼이 폴리곤 단위로 매핑함 -> 모든 폴리곤이 같은 머티리얼을 쓰지 않음(같은 머티리얼을 쓰는 경우 = eAllSame)
				// MaterialCount랑은 전혀 다른 동작임(슬롯이 2개 이상 있어도 매핑 모드가 eAllSame이라서 머티리얼을 하나만 쓰는 경우가 있음)
				if (MaterialElement->GetMappingMode() == FbxGeometryElement::eByPolygon)
				{
					for (int MaterialSlot = 0; MaterialSlot < InNode->GetMaterialCount(); MaterialSlot++)
					{
						int MaterialIndex = 0;
						FbxSurfaceMaterial* Material = InNode->GetMaterial(MaterialSlot);
						if (MaterialToIndex.Contains(Material))
						{
							MaterialIndex = MaterialToIndex[Material];
						}
						else
						{
							FMaterialInfo MaterialInfo{};
							ParseMaterial(Material, MaterialInfo);
							// 새로운 머티리얼, 머티리얼 인덱스 설정
							MaterialIndex = MaterialToIndex.Num();
							MaterialToIndex.Add(Material, MaterialIndex);
							MeshData.GroupInfos.Add(FGroupInfo());
							MeshData.GroupInfos[MaterialIndex].InitialMaterialName = MaterialInfo.MaterialName;

							// material에 cloth관련 이름이 있다면 따로 처리 고려   
							std::string MatNameStr = MaterialInfo.MaterialName;  // FString -> std::string
							std::string LowerMatName = MatNameStr;
							std::transform(LowerMatName.begin(), LowerMatName.end(), LowerMatName.begin(), ::tolower); 
						}
						// MaterialSlot에 대응하는 전역 MaterialIndex 저장
						MaterialSlotToIndex.Add(MaterialIndex);
					}
				}
				// 노드가 하나의 머티리얼만 쓰는 경우
				else if (MaterialElement->GetMappingMode() == FbxGeometryElement::eAllSame)
				{
					int MaterialIndex = 0;
					int MaterialSlot = MaterialElement->GetIndexArray().GetAt(0);
					FbxSurfaceMaterial* Material = InNode->GetMaterial(MaterialSlot);
					if (MaterialToIndex.Contains(Material))
					{
						MaterialIndex = MaterialToIndex[Material];
					}
					else
					{
						FMaterialInfo MaterialInfo{};
						ParseMaterial(Material, MaterialInfo);
						// 새로운 머티리얼, 머티리얼 인덱스 설정
						MaterialIndex = MaterialToIndex.Num();

						MaterialToIndex.Add(Material, MaterialIndex);
						MeshData.GroupInfos.Add(FGroupInfo());
						MeshData.GroupInfos[MaterialIndex].InitialMaterialName = MaterialInfo.MaterialName;

						// material에 cloth관련 이름이 있다면 따로 처리 고려 (eAllSame 케이스)
						std::string MatNameStr = MaterialInfo.MaterialName;  // FString -> std::string
						std::string LowerMatName = MatNameStr;
						std::transform(LowerMatName.begin(), LowerMatName.end(), LowerMatName.begin(), ::tolower); 
					}
					// MaterialSlotToIndex에 추가할 필요 없음(머티리얼 하나일때 해싱 패스하고 Material Index로 바로 그룹핑 할 거라서 안 씀)
					LoadMesh(Mesh, MeshData, MaterialGroupIndexList, BoneToIndex, MaterialSlotToIndex, MaterialIndex);
					continue;
				}
			}

			LoadMesh(Mesh, MeshData, MaterialGroupIndexList, BoneToIndex, MaterialSlotToIndex);
		}
	}

	for (int Index = 0; Index < InNode->GetChildCount(); Index++)
	{
		LoadMeshFromNode(InNode->GetChild(Index), MeshData, MaterialGroupIndexList, BoneToIndex, MaterialToIndex);
	}
}

// Skeleton은 계층구조까지 표현해야하므로 깊이 우선 탐색, ParentNodeIndex 명시.
void UFbxLoader::LoadSkeletonFromNode(FbxNode* InNode, FSkeletalMeshData& MeshData, int32 ParentNodeIndex, TMap<FbxNode*, int32>& BoneToIndex)
{
	int32 BoneIndex = ParentNodeIndex;
	for (int Index = 0; Index < InNode->GetNodeAttributeCount(); Index++)
	{

		FbxNodeAttribute* Attribute = InNode->GetNodeAttributeByIndex(Index);
		if (!Attribute)
		{
			continue;
		}

		if (Attribute->GetAttributeType() == FbxNodeAttribute::eSkeleton)
		{
			FBone BoneInfo{};

			// Mixamo 접두사 제거 (mixamorig:, mixamorig9: 등)
			BoneInfo.Name = FMixamoChainMapper::RemovePrefix(FString(InNode->GetName()));

			BoneInfo.ParentIndex = ParentNodeIndex;

			// 뼈 리스트에 추가
			MeshData.Skeleton.Bones.Add(BoneInfo);

			// 뼈 인덱스 우리가 정해줌(방금 추가한 마지막 원소)
			BoneIndex = MeshData.Skeleton.Bones.Num() - 1;

			// 뼈 이름으로 인덱스 서치 가능하게 함.
			MeshData.Skeleton.BoneNameToIndex.Add(BoneInfo.Name, BoneIndex);

			// 매시 로드할때 써야되서 맵에 인덱스 저장
			BoneToIndex.Add(InNode, BoneIndex);
			// 뼈가 노드 하나에 여러개 있는 경우는 없음. 말이 안되는 상황임.
			break;
		}
	}
	for (int Index = 0; Index < InNode->GetChildCount(); Index++)
	{
		// 깊이 우선 탐색 부모 인덱스 설정(InNOde가 eSkeleton이 아니면 기존 부모 인덱스가 넘어감(BoneIndex = ParentNodeIndex)
		LoadSkeletonFromNode(InNode->GetChild(Index), MeshData, BoneIndex, BoneToIndex);
	}
}

// 예시 코드
void UFbxLoader::LoadMeshFromAttribute(FbxNodeAttribute* InAttribute, FSkeletalMeshData& MeshData)
{

	/*if (!InAttribute)
	{
		return;
	}*/
	//FbxString TypeName = GetAttributeTypeName(InAttribute);
	// 타입과 별개로 Element 자체의 이름도 있음
	//FbxString AttributeName = InAttribute->GetName();

	// Buffer함수로 FbxString->char* 변환
	//UE_LOG("<Attribute Type = %s, Name = %s\n", TypeName.Buffer(), AttributeName.Buffer());
}

void UFbxLoader::LoadMesh(FbxMesh* InMesh, FSkeletalMeshData& MeshData, TMap<int32, TArray<uint32>>& MaterialGroupIndexList, TMap<FbxNode*, int32>& BoneToIndex, TArray<int32> MaterialSlotToIndex, int32 DefaultMaterialIndex)
{
	// 위에서 뼈 인덱스를 구했으므로 일단 ControlPoint에 대응되는 뼈 인덱스와 가중치부터 할당할 것임(이후 MeshData를 채우면서 ControlPoint를 순회할 것이므로)
	struct IndexWeight
	{
		uint32 BoneIndex;
		float BoneWeight;
	};
	// ControlPoint에 대응하는 뼈 인덱스, 가중치를 저장하는 맵
	// ControlPoint에 대응하는 뼈가 여러개일 수 있으므로 TArray로 저장
	TMap<int32, TArray<IndexWeight>> ControlPointToBoneWeight;
	// 메시 로컬 좌표계를 Fbx Scene World 좌표계로 바꿔주는 행렬
	FbxAMatrix FbxSceneWorld{};
	// 역전치(노말용)
	FbxAMatrix FbxSceneWorldInverseTranspose{};

	// Deformer: 매시의 모양을 변형시키는 모든 기능, ex) skin, blendShape(모프 타겟, 두 표정 미리 만들고 블랜딩해서 서서히 변화시킴)
	// 99.9퍼센트는 스킨이 하나만 있고 완전 복잡한 얼굴 표정을 표현하기 위해서 2개 이상을 쓰기도 하는데 0번만 쓰도록 해도 문제 없음(AAA급 게임에서 2개 이상을 처리함)
	// 2개 이상의 스킨이 들어가면 뼈 인덱스가 16개까지도 늘어남.
	bool bHasSkinDeformer = InMesh->GetDeformerCount(FbxDeformer::eSkin) > 0;
	if (bHasSkinDeformer)
	{
		// 클러스터: 뼈라고 봐도 됨(뼈 정보와(Bind Pose 행렬) 그 뼈가 영향을 주는 정점, 가중치 저장)
		for (int Index = 0; Index < ((FbxSkin*)InMesh->GetDeformer(0, FbxDeformer::eSkin))->GetClusterCount(); Index++)
		{
			FbxCluster* Cluster = ((FbxSkin*)InMesh->GetDeformer(0, FbxDeformer::eSkin))->GetCluster(Index);

			if (Index == 0)
			{
				// 클러스터를 담고 있는 Node의(Mesh) Fbx Scene World Transform, 한 번만 구해도 되고
				// 정점을 Fbx Scene World 좌표계로 저장하기 위해 사용(아티스트 의도를 그대로 반영 가능, 서브메시를 단일메시로 처리 가능)
				// 모든 SkeletalMesh는 Scene World 원점을 기준으로 제작되어야함
				Cluster->GetTransformMatrix(FbxSceneWorld);
				FbxSceneWorldInverseTranspose = FbxSceneWorld.Inverse().Transpose();
			}
			int IndexCount = Cluster->GetControlPointIndicesCount();
			// 클러스터가 영향을 주는 ControlPointIndex를 구함.
			int* Indices = Cluster->GetControlPointIndices();
			double* Weights = Cluster->GetControlPointWeights();
            // Bind Pose, Inverse Bind Pose 저장.
            // Fbx 스킨 규약:
            //  - TransformLinkMatrix: 본의 글로벌 바인드 행렬
            //  - TransformMatrix:     메시의 글로벌 바인드 행렬
            // 엔진 로컬(메시 기준) 바인드 행렬 = Inv(TransformMatrix) * TransformLinkMatrix
            FbxAMatrix BoneBindGlobal;
            Cluster->GetTransformLinkMatrix(BoneBindGlobal);
            FbxAMatrix BoneBindGlobalInv = BoneBindGlobal.Inverse();
            // FbxMatrix는 128바이트, FMatrix는 64바이트라서 memcpy쓰면 안 됨. 원소 단위로 복사합니다.
            for (int Row = 0; Row < 4; Row++)
            {
                for (int Col = 0; Col < 4; Col++)
                {
                    MeshData.Skeleton.Bones[BoneToIndex[Cluster->GetLink()]].BindPose.M[Row][Col] = static_cast<float>(BoneBindGlobal[Row][Col]);
                    MeshData.Skeleton.Bones[BoneToIndex[Cluster->GetLink()]].InverseBindPose.M[Row][Col] = static_cast<float>(BoneBindGlobalInv[Row][Col]);
                }
            }


			for (int ControlPointIndex = 0; ControlPointIndex < IndexCount; ControlPointIndex++)
			{
				// GetLink -> 아까 저장한 노드 To Index맵의 노드 (Cluster에 대응되는 뼈 인덱스를 ControlPointIndex에 대응시키는 과정)
				// ControlPointIndex = 클러스터가 저장하는 ControlPointIndex 배열에 대한 Index
				TArray<IndexWeight>& IndexWeightArray = ControlPointToBoneWeight[Indices[ControlPointIndex]];
				IndexWeightArray.Add(IndexWeight(BoneToIndex[Cluster->GetLink()], static_cast<float>(Weights[ControlPointIndex])));
			}
		}
	}
	else
	{
		// 스킨 디포머가 없는 경우 (예: 언리얼 엔진에서 내보낸 FBX 등)
		// 메시 노드의 글로벌 트랜스폼을 사용
		FbxNode* MeshNode = InMesh->GetNode();
		if (MeshNode)
		{
			FbxSceneWorld = MeshNode->EvaluateGlobalTransform();
			FbxSceneWorldInverseTranspose = FbxSceneWorld.Inverse().Transpose();
			UE_LOG("UFbxLoader::LoadMesh: No skin deformer found, using mesh node global transform");
		}
		else
		{
			// 노드가 없는 경우 항등 행렬 사용
			FbxSceneWorld.SetIdentity();
			FbxSceneWorldInverseTranspose.SetIdentity();
			UE_LOG("UFbxLoader::LoadMesh: WARNING - No skin deformer and no mesh node, using identity transform");
		}
	}

	bool bIsUniformScale = false;
	const FbxVector4& ScaleOfSceneWorld = FbxSceneWorld.GetS();
	// 비균등 스케일일 경우 그람슈미트 이용해서 탄젠트 재계산
	bIsUniformScale = ((FMath::Abs(ScaleOfSceneWorld[0] - ScaleOfSceneWorld[1]) < 0.001f) &&
		(FMath::Abs(ScaleOfSceneWorld[0] - ScaleOfSceneWorld[2]) < 0.001f));


	// 로드는 TriangleList를 가정하고 할 것임.
	// TriangleStrip은 한번 만들면 편집이 사실상 불가능함, Fbx같은 호환성이 중요한 모델링 포멧이 유연성 부족한 모델을 저장할 이유도 없고
	// 엔진 최적화 측면에서도 GPU의 Vertex Cache가 Strip과 비슷한 성능을 내면서도 직관적이고 유연해서 잘 쓰지도 않기 때문에 그냥 안 씀.
	int PolygonCount = InMesh->GetPolygonCount();

	// ControlPoints는 정점의 위치 정보를 배열로 저장함, Vertex마다 ControlIndex로 참조함.
	FbxVector4* ControlPoints = InMesh->GetControlPoints();


	// Vertex 위치가 같아도 서로 다른 Normal, Tangent, UV좌표를 가질 수 있음, Fbx는 하나의 인덱스 배열에서 이들을 서로 다른 인덱스로 관리하길 강제하지 않고
	// Vertex 위치는 ControlPoint로 관리하고 그 외의 정보들은 선택적으로 분리해서 관리하도록 함. 그래서 ControlPoint를 Index로 쓸 수도 없어서 따로 만들어야 하고,
	// 위치정보 외의 정보를 참조할때는 매핑 방식별로 분기해서 저장해야함. 만약 매핑 방식이 eByPolygonVertex(꼭짓점 기준)인 경우 폴리곤의 꼭짓점을 순회하는 순서
	// 그대로 참조하면 됨, 그래서 VertexId를 꼭짓점 순회하는 순서대로 증가시키면서 매핑할 것임.
	int VertexId = 0;

	// 위의 이유로 ControlPoint를 인덱스 버퍼로 쓸 수가 없어서 Vertex마다 대응되는 Index Map을 따로 만들어서 계산할 것임.
	TMap<FSkinnedVertex, uint32> IndexMap;


	for (int PolygonIndex = 0; PolygonIndex < PolygonCount; PolygonIndex++)
	{
		// 최종적으로 사용할 머티리얼 인덱스를 구함, MaterialIndex 기본값이 0이므로 없는 경우 처리됨, 머티리얼이 하나일때 materialIndex가 1 이상이므로 처리됨.
		// 머티리얼이 여러개일때만 처리해주면 됌.

		// 머티리얼이 여러개인 경우(머티리얼이 하나 이상 있는데 materialIndex가 0이면 여러개, 하나일때는 MaterialIndex를 설정해주니까)
		// 이때는 해싱을 해줘야함
		int32 MaterialIndex = DefaultMaterialIndex;
		if (DefaultMaterialIndex == 0 && InMesh->GetElementMaterialCount() > 0)
		{
			FbxGeometryElementMaterial* Material = InMesh->GetElementMaterial(0);
			int MaterialSlot = Material->GetIndexArray().GetAt(PolygonIndex);
			MaterialIndex = MaterialSlotToIndex[MaterialSlot];
		}

		// 하나의 Polygon 내에서의 VertexIndex, PolygonSize가 다를 수 있지만 위에서 삼각화를 해줬기 때문에 무조건 3임
		for (int VertexIndex = 0; VertexIndex < InMesh->GetPolygonSize(PolygonIndex); VertexIndex++)
		{
			FSkinnedVertex SkinnedVertex{};
			// 폴리곤 인덱스와 폴리곤 내에서의 vertexIndex로 ControlPointIndex 얻어냄
			int ControlPointIndex = InMesh->GetPolygonVertex(PolygonIndex, VertexIndex);

			const FbxVector4& Position = FbxSceneWorld.MultT(ControlPoints[ControlPointIndex]);
			//const FbxVector4& Position = ControlPoints[ControlPointIndex];
			SkinnedVertex.Position = FVector(static_cast<float>(Position.mData[0]), static_cast<float>(Position.mData[1]), static_cast<float>(Position.mData[2]));


			if (ControlPointToBoneWeight.Contains(ControlPointIndex))
			{
				double TotalWeights = 0.0;


				const TArray<IndexWeight>& WeightArray = ControlPointToBoneWeight[ControlPointIndex];
				for (int BoneIndex = 0; BoneIndex < WeightArray.Num() && BoneIndex < 4; BoneIndex++)
				{
					// Total weight 구하기(정규화)
					TotalWeights += ControlPointToBoneWeight[ControlPointIndex][BoneIndex].BoneWeight;
				}
				// 5개 이상이 있어도 4개만 처리할 것임.
				for (int BoneIndex = 0; BoneIndex < WeightArray.Num() && BoneIndex < 4; BoneIndex++)
				{
					// ControlPoint에 대응하는 뼈 인덱스, 가중치를 모두 저장
					SkinnedVertex.BoneIndices[BoneIndex] = ControlPointToBoneWeight[ControlPointIndex][BoneIndex].BoneIndex;
					SkinnedVertex.BoneWeights[BoneIndex] = static_cast<float>(ControlPointToBoneWeight[ControlPointIndex][BoneIndex].BoneWeight/TotalWeights);
				}
			}


			// 함수명과 다르게 매시가 가진 버텍스 컬러 레이어 개수를 리턴함.( 0번 : Diffuse, 1번 : 블랜딩 마스크 , 기타..)
			// 엔진에서는 항상 0번만 사용하거나 Count가 0임. 그래서 하나라도 있으면 그냥 0번 쓰게 함.
			// 왜 이렇게 지어졌나? -> Fbx가 3D 모델링 관점에서 만들어졌기 때문, 모델링 툴에서는 여러 개의 컬러 레이어를 하나에 매시에 만들 수 있음.
			// 컬러 뿐만 아니라 UV Normal Tangent 모두 다 레이어로 저장하고 모두 다 0번만 쓰면 됨.
			if (InMesh->GetElementVertexColorCount() > 0)
			{
				// 왜 FbxLayerElement를 안 쓰지? -> 구버전 API
				FbxGeometryElementVertexColor* VertexColors = InMesh->GetElementVertexColor(0);
				int MappingIndex;
				// 확장성을 고려하여 switch를 씀, ControlPoint와 PolygonVertex말고 다른 모드들도 있음.
				switch (VertexColors->GetMappingMode())
				{
				case FbxGeometryElement::eByPolygon: //다른 모드 예시
				case FbxGeometryElement::eAllSame:
				case FbxGeometryElement::eNone:
				default:
					break;
					// 가장 단순한 경우, 그냥 ControlPoint(Vertex의 위치)마다 하나의 컬러값을 저장.
				case FbxGeometryElement::eByControlPoint:
					MappingIndex = ControlPointIndex;
					break;
					// 꼭짓점마다 컬러가 저장된 경우(같은 위치여도 다른 컬러 저장 가능), 위와 같지만 꼭짓점마다 할당되는 VertexId를 씀.
				case FbxGeometryElement::eByPolygonVertex:
					MappingIndex = VertexId;
					break;
				}

				// 매핑 방식에 더해서, 실제로 그 ControlPoint에서 어떻게 참조할 것인지가 다를 수 있음.(데이터 압축때문에 필요, IndexBuffer를 쓰는 것과 비슷함)
				switch (VertexColors->GetReferenceMode())
				{
					// 인덱스 자체가 데이터 배열의 인덱스인 경우(중복이 생길 수 있음)
				case FbxGeometryElement::eDirect:
				{
					// 바로 참조 가능.
					const FbxColor& Color = VertexColors->GetDirectArray().GetAt(MappingIndex);
					SkinnedVertex.Color = FVector4(static_cast<float>(Color.mRed), static_cast<float>(Color.mGreen), static_cast<float>(Color.mBlue), static_cast<float>(Color.mAlpha));
				}
				break;
				//인덱스 배열로 간접참조해야함
				case FbxGeometryElement::eIndexToDirect:
				{
					int Id = VertexColors->GetIndexArray().GetAt(MappingIndex);
					const FbxColor& Color = VertexColors->GetDirectArray().GetAt(Id);
					SkinnedVertex.Color = FVector4(static_cast<float>(Color.mRed), static_cast<float>(Color.mGreen), static_cast<float>(Color.mBlue), static_cast<float>(Color.mAlpha));
				}
				break;
				//외의 경우는 일단 배제
				default:
					break;
				}
			}

			if (InMesh->GetElementNormalCount() > 0)
			{
				FbxGeometryElementNormal* Normals = InMesh->GetElementNormal(0);

				// 각진 모서리 표현력 때문에 99퍼센트의 모델은 eByPolygonVertex를 쓴다고 함.
				// 근데 구 같이 각진 모서리가 아예 없는 경우, 부드러운 셰이딩 모델을 익스포트해서 eControlPoint로 저장될 수도 있음
				int MappingIndex;

				switch (Normals->GetMappingMode())
				{
				case FbxGeometryElement::eByControlPoint:
					MappingIndex = ControlPointIndex;
					break;
				case FbxGeometryElement::eByPolygonVertex:
					MappingIndex = VertexId;
					break;
				default:
					break;
				}

				switch (Normals->GetReferenceMode())
				{
				case FbxGeometryElement::eDirect:
				{
					const FbxVector4& Normal = Normals->GetDirectArray().GetAt(MappingIndex);
					FbxVector4 NormalWorld = FbxSceneWorldInverseTranspose.MultT(FbxVector4(Normal.mData[0], Normal.mData[1], Normal.mData[2], 0.0f));
					SkinnedVertex.Normal = FVector(static_cast<float>(NormalWorld.mData[0]), static_cast<float>(NormalWorld.mData[1]), static_cast<float>(NormalWorld.mData[2]));
				}
				break;
				case FbxGeometryElement::eIndexToDirect:
				{
					int Id = Normals->GetIndexArray().GetAt(MappingIndex);
					const FbxVector4& Normal = Normals->GetDirectArray().GetAt(Id);
					FbxVector4 NormalWorld = FbxSceneWorldInverseTranspose.MultT(FbxVector4(Normal.mData[0], Normal.mData[1], Normal.mData[2], 0.0f));
					SkinnedVertex.Normal = FVector(static_cast<float>(NormalWorld.mData[0]), static_cast<float>(NormalWorld.mData[1]), static_cast<float>(NormalWorld.mData[2]));
				}
				break;
				default:
					break;
				}
			}

			if (InMesh->GetElementTangentCount() > 0)
			{
				FbxGeometryElementTangent* Tangents = InMesh->GetElementTangent(0);

				// 왜 Color에서 계산한 Mapping Index를 안 쓰지? -> 컬러, 탄젠트, 노말, UV 모두 다 다른 매핑 방식을 사용 가능함.
				int MappingIndex;

				switch (Tangents->GetMappingMode())
				{
				case FbxGeometryElement::eByControlPoint:
					MappingIndex = ControlPointIndex;
					break;
				case FbxGeometryElement::eByPolygonVertex:
					MappingIndex = VertexId;
					break;
				default:
					break;
				}

				switch (Tangents->GetReferenceMode())
				{
				case FbxGeometryElement::eDirect:
				{
					const FbxVector4& Tangent = Tangents->GetDirectArray().GetAt(MappingIndex);
					FbxVector4 TangentWorld = FbxSceneWorld.MultT(FbxVector4(Tangent.mData[0], Tangent.mData[1], Tangent.mData[2], 0.0f));
					SkinnedVertex.Tangent = FVector4(static_cast<float>(TangentWorld.mData[0]), static_cast<float>(TangentWorld.mData[1]), static_cast<float>(TangentWorld.mData[2]), static_cast<float>(Tangent.mData[3]));
				}
				break;
				case FbxGeometryElement::eIndexToDirect:
				{
					int Id = Tangents->GetIndexArray().GetAt(MappingIndex);
					const FbxVector4& Tangent = Tangents->GetDirectArray().GetAt(Id);
					FbxVector4 TangentWorld = FbxSceneWorld.MultT(FbxVector4(Tangent.mData[0], Tangent.mData[1], Tangent.mData[2], 0.0f));
					SkinnedVertex.Tangent = FVector4(static_cast<float>(TangentWorld.mData[0]), static_cast<float>(TangentWorld.mData[1]), static_cast<float>(TangentWorld.mData[2]), static_cast<float>(Tangent.mData[3]));
				}
				break;
				default:
					break;
				}

				// 유니폼 스케일이 아니므로 그람슈미트, 노말이 필요하므로 노말 이후에 탄젠트 계산해야함
				if (!bIsUniformScale)
				{
					FVector Tangent = FVector(SkinnedVertex.Tangent.X, SkinnedVertex.Tangent.Y, SkinnedVertex.Tangent.Z);
					float Handedness = SkinnedVertex.Tangent.W;
					const FVector& Normal = SkinnedVertex.Normal;

					float TangentToNormalDir = FVector::Dot(Tangent, Normal);

					Tangent = Tangent - Normal * TangentToNormalDir;
					Tangent.Normalize();
					SkinnedVertex.Tangent = FVector4(Tangent.X, Tangent.Y, Tangent.Z, Handedness);
				}

			}

			// UV는 매핑 방식이 위와 다름(eByPolygonVertex에서 VertexId를 안 쓰고 TextureUvIndex를 씀, 참조방식도 위와 다름.)
			// 이유 : 3D 모델의 부드러운 면에 2D 텍스처 매핑을 위해 제봉선(가상의)을 만드는 경우가 생김, 그때 하나의 VertexId가 그 제봉선을 경계로
			//		  서로 다른 uv 좌표를 가져야 할 때가 생김. 그냥 VertexId를 더 나누면 안되나? => 아티스트가 싫어하고 직관적이지도 않음, 실제로
			//		  물리적으로 폴리곤이 찢어진 게 아닌데 텍스처를 입히겠다고 Vertex를 새로 만들고 폴리곤을 찢어야 함.
			//		  그래서 UV는 인덱싱을 나머지와 다르게함
			if (InMesh->GetElementUVCount() > 0)
			{
				FbxGeometryElementUV* UVs = InMesh->GetElementUV(0);

				switch (UVs->GetMappingMode())
				{
				case FbxGeometryElement::eByControlPoint:
					switch (UVs->GetReferenceMode())
					{
					case FbxGeometryElement::eDirect:
					{
						const FbxVector2& UV = UVs->GetDirectArray().GetAt(ControlPointIndex);
						SkinnedVertex.UV = FVector2D(static_cast<float>(UV.mData[0]), static_cast<float>(1 - UV.mData[1]));
					}
					break;
					case FbxGeometryElement::eIndexToDirect:
					{
						int Id = UVs->GetIndexArray().GetAt(ControlPointIndex);
						const FbxVector2& UV = UVs->GetDirectArray().GetAt(Id);
						SkinnedVertex.UV = FVector2D(static_cast<float>(UV.mData[0]), static_cast<float>(1 - UV.mData[1]));
					}
					break;
					default:
						break;
					}
					break;
				case FbxGeometryElement::eByPolygonVertex:
				{
					int TextureUvIndex = InMesh->GetTextureUVIndex(PolygonIndex, VertexIndex);
					switch (UVs->GetReferenceMode())
					{
					case FbxGeometryElement::eDirect:
					case FbxGeometryElement::eIndexToDirect:
					{
						const FbxVector2& UV = UVs->GetDirectArray().GetAt(TextureUvIndex);
						SkinnedVertex.UV = FVector2D(static_cast<float>(UV.mData[0]), static_cast<float>(1 - UV.mData[1]));
					}
					break;
					default:
						break;
					}
				}
				break;
				default:
					break;
				}
			}

			// 실제 인덱스 버퍼에서 사용할 인덱스
			uint32 IndexOfVertex;
			// 기존의 Vertex맵에 있으면 그 인덱스를 사용
			if (IndexMap.Contains(SkinnedVertex))
			{
				IndexOfVertex = IndexMap[SkinnedVertex];
			}
			else
			{
				// 없으면 Vertex 리스트에 추가하고 마지막 원소 인덱스를 사용
				MeshData.Vertices.Add(SkinnedVertex);
				IndexOfVertex = MeshData.Vertices.Num() - 1;

				// 인덱스 맵에 추가
				IndexMap.Add(SkinnedVertex, IndexOfVertex);
			}
			// 대응하는 머티리얼 인덱스 리스트에 추가
			TArray<uint32>& GroupIndexList = MaterialGroupIndexList[MaterialIndex];
			GroupIndexList.Add(IndexOfVertex);

			// 인덱스 리스트에 최종 인덱스 추가(Vertex 리스트와 대응)
			// 머티리얼 사용하면서 필요 없어짐.(머티리얼 소팅 후 한번에 복사할거임)
			//MeshData.Indices.Add(IndexOfVertex);

			// Vertex 하나 저장했고 Vertex마다 Id를 사용하므로 +1
			VertexId++;
		} // for PolygonSize
	} // for PolygonCount



	// FBX에 정점의 탄젠트 벡터가 존재하지 않을 시
	if (InMesh->GetElementTangentCount() == 0)
	{
        // 1. 계산된 탄젠트와 바이탄젠트(Bitangent)를 누적할 임시 저장소를 만듭니다.
        // MeshData.Vertices에 이미 중복 제거된 유일한 정점들이 들어있습니다.
        TArray<FVector> TempTangents(MeshData.Vertices.Num());
        TArray<FVector> TempBitangents(MeshData.Vertices.Num());

        // 2. 모든 머티리얼 그룹의 인덱스 리스트를 순회합니다.
        for (auto& Elem : MaterialGroupIndexList)
        {
            TArray<uint32>& GroupIndexList = Elem.second;

            // 인덱스 리스트를 3개씩(트라이앵글 단위로) 순회합니다.
            for (int32 i = 0; i < GroupIndexList.Num(); i += 3)
            {
                uint32 i0 = GroupIndexList[i];
                uint32 i1 = GroupIndexList[i + 1];
                uint32 i2 = GroupIndexList[i + 2];

                // 트라이앵글을 구성하는 3개의 정점 데이터를 가져옵니다.
                // 이 정점들은 MeshData.Vertices에 있는 *유일한* 정점입니다.
                const FSkinnedVertex& v0 = MeshData.Vertices[i0];
                const FSkinnedVertex& v1 = MeshData.Vertices[i1];
                const FSkinnedVertex& v2 = MeshData.Vertices[i2];

                // 위치(P)와 UV(W)를 가져옵니다.
                const FVector& P0 = v0.Position;
                const FVector& P1 = v1.Position;
                const FVector& P2 = v2.Position;

                const FVector2D& W0 = v0.UV;
                const FVector2D& W1 = v1.UV;
                const FVector2D& W2 = v2.UV;

                // 트라이앵글의 엣지(Edge)와 델타(Delta) UV를 계산합니다.
                FVector Edge1 = P1 - P0;
                FVector Edge2 = P2 - P0;
                FVector2D DeltaUV1 = W1 - W0;
                FVector2D DeltaUV2 = W2 - W0;

                // Lengyel's MikkTSpace/Schwarze Formula (분모)
                float r = 1.0f / (DeltaUV1.X * DeltaUV2.Y - DeltaUV1.Y * DeltaUV2.X);

                // r이 무한대(inf)나 NaN이 아닌지 확인 (UV가 겹치는 경우)
                if (isinf(r) || isnan(r))
                {
                    r = 0.0f; // 이 트라이앵글은 계산에서 제외
                }

                // (정규화되지 않은) 탄젠트(T)와 바이탄젠트(B) 계산
                FVector T = (Edge1 * DeltaUV2.Y - Edge2 * DeltaUV1.Y) * r;
                FVector B = (Edge2 * DeltaUV1.X - Edge1 * DeltaUV2.X) * r;

                // 3개의 정점에 T와 B를 (정규화 없이) 누적합니다.
                // 이렇게 하면 동일한 정점을 공유하는 모든 트라이앵글의 T/B가 합산됩니다.
                TempTangents[i0] += T;
                TempTangents[i1] += T;
                TempTangents[i2] += T;

                TempBitangents[i0] += B;
                TempBitangents[i1] += B;
                TempBitangents[i2] += B;
            }
        }

        // 3. 모든 정점을 순회하며 누적된 T/B를 직교화(Gram-Schmidt)하고 저장합니다.
        for (int32 i = 0; i < MeshData.Vertices.Num(); ++i)
        {
            FSkinnedVertex& V = MeshData.Vertices[i]; // 실제 정점 데이터에 접근
            const FVector& N = V.Normal;
            const FVector& T_accum = TempTangents[i];
            const FVector& B_accum = TempBitangents[i];

            if (T_accum.IsZero() || N.IsZero())
            {
                // T 또는 N이 0이면 계산 불가. 유효한 기본값 설정
                FVector T_fallback = FVector(1.0f, 0.0f, 0.0f);
                if (FMath::Abs(FVector::Dot(N, T_fallback)) > 0.99f) // N이 X축과 거의 평행하면
                {
                    T_fallback = FVector(0.0f, 1.0f, 0.0f); // Y축을 T로 사용
                }
                V.Tangent = FVector4(T_fallback.X, T_fallback.Y, T_fallback.Z, 1.0f);
                continue;
            }

            // Gram-Schmidt 직교화: T = T - (T dot N) * N
            // (T를 N에 투영한 성분을 T에서 빼서, N과 수직인 벡터를 만듭니다)
        	FVector Tangent = (T_accum - N * (FVector::Dot(T_accum, N))).GetSafeNormal();

            // Handedness (W 컴포넌트) 계산:
            // 외적으로 구한 B(N x T)와 누적된 B(B_accum)의 방향을 비교합니다.
            float Handedness = (FVector::Dot((FVector::Cross(Tangent, N)), B_accum) > 0.0f ) ? 1.0f : -1.0f;

            // 최종 탄젠트(T)와 Handedness(W)를 저장합니다.
            V.Tangent = FVector4(Tangent.X, Tangent.Y, Tangent.Z, Handedness);
        }
    }
}


// 머티리얼 파싱해서 FMaterialInfo에 매핑
void UFbxLoader::ParseMaterial(FbxSurfaceMaterial* Material, FMaterialInfo& MaterialInfo)
{
	UMaterial* NewMaterial = NewObject<UMaterial>();

	// FbxPropertyT : 타입에 대해 애니메이션과 연결 지원(키프레임마다 타입 변경 등)
	FbxPropertyT<FbxDouble3> Double3Prop;
	FbxPropertyT<FbxDouble> DoubleProp;

	MaterialInfo.MaterialName = Material->GetName();
	// PBR 제외하고 Phong, Lambert 머티리얼만 처리함.
	if (Material->GetClassId().Is(FbxSurfacePhong::ClassId))
	{
		FbxSurfacePhong* SurfacePhong = (FbxSurfacePhong*)Material;

		Double3Prop = SurfacePhong->Diffuse;
		MaterialInfo.DiffuseColor = FVector(static_cast<float>(Double3Prop.Get()[0]), static_cast<float>(Double3Prop.Get()[1]), static_cast<float>(Double3Prop.Get()[2]));

		Double3Prop = SurfacePhong->Ambient;
		MaterialInfo.AmbientColor = FVector(static_cast<float>(Double3Prop.Get()[0]), static_cast<float>(Double3Prop.Get()[1]), static_cast<float>(Double3Prop.Get()[2]));

		// SurfacePhong->Reflection : 환경 반사, 퐁 모델에선 필요없음
		Double3Prop = SurfacePhong->Specular;
		DoubleProp = SurfacePhong->SpecularFactor;
		MaterialInfo.SpecularColor = FVector(static_cast<float>(Double3Prop.Get()[0]), static_cast<float>(Double3Prop.Get()[1]), static_cast<float>(Double3Prop.Get()[2])) * static_cast<float>(DoubleProp.Get());

		// HDR 안 써서 의미 없음
	/*	Double3Prop = SurfacePhong->Emissive;
		MaterialInfo.EmissiveColor = FVector(Double3Prop.Get()[0], Double3Prop.Get()[1], Double3Prop.Get()[2]);*/

		DoubleProp = SurfacePhong->Shininess;
		MaterialInfo.SpecularExponent = static_cast<float>(DoubleProp.Get());

		DoubleProp = SurfacePhong->TransparencyFactor;
		MaterialInfo.Transparency = static_cast<float>(DoubleProp.Get());
	}
	else if (Material->GetClassId().Is(FbxSurfaceLambert::ClassId))
	{
		FbxSurfaceLambert* SurfacePhong = (FbxSurfaceLambert*)Material;

		Double3Prop = SurfacePhong->Diffuse;
		MaterialInfo.DiffuseColor = FVector(static_cast<float>(Double3Prop.Get()[0]), static_cast<float>(Double3Prop.Get()[1]), static_cast<float>(Double3Prop.Get()[2]));

		Double3Prop = SurfacePhong->Ambient;
		MaterialInfo.AmbientColor = FVector(static_cast<float>(Double3Prop.Get()[0]), static_cast<float>(Double3Prop.Get()[1]), static_cast<float>(Double3Prop.Get()[2]));

		// HDR 안 써서 의미 없음
	/*	Double3Prop = SurfacePhong->Emissive;
		MaterialInfo.EmissiveColor = FVector(Double3Prop.Get()[0], Double3Prop.Get()[1], Double3Prop.Get()[2]);*/

		DoubleProp = SurfacePhong->TransparencyFactor;
		MaterialInfo.Transparency = static_cast<float>(DoubleProp.Get());
	}


	FbxProperty Property;

	Property = Material->FindProperty(FbxSurfaceMaterial::sDiffuse);
	MaterialInfo.DiffuseTextureFileName = ParseTexturePath(Property);

	Property = Material->FindProperty(FbxSurfaceMaterial::sNormalMap);
	MaterialInfo.NormalTextureFileName = ParseTexturePath(Property);

	Property = Material->FindProperty(FbxSurfaceMaterial::sAmbient);
	MaterialInfo.AmbientTextureFileName = ParseTexturePath(Property);

	Property = Material->FindProperty(FbxSurfaceMaterial::sSpecular);
	MaterialInfo.SpecularTextureFileName = ParseTexturePath(Property);

	Property = Material->FindProperty(FbxSurfaceMaterial::sEmissive);
	MaterialInfo.EmissiveTextureFileName = ParseTexturePath(Property);

	Property = Material->FindProperty(FbxSurfaceMaterial::sTransparencyFactor);
	MaterialInfo.TransparencyTextureFileName = ParseTexturePath(Property);

	Property = Material->FindProperty(FbxSurfaceMaterial::sShininess);
	MaterialInfo.SpecularExponentTextureFileName = ParseTexturePath(Property);

	UMaterial* Default = UResourceManager::GetInstance().GetDefaultMaterial();
	NewMaterial->SetMaterialInfo(MaterialInfo);
	NewMaterial->SetShader(Default->GetShader());
	NewMaterial->SetShaderMacros(Default->GetShaderMacros());

	MaterialInfos.Add(MaterialInfo);
	UResourceManager::GetInstance().Add<UMaterial>(MaterialInfo.MaterialName, NewMaterial);
}

FString UFbxLoader::ParseTexturePath(FbxProperty& Property)
{
	if (Property.IsValid())
	{
		if (Property.GetSrcObjectCount<FbxFileTexture>() > 0)
		{
			FbxFileTexture* Texture = Property.GetSrcObject<FbxFileTexture>(0);
			if (Texture)
			{
				const char* AcpPath = Texture->GetRelativeFileName();
				if (!AcpPath || strlen(AcpPath) == 0)
				{
					AcpPath = Texture->GetFileName();
				}

				if (!AcpPath)
				{
					return FString();
				}

				FString TexturePath = ACPToUTF8(AcpPath);
				return ResolveAssetRelativePath(TexturePath, this->CurrentFbxBaseDir);
			}
		}
	}
	return FString();
}

FbxString UFbxLoader::GetAttributeTypeName(FbxNodeAttribute* InAttribute)
{
	// 테스트코드
	// Attribute타입에 대한 자료형, 이것으로 Skeleton만 빼낼 수 있을 듯
	/*FbxNodeAttribute::EType Type = InAttribute->GetAttributeType();
	switch (Type) {
	case FbxNodeAttribute::eUnknown: return "unidentified";
	case FbxNodeAttribute::eNull: return "null";
	case FbxNodeAttribute::eMarker: return "marker";
	case FbxNodeAttribute::eSkeleton: return "skeleton";
	case FbxNodeAttribute::eMesh: return "mesh";
	case FbxNodeAttribute::eNurbs: return "nurbs";
	case FbxNodeAttribute::ePatch: return "patch";
	case FbxNodeAttribute::eCamera: return "camera";
	case FbxNodeAttribute::eCameraStereo: return "stereo";
	case FbxNodeAttribute::eCameraSwitcher: return "camera switcher";
	case FbxNodeAttribute::eLight: return "light";
	case FbxNodeAttribute::eOpticalReference: return "optical reference";
	case FbxNodeAttribute::eOpticalMarker: return "marker";
	case FbxNodeAttribute::eNurbsCurve: return "nurbs curve";
	case FbxNodeAttribute::eTrimNurbsSurface: return "trim nurbs surface";
	case FbxNodeAttribute::eBoundary: return "boundary";
	case FbxNodeAttribute::eNurbsSurface: return "nurbs surface";
	case FbxNodeAttribute::eShape: return "shape";
	case FbxNodeAttribute::eLODGroup: return "lodgroup";
	case FbxNodeAttribute::eSubDiv: return "subdiv";
	default: return "unknown";
	}*/
	return "test";
}

void UFbxLoader::EnsureSingleRootBone(FSkeletalMeshData& MeshData)
{
	// 본이 없는 경우 더미 본 생성 (Cloth 시뮬레이션용)
	if (MeshData.Skeleton.Bones.IsEmpty())
	{
		UE_LOG("UFbxLoader: No skeleton found, creating dummy root bone for cloth simulation");

		FBone DummyRoot;
		DummyRoot.Name = "Root";
		DummyRoot.ParentIndex = -1;
		DummyRoot.BindPose = FMatrix::Identity();
		DummyRoot.InverseBindPose = FMatrix::Identity();

		MeshData.Skeleton.Bones.Add(DummyRoot);

		// 모든 정점을 더미 본에 바인딩
		for (auto& Vertex : MeshData.Vertices)
		{
			Vertex.BoneIndices[0] = 0;  // 첫 번째 본(더미 본)
			Vertex.BoneWeights[0] = 1.0f;  // 100% 가중치

			// 나머지는 0으로 초기화
			for (int32 i = 1; i < 4; ++i)
			{
				Vertex.BoneIndices[i] = 0;
				Vertex.BoneWeights[i] = 0.0f;
			}
		}

		return;
	}

	// 루트 본 개수 세기
	TArray<int32> RootBoneIndices;
	for (int32 i = 0; i < MeshData.Skeleton.Bones.size(); ++i)
	{
		if (MeshData.Skeleton.Bones[i].ParentIndex == -1)
		{
			RootBoneIndices.Add(i);
		}
	}

	// 루트 본이 2개 이상이면 가상 루트 생성
	if (RootBoneIndices.Num() > 1)
	{
		// 가상 루트 본 생성
		FBone VirtualRoot;
		VirtualRoot.Name = "VirtualRoot";
		VirtualRoot.ParentIndex = -1;

		// 항등 행렬로 초기화 (원점에 위치, 회전/스케일 없음)
		VirtualRoot.BindPose = FMatrix::Identity();
		VirtualRoot.InverseBindPose = FMatrix::Identity();

		// 가상 루트를 배열 맨 앞에 삽입
		MeshData.Skeleton.Bones.Insert(VirtualRoot, 0);

		// 기존 본들의 인덱스가 모두 +1 씩 밀림
		// 모든 본의 ParentIndex 업데이트
		for (int32 i = 1; i < MeshData.Skeleton.Bones.size(); ++i)
		{
			if (MeshData.Skeleton.Bones[i].ParentIndex >= 0)
			{
				MeshData.Skeleton.Bones[i].ParentIndex += 1;
			}
			else // 원래 루트 본들
			{
				MeshData.Skeleton.Bones[i].ParentIndex = 0; // 가상 루트를 부모로 설정
			}
		}

		// Vertex의 BoneIndex도 모두 +1 해줘야 함
		for (auto& Vertex : MeshData.Vertices)
		{
			for (int32 i = 0; i < 4; ++i)
			{
				Vertex.BoneIndices[i] += 1;
			}
		}

		UE_LOG("UFbxLoader: Created virtual root bone. Found %d root bones.", RootBoneIndices.Num());
	}
}

// ========================================
// 애니메이션 임포트
// ========================================

// UAnimSequence* UFbxLoader::LoadFbxAnimation(const FString& FilePath, const FSkeleton& TargetSkeleton, const FString& AnimStackName)
// {
// 	// 1. FBX 파일 로드
// 	FString NormalizedPath = NormalizePath(FilePath);
// 	FString FbxPathAcp = UTF8ToACP(NormalizedPath);
//
// 	FbxIOSettings* IOSettings = FbxIOSettings::Create(SdkManager, IOSROOT);
// 	SdkManager->SetIOSettings(IOSettings);
//
// 	FbxImporter* Importer = FbxImporter::Create(SdkManager, "");
// 	if (!Importer->Initialize(FbxPathAcp.c_str(), -1, SdkManager->GetIOSettings()))
// 	{
// 		UE_LOG("UFbxLoader::LoadFbxAnimation: Failed to initialize FBX importer for %s", FilePath.c_str());
// 		UE_LOG("Error: %s", Importer->GetStatus().GetErrorString());
// 		Importer->Destroy();
// 		return nullptr;
// 	}
//
// 	FbxScene* Scene = FbxScene::Create(SdkManager, "AnimScene");
// 	if (!Importer->Import(Scene))
// 	{
// 		UE_LOG("UFbxLoader::LoadFbxAnimation: Failed to import FBX scene from %s", FilePath.c_str());
// 		Importer->Destroy();
// 		Scene->Destroy();
// 		return nullptr;
// 	}
//
// 	Importer->Destroy();
//
// 	// 좌표계 변환 (메시 로딩과 동일하게 적용)
// 	FbxAxisSystem UnrealImportAxis(FbxAxisSystem::eZAxis, FbxAxisSystem::eParityEven, FbxAxisSystem::eLeftHanded);
// 	FbxAxisSystem SourceSetup = Scene->GetGlobalSettings().GetAxisSystem();
//
// 	// 단위 변환: FBX의 원본 단위를 확인하고 엔진 단위(m)로 변환
// 	FbxSystemUnit SceneSystemUnit = Scene->GetGlobalSettings().GetSystemUnit();
// 	if (SceneSystemUnit != FbxSystemUnit::m)
// 	{
// 		// 원본 단위를 미터로 변환
// 		FbxSystemUnit::m.ConvertScene(Scene);
// 		UE_LOG("FBX 단위 변환: %s -> m", SceneSystemUnit.GetScaleFactorAsString().Buffer());
// 	}
//
// 	if (SourceSetup != UnrealImportAxis)
// 	{
// 		UE_LOG("Fbx 애니메이션 축 변환 완료!\n");
// 		UnrealImportAxis.DeepConvertScene(Scene);
// 	}
//
// 	// 2. 애니메이션 스택 찾기
// 	int32 AnimStackCount = Scene->GetSrcObjectCount<FbxAnimStack>();
// 	if (AnimStackCount == 0)
// 	{
// 		UE_LOG("UFbxLoader::LoadFbxAnimation: No animation stack found in %s", FilePath.c_str());
// 		Scene->Destroy();
// 		return nullptr;
// 	}
//
// 	FbxAnimStack* AnimStack = nullptr;
// 	if (AnimStackName.empty())
// 	{
// 		AnimStack = Scene->GetSrcObject<FbxAnimStack>(1); // 두 번째 스택
// 		UE_LOG("UFbxLoader::LoadFbxAnimation: Using first animation stack: %s", AnimStack->GetName());
// 	}
// 	else
// 	{
// 		// 이름으로 검색
// 		for (int32 i = 0; i < AnimStackCount; ++i)
// 		{
// 			FbxAnimStack* Stack = Scene->GetSrcObject<FbxAnimStack>(i);
// 			if (FString(Stack->GetName()) == AnimStackName)
// 			{
// 				AnimStack = Stack;
// 				break;
// 			}
// 		}
//
// 		if (!AnimStack)
// 		{
// 			UE_LOG("UFbxLoader::LoadFbxAnimation: Animation stack '%s' not found", AnimStackName.c_str());
// 			Scene->Destroy();
// 			return nullptr;
// 		}
// 	}
//
// 	// 3. 애니메이션 레이어 가져오기
// 	int32 AnimLayerCount = AnimStack->GetMemberCount<FbxAnimLayer>();
// 	UE_LOG("UFbxLoader::LoadFbxAnimation: AnimStack has %d animation layers", AnimLayerCount);
//
// 	FbxAnimLayer* AnimLayer = AnimStack->GetMember<FbxAnimLayer>(0);
// 	if (!AnimLayer)
// 	{
// 		UE_LOG("UFbxLoader::LoadFbxAnimation: No animation layer found");
// 		Scene->Destroy();
// 		return nullptr;
// 	}
//
// 	UE_LOG("UFbxLoader::LoadFbxAnimation: Using AnimLayer: %s", AnimLayer->GetName());
//
// 	FbxNode* RootNode = Scene->GetRootNode();
// 	if (!RootNode)
// 	{
// 		UE_LOG("UFbxLoader::LoadFbxAnimation: Scene has no root node");
// 		Scene->Destroy();
// 		return nullptr;
// 	}
//
// 	// 4. 시간 범위 설정 - AnimStack의 LocalTimeSpan 사용
// 	FbxTimeSpan TimeSpan = AnimStack->GetLocalTimeSpan();
// 	FbxTime StartTime = TimeSpan.GetStart();
// 	FbxTime StopTime = TimeSpan.GetStop();
//
// 	UE_LOG("UFbxLoader::LoadFbxAnimation: AnimStack time range: %.3fs ~ %.3fs (duration: %.3fs)",
// 		StartTime.GetSecondDouble(),
// 		StopTime.GetSecondDouble(),
// 		(StopTime - StartTime).GetSecondDouble());
//
// 	// 모션 감지로 실제 애니메이션 끝 찾기
// 	FbxTime::EMode TimeMode = Scene->GetGlobalSettings().GetTimeMode();
// 	double SampleRate = FbxTime::GetFrameRate(TimeMode);
// 	if (SampleRate <= 0.0)
// 	{
// 		SampleRate = 60.0;
// 	}
//
// 	FbxTime SampleInterval;
// 	SampleInterval.SetSecondDouble(1.0 / SampleRate);
// 	FbxTime LastMotionTime = StartTime;
// 	const double MotionThreshold = 0.00001;
//
// 	// 주요 본들 샘플링
// 	const char* ImportantBoneNames[] = {
// 		"mixamorig:Hips",
// 		"mixamorig:Spine",
// 		"mixamorig:Head",
// 		"mixamorig:RightHand",
// 		"mixamorig:LeftHand",
// 		"mixamorig:RightFoot",
// 		"mixamorig:LeftFoot"
// 	};
//
// 	TArray<FbxNode*> SampleNodes;
// 	for (const char* BoneName : ImportantBoneNames)
// 	{
// 		FbxNode* Node = FindNodeByName(RootNode, BoneName);
// 		if (Node)
// 		{
// 			SampleNodes.Add(Node);
// 		}
// 	}
//
// 	if (SampleNodes.Num() == 0 && TargetSkeleton.Bones.Num() > 0)
// 	{
// 		FbxNode* Node = FindNodeByName(RootNode, TargetSkeleton.Bones[0].Name.c_str());
// 		if (Node)
// 		{
// 			SampleNodes.Add(Node);
// 		}
// 	}
//
// 	if (SampleNodes.Num() > 0)
// 	{
// 		UE_LOG("UFbxLoader::LoadFbxAnimation: Sampling motion on %d bones at %.2f fps...",
// 			SampleNodes.Num(), SampleRate);
//
// 		TArray<FbxAMatrix> PrevTransforms;
// 		for (FbxNode* Node : SampleNodes)
// 		{
// 			PrevTransforms.Add(Node->EvaluateLocalTransform(StartTime));
// 		}
//
// 		FbxTime CurrentTime = StartTime + SampleInterval;
//
// 		while (CurrentTime <= StopTime)
// 		{
// 			for (int32 i = 0; i < SampleNodes.Num(); ++i)
// 			{
// 				FbxNode* Node = SampleNodes[i];
// 				FbxAMatrix CurrentTransform = Node->EvaluateLocalTransform(CurrentTime);
// 				FbxAMatrix& PrevTransform = PrevTransforms[i];
//
// 				FbxVector4 PrevT = PrevTransform.GetT();
// 				FbxVector4 CurrT = CurrentTransform.GetT();
// 				double TransDelta = (CurrT - PrevT).Length();
//
// 				FbxQuaternion PrevQ = PrevTransform.GetQ();
// 				FbxQuaternion CurrQ = CurrentTransform.GetQ();
// 				FbxQuaternion DiffQ = CurrQ - PrevQ;
// 				double RotDelta = std::sqrt(DiffQ[0] * DiffQ[0] + DiffQ[1] * DiffQ[1] + DiffQ[2] * DiffQ[2] + DiffQ[3] * DiffQ[3]);
//
// 				if (TransDelta > MotionThreshold || RotDelta > MotionThreshold)
// 				{
// 					LastMotionTime = CurrentTime;
// 				}
//
// 				PrevTransform = CurrentTransform;
// 			}
//
// 			CurrentTime += SampleInterval;
// 		}
//
// 		LastMotionTime += SampleInterval;
//
// 		if (LastMotionTime < StopTime)
// 		{
// 			UE_LOG("UFbxLoader::LoadFbxAnimation: Detected motion end at %.3fs (trimmed from 10.0s)",
// 				LastMotionTime.GetSecondDouble());
// 			StopTime = LastMotionTime;
// 			TimeSpan.Set(StartTime, StopTime);
// 		}
// 		else
// 		{
// 			UE_LOG("UFbxLoader::LoadFbxAnimation: Motion detected throughout 10 second span");
// 		}
// 	}
//
// 	double PlayLength = FMath::Max((StopTime - StartTime).GetSecondDouble(), 0.0);
//
// 	// FPS는 Scene TimeMode에서 가져오기
// 	double FrameRate = FbxTime::GetFrameRate(Scene->GetGlobalSettings().GetTimeMode());
// 	if (FrameRate <= 0.0)
// 	{
// 		FrameRate = 60.0; // 기본값
// 	}
//
// 	UE_LOG("UFbxLoader::LoadFbxAnimation: Using FPS = %.2f", FrameRate);
//
// 	// 언리얼 엔진 방식: 프레임 개수 계산
// 	// NumFrame = RoundToInt(SequenceLength * BakeFrequency)
// 	// BakeKeyCount = NumFrame + 1 (시작 프레임 포함)
// 	const double SequenceLength = FMath::Max(PlayLength, 1.0 / FrameRate);
// 	int32 NumFrames = static_cast<int32>(std::round(SequenceLength * FrameRate));
// 	int32 NumberOfFrames = NumFrames + 1;  // 시작 프레임 포함 (0-based indexing)
//
// 	UE_LOG("UFbxLoader::LoadFbxAnimation: SequenceLength=%.3fs, NumFrames=%d, BakeKeyCount=%d, FPS=%.2f",
// 		SequenceLength, NumFrames, NumberOfFrames, FrameRate);
//
// 	// 5. UAnimSequence 생성
// 	UAnimSequence* AnimSequence = ObjectFactory::NewObject<UAnimSequence>();
// 	UAnimDataModel* DataModel = ObjectFactory::NewObject<UAnimDataModel>();
// 	AnimSequence->SetDataModel(DataModel);
//
// 	// 6. 메타데이터 설정 (원본 FPS 사용)
// 	DataModel->NumberOfFrames = NumberOfFrames;
// 	DataModel->NumberOfKeys = NumberOfFrames;
// 	DataModel->PlayLength = static_cast<float>(PlayLength);
// 	DataModel->FrameRate = FFrameRate(static_cast<int32>(FrameRate), 1);
//
// 	// 7. 각 본에 대해 애니메이션 추출
// 	for (const FBone& Bone : TargetSkeleton.Bones)
// 	{
// 		// 본 노드 찾기
// 		FbxNode* BoneNode = FindNodeByName(RootNode, Bone.Name.c_str());
// 		if (!BoneNode)
// 		{
// 			UE_LOG("UFbxLoader::LoadFbxAnimation: Bone '%s' not found in FBX scene, skipping", Bone.Name.c_str());
// 			continue;
// 		}
//
// 		// 본 애니메이션 트랙 생성
// 		FBoneAnimationTrack BoneTrack;
// 		BoneTrack.BoneName = Bone.Name;
//
// 		// 애니메이션 데이터 추출
// 		ExtractBoneAnimation(BoneNode, AnimLayer, TimeSpan, FrameRate, BoneTrack);
//
// 		// 트랙 추가
// 		DataModel->BoneAnimationTracks.Add(BoneTrack);
//
// 		UE_LOG("UFbxLoader::LoadFbxAnimation: Extracted animation for bone '%s' (%d keys)",
// 			Bone.Name.c_str(), BoneTrack.InternalTrack.PosKeys.Num());
// 	}
//
// 	Scene->Destroy();
//
// 	UE_LOG("UFbxLoader::LoadFbxAnimation: Successfully loaded animation with %d bone tracks",
// 		DataModel->BoneAnimationTracks.Num());
//
// 	return AnimSequence;
// }

TArray<UAnimSequence*> UFbxLoader::LoadAllFbxAnimations(const FString& FilePath, const FSkeleton& TargetSkeleton)
{
	TArray<UAnimSequence*> Results;

	// 1. 경로 정규화
	FString NormalizedPath = NormalizePath(FilePath);
	FString FbxPathAcp = UTF8ToACP(NormalizedPath);

	FWideString WNormalizedPath = UTF8ToWide(NormalizedPath);
	std::filesystem::path FbxPath(WNormalizedPath);

	// FBX SDK는 스레드 안전하지 않으므로 mutex로 보호
	std::lock_guard<std::mutex> FbxLock(GFbxSdkMutex);

	// 2. FBX 파일 로드
	FbxImporter* Importer = FbxImporter::Create(SdkManager, "");
	if (!Importer->Initialize(FbxPathAcp.c_str(), -1, SdkManager->GetIOSettings()))
	{
		UE_LOG("UFbxLoader::LoadAllFbxAnimations: Failed to initialize FBX importer for %s", FilePath.c_str());
		UE_LOG("Error: %s", Importer->GetStatus().GetErrorString());
		Importer->Destroy();
		return Results;
	}

	FbxScene* Scene = FbxScene::Create(SdkManager, "AnimScene");
	if (!Importer->Import(Scene))
	{
		UE_LOG("UFbxLoader::LoadAllFbxAnimations: Failed to import FBX scene from %s", FilePath.c_str());
		Importer->Destroy();
		Scene->Destroy();
		return Results;
	}

	Importer->Destroy();

	// 모든 FBX에 대해 Armature 스케일 보정을 시도
	// 루트 본 바로 위 노드를 Armature로 간주하고, 스케일이 (1,1,1)이 아니면 적용
	m_bNeedsScaleCorrection = true;
	UE_LOG("=== Loading FBX file: '%s' ===", FilePath.c_str());
	UE_LOG(">>> Armature scale correction enabled for all FBX files.");

	// 3. 좌표계 변환
	FbxAxisSystem UnrealImportAxis(FbxAxisSystem::eZAxis, FbxAxisSystem::eParityEven, FbxAxisSystem::eLeftHanded);
	FbxAxisSystem SourceSetup = Scene->GetGlobalSettings().GetAxisSystem();

	FbxSystemUnit SceneSystemUnit = Scene->GetGlobalSettings().GetSystemUnit();
	if (SceneSystemUnit != FbxSystemUnit::m)
	{
		FbxSystemUnit::m.ConvertScene(Scene);
	}

	if (SourceSetup != UnrealImportAxis)
	{
		UnrealImportAxis.DeepConvertScene(Scene);
	}

	// 4. AnimStack 개수 확인
	int32 AnimStackCount = Scene->GetSrcObjectCount<FbxAnimStack>();
	if (AnimStackCount == 0)
	{
		UE_LOG("UFbxLoader::LoadAllFbxAnimations: No animation stack found in %s", FilePath.c_str());
		Scene->Destroy();
		return Results;
	}

	UE_LOG("UFbxLoader::LoadAllFbxAnimations: Found %d animation stacks in %s", AnimStackCount, FilePath.c_str());

	FbxNode* RootNode = Scene->GetRootNode();
	if (!RootNode)
	{
		UE_LOG("UFbxLoader::LoadAllFbxAnimations: Scene has no root node");
		Scene->Destroy();
		return Results;
	}

	// 5. 베이스 캐시 경로 생성
	FString BaseCachePath = ConvertDataPathToCachePath(NormalizedPath);

	// 6. 각 AnimStack에 대해 처리
	for (int32 StackIndex = 0; StackIndex < AnimStackCount; ++StackIndex)
	{
		FbxAnimStack* AnimStack = Scene->GetSrcObject<FbxAnimStack>(StackIndex);
		if (!AnimStack)
			continue;

		FString AnimStackName = AnimStack->GetName();
		UE_LOG("AnimStackName Begin: %s", AnimStackName.c_str());

		// "|" 문자를 "_"로 변경 (파일 경로에 유효하지 않은 문자 처리)
		std::replace(AnimStackName.begin(), AnimStackName.end(), '|', '_');

		UE_LOG("AnimStackName End  : %s", AnimStackName.c_str());

		// "Take 001" 애니메이션 필터링 (길이 무관)
		if (AnimStackName == "Take 001")
		{
			UE_LOG("UFbxLoader::LoadAllFbxAnimations: Skipping default Take 001 animation");
			continue;
		}

		UE_LOG("UFbxLoader::LoadAllFbxAnimations: Processing AnimStack %d/%d: '%s'",
			StackIndex + 1, AnimStackCount, AnimStackName.c_str());

		UAnimSequence* AnimSequence = nullptr;
		UAnimDataModel* DataModel = nullptr;

		// .anim 파일 경로 생성 (FBX경로_AnimStackName.anim)
		size_t DotPos = NormalizedPath.find_last_of('.');
		FString AnimFilePath;
		if (DotPos != FString::npos)
		{
			AnimFilePath = NormalizedPath.substr(0, DotPos) + "_" + AnimStackName + ".anim";
		}
		else
		{
			AnimFilePath = NormalizedPath + "_" + AnimStackName + ".anim";
		}

		FWideString WAnimFilePath = UTF8ToWide(AnimFilePath);
		std::filesystem::path AnimFilePathObj(WAnimFilePath);

		// .anim 파일이 존재하고 FBX보다 최신이면 로드
		if (std::filesystem::exists(AnimFilePathObj))
		{
			try
			{
				auto animTime = std::filesystem::last_write_time(AnimFilePathObj);
				auto fbxTime = std::filesystem::last_write_time(FbxPath);

				if (fbxTime <= animTime)
				{
					// .anim에서 로드
					AnimSequence = NewObject<UAnimSequence>();
					AnimSequence->LoadFromAnimFile(AnimFilePath);

					if (!AnimSequence->GetName().empty())
					{
						// 기존 .anim 파일의 이름을 새 형식으로 업데이트 (ImGui ID 충돌 방지)
						std::filesystem::path FbxFilePath(NormalizedPath);
						FString FbxFileName = FbxFilePath.stem().string();
						FString UniqueAnimName = FbxFileName + "_" + AnimStackName;
						AnimSequence->SetName(UniqueAnimName);

						AnimSequence->SetFilePath(AnimFilePath);
						AnimSequence->SetLastModifiedTime(animTime);
						UResourceManager::GetInstance().Add<UAnimSequence>(AnimFilePath, AnimSequence);
						Results.Add(AnimSequence);
						continue;
					}
					else
					{
						ObjectFactory::DeleteObject(AnimSequence);
						AnimSequence = nullptr;
					}
				}
			}
			catch (const std::filesystem::filesystem_error& e)
			{
				UE_LOG("FbxLoader: LoadAllFbxAnimations: Filesystem error: %s", e.what());
			}
		}

#ifdef USE_OBJ_CACHE
		// 캐시 파일 경로: [MeshName]_[AnimStackName].anim.bin
		FString CachePath = BaseCachePath + "_" + AnimStackName + ".anim.bin";
		FWideString WCachePath = UTF8ToWide(CachePath);
		std::filesystem::path CacheFilePath(WCachePath);

		// 캐시 디렉토리 생성
		if (CacheFilePath.has_parent_path())
		{
			std::filesystem::create_directories(CacheFilePath.parent_path());
		}

		bool bLoadedFromCache = false;
		bool bShouldRegenerate = true;

		// 타임스탬프 검증
		if (std::filesystem::exists(CacheFilePath))
		{
			try
			{
				auto cacheTime = std::filesystem::last_write_time(CacheFilePath);
				auto fbxTime = std::filesystem::last_write_time(FbxPath);

				if (fbxTime <= cacheTime)
				{
					bShouldRegenerate = false;
				}
			}
			catch (const std::filesystem::filesystem_error& e)
			{
				UE_LOG("FbxLoader: LoadAllFbxAnimations: Filesystem error during cache validation: %s", e.what());
				bShouldRegenerate = true;
			}
		}

		// AnimStack 활성화 및 AnimLayer/StartTime 가져오기 (캐시/FBX 양쪽 모두 사용)
		Scene->SetCurrentAnimationStack(AnimStack);

		// 애니메이션 레이어 가져오기
		int32 AnimLayerCount = AnimStack->GetMemberCount<FbxAnimLayer>();
		FbxAnimLayer* AnimLayer = AnimStack->GetMember<FbxAnimLayer>(0);
		if (!AnimLayer)
		{
			UE_LOG("  No animation layer found, skipping");
			continue;
		}

		// 시간 범위 설정
		FbxTimeSpan TimeSpan = AnimStack->GetLocalTimeSpan();
		FbxTime StartTime = TimeSpan.GetStart();
		FbxTime StopTime = TimeSpan.GetStop();
		double PlayLength = FMath::Max((StopTime - StartTime).GetSecondDouble(), 0.0);

				// 캐시에서 로드 시도
		if (!bShouldRegenerate)
		{
			UE_LOG("  Attempting to load AnimStack '%s' from cache", AnimStackName.c_str());
			try
			{
				FWindowsBinReader Reader(CachePath);
				if (!Reader.IsOpen())
				{
					throw std::runtime_error("Failed to open cache file");
				}

				// AnimSequence와 DataModel 생성
				AnimSequence = NewObject<UAnimSequence>();
				DataModel = NewObject<UAnimDataModel>();

				// Name 로드
				FString AnimName;
				Serialization::ReadString(Reader, AnimName);
				AnimSequence->SetName(AnimName);

				// Notifies 로드
				uint32 NotifyCount = 0;
				Reader << NotifyCount;
				AnimSequence->Notifies.resize(NotifyCount);
				for (uint32 i = 0; i < NotifyCount; ++i)
				{
					Reader << AnimSequence->Notifies[i];
				}

				// DataModel 로드
				Reader << *DataModel;

				// CRITICAL FIX: Skeleton 복사본 생성 (캐시 로드 시)
				DataModel->SetSkeleton(TargetSkeleton);

				// Blender FBX의 경우 Armature 스케일 적용 (캐시 로드 시에도 적용)
				// RootBoneNode는 nullptr로 전달 -> 함수 내부에서 이름으로 찾기
				ApplyArmatureScaleCorrection(DataModel, TargetSkeleton, nullptr, RootNode, AnimLayer, StartTime);

				AnimSequence->SetDataModel(DataModel);
				AnimSequence->SetFilePath(AnimFilePath);
				Reader.Close();

				// 캐시에서 로드한 이름을 새 형식으로 업데이트 (ImGui ID 충돌 방지)
				// 기존 캐시는 AnimStackName만 저장되어 있으므로 FBX파일명_ 접두사 추가
				std::filesystem::path FbxFilePath(NormalizedPath);
				FString FbxFileName = FbxFilePath.stem().string();
				FString UniqueAnimName = FbxFileName + "_" + AnimStackName;
				AnimSequence->SetName(UniqueAnimName);

				bLoadedFromCache = true;
				UE_LOG("  Successfully loaded AnimStack '%s' from cache", AnimStackName.c_str());
				UE_LOG("    Name: %s", UniqueAnimName.c_str());
				UE_LOG("    Duration: %.3f seconds", DataModel->GetPlayLength());
				UE_LOG("    Bone Tracks: %d", DataModel->GetBoneAnimationTracks().Num());
			}
			catch (const std::exception& e)
			{
				UE_LOG("  Error loading from cache: %s. Regenerating...", e.what());
				std::filesystem::remove(CacheFilePath);
				if (AnimSequence)
				{
					ObjectFactory::DeleteObject(AnimSequence);
					AnimSequence = nullptr;
				}
				if (DataModel)
				{
					ObjectFactory::DeleteObject(DataModel);
					DataModel = nullptr;
				}
				bLoadedFromCache = false;
			}
		}

		// 캐시 로드 실패 시 FBX에서 추출
		if (!bLoadedFromCache)
#endif // USE_OBJ_CACHE
		{
			UE_LOG("  Extracting AnimStack '%s' from FBX", AnimStackName.c_str());

			// FPS 설정
			FbxTime::EMode TimeMode = Scene->GetGlobalSettings().GetTimeMode();
			double FrameRate = FbxTime::GetFrameRate(TimeMode);
			if (FrameRate <= 0.0)
			{
				FrameRate = 60.0;
			}

			const double SequenceLength = FMath::Max(PlayLength, 1.0 / FrameRate);
			int32 NumFrames = static_cast<int32>(std::round(SequenceLength * FrameRate));
			int32 BakeKeyCount = NumFrames + 1;

			// AnimSequence 생성
			AnimSequence = NewObject<UAnimSequence>();

			// 고유한 이름 생성: FBX파일명_AnimStackName (ImGui ID 충돌 방지)
			std::filesystem::path FbxFilePath(NormalizedPath);
			FString FbxFileName = FbxFilePath.stem().string();  // 확장자 제외한 파일명
			FString UniqueAnimName = FbxFileName + "_" + AnimStackName;
			AnimSequence->SetName(UniqueAnimName);
			AnimSequence->SetFilePath(AnimFilePath);

			DataModel = NewObject<UAnimDataModel>();
			AnimSequence->SetDataModel(DataModel);

			// CRITICAL FIX: Skeleton 복사본 생성
			DataModel->SetSkeleton(TargetSkeleton);

			DataModel->BoneAnimationTracks.Reserve(TargetSkeleton.Bones.Num());

			// 본 애니메이션 추출 (Mixamo ChainMapping 적용)
			FbxNode* RootBoneNode = nullptr;  // 루트 본 노드 저장용
			for (const FBone& Bone : TargetSkeleton.Bones)
			{
				// Canonical 이름으로 노드 찾기 (Mixamo 접두사 고려)
				FbxNode* BoneNode = FindNodeByCanonicalName(RootNode, Bone.Name.c_str());
				if (!BoneNode)
					continue;

				// 루트 본 노드 저장 (ParentIndex == -1)
				if (Bone.ParentIndex == -1 && !RootBoneNode)
				{
					RootBoneNode = BoneNode;
					UE_LOG("  Found root bone node: '%s' (FBX node: '%s')", Bone.Name.c_str(), BoneNode->GetName());
				}

				FBoneAnimationTrack BoneTrack;
				BoneTrack.BoneName = Bone.Name;
				ExtractBoneAnimation(BoneNode, AnimLayer, TimeSpan, FrameRate, BoneTrack);

				if (BoneTrack.InternalTrack.PosKeys.Num() > 0)
				{
					DataModel->BoneAnimationTracks.Add(BoneTrack);
				}
			}

			// FFrameRate 설정
			FFrameRate AnimFrameRate;
			AnimFrameRate.Numerator = static_cast<int32>(FrameRate);
			AnimFrameRate.Denominator = 1;
			DataModel->FrameRate = AnimFrameRate;
			DataModel->PlayLength = static_cast<float>(PlayLength);
			DataModel->NumberOfFrames = NumFrames;
			DataModel->NumberOfKeys = BakeKeyCount;

			UE_LOG("  Successfully extracted animation: %d bone tracks", DataModel->BoneAnimationTracks.Num());

			// Blender에서 Export한 경우, Armature의 Scale을 루트 본에 곱해줌
		if (RootBoneNode)
		{
			ApplyArmatureScaleCorrection(DataModel, TargetSkeleton, RootBoneNode, RootNode, AnimLayer, StartTime);
		}
		else
		{
			UE_LOG("  Warning: Root bone node not found, skipping Armature scale correction");
		}



#ifdef USE_OBJ_CACHE
			// 캐시 저장 전 데이터 검증
			UE_LOG("  Validating data before cache save:");
			UE_LOG("    Total Tracks: %d", DataModel->BoneAnimationTracks.Num());
			if (DataModel->BoneAnimationTracks.Num() > 0)
			{
				const FBoneAnimationTrack& FirstTrack = DataModel->BoneAnimationTracks[0];
				UE_LOG("    First Track Name: %s", FirstTrack.BoneName.c_str());
				UE_LOG("    First Track PosKeys: %d", FirstTrack.InternalTrack.PosKeys.Num());
				UE_LOG("    First Track RotKeys: %d", FirstTrack.InternalTrack.RotKeys.Num());
				UE_LOG("    First Track ScaleKeys: %d", FirstTrack.InternalTrack.ScaleKeys.Num());
			}

			// 캐시 저장
			try
			{
				FWindowsBinWriter Writer(CachePath);

				// Name 저장
				Serialization::WriteString(Writer, AnimSequence->GetName());

				// Notifies 저장
				uint32 NotifyCount = static_cast<uint32>(AnimSequence->Notifies.Num());
				Writer << NotifyCount;
				for (FAnimNotifyEvent& Notify : AnimSequence->Notifies)
				{
					Writer << Notify;
				}

				// DataModel 저장
				Writer << *DataModel;

				Writer.Close();
				UE_LOG("  Cache saved for AnimStack '%s'", AnimStackName.c_str());
				UE_LOG("    Name: %s", AnimSequence->GetName().c_str());
				UE_LOG("    Duration: %.3f seconds", DataModel->GetPlayLength());
				UE_LOG("    Bone Tracks: %d", DataModel->GetBoneAnimationTracks().Num());
				UE_LOG("    Cache Path: %s", CachePath.c_str());
			}
			catch (const std::exception& e)
			{
				UE_LOG("  Failed to save cache: %s", e.what());
			}
#endif // USE_OBJ_CACHE
		}

		// .anim 파일 저장은 PreLoad 끝에서 별도로 수행 (SaveAllAnimSequencesToAnimFiles)

		if (AnimSequence)
		{
			Results.Add(AnimSequence);
		}
	}

	Scene->Destroy();

	// FBX 파일의 LastModifiedTime 가져오기 (FbxPath는 이미 1570줄에서 선언됨)
	std::filesystem::file_time_type FbxModifiedTime;
	if (std::filesystem::exists(FbxPath))
	{
		FbxModifiedTime = std::filesystem::last_write_time(FbxPath);
	}

	// 각 AnimSequence에 LastModifiedTime 설정 후 ResourceManager에 등록
	// FilePath는 이미 .anim 경로로 설정되어 있음
	for (int32 i = 0; i < Results.Num(); ++i)
	{
		UAnimSequence* AnimSeq = Results[i];
		if (!AnimSeq)
		{
			continue;
		}

		try
		{
			// FilePath가 비어있으면 건너뜀
			FString AnimResourcePath = AnimSeq->GetFilePath();
			if (AnimResourcePath.empty())
			{
				continue;
			}

			// .anim 파일에서 로드한 경우 이미 등록되어 있으므로 건너뜀
			if (UResourceManager::GetInstance().Get<UAnimSequence>(AnimResourcePath))
			{
				continue;
			}

			// LastModifiedTime 설정 (.anim 파일 시간 사용)
			FWideString WAnimPath = UTF8ToWide(AnimResourcePath);
			std::filesystem::path AnimPath(WAnimPath);
			if (std::filesystem::exists(AnimPath))
			{
				AnimSeq->SetLastModifiedTime(std::filesystem::last_write_time(AnimPath));
			}
			else
			{
				AnimSeq->SetLastModifiedTime(FbxModifiedTime);
			}

			// ResourceManager에 등록
			UResourceManager::GetInstance().Add<UAnimSequence>(AnimResourcePath, AnimSeq);
		}
		catch (const std::exception& e)
		{
			UE_LOG("FbxLoader: LoadAllFbxAnimations: Error registering AnimSequence: %s", e.what());
		}
	}

	UE_LOG("FbxLoader: LoadAllFbxAnimations: Successfully loaded %d animations from %s",
		Results.Num(), FilePath.c_str());

	return Results;
}

void UFbxLoader::ExtractBoneAnimation(
	FbxNode* BoneNode,
	FbxAnimLayer* AnimLayer,
	const FbxTimeSpan& TimeSpan,
	double FrameRate,
	FBoneAnimationTrack& OutTrack)
{
	FbxTime StartTime = TimeSpan.GetStart();
	FbxTime StopTime = TimeSpan.GetStop();
	double PlayLength = (StopTime - StartTime).GetSecondDouble();

	// 언리얼 엔진 방식: Bake Transform (균등 간격 샘플링)
	// 키프레임 기반이 아니라 일정한 FPS로 샘플링합니다
	const double SequenceLength = FMath::Max(PlayLength, 1.0 / FrameRate);
	const int32 NumFrames = static_cast<int32>(std::round(SequenceLength * FrameRate));
	const int32 BakeKeyCount = NumFrames + 1;  // 시작 프레임 포함

	UE_LOG("UFbxLoader::ExtractBoneAnimation: Bone '%s' - Baking %d keys at %.2f fps (%.3fs duration)",
		BoneNode->GetName(), BakeKeyCount, FrameRate, SequenceLength);

	if (BakeKeyCount <= 0)
	{
		UE_LOG("UFbxLoader::ExtractBoneAnimation: ERROR - Invalid BakeKeyCount for bone '%s'", BoneNode->GetName());
		return;
	}

	// 시간 간격 계산
	FbxTime TimeStep;
	TimeStep.SetSecondDouble(1.0 / FrameRate);

	// 균등 간격으로 샘플링 (언리얼 엔진 방식)
	FbxTime CurrentTime = StartTime;
	for (int32 FrameIndex = 0; FrameIndex < BakeKeyCount; ++FrameIndex, CurrentTime += TimeStep)
	{
		// Position 샘플링
		FVector Position = SamplePosition(BoneNode, AnimLayer, CurrentTime);
		OutTrack.InternalTrack.PosKeys.Add(Position);

		// Rotation 샘플링
		FQuat Rotation = SampleRotation(BoneNode, AnimLayer, CurrentTime);
		OutTrack.InternalTrack.RotKeys.Add(Rotation);

		// Scale 샘플링
		FVector Scale = SampleScale(BoneNode, AnimLayer, CurrentTime);
		OutTrack.InternalTrack.ScaleKeys.Add(Scale);
	}

	UE_LOG("UFbxLoader::ExtractBoneAnimation: Bone '%s' - Extracted %d transform keys",
		BoneNode->GetName(), OutTrack.InternalTrack.PosKeys.Num());
}

FVector UFbxLoader::SamplePosition(FbxNode* Node, FbxAnimLayer* Layer, FbxTime Time)
{
	// 현재 시간의 Transform 평가
	FbxAMatrix Transform = Node->EvaluateLocalTransform(Time);
	FbxVector4 Translation = Transform.GetT();

	// 메시 임포트 시 FbxAxisSystem::DeepConvertScene이 이미 적용되어 있으므로
	// 애니메이션도 동일한 좌표계를 사용 (축 변환 없음)
	return FVector(
		static_cast<float>(Translation[0]),   // X
		static_cast<float>(Translation[1]),   // Y
		static_cast<float>(Translation[2])    // Z
	);
}

FQuat UFbxLoader::SampleRotation(FbxNode* Node, FbxAnimLayer* Layer, FbxTime Time)
{
	FbxAMatrix Transform = Node->EvaluateLocalTransform(Time);
	FbxQuaternion Quat = Transform.GetQ();

	// 메시 임포트 시 FbxAxisSystem::DeepConvertScene이 이미 적용되어 있으므로
	// 애니메이션도 동일한 좌표계를 사용 (축 변환 없음)
	return FQuat(
		static_cast<float>(Quat[0]),   // X
		static_cast<float>(Quat[1]),   // Y
		static_cast<float>(Quat[2]),   // Z
		static_cast<float>(Quat[3])    // W
	);
}

FVector UFbxLoader::SampleScale(FbxNode* Node, FbxAnimLayer* Layer, FbxTime Time)
{
	FbxAMatrix Transform = Node->EvaluateLocalTransform(Time);
	FbxVector4 Scale = Transform.GetS();

	// 메시 임포트 시 FbxAxisSystem::DeepConvertScene이 이미 적용되어 있으므로
	// 애니메이션도 동일한 좌표계를 사용 (축 변환 없음)
	return FVector(
		static_cast<float>(Scale[0]),   // X
		static_cast<float>(Scale[1]),   // Y
		static_cast<float>(Scale[2])    // Z
	);
}

FbxNode* UFbxLoader::FindNodeByName(FbxNode* RootNode, const char* NodeName)
{
	if (!RootNode || !NodeName)
	{
		return nullptr;
	}

	// 현재 노드 확인
	if (strcmp(RootNode->GetName(), NodeName) == 0)
	{
		return RootNode;
	}

	// 자식 노드 재귀 탐색
	for (int32 i = 0; i < RootNode->GetChildCount(); ++i)
	{
		FbxNode* Found = FindNodeByName(RootNode->GetChild(i), NodeName);
		if (Found)
		{
			return Found;
		}
	}

	return nullptr;
}

FbxNode* UFbxLoader::FindNodeByCanonicalName(FbxNode* RootNode, const char* CanonicalName)
{
	if (!RootNode || !CanonicalName)
	{
		return nullptr;
	}

	// 현재 노드의 이름에서 접두사 제거 후 비교
	FString NodeName = RootNode->GetName();
	FString NodeCanonicalName = FMixamoChainMapper::RemovePrefix(NodeName);

	if (NodeCanonicalName == CanonicalName)
	{
		return RootNode;
	}

	// 자식 노드 재귀 탐색
	for (int32 i = 0; i < RootNode->GetChildCount(); ++i)
	{
		FbxNode* Found = FindNodeByCanonicalName(RootNode->GetChild(i), CanonicalName);
		if (Found)
		{
			return Found;
		}
	}

	return nullptr;
}

bool UFbxLoader::IsArmatureNode(FbxNode* Node, FVector& OutScale)
{
	if (!Node)
	{
		return false;
	}

	FString NodeName = Node->GetName();

	// 1. 노드의 스케일 확인 (1,1,1이 아닌지)
	FbxDouble3 LocalScale = Node->LclScaling.Get();
	OutScale = FVector(static_cast<float>(LocalScale[0]),
	                   static_cast<float>(LocalScale[1]),
	                   static_cast<float>(LocalScale[2]));

	// 스케일이 거의 (1,1,1)인 경우 Armature가 아니거나 스케일 적용이 불필요
	const float Tolerance = 0.001f;
	bool bHasNonUniformScale = (FMath::Abs(OutScale.X - 1.0f) > Tolerance ||
	                             FMath::Abs(OutScale.Y - 1.0f) > Tolerance ||
	                             FMath::Abs(OutScale.Z - 1.0f) > Tolerance);

	// 2. 노드 타입 확인 (일반적으로 Null 타입이거나 Skeleton Root)
	FbxNodeAttribute* Attr = Node->GetNodeAttribute();
	bool bIsNullOrSkeletonRoot = (!Attr ||
	                               Attr->GetAttributeType() == FbxNodeAttribute::eNull ||
	                               Attr->GetAttributeType() == FbxNodeAttribute::eSkeleton);

	// 3. 이름 확인 (선택적 - Armature, armature 등)
	FString LowerNodeName = NodeName;
	std::transform(LowerNodeName.begin(), LowerNodeName.end(), LowerNodeName.begin(), ::tolower);
	bool bNameContainsArmature = LowerNodeName.find("armature") != std::string::npos;

	// 최종 판정 개선:
	// - 이름에 "armature"가 포함되고, 스케일이 (1,1,1)이 아니고, 타입이 적절한 경우 (엄격한 조건)
	// - 또는 타입이 적절하고 스케일이 (1,1,1)이 아닌 경우 (완화된 조건)
	bool bStrictMatch = bNameContainsArmature && bHasNonUniformScale && bIsNullOrSkeletonRoot;
	bool bRelaxedMatch = bIsNullOrSkeletonRoot && bHasNonUniformScale;

	return bStrictMatch || bRelaxedMatch;
}

FbxNode* UFbxLoader::FindArmatureNodeForRootBone(FbxNode* RootBoneNode, FbxNode* SceneRootNode)
{
	if (!RootBoneNode || !SceneRootNode)
	{
		return nullptr;
	}

	UE_LOG("  === Searching for Armature node ===");
	UE_LOG("  Root bone: '%s'", RootBoneNode->GetName());

	// 루트 본부터 씬 루트까지 모든 부모 체인을 순회
	FbxNode* CurrentNode = RootBoneNode->GetParent();
	int32 Depth = 1;

	while (CurrentNode && CurrentNode != SceneRootNode)
	{
		FString NodeName = CurrentNode->GetName();

		// 노드 스케일 확인
		FbxDouble3 LocalScale = CurrentNode->LclScaling.Get();
		FVector Scale(static_cast<float>(LocalScale[0]),
		              static_cast<float>(LocalScale[1]),
		              static_cast<float>(LocalScale[2]));

		// 노드 타입 확인
		FbxNodeAttribute* Attr = CurrentNode->GetNodeAttribute();
		FString AttrType = "None";
		if (Attr)
		{
			switch (Attr->GetAttributeType())
			{
			case FbxNodeAttribute::eNull: AttrType = "Null"; break;
			case FbxNodeAttribute::eSkeleton: AttrType = "Skeleton"; break;
			case FbxNodeAttribute::eMesh: AttrType = "Mesh"; break;
			default: AttrType = "Other"; break;
			}
		}

		UE_LOG("  [Depth %d] Node '%s': Scale=(%.3f, %.3f, %.3f), Type=%s",
		       Depth, NodeName.c_str(), Scale.X, Scale.Y, Scale.Z, AttrType.c_str());

		// non-uniform 스케일 체크 (매우 완화된 조건)
		const float Tolerance = 0.001f;
		bool bHasNonUniformScale = (FMath::Abs(Scale.X - 1.0f) > Tolerance ||
		                             FMath::Abs(Scale.Y - 1.0f) > Tolerance ||
		                             FMath::Abs(Scale.Z - 1.0f) > Tolerance);

		if (bHasNonUniformScale)
		{
			UE_LOG("  >>> Found node with non-uniform scale: '%s' at depth %d", NodeName.c_str(), Depth);
			UE_LOG("  >>> Using this as Armature node");
			return CurrentNode;
		}

		// 다음 부모로 이동
		CurrentNode = CurrentNode->GetParent();
		Depth++;
	}

	UE_LOG("  No node with non-uniform scale found in parent chain");
	return nullptr;
}

void UFbxLoader::ApplyArmatureScaleCorrection(UAnimDataModel* DataModel, const FSkeleton& TargetSkeleton, FbxNode* RootBoneNode, FbxNode* SceneRootNode, FbxAnimLayer* AnimLayer, FbxTime StartTime)
{
	if (!DataModel || !m_bNeedsScaleCorrection)
	{
		return;
	}

	UE_LOG("  === Applying Blender FBX Armature scale correction ===");

	// RootBoneNode가 없으면 이름으로 찾기 (캐시 로드 시)
	if (!RootBoneNode)
	{
		if (!SceneRootNode)
		{
			UE_LOG("  Error: Both RootBoneNode and SceneRootNode are null");
			return;
		}

		// 루트 본 이름 찾기
		FString RootBoneName = "";
		for (const auto& Bone : TargetSkeleton.Bones)
		{
			if (Bone.ParentIndex == -1)
			{
				RootBoneName = Bone.Name;
				break;
			}
		}

		if (RootBoneName.empty())
		{
			UE_LOG("  Warning: Could not find root bone name in skeleton");
			return;
		}

		// FBX 씬에서 루트 본 노드 찾기
		RootBoneNode = FindNodeByCanonicalName(SceneRootNode, RootBoneName.c_str());
		if (!RootBoneNode)
		{
			UE_LOG("  Warning: Could not find root bone node '%s' in FBX scene", RootBoneName.c_str());
			return;
		}

		UE_LOG("  Found root bone node by name: '%s'", RootBoneNode->GetName());
	}
	else
	{
		UE_LOG("  Using provided root bone node: '%s'", RootBoneNode->GetName());
	}

	// 1. 루트 본의 직접 부모가 Armature
	FbxNode* ArmatureNode = RootBoneNode->GetParent();
	if (!ArmatureNode)
	{
		UE_LOG("  Root bone has no parent, skipping scale correction");
		return;
	}

	UE_LOG("  Parent node (Armature): '%s'", ArmatureNode->GetName());

	// 2. Armature의 스케일 확인
	FVector ArmatureScale = SampleScale(ArmatureNode, AnimLayer, StartTime);
	UE_LOG("  Armature scale: (%.6f, %.6f, %.6f)", ArmatureScale.X, ArmatureScale.Y, ArmatureScale.Z);

	// 3. 스케일이 (1,1,1)인지 확인
	const float Tolerance = 0.001f;
	bool bHasNonUniformScale = (FMath::Abs(ArmatureScale.X - 1.0f) > Tolerance ||
	                             FMath::Abs(ArmatureScale.Y - 1.0f) > Tolerance ||
	                             FMath::Abs(ArmatureScale.Z - 1.0f) > Tolerance);

	if (!bHasNonUniformScale)
	{
		UE_LOG("  Armature scale is uniform (1,1,1), no correction needed");
		return;
	}

	// 4. 루트 본 이름 찾기 (애니메이션 트랙 매칭용)
	FString RootBoneName = "";
	for (const auto& Bone : TargetSkeleton.Bones)
	{
		if (Bone.ParentIndex == -1)
		{
			RootBoneName = Bone.Name;
			break;
		}
	}

	if (RootBoneName.empty())
	{
		UE_LOG("  Warning: Could not find root bone name in skeleton");
		return;
	}

	// 5. 루트 본의 애니메이션 트랙에 스케일 적용
	bool bAppliedScale = false;
	for (FBoneAnimationTrack& Track : DataModel->BoneAnimationTracks)
	{
		if (Track.BoneName == RootBoneName)
		{
			UE_LOG("  >>> Applying Armature scale to root bone '%s' animation", RootBoneName.c_str());
			UE_LOG("      Position keys: %d, Scale keys: %d", Track.InternalTrack.PosKeys.Num(), Track.InternalTrack.ScaleKeys.Num());

			for (FVector& PosKey : Track.InternalTrack.PosKeys)
			{
				PosKey.X *= ArmatureScale.X;
				PosKey.Y *= ArmatureScale.Y;
				PosKey.Z *= ArmatureScale.Z;
			}

			for (FVector& ScaleKey : Track.InternalTrack.ScaleKeys)
			{
				ScaleKey.X *= ArmatureScale.X;
				ScaleKey.Y *= ArmatureScale.Y;
				ScaleKey.Z *= ArmatureScale.Z;
			}

			bAppliedScale = true;
			break;
		}
	}

	if (bAppliedScale)
	{
		UE_LOG("  >>> Scale correction applied successfully!");
	}
	else
	{
		UE_LOG("  Warning: Root bone '%s' not found in animation tracks", RootBoneName.c_str());
	}
}

FSkeleton* UFbxLoader::ExtractSkeletonFromFbx(const FString& FilePath)
{
	// FBX SDK는 스레드 안전하지 않으므로 mutex로 보호
	std::lock_guard<std::mutex> FbxLock(GFbxSdkMutex);

	if (!SdkManager)
	{
		UE_LOG("FBX SDK Manager not initialized");
		return nullptr;
	}

	FbxImporter* Importer = FbxImporter::Create(SdkManager, "");
	FbxScene* Scene = FbxScene::Create(SdkManager, "TempScene");

	// 2. FBX 파일 로드
	std::string NarrowPath = WideToUTF8(UTF8ToWide(FilePath));
	if (!Importer->Initialize(NarrowPath.c_str(), -1, SdkManager->GetIOSettings()))
	{
		UE_LOG("Failed to initialize FBX Importer: %s", FilePath.c_str());
		Importer->Destroy();
		Scene->Destroy();
		return nullptr;
	}

	if (!Importer->Import(Scene))
	{
		UE_LOG("Failed to import FBX scene: %s", FilePath.c_str());
		Importer->Destroy();
		Scene->Destroy();
		return nullptr;
	}

	Importer->Destroy();

	// 좌표계 변환 (메시 로딩과 동일하게 적용)
	FbxAxisSystem UnrealImportAxis(FbxAxisSystem::eZAxis, FbxAxisSystem::eParityEven, FbxAxisSystem::eLeftHanded);
	FbxAxisSystem SourceSetup = Scene->GetGlobalSettings().GetAxisSystem();

	// 단위 변환: FBX의 원본 단위를 확인하고 엔진 단위(m)로 변환
	FbxSystemUnit SceneSystemUnit = Scene->GetGlobalSettings().GetSystemUnit();
	if (SceneSystemUnit != FbxSystemUnit::m)
	{
		// 원본 단위를 미터로 변환
		FbxSystemUnit::m.ConvertScene(Scene);
		UE_LOG("FBX 단위 변환: %s -> m", SceneSystemUnit.GetScaleFactorAsString().Buffer());
	}

	if (SourceSetup != UnrealImportAxis)
	{
		UE_LOG("Fbx 스켈레톤 축 변환 완료!\n");
		UnrealImportAxis.DeepConvertScene(Scene);
	}

	// 3. Skeleton 추출
	FSkeleton* ExtractedSkeleton = new FSkeleton();

	// Skeleton Name을 FBX 파일명에서 추출
	FString NormalizedPath = NormalizePath(FilePath);
	FWideString WPath = UTF8ToWide(NormalizedPath);
	std::filesystem::path FbxFilePath(WPath);
	ExtractedSkeleton->Name = WideToUTF8(FbxFilePath.stem().wstring());

	TMap<FbxNode*, int32> BoneToIndex;

	FbxNode* RootNode = Scene->GetRootNode();
	if (RootNode)
	{
		// 재귀적으로 Skeleton 구조 추출
		std::function<void(FbxNode*, int32)> ExtractSkeletonRecursive = [&](FbxNode* Node, int32 ParentIndex)
		{
			if (!Node)
				return;

			FbxNodeAttribute* Attribute = Node->GetNodeAttribute();
			if (Attribute && Attribute->GetAttributeType() == FbxNodeAttribute::eSkeleton)
			{
				FBone Bone;
				// Mixamo 접두사 제거 (mixamorig:, mixamorig9: 등)
				Bone.Name = FMixamoChainMapper::RemovePrefix(FString(Node->GetName()));
				Bone.ParentIndex = ParentIndex;

				// BindPose Transform (Global 변환)
				FbxAMatrix BoneBindGlobal = Node->EvaluateGlobalTransform();
				FbxAMatrix BoneBindGlobalInv = BoneBindGlobal.Inverse();

				// FbxAMatrix → FMatrix 변환 (기존 LoadFbxMesh 방식과 동일)
				for (int Row = 0; Row < 4; Row++)
				{
					for (int Col = 0; Col < 4; Col++)
					{
						Bone.BindPose.M[Row][Col] = static_cast<float>(BoneBindGlobal[Row][Col]);
						Bone.InverseBindPose.M[Row][Col] = static_cast<float>(BoneBindGlobalInv[Row][Col]);
					}
				}

				int32 BoneIndex = static_cast<int32>(ExtractedSkeleton->Bones.Num());
				ExtractedSkeleton->Bones.push_back(Bone);
				ExtractedSkeleton->BoneNameToIndex[Bone.Name] = BoneIndex;
				BoneToIndex[Node] = BoneIndex;

				ParentIndex = BoneIndex;
			}

			// 자식 노드 순회
			for (int32 i = 0; i < Node->GetChildCount(); ++i)
			{
				ExtractSkeletonRecursive(Node->GetChild(i), ParentIndex);
			}
		};

		// Root 노드부터 시작
		for (int32 i = 0; i < RootNode->GetChildCount(); ++i)
		{
			ExtractSkeletonRecursive(RootNode->GetChild(i), -1);
		}
	}

	Scene->Destroy();

	if (ExtractedSkeleton->Bones.Num() == 0)
	{
		delete ExtractedSkeleton;
		return nullptr;
	}

	// 캐시 데이터 초기화 (CRITICAL FIX)
	ExtractedSkeleton->InitializeCachedData();

	// Mixamo 접두사는 이미 본 읽기 단계에서 제거됨 (line 2142)
	UE_LOG("Extracted skeleton from '%s' (%d bones)", FilePath.c_str(), ExtractedSkeleton->Bones.Num());

	return ExtractedSkeleton;
}

void UFbxLoader::PrintAllSkeletonHierarchies()
{
	UE_LOG("\n========================================");
	UE_LOG("SKELETAL MESHES & ANIMATIONS BONE HIERARCHY");
	UE_LOG("========================================\n");

	// 모든 SkeletalMesh 출력
	TArray<USkeletalMesh*> AllSkeletalMeshes = RESOURCE.GetAll<USkeletalMesh>();

	UE_LOG("=== SKELETAL MESHES (%d) ===\n", AllSkeletalMeshes.Num());

	for (USkeletalMesh* Mesh : AllSkeletalMeshes)
	{
		if (Mesh && Mesh->GetSkeleton())
		{
			FString Title = "SkeletalMesh: " + Mesh->GetName();
			PrintSkeletonHierarchy(Mesh->GetSkeleton(), Title);
			UE_LOG(""); // 빈 줄
		}
	}

	// 모든 AnimSequence 출력
	TArray<UAnimSequence*> AllAnimSequences = RESOURCE.GetAll<UAnimSequence>();

	UE_LOG("\n=== ANIMATIONS (%d) ===\n", AllAnimSequences.Num());

	// TODO: AnimSequence 손상 문제 해결 후 활성화
	// for (UAnimSequence* AnimSeq : AllAnimSequences)
	// {
	// 	if (AnimSeq)
	// 	{
	// 		UAnimDataModel* DataModel = AnimSeq->GetDataModel();
	// 		if (DataModel && DataModel->GetSkeleton())
	// 		{
	// 			FString Title = "Animation: " + AnimSeq->GetName();
	// 			PrintSkeletonHierarchy(DataModel->GetSkeleton(), Title);
	// 			UE_LOG(""); // 빈 줄
	// 		}
	// 	}
	// }

	UE_LOG("========================================\n");
}

void UFbxLoader::PrintSkeletonHierarchy(const FSkeleton* Skeleton, const FString& Title)
{
	if (!Skeleton)
		return;

	UE_LOG("--- %s ---", Title.c_str());
	UE_LOG("Total Bones: %d", Skeleton->Bones.Num());

	// 루트 본 찾기 (ParentIndex == -1)
	for (int32 i = 0; i < Skeleton->Bones.Num(); ++i)
	{
		const FBone& Bone = Skeleton->Bones[i];
		if (Bone.ParentIndex == -1)
		{
			// 루트 본부터 재귀적으로 출력
			PrintBoneRecursive(Skeleton, i, 0);
		}
	}
}

void UFbxLoader::PrintBoneRecursive(const FSkeleton* Skeleton, int32 BoneIndex, int32 Depth)
{
	if (!Skeleton || BoneIndex < 0 || BoneIndex >= Skeleton->Bones.Num())
		return;

	const FBone& Bone = Skeleton->Bones[BoneIndex];

	// 들여쓰기 생성
	FString Indent = "";
	for (int32 i = 0; i < Depth; ++i)
	{
		if (i == Depth - 1)
			Indent += "  ㄴ";
		else
			Indent += "  ";
	}

	// 본 이름 출력
	UE_LOG("%s%s", Indent.c_str(), Bone.Name.c_str());

	// 자식 본 찾기
	for (int32 i = 0; i < Skeleton->Bones.Num(); ++i)
	{
		const FBone& ChildBone = Skeleton->Bones[i];
		if (ChildBone.ParentIndex == BoneIndex)
		{
			PrintBoneRecursive(Skeleton, i, Depth + 1);
		}
	}
}

void UFbxLoader::SaveAllAnimSequencesToAnimFiles()
{
	UE_LOG("FbxLoader: SaveAllAnimSequencesToAnimFiles: Saving animations to .anim files...");

	TArray<UAnimSequence*> AllAnimSequences = RESOURCE.GetAll<UAnimSequence>();
	UE_LOG("FbxLoader: SaveAllAnimSequencesToAnimFiles: Found %d AnimSequences", AllAnimSequences.Num());

	int32 SavedCount = 0;

	for (int32 i = 0; i < AllAnimSequences.Num(); ++i)
	{
		try
		{
			UAnimSequence* AnimSeq = AllAnimSequences[i];
			if (!AnimSeq)
			{
				continue;
			}

			// Name이 비어있으면 손상된 AnimSequence이므로 건너뜀
			if (AnimSeq->Name.empty())
			{
				continue;
			}

			// FilePath 직접 접근 (복사 방지)
			const FString& CurrentPath = AnimSeq->GetFilePath();
			if (CurrentPath.empty() || CurrentPath.size() > 1024)
			{
				continue;
			}

			// .anim 경로 결정
			FString AnimFilePath;

			if (CurrentPath.ends_with(".anim"))
			{
				// 이미 .anim 경로가 설정되어 있음
				AnimFilePath = CurrentPath;
			}
			else
			{
				// FBX 경로에서 .anim 경로 생성
				// 경로 형식: "FBX경로#AnimStackName" 또는 "FBX경로"
				FString FbxPath = CurrentPath;
				FString AnimStackName;

				size_t HashPos = CurrentPath.find('#');
				if (HashPos != FString::npos)
				{
					// "FBX경로#AnimStackName" 형식
					FbxPath = CurrentPath.substr(0, HashPos);
					AnimStackName = CurrentPath.substr(HashPos + 1);
				}
				else
				{
					// "FBX경로" 형식 - Name에서 AnimStackName 추출
					// Name 형식: "FBX파일명_AnimStackName"
					std::filesystem::path PathObj(FbxPath);
					FString FileName = PathObj.stem().string();
					FString AnimName = AnimSeq->GetName();

					// FBX파일명_ 접두사 제거하여 AnimStackName 추출
					if (AnimName.find(FileName + "_") == 0)
					{
						AnimStackName = AnimName.substr(FileName.size() + 1);
					}
					else
					{
						AnimStackName = AnimName;
					}
				}

				// .anim 경로 생성: FBX경로_AnimStackName.anim
				size_t DotPos = FbxPath.find_last_of('.');
				if (DotPos != FString::npos)
				{
					AnimFilePath = FbxPath.substr(0, DotPos) + "_" + AnimStackName + ".anim";
				}
				else
				{
					AnimFilePath = FbxPath + "_" + AnimStackName + ".anim";
				}
			}

			// .anim 파일이 이미 존재하면 건너뜀
			FWideString WAnimFilePath = UTF8ToWide(AnimFilePath);
			std::filesystem::path AnimFilePathObj(WAnimFilePath);
			if (std::filesystem::exists(AnimFilePathObj))
			{
				continue;
			}

			// .anim 파일 저장
			// 디렉토리 생성
			if (AnimFilePathObj.has_parent_path())
			{
				std::filesystem::create_directories(AnimFilePathObj.parent_path());
			}

			FWindowsBinWriter AnimWriter(AnimFilePath);

			// Name 저장
			FString AnimName = AnimSeq->GetName();
			Serialization::WriteString(AnimWriter, AnimName);

			// Notifies 저장
			uint32 NotifyCount = static_cast<uint32>(AnimSeq->Notifies.Num());
			AnimWriter << NotifyCount;
			for (int32 j = 0; j < AnimSeq->Notifies.Num(); ++j)
			{
				AnimWriter << AnimSeq->Notifies[j];
			}

			// DataModel 저장
			UAnimDataModel* DataModel = AnimSeq->GetDataModel();
			if (DataModel)
			{
				AnimWriter << *DataModel;
			}

			AnimWriter.Close();

			UE_LOG("FbxLoader: SaveAllAnimSequencesToAnimFiles: Saved %s", AnimFilePath.c_str());
			++SavedCount;
		}
		catch (const std::exception& e)
		{
			UE_LOG("FbxLoader: SaveAllAnimSequencesToAnimFiles: Exception at [%d]: %s", i, e.what());
		}
	}

	UE_LOG("FbxLoader: SaveAllAnimSequencesToAnimFiles: Completed, saved %d .anim files", SavedCount);
}

// 재귀적으로 노드 트리에서 메시 존재 여부 검사
static bool HasMeshNodeRecursive(FbxNode* Node)
{
	if (!Node)
	{
		return false;
	}

	// 현재 노드에서 메시 어트리뷰트 확인
	FbxNodeAttribute* Attribute = Node->GetNodeAttribute();
	if (Attribute && Attribute->GetAttributeType() == FbxNodeAttribute::eMesh)
	{
		FbxMesh* Mesh = static_cast<FbxMesh*>(Attribute);
		// 실제 버텍스가 있는 메시인지 확인
		if (Mesh && Mesh->GetControlPointsCount() > 0)
		{
			return true;
		}
	}

	// 자식 노드 재귀 검사
	for (int32 i = 0; i < Node->GetChildCount(); ++i)
	{
		if (HasMeshNodeRecursive(Node->GetChild(i)))
		{
			return true;
		}
	}

	return false;
}

bool UFbxLoader::GetFbxContentType(const FString& FilePath, bool& OutHasMesh, bool& OutHasAnimation)
{
	OutHasMesh = false;
	OutHasAnimation = false;

	if (FilePath.empty())
	{
		return false;
	}

	// 경로 정규화
	FString NormalizedPath = NormalizePath(FilePath);
	FWideString WNormalizedPath = UTF8ToWide(NormalizedPath);
	FString FbxPathAcp = UTF8ToACP(NormalizedPath);

	// FBX SDK 스레드 안전성
	extern std::mutex GFbxSdkMutex;
	std::lock_guard<std::mutex> FbxLock(GFbxSdkMutex);

	// FBX Importer 생성
	FbxImporter* Importer = FbxImporter::Create(SdkManager, "ContentTypeChecker");
	if (!Importer->Initialize(FbxPathAcp.c_str(), -1, SdkManager->GetIOSettings()))
	{
		UE_LOG("UFbxLoader::GetFbxContentType: Failed to initialize FBX importer for %s", FilePath.c_str());
		Importer->Destroy();
		return false;
	}

	// 씬 생성 및 임포트
	FbxScene* Scene = FbxScene::Create(SdkManager, "ContentTypeScene");
	if (!Importer->Import(Scene))
	{
		UE_LOG("UFbxLoader::GetFbxContentType: Failed to import FBX scene from %s", FilePath.c_str());
		Scene->Destroy();
		Importer->Destroy();
		return false;
	}
	Importer->Destroy();

	// 애니메이션 존재 여부 확인
	int32 AnimStackCount = Scene->GetSrcObjectCount<FbxAnimStack>();
	OutHasAnimation = (AnimStackCount > 0);

	// 메시 존재 여부 확인 (재귀적으로 모든 노드 검사)
	FbxNode* RootNode = Scene->GetRootNode();
	if (RootNode)
	{
		OutHasMesh = HasMeshNodeRecursive(RootNode);
	}

	// 정리
	Scene->Destroy();

	return true;
}

