#include "CharacterStateComponent.h"
#include "Attributes/AttributeComponent.h"
#include "GameFramework/Actor.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Components/InstancedStaticMeshComponent.h"

UCharacterStateComponent::UCharacterStateComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
}

void UCharacterStateComponent::BeginPlay()
{
    Super::BeginPlay();
    if (AActor* Owner = GetOwner())
    {
        TargetWorldLocation = Owner->GetActorLocation();
    }
}

void UCharacterStateComponent::MoveInputWASD(FVector2D Direction)
{
    if (bIsMoving || Direction.IsNearlyZero()) return;

    FVector FinalTargetLoc;
    
    // 调用判断函数  返回true能走，且 FinalTargetLoc 已经被赋上了正确的坐标
    if (IsGridMove(Direction, FinalTargetLoc))
    {
        TargetWorldLocation = FinalTargetLoc;
        bIsMoving = true;
        
        SetComponentTickEnabled(true);
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("【移动拦截】前方不可通行！"));
    }
}

bool UCharacterStateComponent::IsGridMove(FVector2D Direction,FVector& OutTargetLoc) const
{
    AActor* Owner = GetOwner();
    APawn* OwnerPawn = Cast<APawn>(Owner);
    if (!OwnerPawn || !OwnerPawn->GetController()) return false;

    // 1. 基础视角方向换算
    FRotator ControlRot = OwnerPawn->GetController()->GetControlRotation();
    FRotator YawRot(0.0f, ControlRot.Yaw, 0.0f);
    FVector ForwardVector = YawRot.Vector();
    FVector RightVector = FRotationMatrix(YawRot).GetUnitAxis(EAxis::Y);

    FVector DesiredWorldDir = (ForwardVector * Direction.Y) + (RightVector * Direction.X);

    FVector MoveOffset = FVector::ZeroVector;
    if (FMath::Abs(DesiredWorldDir.X) > FMath::Abs(DesiredWorldDir.Y))
    {
        MoveOffset.X = DesiredWorldDir.X > 0.0f ? MoveDistance : -MoveDistance;
    }
    else
    {
        MoveOffset.Y = DesiredWorldDir.Y > 0.0f ? MoveDistance : -MoveDistance;
    }

    FVector RoughTargetLoc = Owner->GetActorLocation() + MoveOffset;
    
    // 2. 向下检测格子实例
    FHitResult FloorHit;
    FVector FloorBoxSize = FVector(40.0f, 40.0f, 10.0f); 
    
    TArray<TEnumAsByte<EObjectTypeQuery>> FloorTypes;
    FloorTypes.Add(UEngineTypes::ConvertToObjectType(ECC_WorldStatic));
    FloorTypes.Add(UEngineTypes::ConvertToObjectType(ECC_WorldDynamic));

    TArray<AActor*> ActorsToIgnore;
    ActorsToIgnore.Add(Owner);

    bool bHitFloor = UKismetSystemLibrary::BoxTraceSingleForObjects(
        GetWorld(), RoughTargetLoc, RoughTargetLoc - FVector(0.0f, 0.0f, 200.0f),
        FloorBoxSize, FRotator::ZeroRotator, FloorTypes, false, ActorsToIgnore,
        EDrawDebugTrace::None, FloorHit, true
    );

    // 3. 必须击中组件，且必须是实例化静态网格体
    UInstancedStaticMeshComponent* ISMC = Cast<UInstancedStaticMeshComponent>(FloorHit.GetComponent());
    
    // 如果转不成 ISMC，或者 Item 是 INDEX_NONE (-1)，说明踩到了边缘外，直接返回 false
    if (!bHitFloor || !ISMC || FloorHit.Item == INDEX_NONE)
    {
        return false; 
    }

    // 4. 此时绝对安全，精准拿到这块格子实例的世界坐标
    FTransform InstanceTransform;
    ISMC->GetInstanceTransform(FloorHit.Item, InstanceTransform, true);
    FVector AbsoluteGridCenter = InstanceTransform.GetLocation();
    
    // 5. 将计算好的合法坐标赋值给引用参数 OutTargetLoc
    OutTargetLoc = FVector(AbsoluteGridCenter.X, AbsoluteGridCenter.Y, Owner->GetActorLocation().Z);
    
    // 以后如果需要扩展“格子上有没有人”的检测，也可以继续写在这里
    // if (格子上有人) return false;

    return true;
}


void UCharacterStateComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (bIsMoving)
    {
        AActor* Owner = GetOwner();
        if (!Owner) return;

        // 平滑插值到目标位置
        FVector NewLoc = FMath::VInterpTo(Owner->GetActorLocation(), TargetWorldLocation, DeltaTime, MoveLerpSpeed);
        Owner->SetActorLocation(NewLoc);

        // 如果距离目标不到 1 个单位，强制对齐并停止移动，允许下一次按键
        if (FVector::DistSquared(Owner->GetActorLocation(), TargetWorldLocation) < 1.0f)
        {
            Owner->SetActorLocation(TargetWorldLocation);
            bIsMoving = false;
            
            
            SetComponentTickEnabled(false);
        }
    }
}








