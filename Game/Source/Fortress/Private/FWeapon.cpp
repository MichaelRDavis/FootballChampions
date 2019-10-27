// Fill out your copyright notice in the Description page of Project Settings.

#include "FWeapon.h"
#include "FCharacter.h"
#include "FPlayerController.h"
#include "FProjectile.h"
#include "Components/SkeletalMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "DrawDebugHelpers.h"
#include "TimerManager.h"

#define COLLISION_WEAPON ECC_GameTraceChannel2

AFWeapon::AFWeapon()
{
	Mesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("WeaponMesh1P"));
	Mesh->SetupAttachment(RootComponent);
	Mesh->SetCollisionObjectType(ECC_WorldDynamic);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	Mesh->bReceivesDecals = false;
	Mesh->CastShadow = false;
	Mesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;

	Ammo = 0;
	MagazineSize = 0;
	MaxAmmo = 100;
	MaxMagazineSize = 20;
	InitialMagazines = 4;
	bInfiniteAmmo = false;
	bInfiniteMagazine = false;
	CurrentState = EWeaponState::EIdle;
	FireRate = 0.2f;
	Spread = 5.0f;
	MaxTracerRange = 10000.0f;
	bIsEquipped = false;
	bIsFiring = false;
	bIsReloading = false;
	LastFireTime = 0.0f;

	SetReplicates(true);
	bNetUseOwnerRelevancy = true;
	PrimaryActorTick.bCanEverTick = true;
}

void AFWeapon::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (bIsFiring)
	{
		ApplyRecoil(DeltaSeconds);
	}
}

void AFWeapon::BeginPlay()
{
	Super::BeginPlay();

	if (InitialMagazines > 0)
	{
		MagazineSize = MaxMagazineSize;
		Ammo = MaxMagazineSize * InitialMagazines;
	}
}

void AFWeapon::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(AFWeapon, Ammo, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(AFWeapon, MagazineSize, COND_OwnerOnly);
}

void AFWeapon::AddAmmo(int32 AddAmount)
{
	const int32 AmmoDelta = FMath::Max(0, MaxAmmo - Ammo);
	AddAmount = FMath::Min(AddAmount, AmmoDelta);
	Ammo += AddAmount;
}

void AFWeapon::ConsumeAmmo()
{
	MagazineSize--;
	Ammo--;
}

bool AFWeapon::CanFire() const
{
	return CurrentState == EWeaponState::EIdle || CurrentState == EWeaponState::EFiring;
}

bool AFWeapon::CanReload() const
{
	return MagazineSize < MaxMagazineSize && Ammo - MagazineSize > 0;
}

void AFWeapon::OnEquip()
{
	AttachToOwner();
	bIsEquipped = true;
}

void AFWeapon::UnEquip()
{
	DetachFromOwner();
	bIsEquipped = false;
}

void AFWeapon::K2_Fire_Implementation()
{
	
}

void AFWeapon::FireShot()
{
	if (MagazineSize > 0 && CanFire())
	{
		Fire();
		ConsumeAmmo();
		PlayFiringEffects();
	}

	if (FOwner)
	{
		bool bRefiring = CurrentState == EWeaponState::EFiring && FireRate > 0.0f;
		if (bRefiring)
		{
			GetWorldTimerManager().SetTimer(FiringTimer, this, &AFWeapon::FireShot, FireRate, false);
		}
	}

	LastFireTime = GetWorld()->GetTimeSeconds();
}

void AFWeapon::StartFire()
{
	if (Role < ROLE_Authority)
	{
		ServerStartFire();
	}

	if (!bIsFiring)
	{
		bIsFiring = true;
		UpdateWeaponState();
	}
}

void AFWeapon::StopFire()
{
	if (Role < ROLE_Authority)
	{
		ServerStopFire();
	}

	if (bIsFiring)
	{
		bIsFiring = false;
		UpdateWeaponState();
	}
}

void AFWeapon::ServerStartFire_Implementation()
{
	StartFire();
}

bool AFWeapon::ServerStartFire_Validate()
{
	return true;
}

void AFWeapon::ServerStopFire_Implementation()
{
	StopFire();
}

bool AFWeapon::ServerStopFire_Validate()
{
	return true;
}

void AFWeapon::Reload()
{
	int32 ClipDelta = FMath::Min(MaxMagazineSize - MagazineSize, Ammo - MagazineSize);
	if (ClipDelta > 0)
	{
		MagazineSize += ClipDelta;
	}
}

void AFWeapon::StartReload()
{
	if (Role < ROLE_Authority)
	{
		ServerStartReload();
	}

	if (CanReload())
	{
		bIsReloading = true;
		Reload();
	}

	// TODO: Play reload animation
	// TODO: Play reload sound
	// TODO: Notify player HUD of reload
}

void AFWeapon::StopReload()
{
	if (CurrentState == EWeaponState::EReloading)
	{
		bIsReloading = false;
	}
}

void AFWeapon::ServerStartReload_Implementation()
{
	StartReload();
}

bool AFWeapon::ServerStartReload_Validate()
{
	return true;
}

void AFWeapon::ServerStopReload_Implementation()
{
	StopReload();
}

bool AFWeapon::ServerStopReload_Validate()
{
	return true;
}

void AFWeapon::GoToWeaponState(EWeaponState NewWeaponState)
{
	const EWeaponState LocalCurrentState = CurrentState;

	if (LocalCurrentState == EWeaponState::EFiring && NewWeaponState != EWeaponState::EFiring)
	{
		EndFiring();
	}

	CurrentState = NewWeaponState;

	if (LocalCurrentState != EWeaponState::EFiring && NewWeaponState == EWeaponState::EFiring)
	{
		BeginFiring();
	}
}

void AFWeapon::UpdateWeaponState()
{
	EWeaponState NewState = EWeaponState::EIdle;

	if (bIsEquipped)
	{
		//if (CanReload() == false)
		//{
		//	NewState = CurrentState;
		//}
		//else
		//{
		//	NewState = EWeaponState::EReloading;
		//}

		if (bIsFiring == true && CanFire() == true)
		{
			NewState = EWeaponState::EFiring;
		}
	}

	GoToWeaponState(NewState);
}

void AFWeapon::BeginFiring()
{
	const float GameTime = GetWorld()->GetTimeSeconds();
	if (LastFireTime > 0.0f && FireRate > 0.0f && LastFireTime + FireRate > GameTime)
	{
		GetWorldTimerManager().SetTimer(FiringTimer, this, &AFWeapon::FireShot, LastFireTime + FireRate - GameTime, false);
	}
	else
	{
		FireShot();
	}
}

void AFWeapon::EndFiring()
{
	GetWorldTimerManager().ClearTimer(FiringTimer);
}

void AFWeapon::FireInstantHit()
{
	const int32 RandomSeed = FMath::Rand();
	FRandomStream WeaponRandomStream(RandomSeed);
	const float CurrentSpread = Spread;
	const float ConeHalfAngle = FMath::DegreesToRadians(CurrentSpread * 0.5f);

	const FVector AimDir = GetAdjustedAim();
	const FVector StartTrace = GetFireStartLocation(AimDir);
	const FVector ShootDir = WeaponRandomStream.VRandCone(AimDir, ConeHalfAngle, ConeHalfAngle);
	const FVector EndTrace = StartTrace + ShootDir * MaxTracerRange;

	const FHitResult Impact = WeaponTrace(StartTrace, EndTrace);

	OnHitDamage(Impact, ShootDir);
}

void AFWeapon::FireProjectile()
{
	FVector ShootDir = GetAdjustedAim();
	FVector Origin = GetMuzzleLocation();

	const float ProjectileAdjustedRange = 10000.0f;
	const FVector StartTrace = GetFireStartLocation(ShootDir);
	const FVector EndTrace = StartTrace + ShootDir * ProjectileAdjustedRange;
	FHitResult Impact = WeaponTrace(StartTrace, EndTrace);

	if (Impact.bBlockingHit)
	{
		const FVector AdjustedDir = (Impact.ImpactPoint - Origin).GetSafeNormal();
		bool bWeaponPenetration = false;

		const float DirectionDot = FVector::DotProduct(AdjustedDir, ShootDir);
		if (DirectionDot < 0.0f)
		{
			bWeaponPenetration = true;
		}
		else if (DirectionDot < 0.5f)
		{
			FVector MuzzleStartTrace = Origin - GetMuzzleDirection() * 150.0f;
			FVector MuzzleEndTrace = Origin;
			FHitResult MuzzleImpact = WeaponTrace(MuzzleStartTrace, MuzzleEndTrace);

			if (MuzzleImpact.bBlockingHit)
			{
				bWeaponPenetration = true;
			}
		}

		if (bWeaponPenetration)
		{
			Origin = Impact.ImpactPoint - ShootDir * 10.0f;
		}
		else
		{
			ShootDir = AdjustedDir;
		}
	}

	SpawnProjectile(Origin, ShootDir);
}

void AFWeapon::ApplyRecoil(float DeltaTime)
{
	float Pitch = 0.0f;
	Recoil = FMath::FInterpTo(Recoil, 0, DeltaTime, -10.0f);
	RecoilRecovery = FMath::FInterpTo(RecoilRecovery, -Recoil, DeltaTime, 20.0f);
	Pitch = Recoil + RecoilRecovery;

	if (FOwner)
	{
		FOwner->AddControllerPitchInput(-Pitch);
	}
}

void AFWeapon::OnHitDamage(FHitResult Hit, const FVector& FireDir)
{
	if (Hit.Actor != nullptr)
	{
		FPointDamageEvent PointDamage;
		PointDamage.DamageTypeClass = InstantHitInfo.DamageType;
		PointDamage.HitInfo = Hit;
		PointDamage.ShotDirection = FireDir;
		PointDamage.Damage = InstantHitInfo.Damage;

		Hit.Actor->TakeDamage(InstantHitInfo.Damage, PointDamage, FOwner->GetController(), this);
	}
}

void AFWeapon::SpawnProjectile_Implementation(FVector Origin, FVector_NetQuantizeNormal ShootDir)
{
	FTransform SpawnTransform(ShootDir.Rotation(), Origin);
	AFProjectile* Proj = Cast<AFProjectile>(UGameplayStatics::BeginDeferredActorSpawnFromClass(this, ProjectileClass, SpawnTransform));
	if (Proj)
	{
		Proj->Instigator = Instigator;
		Proj->SetOwner(this);
		Proj->InitVelocity(ShootDir);

		UGameplayStatics::FinishSpawningActor(Proj, SpawnTransform);
	}
}

bool AFWeapon::SpawnProjectile_Validate(FVector Origin, FVector_NetQuantizeNormal ShootDir)
{
	return true;
}

void AFWeapon::PlayFiringEffects()
{
	if (FireSound)
	{
		PlayWeaponSound(FireSound);
	}

	if (FireAnim)
	{
		PlayWeaponAnim(FireAnim);
	}
}

void AFWeapon::PlayWeaponSound(USoundBase* Sound)
{
	UAudioComponent* AudioComp;
	if (Sound && FOwner)
	{
		AudioComp = UGameplayStatics::SpawnSoundAttached(Sound, GetRootComponent());
	}
}

void AFWeapon::PlayWeaponAnim(UAnimMontage* Anim)
{
	if (FOwner)
	{
		if (Anim)
		{
			FOwner->PlayAnimMontage(Anim);
		}
	}
}

void AFWeapon::AttachToOwner()
{
	if (FOwner)
	{
		Mesh->AttachToComponent(FOwner->FirstPersonMesh, FAttachmentTransformRules::KeepRelativeTransform, HandsAttachSocket);
		Mesh->SetHiddenInGame(false);
	}
}

void AFWeapon::DetachFromOwner()
{
	Mesh->DetachFromComponent(FDetachmentTransformRules::KeepRelativeTransform);
	Mesh->SetHiddenInGame(true);
}

FVector AFWeapon::GetAdjustedAim() const
{
	AFPlayerController* const PlayerController = Instigator ? Cast<AFPlayerController>(Instigator->Controller) : nullptr;
	FVector FinalAim = FVector::ZeroVector;
	if (PlayerController)
	{
		FVector CamLoc;
		FRotator CamRot;
		PlayerController->GetPlayerViewPoint(CamLoc, CamRot);
		FinalAim = CamRot.Vector();
	}

	return FinalAim;
}

FVector AFWeapon::GetFireStartLocation(const FVector& AimDir) const
{
	AFPlayerController* PlayerController = FOwner ? Cast<AFPlayerController>(FOwner->GetController()) : nullptr;
	FVector OutStartTrace = FVector::ZeroVector;
	if (PlayerController)
	{
		FRotator UnusedRot;
		PlayerController->GetPlayerViewPoint(OutStartTrace, UnusedRot);

		OutStartTrace = OutStartTrace + AimDir * ((Instigator->GetActorLocation() - OutStartTrace) | AimDir);
	}

	return OutStartTrace;
}

FVector AFWeapon::GetMuzzleLocation() const
{
	return Mesh->GetSocketLocation(MuzzleSocket);
}

FVector AFWeapon::GetMuzzleDirection() const
{
	return Mesh->GetSocketRotation(MuzzleSocket).Vector();
}

FHitResult AFWeapon::WeaponTrace(const FVector& TraceFrom, const FVector TraceTo) const
{
	FCollisionQueryParams TraceParams(SCENE_QUERY_STAT(WeaponTrace), true, Instigator);
	TraceParams.bReturnPhysicalMaterial = true;

	FHitResult Hit(ForceInit);
	GetWorld()->LineTraceSingleByChannel(Hit, TraceFrom, TraceTo, COLLISION_WEAPON, TraceParams);
	//DrawDebugLine(GetWorld(), TraceFrom, TraceTo, FColor::Red, false, 1, 0, 1);
	return Hit;
}
