#pragma once
#include "CoreMinimal.h"

class UEWGameInstance;

// A passive desktop-composited HUD. It has no swap chain, receives no input,
// and is owned by the game window so text is never an input to frame generation.
class FEWHudLayer
{
public:
    FEWHudLayer();
    ~FEWHudLayer();
    void Update(UEWGameInstance& Game);
    void Hide();
    void Shutdown();
    bool IsVisible() const;
    FString Evidence() const;
private:
    struct FState;
    TUniquePtr<FState> State;
};
