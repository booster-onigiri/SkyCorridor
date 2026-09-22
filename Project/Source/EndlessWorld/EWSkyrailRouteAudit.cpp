#include "EWSkyrailPlan.h"
#include "EWClock.h"
#include "Dom/JsonObject.h"
#if WITH_EDITOR
#include "Engine/StaticMesh.h"
#include "MeshDescription.h"
#include "StaticMeshAttributes.h"
#endif
TSharedRef<FJsonObject> EWSkyrailPlan::AuditRoute()
{
    auto O=MakeShared<FJsonObject>();TArray<TSharedPtr<FJsonValue>> Hits;int32 Candidates=0,Triangles=0,Boxes=0,Poses=0;
#if WITH_EDITOR
    TMap<FName,UStaticMesh*> Cache;
    for(int Line=0;Line<2;++Line)
    {
        // Each carriage follows the curve independently. Sample the complete
        // swept bounds densely; two cm of expansion covers the sub-sample arc.
        TArray<FBox> Swept;FBox Total(ForceInit);
        for(double X=StopX(0,Line);X<=StopX(Count(Line)-1,Line)+25;X+=25)for(double Car:{-560.,560.})
        {
            FTransform T=TrackPose(FMath::Min(X,StopX(Count(Line)-1,Line))+Car,Line);
            auto B=FBox(FVector(-630,-162,-90),FVector(630,162,330)).TransformBy(T);Swept.Add(B);Total+=B;++Poses;
        }
        auto Touch=[&](const FBox& B){if(!B.Intersect(Total))return false;for(const auto& S:Swept)if(S.Intersect(B))return true;return false;};
        auto TriangleBox=[](FVector A,FVector B,FVector C,const FBox& Box)
        {
            const auto Centre=Box.GetCenter(),Extent=Box.GetExtent();A-=Centre;B-=Centre;C-=Centre;const FVector Edges[]={B-A,C-B,A-C};
            auto Axis=[&](FVector N){const double X=FVector::DotProduct(A,N),Y=FVector::DotProduct(B,N),Z=FVector::DotProduct(C,N);
                const double R=Extent.X*FMath::Abs(N.X)+Extent.Y*FMath::Abs(N.Y)+Extent.Z*FMath::Abs(N.Z);return FMath::Min3(X,Y,Z)<=R && FMath::Max3(X,Y,Z)>=-R;};
            for(FVector N:{FVector::XAxisVector,FVector::YAxisVector,FVector::ZAxisVector}){if(!Axis(N))return false;for(auto E:Edges)if(!Axis(FVector::CrossProduct(E,N)))return false;}
            return Axis(FVector::CrossProduct(Edges[0],Edges[1]));
        };
        for(int C=0;C<=StopChunk(Count(Line)-1,Line).X;++C)
        {
            const auto R=EW::GenerateChunk(EW::WorldDescriptor::ReferenceWorld(),{C,0});const FVector Offset(C*EW::ChunkSize,0,0);
            if(R.bFallback)Hits.Add(MakeShared<FJsonValueString>(TEXT("recipe fallback")));
            if(R.Lifts.ContainsByPredicate([](const auto& L){return L.Id==TEXT("skyrail78/1");}))Hits.Add(MakeShared<FJsonValueString>(TEXT("removed East Gallery station still exists")));
            for(const auto& L:R.Lifts)
            {
                double Low=DBL_MAX,High=-DBL_MAX,Radius=0;
                for(const auto& S:L.Stops)
                {
                    Low=FMath::Min(Low,S.Height);High=FMath::Max(High,S.Height);
                    const auto B=FBox(FVector(-215,-215,0),FVector(215,215,0)).TransformBy(FTransform(S.BoardingRotation));Radius=FMath::Max(Radius,B.GetExtent().X);
                }
                const FVector Centre=L.Cabin+Offset;const FBox Shaft(FVector(Centre.X-Radius,Centre.Y-Radius,Low-38),FVector(Centre.X+Radius,Centre.Y+Radius,High+333));
                if(Touch(Shaft))Hits.Add(MakeShared<FJsonValueString>(FString::Printf(TEXT("line %d moving lift "),Line)+L.Id));
            }
            for(const auto& B:R.Colliders)
            {
                ++Boxes;auto T=B.Transform;T.AddToTranslation(Offset);
                if(Touch(FBox(-B.Extent,B.Extent).TransformBy(T)))Hits.Add(MakeShared<FJsonValueString>(FString::Printf(TEXT("line %d collider "),Line)+T.GetLocation().ToString()));
            }
            for(const auto& P:R.Parts)
            {
                if(P.Mesh.ToString().StartsWith(TEXT("Skyrail")))continue;
                UStaticMesh* Mesh=nullptr;if(auto** Found=Cache.Find(P.Mesh))Mesh=*Found;
                else{Mesh=LoadObject<UStaticMesh>(nullptr,*FString::Printf(TEXT("/Game/EndlessWorld/Kit/SM_%s.SM_%s"),*P.Mesh.ToString(),*P.Mesh.ToString()));Cache.Add(P.Mesh,Mesh);}
                if(!Mesh){Hits.Add(MakeShared<FJsonValueString>(TEXT("missing mesh ")+P.Mesh.ToString()));continue;}
                FTransform T=P.Transform;T.AddToTranslation(Offset);const auto Bounds=Mesh->GetBoundingBox().TransformBy(T);if(!Touch(Bounds))continue;++Candidates;
                const auto* D=Mesh->GetMeshDescription(0);if(!D){Hits.Add(MakeShared<FJsonValueString>(TEXT("missing mesh description")));continue;}
                TArray<FBox> LocalSweeps;for(const auto& B:Swept)if(B.Intersect(Bounds))LocalSweeps.Add(B);
                FStaticMeshConstAttributes Attr(*D);const auto Pos=Attr.GetVertexPositions();int Count=0;
                for(const auto ID:D->Triangles().GetElementIDs())
                {
                    const auto V=D->GetTriangleVertices(ID);++Triangles;
                    const FVector A=T.TransformPosition(FVector(Pos[V[0]])),B=T.TransformPosition(FVector(Pos[V[1]])),V2=T.TransformPosition(FVector(Pos[V[2]]));
                    FBox TriangleBounds(ForceInit);TriangleBounds+=A;TriangleBounds+=B;TriangleBounds+=V2;
                    for(const auto& S:LocalSweeps)if(TriangleBounds.Intersect(S) && TriangleBox(A,B,V2,S)){++Count;break;}
                }
                if(Count)Hits.Add(MakeShared<FJsonValueString>(FString::Printf(TEXT("line %d "),Line)+P.Mesh.ToString()+TEXT(" ")+T.GetLocation().ToString()+FString::Printf(TEXT(" triangles=%d"),Count)));
            }
            if(C==0){const auto P=EWClock::ScreenCentre(R);if(Touch(FBox(P-FVector(835,55,480),P+FVector(835,55,480))))Hits.Add(MakeShared<FJsonValueString>(TEXT("plaza screen")));}
        }
    }
    O->SetBoolField(TEXT("success"),Hits.IsEmpty());
#else
    O->SetBoolField(TEXT("success"),false);
#endif
    O->SetArrayField(TEXT("intersections"),Hits);O->SetNumberField(TEXT("collision_boxes_checked"),Boxes);O->SetNumberField(TEXT("nearby_mesh_instances"),Candidates);
    O->SetNumberField(TEXT("triangles_checked"),Triangles);O->SetNumberField(TEXT("carriage_poses"),Poses);
    O->SetStringField(TEXT("method"),TEXT("both lines: carriage swept bounds at 25 cm intervals vs authored triangles, colliders and rotated lift-car envelopes; removed station checked"));return O;
}
