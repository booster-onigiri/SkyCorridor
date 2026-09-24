#pragma once

#include "CoreMinimal.h"

class FJsonObject;

namespace EWNativeDisplayProbe
{
// On-demand game-thread diagnostic. Flushes rendering and briefly reads the
// native game-window swapchain; never changes the display or swapchain state.
// DXGI has no applied-colorspace getter, so that field is always NOT_MEASURED.
TSharedPtr<FJsonObject> Read();
}
