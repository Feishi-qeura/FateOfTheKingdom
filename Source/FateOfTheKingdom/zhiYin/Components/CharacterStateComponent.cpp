#include "CharacterStateComponent.h"
#include "../Public/Attributes/AttributeComponent.h"
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

void UCharacterStateComponent::TryAttackLookAtTarget(float MaxAttackRangeGrids)
{
    AActor* Owner = GetOwner();
    APawn* OwnerPawn = Cast<APawn>(Owner);
    if (!OwnerPawn) return;

    APlayerController* PC = Cast<APlayerController>(OwnerPawn->GetController());
    if (!PC) return;
    
    //检查玩家自身的行动点是否足够
    UAttributeComponent* MyAttributes = Owner->FindComponentByClass<UAttributeComponent>();
    if (!MyAttributes || MyAttributes->BaseAttribute.CurrentActionPoint < 1)
    {
        UE_LOG(LogTemp, Warning, TEXT("【战斗拦截】行动点不足，无法攻击！"));
        return;
    }


    //第一人称视线准星射线检测（寻找玩家正在看着的敌人）
    FVector CameraLocation;
    FRotator CameraRotation;
    PC->GetPlayerViewPoint(CameraLocation, CameraRotation); // 获取第一人称相机视点

    FVector TraceEnd = CameraLocation + (CameraRotation.Vector() * 2000.0f); // 射线向前延伸 20 米

    FHitResult HitResult;
    TArray<AActor*> ToIgnore;
    ToIgnore.Add(Owner);

    TArray<TEnumAsByte<EObjectTypeQuery>> PawnType;
    PawnType.Add(UEngineTypes::ConvertToObjectType(ECC_Pawn)); // 只看角色

    bool bHit = UKismetSystemLibrary::LineTraceSingleForObjects(
        GetWorld(), CameraLocation, TraceEnd, PawnType, false, ToIgnore,
        EDrawDebugTrace::None, HitResult, true
    );

    if (!bHit || !HitResult.GetActor())
    {
        UE_LOG(LogTemp, Warning, TEXT("【战斗拦截】准星没有对准任何目标！"));
        return;
    }

    AActor* TargetEnemy = HitResult.GetActor();
    
    //基于格子距离（Grid Distance）的范围校验
    float Distance2D = FVector::Dist2D(Owner->GetActorLocation(), TargetEnemy->GetActorLocation());
    // 换算成格子数
    float GridDistance = Distance2D / MoveDistance;
    
    int32 ActualGridRange = FMath::RoundToInt(GridDistance);

    if (ActualGridRange > MaxAttackRangeGrids)
    {
        UE_LOG(LogTemp, Warning, TEXT("【战斗拦截】目标太远了！当前距离 %d 格，武器范围 %f 格"), ActualGridRange, MaxAttackRangeGrids);
        return;
    }
    
    //校验通过，扣除 1 点行动点，并进入伤害结算
    MyAttributes->BaseAttribute.CurrentActionPoint -= 1;
    
    UE_LOG(LogTemp, Log, TEXT("成功消耗 1 点行动点发起攻击！剩余 AP: %d"), MyAttributes->BaseAttribute.CurrentActionPoint);
    
    ExecuteDamage(Owner, TargetEnemy);
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

void UCharacterStateComponent::ExecuteDamage(const AActor* Attacker,const AActor* Target)
{
    UAttributeComponent* AttackerAttr = Attacker->FindComponentByClass<UAttributeComponent>();
    UAttributeComponent* TargetAttr = Target->FindComponentByClass<UAttributeComponent>();

    if (!AttackerAttr || !TargetAttr) return;

    //读取攻击方的战斗属性（力量 Strength）
    float AttackPower = AttackerAttr->GetCombatAttribute().Strength;
    //读取防守方的战斗属性（物理抗性 PhysicalResistance）
    float DefensePower = TargetAttr->GetCombatAttribute().PhysicalResistance;
    //伤害公式（基础公式：伤害 = 攻击力 - 防御力，最小保底 1 点伤害）
    float FinalDamage = FMath::Max(1.0f, AttackPower - DefensePower);
    //应用伤害：直接修改对方的当前生命值
    FBaseAttribute TargetBase = TargetAttr->GetBaseAttribute();
    TargetBase.CurrentHealth = FMath::Max(0.0f, TargetBase.CurrentHealth - FinalDamage);
    
    // 把修改后的结构体重新写回对方的组件中
    TargetAttr->SetBaseAttribute(TargetBase);

    UE_LOG(LogTemp, Log, TEXT("【战斗结算】%s 攻击了 %s，造成伤害: %f，对方剩余血量: %f"), 
        *Attacker->GetName(), *Target->GetName(), FinalDamage, TargetBase.CurrentHealth);
    // 6. 检查死亡
    if (TargetBase.CurrentHealth <= 0.0f)
    {
        UE_LOG(LogTemp, Warning, TEXT("%s 已被击败！"), *Target->GetName());
        // Target->Destroy(); // 或者触发死亡动画
    }
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








