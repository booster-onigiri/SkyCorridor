#include "EWLocalization.h"
#include "HAL/PlatformMisc.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Internationalization/Regex.h"

namespace
{
bool English = false;
FString PreferenceFile;
struct FEntry { const TCHAR* Japanese; const TCHAR* English; };
const TMap<FString, FString>& Dictionary()
{
    static const TMap<FString, FString> Map = []
    {
        const FEntry Entries[] = {
#include "EWLocalizationCore.inl"
#include "EWLocalizationWorld.inl"
#include "EWLocalizationPlaces.inl"
        };
        TMap<FString, FString> Result;
        for (const FEntry& Entry : Entries) Result.Add(Entry.Japanese, Entry.English);
        return Result;
    }();
    return Map;
}
}
bool EWL::IsEnglish() { return English; }
FString EWL::Language() { return English ? TEXT("en") : TEXT("ja"); }
bool EWL::SetLanguage(const FString& Value)
{
    if (Value != TEXT("ja") && Value != TEXT("en")) return false;
    English = Value == TEXT("en");
    return true;
}
void EWL::Initialize(const FString& Directory)
{
    PreferenceFile = FPaths::Combine(Directory, TEXT("language.txt"));
    FString Saved;
    if (FFileHelper::LoadFileToString(Saved, *PreferenceFile)) Saved.TrimStartAndEndInline();
    if (!SetLanguage(Saved))
        English = !FPlatformMisc::GetDefaultLanguage().StartsWith(TEXT("ja"), ESearchCase::IgnoreCase);
    // Explicit test/developer overrides do not overwrite the user's preference.
    FString Override;
    if (FParse::Value(FCommandLine::Get(), TEXT("EWLanguage="), Override)) SetLanguage(Override);
}
bool EWL::SavePreference()
{
    if (PreferenceFile.IsEmpty() || !IFileManager::Get().MakeDirectory(*FPaths::GetPath(PreferenceFile), true)) return false;
    const FString Temporary = PreferenceFile + TEXT(".tmp");
    if (!FFileHelper::SaveStringToFile(Language(), *Temporary, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM)) return false;
    return IFileManager::Get().Move(*PreferenceFile, *Temporary, true, true);
}
const TCHAR* EWL::Pick(const TCHAR* Japanese, const TCHAR* EnglishText) { return English ? EnglishText : Japanese; }
bool EWL::HasJapanese(const FString& Text)
{
    for (TCHAR C : Text) if ((C >= 0x3040 && C <= 0x30ff) || (C >= 0x3400 && C <= 0x9fff)) return true;
    return false;
}
int32 EWL::EntryCount() { return Dictionary().Num(); }
FString EWL::Translate(const FString& Source)
{
    if (!English || Source.IsEmpty()) return Source;
    if (const FString* Found = Dictionary().Find(Source)) return *Found;
    // These are generated floor labels, not general user-authored text.
    // Match the complete original grammar before extracting a signed floor.
    {
        static const FRegexPattern Pattern(TEXT("^(?:(出発広場|屋上庭園)　([+-][0-9]+) 階|([+-][0-9]+) 階　空中回廊)$"));
        FRegexMatcher Match(Pattern, Source);
        if (Match.FindNext())
        {
            if (!Match.GetCaptureGroup(3).IsEmpty())
                return TEXT("Floor ") + Match.GetCaptureGroup(3) + TEXT(" - Sky Corridor");
            return FString(Match.GetCaptureGroup(1) == TEXT("出発広場") ? TEXT("Starting Plaza - Floor ") : TEXT("Roof Garden - Floor ")) + Match.GetCaptureGroup(2);
        }
    }
    // Generated signs combine stable names with explicit separators. Translate
    // complete components only; never replace substrings within user content.
    for (const TCHAR* Separator : {TEXT("\n"), TEXT("　"), TEXT(" / "), TEXT("・")})
    {
        const int32 Split = Source.Find(Separator);
        if (Split == INDEX_NONE) continue;
        // Keep the remaining label intact so prefixed facility names do not
        // split a valid floor label before its complete grammar is recognized.
        const FString Left = Source.Left(Split);
        const FString Right = Source.Mid(Split + FCString::Strlen(Separator));
        const FString LocalizedLeft = Translate(Left);
        const FString LocalizedRight = Translate(Right);
        if (LocalizedLeft != Left || LocalizedRight != Right)
            return LocalizedLeft + (FCString::Strcmp(Separator, TEXT("\n")) == 0 ? TEXT("\n") : TEXT(" / ")) + LocalizedRight;
    }
    return Source;
}
