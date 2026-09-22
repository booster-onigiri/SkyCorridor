#include "EWBrowserAudioWave.h"
#include "Misc/ScopeLock.h"
void UEWBrowserAudioWave::PushSamples(const uint8* Data,int32 Bytes)
{
    if(!Data || Bytes<=0 || Bytes%2 || Bytes>19200)return;
    FScopeLock Lock(&BufferMutex);if(Pending.Num()+Bytes>19200)Pending.Reset();Pending.Append(Data,Bytes);
}
void UEWBrowserAudioWave::FlushSamples(){FScopeLock Lock(&BufferMutex);Pending.Reset();}
int32 UEWBrowserAudioWave::PendingBytes() const{FScopeLock Lock(&BufferMutex);return Pending.Num();}
int32 UEWBrowserAudioWave::OnGeneratePCMAudio(TArray<uint8>& OutAudio,int32 NumSamples)
{
    const int32 Bytes=NumSamples*2;OutAudio.SetNumUninitialized(Bytes);FMemory::Memzero(OutAudio.GetData(),Bytes);FScopeLock Lock(&BufferMutex);
    const int32 Count=FMath::Min(Bytes,Pending.Num());if(Count){FMemory::Memcpy(OutAudio.GetData(),Pending.GetData(),Count);Pending.RemoveAt(0,Count,EAllowShrinking::No);}
    return NumSamples;
}
