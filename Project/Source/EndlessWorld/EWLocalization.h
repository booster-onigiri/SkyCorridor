#pragma once
#include "CoreMinimal.h"

// Presentation only: never translate world IDs, player input or save keys.
namespace EWL
{
    bool IsEnglish();
    FString Language();
    bool SetLanguage(const FString& Language);
    void Initialize(const FString& SaveDirectory);
    bool SavePreference();
    const TCHAR* Pick(const TCHAR* Japanese, const TCHAR* English);
    FString Translate(const FString& Source);
    bool HasJapanese(const FString& Text);
    int32 EntryCount();

    template<typename... Types>
    FString Format(UE::Core::TCheckedFormatString<TCHAR, Types...> Japanese,
        UE::Core::TCheckedFormatString<TCHAR, Types...> English, Types... Args)
    {
        return IsEnglish() ? FString::Printf(English, Args...) : FString::Printf(Japanese, Args...);
    }
}
