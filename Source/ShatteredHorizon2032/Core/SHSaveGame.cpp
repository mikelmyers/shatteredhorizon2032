// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#include "SHSaveGame.h"

USHSaveGame::USHSaveGame()
	: SaveTimestamp(FDateTime::UtcNow())
	, SaveVersion(LatestSaveVersion)
{
}
