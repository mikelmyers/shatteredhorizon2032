// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#include "AI/SHAIPerceptionConfig.h"

USHAIPerceptionConfig::USHAIPerceptionConfig()
{
	// --- Default sight ranges ---
	{
		FSHSightRangeEntry ClearDay;
		ClearDay.Condition = ESHVisibilityCondition::ClearDay;
		ClearDay.MaxRange = 80000.f;  // 800m — doctrine minimum engagement distance
		ClearDay.InstantDetectionRange = 15000.f; // 150m instant detection
		SightRanges.Add(ClearDay);

		FSHSightRangeEntry Overcast;
		Overcast.Condition = ESHVisibilityCondition::Overcast;
		Overcast.MaxRange = 60000.f;  // 600m
		Overcast.InstantDetectionRange = 12000.f;
		SightRanges.Add(Overcast);

		FSHSightRangeEntry Dusk;
		Dusk.Condition = ESHVisibilityCondition::Dusk;
		Dusk.MaxRange = 30000.f;  // 300m
		Dusk.InstantDetectionRange = 5000.f;
		SightRanges.Add(Dusk);

		FSHSightRangeEntry Night;
		Night.Condition = ESHVisibilityCondition::Night;
		Night.MaxRange = 8000.f;   // 80m unaided night vision
		Night.InstantDetectionRange = 1500.f;
		Night.PeripheralHalfAngleDeg = 50.f;
		SightRanges.Add(Night);

		FSHSightRangeEntry NightNVG;
		NightNVG.Condition = ESHVisibilityCondition::NightNVG;
		NightNVG.MaxRange = 50000.f;  // 500m with NVGs
		NightNVG.InstantDetectionRange = 8000.f;
		NightNVG.PeripheralHalfAngleDeg = 40.f;  // NVG narrows field of view.
		NightNVG.CentralHalfAngleDeg = 20.f;
		SightRanges.Add(NightNVG);

		FSHSightRangeEntry Fog;
		Fog.Condition = ESHVisibilityCondition::Fog;
		Fog.MaxRange = 15000.f;  // 150m in fog
		Fog.InstantDetectionRange = 3000.f;
		SightRanges.Add(Fog);

		FSHSightRangeEntry Rain;
		Rain.Condition = ESHVisibilityCondition::HeavyRain;
		Rain.MaxRange = 40000.f;  // 400m in heavy rain
		Rain.InstantDetectionRange = 8000.f;
		SightRanges.Add(Rain);

		FSHSightRangeEntry Smoke;
		Smoke.Condition = ESHVisibilityCondition::Smoke;
		Smoke.MaxRange = 1500.f;  // 15m in smoke
		Smoke.InstantDetectionRange = 500.f;
		SightRanges.Add(Smoke);
	}

	// --- Default hearing ranges ---
	{
		auto AddHearing = [this](ESHSoundType Type, float Max, float Alert, bool bDirection)
		{
			FSHHearingRangeEntry Entry;
			Entry.SoundType = Type;
			Entry.MaxRange = Max;
			Entry.AlertRange = Alert;
			Entry.bProvidesDirection = bDirection;
			HearingRanges.Add(Entry);
		};

		AddHearing(ESHSoundType::Footstep_Walk,         800.f,   300.f,  false);
		AddHearing(ESHSoundType::Footstep_Run,          1500.f,  600.f,  true);
		AddHearing(ESHSoundType::Footstep_Sprint,       2500.f,  1000.f, true);
		AddHearing(ESHSoundType::Voice_Normal,           1200.f,  500.f,  true);
		AddHearing(ESHSoundType::Voice_Shout,            3000.f,  1500.f, true);
		AddHearing(ESHSoundType::GunfireUnsuppressed,   15000.f, 8000.f, true);
		AddHearing(ESHSoundType::GunfireSuppressed,      4000.f, 1500.f, false);
		AddHearing(ESHSoundType::Explosion,             20000.f, 10000.f, true);
		AddHearing(ESHSoundType::VehicleEngine,          8000.f, 4000.f, true);
		AddHearing(ESHSoundType::EquipmentRattle,        600.f,   200.f, false);
		AddHearing(ESHSoundType::DoorBreach,             5000.f, 2000.f, true);
		AddHearing(ESHSoundType::GrenadeImpact,          3000.f, 1500.f, true);
	}
}

FSHSightRangeEntry USHAIPerceptionConfig::GetSightRange(ESHVisibilityCondition Condition) const
{
	for (const FSHSightRangeEntry& Entry : SightRanges)
	{
		if (Entry.Condition == Condition)
		{
			return Entry;
		}
	}

	// Fallback default.
	FSHSightRangeEntry Default;
	Default.Condition = Condition;
	return Default;
}

FSHHearingRangeEntry USHAIPerceptionConfig::GetHearingRange(ESHSoundType SoundType) const
{
	for (const FSHHearingRangeEntry& Entry : HearingRanges)
	{
		if (Entry.SoundType == SoundType)
		{
			return Entry;
		}
	}

	FSHHearingRangeEntry Default;
	Default.SoundType = SoundType;
	return Default;
}
