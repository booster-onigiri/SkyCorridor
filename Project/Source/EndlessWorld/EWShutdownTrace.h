#pragma once
#include "CoreMinimal.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"

// Opt-in local diagnostics; no trace or additional file in ordinary play.
inline void EWShutdownTrace(const TCHAR* Stage)
{
    FString Path;
    if(!FParse::Value(FCommandLine::Get(),TEXT("EWShutdownTrace="),Path))return;
    FFileHelper::SaveStringToFile(FString::Printf(TEXT("%.3f %s\n"),FPlatformTime::Seconds(),Stage),
        *Path,FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM,&IFileManager::Get(),FILEWRITE_Append);
}
