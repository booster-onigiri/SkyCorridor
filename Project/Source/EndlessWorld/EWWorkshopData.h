#pragma once
#include "CoreMinimal.h"
#include "EWWorld.h"

namespace EWWorkshop
{
struct Egg { FString Id,Title; EW::WorldDescriptor World; EW::ChunkCoord Arrival; };
struct Concept
{
    FString Id,Title;
    double Size=160;
    FLinearColor Light=FLinearColor(.82f,.77f,.62f),Dark=FLinearColor(.035f,.14f,.14f),Accent=FLinearColor(.35f,.22f,.06f);
};
bool ReadEgg(const FString& Text,Egg& Out,FString& Error);
bool ReadConcept(const FString& Text,Concept& Out,FString& Error);
bool ReadBoundedFile(const FString& Path,FString& Text,FString& Error);
bool SafeId(const FString& Id);
bool SafeFilename(const FString& Name,const FString& Suffix);
bool AtomicWrite(const FString& Path,const FString& Text);
}
