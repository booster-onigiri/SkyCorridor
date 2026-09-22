#include "EWCraft95.h"

void EWCraft95::Apply(EW::ChunkRecipe& R)
{
    if(R.Region!=EW::RegionKind::City || R.bCascadeCity)return;
    TArray<EW::Part> Added;
    for(auto& P:R.Parts)
    {
        if(P.Mesh.ToString().StartsWith(TEXT("UrbanGarden_")))
        {
            // Existing solid planters and the trunk collision stay in place.
            // The roof mesh now contains the garden shell without the old crowns.
            for(int32 I=0;I<2;++I)
            {
                const FVector Local=I==0?FVector(1120,-1100,110):FVector(-1180,1000,110);
                const FVector Scale=P.Transform.GetScale3D()*(I==0?1.f:.83f);
                Added.Add({I==0?FName(TEXT("Craft95CourtyardTreeA")):FName(TEXT("Craft95CourtyardTreeB")),
                    FTransform(P.Transform.GetRotation(),P.Transform.TransformPosition(Local),Scale),1});
            }
        }
        // World-space 50cm paving maintains density across differently scaled slabs.
        if(R.Coord==EW::ChunkCoord{0,0} && P.Mesh==TEXT("StoneDeck"))P.Mesh=TEXT("Craft95StoneDeck");
    }
    for(const auto& Room:R.Interiors)if(Room.Kind==19)
    {
        Added.Add({TEXT("Craft95CafeJoinery"),Room.Frame,0});
        const FVector Table(-650,-1250,0);
        Added.Add({TEXT("Craft95CafeTableSet"),FTransform(Room.Frame.GetRotation(),Room.Frame.TransformPosition(Table),Room.Frame.GetScale3D()),1});
        // An honest solid envelope for the added set, clear of the 4m entry route
        // and the memory at local (-400,-1450). No change to existing seats.
        R.Colliders.Add({FTransform(Room.Frame.GetRotation(),Room.Frame.TransformPosition(Table+FVector(0,0,42))),FVector(129,60,42)*Room.Frame.GetScale3D().GetAbs(),false});
        for(const FVector V:{FVector(-1460,-1660,0),FVector(1470,1380,0)})
        {
            Added.Add({TEXT("Craft95CourtyardUrn"),FTransform(Room.Frame.GetRotation(),Room.Frame.TransformPosition(V),Room.Frame.GetScale3D()),1});
            R.Colliders.Add({FTransform(Room.Frame.GetRotation(),Room.Frame.TransformPosition(V+FVector(0,0,42))),FVector(44,44,42)*Room.Frame.GetScale3D().GetAbs(),false});
        }
    }
    R.Parts.Append(Added);
}
