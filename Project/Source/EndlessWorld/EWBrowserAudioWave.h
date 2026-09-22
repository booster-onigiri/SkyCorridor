#pragma once
#include "CoreMinimal.h"
#include "Sound/SoundWaveProcedural.h"
#include "EWBrowserAudioWave.generated.h"

// One mono screen channel. A single lock covers both clearing and consuming,
// so restarting cannot reset a counter after fresh samples have been queued.
UCLASS()
class ENDLESSWORLD_API UEWBrowserAudioWave:public USoundWaveProcedural
{
    GENERATED_BODY()
public:
    void PushSamples(const uint8* Data,int32 Bytes);
    void FlushSamples();
    int32 PendingBytes() const;
protected:
    virtual int32 OnGeneratePCMAudio(TArray<uint8>& OutAudio,int32 NumSamples) override;
private:
    mutable FCriticalSection BufferMutex;
    TArray<uint8> Pending;
};
