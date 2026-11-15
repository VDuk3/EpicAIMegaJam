// Where the first-person staff's head is drawn, so its shots leave from it (owner, 2026-11-15).
#pragma once

#include "CoreMinimal.h"

namespace GBStaffView
{
	/** The HUD records, each frame it draws the held staff, the direction from the camera to the staff's head, in
	 *  aim space (X forward, Y right, Z up). */
	void SetHeadAimDir(const FVector& AimSpaceDir);
	/** The recorded direction, if the staff was drawn in the last few frames. */
	bool GetHeadAimDir(FVector& OutAimSpaceDir);
}
