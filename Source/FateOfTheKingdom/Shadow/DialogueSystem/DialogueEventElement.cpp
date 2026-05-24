
#include "DialogueEventElement.h"

UDialogueEventElement::UDialogueEventElement()
{
    World = GetOuter()->GetWorld();
    OnBeginplay();


}

UWorld* UDialogueEventElement::GetWorld() const
{
    if(Owner) return Owner->GetWorld();
    if(World) return World;
        UE_LOG(LogTemp, Warning, TEXT("UDialogueEventElement::GetWorld() - Owner and Outer have no valid world!"));
        return nullptr;
    // return Owner ? Owner->GetWorld() : nullptr;
}
