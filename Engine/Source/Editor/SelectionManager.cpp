#include "pch.h"
#include "SelectionManager.h"
#include "Actor.h"
#include "SceneComponent.h"

IMPLEMENT_CLASS(USelectionManager)

void USelectionManager::SelectActor(AActor* Actor)
{
    if (!Actor) return;
    
    // 단일 선택 모드 (기존 선택 해제)
    ClearSelection();
    
    // 새 액터 선택
    SelectedActors.Add(Actor);
    SelectedComponent = Actor->GetRootComponent();
    bIsActorMode = true;
}

void USelectionManager::SelectComponent(UActorComponent* Component)
{
    if (!Component)
    {
        SelectedComponent = nullptr;
        bIsActorMode = false;
        return;
    }

    AActor* OwnerActor = Component->GetOwner();
    if (!OwnerActor)
    {
        return;
    }

    // 에디터블하지 않은 컴포넌트는 부모 또는 루트로 대체
    UActorComponent* ComponentToSelect = Component;
    if (!Component->IsEditable())
    {
        if (USceneComponent* SceneComp = Cast<USceneComponent>(Component))
        {
            if (USceneComponent* Parent = SceneComp->GetAttachParent())
            {
                ComponentToSelect = Parent;
            }
            else
            {
                ComponentToSelect = OwnerActor->GetRootComponent();
            }
        }
        else
        {
            ComponentToSelect = OwnerActor->GetRootComponent();
        }
    }

    SelectedComponent = ComponentToSelect;
    bIsActorMode = false;
}

void USelectionManager::SelectActorAndComponent(AActor* Actor, UActorComponent* Component)
{
    if (!Actor || !Component)
    {
        return;
    }

    // 액터가 선택되어 있지 않으면 먼저 선택
    if (!IsActorSelected(Actor))
    {
        ClearSelection();
        SelectedActors.Add(Actor);
    }

    // 컴포넌트 선택 (컴포넌트 모드)
    SelectComponent(Component);
}

void USelectionManager::DeselectActor(AActor* Actor)
{
    if (!Actor)
    {
        return;
    }
    
    bool bDeselected = false;

    auto It = std::find(SelectedActors.begin(), SelectedActors.end(), Actor);
    if (It != SelectedActors.end())
    {
        SelectedActors.erase(It);
        bDeselected = true;
    }

    if (bDeselected)
    {
        SelectedComponent = nullptr;
    }
}

void USelectionManager::ClearSelection()
{
    for (AActor* Actor : SelectedActors)
    {
        if (Actor) // null 체크 추가
        {
            Actor->SetIsPicked(false);
        }
    }
    SelectedActors.clear();
    SelectedComponent = nullptr;
}

bool USelectionManager::IsActorSelected(AActor* Actor) const
{
    if (!Actor) return false;
    
    return std::find(SelectedActors.begin(), SelectedActors.end(), Actor) != SelectedActors.end();
}

AActor* USelectionManager::GetSelectedActor() const
{
    // 첫 번째 유효한 액터 연기
    for (AActor* Actor : SelectedActors)
    {
        if (Actor) return Actor;
    }
    return nullptr;
}

void USelectionManager::CleanupInvalidActors()
{
    // null이거나 삭제된 액터들을 제거
    auto it = std::remove_if(SelectedActors.begin(), SelectedActors.end(), 
        [](AActor* Actor) { return Actor == nullptr; });
    SelectedActors.erase(it, SelectedActors.end());
}

USelectionManager::USelectionManager()
{
    SelectedActors.Reserve(1);
}

USelectionManager::~USelectionManager()
{
    // No-op: instances are destroyed by ObjectFactory::DeleteAll
}
