#include "EWEOSConnection.h"
#include "EWSocialProtocol.h"
#include "Modules/ModuleManager.h"
#include "IEOSSDKManager.h"
#include "EOSVoiceChatFactory.h"
#include "EOSVoiceChatUser.h"
#include "VoiceChat.h"
#include "Misc/FileHelper.h"
#include "Misc/ScopeLock.h"
#include "HAL/FileManager.h"
#include "eos_sdk.h"
#include "eos_connect.h"
#include "eos_lobby.h"
#include "eos_p2p.h"
#include "eos_reports.h"
#include "eos_sanctions.h"

namespace
{
FString ProductId(EOS_ProductUserId Id)
{
    char Text[EOS_PRODUCTUSERID_MAX_LENGTH+1]={};int32 Length=sizeof(Text);
    return Id && EOS_ProductUserId_ToString(Id,Text,&Length)==EOS_EResult::EOS_Success?UTF8_TO_TCHAR(Text):FString();
}
EOS_ProductUserId Product(const FString& Id){return EOS_ProductUserId_FromString(TCHAR_TO_UTF8(*Id));}

}
struct FEWEOSConnection::FImpl
{
    struct FLife { FImpl* Owner=nullptr; };
    struct FCall { TSharedPtr<FLife> Life;uint64 Epoch;FString Text; };
    FEWEOSConnection& Outer;
    TSharedPtr<FLife> Life=MakeShared<FLife>();
    IEOSPlatformHandlePtr Platform;
    IVoiceChatPtr Voice;
    IVoiceChatUser* VoiceUser=nullptr;
    FDelegateHandle Unmixed,Mixed;
    EOS_ProductUserId Self=nullptr;
    EOS_HP2P P2P=nullptr;EOS_HLobby Lobby=nullptr;EOS_HConnect Connect=nullptr;
    EOS_HLobbySearch Search=nullptr;
    EOS_P2P_SocketId Socket={};
    EOS_NotificationId RequestNotice=EOS_INVALID_NOTIFICATIONID,AuthNotice=EOS_INVALID_NOTIFICATIONID;
    EOS_NotificationId LobbyNotice=EOS_INVALID_NOTIFICATIONID,MemberNotice=EOS_INVALID_NOTIFICATIONID;
    FString SelfText,OwnerText,CurrentLobby,Message=TEXT("公開の街を準備しています。"),VoiceMessage;
    FString ProductConfig,SandboxConfig,DeploymentConfig,ClientConfig,SecretConfig,CacheConfig;
    FString PendingName,PendingLanguage,PendingActivity,PendingJoin;
    bool bConfigured=false,bLogged=false,bBusy=false,bClosing=false,bHost=false,bMicSending=false;
    uint64 Epoch=1;uint32 SendSequence=0;
    double LastMembership=0;
    TSet<FString> MemberIds,VoiceBlocks;
    TArray<FEWCityListing> CityList;
    EWSocial::FWireInbox Wire;
    explicit FImpl(FEWEOSConnection& In):Outer(In){Life->Owner=this;Socket.ApiVersion=EOS_P2P_SOCKETID_API_LATEST;FCStringAnsi::Strcpy(Socket.SocketName,"EndlessCity1");}
    ~FImpl()
    {
        bClosing=true;Leave();Life->Owner=nullptr;
        if(P2P && RequestNotice!=EOS_INVALID_NOTIFICATIONID)EOS_P2P_RemoveNotifyPeerConnectionRequest(P2P,RequestNotice);
        if(Connect && AuthNotice!=EOS_INVALID_NOTIFICATIONID)EOS_Connect_RemoveNotifyAuthExpiration(Connect,AuthNotice);
        if(Lobby && LobbyNotice!=EOS_INVALID_NOTIFICATIONID)EOS_Lobby_RemoveNotifyLobbyUpdateReceived(Lobby,LobbyNotice);
        if(Lobby && MemberNotice!=EOS_INVALID_NOTIFICATIONID)EOS_Lobby_RemoveNotifyLobbyMemberStatusReceived(Lobby,MemberNotice);
        if(Search)EOS_LobbySearch_Release(Search);
        Platform.Reset();
    }
    void Changed(){if(!bClosing && Outer.OnChanged)Outer.OnChanged();}
    void Error(const FString& What,EOS_EResult Result)
    {bBusy=false;Message=What+TEXT("（")+UTF8_TO_TCHAR(EOS_EResult_ToString(Result))+TEXT("）");Changed();}
    FCall* Call(const FString& Text={}){return new FCall{Life,Epoch,Text};}
    template<typename T,typename F> static void Complete(const T* D,F Func)
    {
        TUniquePtr<FCall> C(static_cast<FCall*>(D->ClientData));
        if(C->Life && C->Life->Owner && C->Epoch==C->Life->Owner->Epoch && !C->Life->Owner->bClosing)Func(*C->Life->Owner,*C);
    }
    bool Configure(const FString& Path,const FString& Cache)
    {
        if(Platform)return false;
        bConfigured=false;ProductConfig.Reset();SandboxConfig.Reset();DeploymentConfig.Reset();ClientConfig.Reset();SecretConfig.Reset();
        FString Text;TSharedPtr<FJsonObject> O;
        if(IFileManager::Get().FileSize(*Path)>65536 || !FFileHelper::LoadFileToString(Text,*Path) || !EWSocial::Decode(Text,O))
        {Message=TEXT("公開の街は準備中です。開発者のEOS接続設定がまだありません。");return false;}
        for(const TCHAR* Key:{TEXT("ProductId"),TEXT("SandboxId"),TEXT("DeploymentId"),TEXT("ClientId"),TEXT("ClientSecret")})
        {
            if(!O->HasTypedField<EJson::String>(Key)){Message=TEXT("開発者のEOS接続設定の形式が正しくありません。");return false;}
            const FString Value=O->GetStringField(Key);
            if(Value.IsEmpty() || Value.Len()>2048){Message=TEXT("開発者のEOS接続設定に不足があります。");return false;}
            for(const TCHAR C:Value)if(C<=32){Message=TEXT("開発者のEOS接続設定の形式が正しくありません。");return false;}
        }
        const bool Valid=O->TryGetStringField(TEXT("ProductId"),ProductConfig) && O->TryGetStringField(TEXT("SandboxId"),SandboxConfig) &&
            O->TryGetStringField(TEXT("DeploymentId"),DeploymentConfig) && O->TryGetStringField(TEXT("ClientId"),ClientConfig) &&
            O->TryGetStringField(TEXT("ClientSecret"),SecretConfig);
        if(!Valid || ProductConfig.IsEmpty() || SandboxConfig.IsEmpty() || DeploymentConfig.IsEmpty() || ClientConfig.IsEmpty() || SecretConfig.IsEmpty())
        {Message=TEXT("開発者のEOS接続設定に不足があります。");return false;}
        CacheConfig=Cache;bConfigured=true;Message=TEXT("登録せずに街へ参加できます。");return true;
    }
    bool EnsurePlatform()
    {
        if(Platform)return true;if(!bConfigured || IsRunningCommandlet())return false;
        FModuleManager::LoadModuleChecked<IModuleInterface>("EOSShared");
        auto* M=IEOSSDKManager::Get();if(!M){Message=TEXT("EOSを起動できませんでした。");return false;}
        FEOSSDKPlatformConfig C;C.Name=TEXT("EndlessCity");C.ProductId=ProductConfig;C.SandboxId=SandboxConfig;
        C.DeploymentId=DeploymentConfig;C.ClientId=ClientConfig;C.ClientSecret=SecretConfig;C.CacheDirectory=CacheConfig;
        C.bDisableOverlay=C.bDisableSocialOverlay=true;C.bEnableRTC=true;C.TickBudgetInMilliseconds=1;
        IFileManager::Get().MakeDirectory(*CacheConfig,true);M->AddPlatformConfig(C,true);
        Platform=M->CreatePlatform(C.Name,FName(*FGuid::NewGuid().ToString(EGuidFormats::Digits)));
        if(!Platform){Message=TEXT("EOSを起動できませんでした。接続設定を確認してください。");return false;}
        const EOS_HPlatform H=*Platform;
        Connect=EOS_Platform_GetConnectInterface(H);Lobby=EOS_Platform_GetLobbyInterface(H);P2P=EOS_Platform_GetP2PInterface(H);
        EOS_P2P_SetRelayControlOptions Relay={};Relay.ApiVersion=EOS_P2P_SETRELAYCONTROL_API_LATEST;Relay.RelayControl=EOS_ERelayControl::EOS_RC_ForceRelays;
        EOS_P2P_SetRelayControl(P2P,&Relay);
        EOS_Connect_AddNotifyAuthExpirationOptions A={};A.ApiVersion=EOS_CONNECT_ADDNOTIFYAUTHEXPIRATION_API_LATEST;
        AuthNotice=EOS_Connect_AddNotifyAuthExpiration(Connect,&A,this,[](const EOS_Connect_AuthExpirationCallbackInfo* D)
        {auto& S=*static_cast<FImpl*>(D->ClientData);if(!S.bClosing)S.Login();});
        EOS_Lobby_AddNotifyLobbyUpdateReceivedOptions LU={};LU.ApiVersion=EOS_LOBBY_ADDNOTIFYLOBBYUPDATERECEIVED_API_LATEST;
        LobbyNotice=EOS_Lobby_AddNotifyLobbyUpdateReceived(Lobby,&LU,this,[](const EOS_Lobby_LobbyUpdateReceivedCallbackInfo* D)
        {auto& S=*static_cast<FImpl*>(D->ClientData);if(!S.bClosing && S.CurrentLobby==UTF8_TO_TCHAR(D->LobbyId))S.RefreshMembers();});
        EOS_Lobby_AddNotifyLobbyMemberStatusReceivedOptions LM={};LM.ApiVersion=EOS_LOBBY_ADDNOTIFYLOBBYMEMBERSTATUSRECEIVED_API_LATEST;
        MemberNotice=EOS_Lobby_AddNotifyLobbyMemberStatusReceived(Lobby,&LM,this,[](const EOS_Lobby_LobbyMemberStatusReceivedCallbackInfo* D)
        {
            auto& S=*static_cast<FImpl*>(D->ClientData);if(S.bClosing || S.CurrentLobby!=UTF8_TO_TCHAR(D->LobbyId))return;
            if(D->CurrentStatus==EOS_ELobbyMemberStatus::EOS_LMS_CLOSED || (D->TargetUserId==S.Self &&
                (D->CurrentStatus==EOS_ELobbyMemberStatus::EOS_LMS_KICKED || D->CurrentStatus==EOS_ELobbyMemberStatus::EOS_LMS_DISCONNECTED)))
            {S.Leave();S.Message=TEXT("街との接続が終了しました。");S.Changed();}
            else S.RefreshMembers();
        });
        return true;
    }
    void Authenticate()
    {
        if(bLogged || bBusy)return;if(!EnsurePlatform()){Changed();return;}
        bBusy=true;Message=TEXT("匿名で接続しています…");Changed();
        EOS_Connect_CreateDeviceIdOptions O={};O.ApiVersion=EOS_CONNECT_CREATEDEVICEID_API_LATEST;O.DeviceModel="PC Windows";
        EOS_Connect_CreateDeviceId(Connect,&O,Call(),[](const EOS_Connect_CreateDeviceIdCallbackInfo* D)
        {
            Complete(D,[D](FImpl& S,const FCall&)
            {if(D->ResultCode==EOS_EResult::EOS_Success || D->ResultCode==EOS_EResult::EOS_DuplicateNotAllowed)S.Login();else S.Error(TEXT("匿名IDを準備できませんでした。"),D->ResultCode);});
        });
    }
    void Login()
    {
        EOS_Connect_Credentials C={};C.ApiVersion=EOS_CONNECT_CREDENTIALS_API_LATEST;C.Type=EOS_EExternalCredentialType::EOS_ECT_DEVICEID_ACCESS_TOKEN;
        EOS_Connect_UserLoginInfo U={};U.ApiVersion=EOS_CONNECT_USERLOGININFO_API_LATEST;U.DisplayName="Traveller";
        EOS_Connect_LoginOptions O={};O.ApiVersion=EOS_CONNECT_LOGIN_API_LATEST;O.Credentials=&C;O.UserLoginInfo=&U;
        EOS_Connect_Login(Connect,&O,Call(),[](const EOS_Connect_LoginCallbackInfo* D)
        {
            Complete(D,[D](FImpl& S,const FCall&)
            {
                if(D->ResultCode==EOS_EResult::EOS_Success)S.Logged(D->LocalUserId);
                else if(D->ResultCode==EOS_EResult::EOS_InvalidUser && D->ContinuanceToken)
                {
                    EOS_Connect_CreateUserOptions UO={};UO.ApiVersion=EOS_CONNECT_CREATEUSER_API_LATEST;UO.ContinuanceToken=D->ContinuanceToken;
                    EOS_Connect_CreateUser(S.Connect,&UO,S.Call(),[](const EOS_Connect_CreateUserCallbackInfo* R)
                    {Complete(R,[R](FImpl& P,const FCall&){if(R->ResultCode==EOS_EResult::EOS_Success)P.Logged(R->LocalUserId);else P.Error(TEXT("匿名IDを登録できませんでした。"),R->ResultCode);});});
                }
                else {S.bLogged=false;S.Leave();S.Error(TEXT("匿名接続に失敗しました。"),D->ResultCode);}
            });
        });
    }
    void Logged(EOS_ProductUserId Id)
    {
        Self=Id;SelfText=ProductId(Id);bLogged=!SelfText.IsEmpty();bBusy=false;
        if(RequestNotice!=EOS_INVALID_NOTIFICATIONID)EOS_P2P_RemoveNotifyPeerConnectionRequest(P2P,RequestNotice);
        EOS_P2P_AddNotifyPeerConnectionRequestOptions N={};N.ApiVersion=EOS_P2P_ADDNOTIFYPEERCONNECTIONREQUEST_API_LATEST;N.SocketId=&Socket;N.LocalUserId=Self;
        RequestNotice=EOS_P2P_AddNotifyPeerConnectionRequest(P2P,&N,this,[](const EOS_P2P_OnIncomingConnectionRequestInfo* D)
        {
            auto& S=*static_cast<FImpl*>(D->ClientData);const FString Remote=ProductId(D->RemoteUserId);
            if(S.bClosing || !S.MemberIds.Contains(Remote) || (Remote!=S.OwnerText && !S.bHost) ||
                FCStringAnsi::Strcmp(D->SocketId->SocketName,S.Socket.SocketName)!=0)return;
            EOS_P2P_AcceptConnectionOptions A={};A.ApiVersion=EOS_P2P_ACCEPTCONNECTION_API_LATEST;
            A.LocalUserId=S.Self;A.RemoteUserId=D->RemoteUserId;A.SocketId=&S.Socket;EOS_P2P_AcceptConnection(S.P2P,&A);
        });
        Message=TEXT("接続しました。街を探すか、新しい街を開けます。");Changed();
    }
    void SetAttribute(EOS_HLobbyModification Mod,const ANSICHAR* Key,const FString& Value)
    {
        FTCHARToUTF8 V(*Value);EOS_Lobby_AttributeData D={};D.ApiVersion=EOS_LOBBY_ATTRIBUTEDATA_API_LATEST;
        D.Key=Key;D.ValueType=EOS_EAttributeType::EOS_AT_STRING;D.Value.AsUtf8=V.Get();
        EOS_LobbyModification_AddAttributeOptions O={};O.ApiVersion=EOS_LOBBYMODIFICATION_ADDATTRIBUTE_API_LATEST;
        O.Attribute=&D;O.Visibility=EOS_ELobbyAttributeVisibility::EOS_LAT_PUBLIC;EOS_LobbyModification_AddAttribute(Mod,&O);
    }
    FString Attribute(EOS_HLobbyDetails Details,const ANSICHAR* Key) const
    {
        EOS_LobbyDetails_CopyAttributeByKeyOptions O={};O.ApiVersion=EOS_LOBBYDETAILS_COPYATTRIBUTEBYKEY_API_LATEST;O.AttrKey=Key;
        EOS_Lobby_Attribute* A=nullptr;FString Result;
        if(EOS_LobbyDetails_CopyAttributeByKey(Details,&O,&A)==EOS_EResult::EOS_Success && A)
        {if(A->Data && A->Data->ValueType==EOS_EAttributeType::EOS_AT_STRING && A->Data->Value.AsUtf8)Result=UTF8_TO_TCHAR(A->Data->Value.AsUtf8);EOS_Lobby_Attribute_Release(A);}
        return Result;
    }
    void Host(const FString& Name,const FString& Language,const FString& Activity)
    {
        if(!bLogged || bBusy || !CurrentLobby.IsEmpty())return;
        PendingName=EWSocial::CleanText(Name,30);if(PendingName.IsEmpty())PendingName=TEXT("空の回廊");
        PendingLanguage=EWSocial::CleanText(Language,8);PendingActivity=EWSocial::CleanText(Activity,12);
        bBusy=true;Message=TEXT("街を開いています…");Changed();
        EOS_Lobby_CreateLobbyOptions O={};O.ApiVersion=EOS_LOBBY_CREATELOBBY_API_LATEST;O.LocalUserId=Self;
        O.MaxLobbyMembers=EWSocial::Capacity;O.PermissionLevel=EOS_ELobbyPermissionLevel::EOS_LPL_PUBLICADVERTISED;
        O.bAllowInvites=true;O.bDisableHostMigration=true;O.bEnableRTCRoom=true;O.BucketId="ew-city88-glasswater";
        O.bEnableJoinById=true;O.bRejoinAfterKickRequiresInvite=true;O.RTCRoomJoinActionType=EOS_ELobbyRTCRoomJoinActionType::EOS_LRRJAT_AutomaticJoin;
        EOS_Lobby_CreateLobby(Lobby,&O,Call(),[](const EOS_Lobby_CreateLobbyCallbackInfo* D)
        {
            TUniquePtr<FCall> C(static_cast<FCall*>(D->ClientData));auto* S=C->Life?C->Life->Owner:nullptr;
            if(!S)return;
            if(C->Epoch!=S->Epoch || S->bClosing){if(D->ResultCode==EOS_EResult::EOS_Success && D->LobbyId)S->CloseLobby(UTF8_TO_TCHAR(D->LobbyId),true);return;}
            if(D->ResultCode!=EOS_EResult::EOS_Success){S->Error(TEXT("街を開けませんでした。"),D->ResultCode);return;}
            S->CurrentLobby=UTF8_TO_TCHAR(D->LobbyId);S->OwnerText=S->SelfText;S->bHost=true;S->MemberIds.Add(S->SelfText);
            EOS_Lobby_UpdateLobbyModificationOptions U={};U.ApiVersion=EOS_LOBBY_UPDATELOBBYMODIFICATION_API_LATEST;
            FTCHARToUTF8 Id(*S->CurrentLobby);U.LobbyId=Id.Get();U.LocalUserId=S->Self;
            EOS_HLobbyModification Mod=nullptr;const auto Result=EOS_Lobby_UpdateLobbyModification(S->Lobby,&U,&Mod);
            if(Result!=EOS_EResult::EOS_Success){S->Leave();S->Error(TEXT("街の情報を登録できませんでした。"),Result);return;}
            S->SetAttribute(Mod,"ew_build",EWSocial::Build);S->SetAttribute(Mod,"ew_name",S->PendingName);
            S->SetAttribute(Mod,"ew_language",S->PendingLanguage);S->SetAttribute(Mod,"ew_activity",S->PendingActivity);
            EOS_Lobby_UpdateLobbyOptions Update={};Update.ApiVersion=EOS_LOBBY_UPDATELOBBY_API_LATEST;Update.LobbyModificationHandle=Mod;
            EOS_Lobby_UpdateLobby(S->Lobby,&Update,S->Call(),[](const EOS_Lobby_UpdateLobbyCallbackInfo* R)
            {Complete(R,[R](FImpl& P,const FCall&){if(R->ResultCode==EOS_EResult::EOS_Success)P.Entered();else {P.Leave();P.Error(TEXT("街の情報を登録できませんでした。"),R->ResultCode);}});});
            EOS_LobbyModification_Release(Mod);
        });
    }
    void Find(const FString& Exact={})
    {
        if(!bLogged || bBusy || !CurrentLobby.IsEmpty())return;
        bBusy=true;PendingJoin=Exact;CityList.Reset();Message=Exact.IsEmpty()?TEXT("街を探しています…"):TEXT("招待された街を探しています…");Changed();
        if(Search){EOS_LobbySearch_Release(Search);Search=nullptr;}
        EOS_Lobby_CreateLobbySearchOptions C={};C.ApiVersion=EOS_LOBBY_CREATELOBBYSEARCH_API_LATEST;C.MaxResults=30;
        auto Result=EOS_Lobby_CreateLobbySearch(Lobby,&C,&Search);if(Result!=EOS_EResult::EOS_Success){Error(TEXT("街を探せませんでした。"),Result);return;}
        if(!Exact.IsEmpty())
        {
            FTCHARToUTF8 Id(*Exact);EOS_LobbySearch_SetLobbyIdOptions O={};O.ApiVersion=EOS_LOBBYSEARCH_SETLOBBYID_API_LATEST;O.LobbyId=Id.Get();
            EOS_LobbySearch_SetLobbyId(Search,&O);
        }
        else
        {
            const FTCHARToUTF8 Build(EWSocial::Build);
            EOS_Lobby_AttributeData A={};A.ApiVersion=EOS_LOBBY_ATTRIBUTEDATA_API_LATEST;A.Key="ew_build";A.ValueType=EOS_EAttributeType::EOS_AT_STRING;A.Value.AsUtf8=Build.Get();
            EOS_LobbySearch_SetParameterOptions O={};O.ApiVersion=EOS_LOBBYSEARCH_SETPARAMETER_API_LATEST;O.Parameter=&A;O.ComparisonOp=EOS_EComparisonOp::EOS_CO_EQUAL;EOS_LobbySearch_SetParameter(Search,&O);
        }
        EOS_LobbySearch_FindOptions F={};F.ApiVersion=EOS_LOBBYSEARCH_FIND_API_LATEST;F.LocalUserId=Self;
        EOS_LobbySearch_Find(Search,&F,Call(),[](const EOS_LobbySearch_FindCallbackInfo* D)
        {
            Complete(D,[D](FImpl& S,const FCall&)
            {
                if(D->ResultCode!=EOS_EResult::EOS_Success){S.Error(TEXT("街を探せませんでした。"),D->ResultCode);return;}
                EOS_LobbySearch_GetSearchResultCountOptions C={};C.ApiVersion=EOS_LOBBYSEARCH_GETSEARCHRESULTCOUNT_API_LATEST;
                const uint32 Count=EOS_LobbySearch_GetSearchResultCount(S.Search,&C);
                for(uint32 I=0;I<FMath::Min(Count,30u);++I)
                {
                    EOS_LobbySearch_CopySearchResultByIndexOptions O={};O.ApiVersion=EOS_LOBBYSEARCH_COPYSEARCHRESULTBYINDEX_API_LATEST;O.LobbyIndex=I;
                    EOS_HLobbyDetails Details=nullptr;if(EOS_LobbySearch_CopySearchResultByIndex(S.Search,&O,&Details)!=EOS_EResult::EOS_Success)continue;
                    EOS_LobbyDetails_CopyInfoOptions CI={};CI.ApiVersion=EOS_LOBBYDETAILS_COPYINFO_API_LATEST;EOS_LobbyDetails_Info* Info=nullptr;
                    if(EOS_LobbyDetails_CopyInfo(Details,&CI,&Info)==EOS_EResult::EOS_Success && Info)
                    {
                        if(S.Attribute(Details,"ew_build")==EWSocial::Build && Info->AvailableSlots>0)
                        {
                            FEWCityListing Row;Row.Id=UTF8_TO_TCHAR(Info->LobbyId);Row.Name=EWSocial::CleanText(S.Attribute(Details,"ew_name"),30);
                            Row.Language=EWSocial::CleanText(S.Attribute(Details,"ew_language"),8);Row.Activity=EWSocial::CleanText(S.Attribute(Details,"ew_activity"),12);
                            Row.Capacity=Info->MaxMembers;Row.Members=Info->MaxMembers-Info->AvailableSlots;S.CityList.Add(Row);
                            if(Row.Id==S.PendingJoin){EOS_LobbyDetails_Info_Release(Info);S.JoinDetails(Details);EOS_LobbyDetails_Release(Details);return;}
                        }
                        EOS_LobbyDetails_Info_Release(Info);
                    }
                    EOS_LobbyDetails_Release(Details);
                }
                S.CityList.Sort([](const FEWCityListing& A,const FEWCityListing& B){return A.Members!=B.Members?A.Members>B.Members:A.Name<B.Name;});
                S.bBusy=false;S.Message=S.CityList.IsEmpty()?TEXT("参加できる街はまだありません。自分で街を開くこともできます。"):TEXT("参加する街を選んでください。");S.Changed();
            });
        });
    }
    void JoinDetails(EOS_HLobbyDetails Details)
    {
        EOS_Lobby_JoinLobbyOptions O={};O.ApiVersion=EOS_LOBBY_JOINLOBBY_API_LATEST;O.LocalUserId=Self;O.LobbyDetailsHandle=Details;
        O.RTCRoomJoinActionType=EOS_ELobbyRTCRoomJoinActionType::EOS_LRRJAT_AutomaticJoin;
        EOS_Lobby_JoinLobby(Lobby,&O,Call(),[](const EOS_Lobby_JoinLobbyCallbackInfo* D)
        {
            TUniquePtr<FCall> C(static_cast<FCall*>(D->ClientData));auto* S=C->Life?C->Life->Owner:nullptr;if(!S)return;
            if(C->Epoch!=S->Epoch || S->bClosing){if(D->ResultCode==EOS_EResult::EOS_Success && D->LobbyId)S->CloseLobby(UTF8_TO_TCHAR(D->LobbyId),false);return;}
            if(D->ResultCode!=EOS_EResult::EOS_Success){S->Error(TEXT("街に参加できませんでした。"),D->ResultCode);return;}
            S->CurrentLobby=UTF8_TO_TCHAR(D->LobbyId);S->bHost=false;S->RefreshMembers();S->Entered();
        });
    }
    void Entered(){bBusy=false;RefreshMembers();StartVoice();Message=bHost?TEXT("街を開きました。"):TEXT("街に接続しました。");Changed();}
    void RefreshMembers()
    {
        if(CurrentLobby.IsEmpty() || !Self)return;FTCHARToUTF8 Id(*CurrentLobby);
        EOS_Lobby_CopyLobbyDetailsHandleOptions O={};O.ApiVersion=EOS_LOBBY_COPYLOBBYDETAILSHANDLE_API_LATEST;O.LocalUserId=Self;O.LobbyId=Id.Get();
        EOS_HLobbyDetails D=nullptr;if(EOS_Lobby_CopyLobbyDetailsHandle(Lobby,&O,&D)!=EOS_EResult::EOS_Success)
        {Leave();Message=TEXT("街が閉じられました。");Changed();return;}
        EOS_LobbyDetails_GetLobbyOwnerOptions Owner={};Owner.ApiVersion=EOS_LOBBYDETAILS_GETLOBBYOWNER_API_LATEST;
        const FString NewOwner=ProductId(EOS_LobbyDetails_GetLobbyOwner(D,&Owner));
        if(!OwnerText.IsEmpty() && OwnerText!=NewOwner){EOS_LobbyDetails_Release(D);Leave();Message=TEXT("ホストとの接続が終了しました。");Changed();return;}
        OwnerText=NewOwner;
        EOS_LobbyDetails_GetMemberCountOptions C={};C.ApiVersion=EOS_LOBBYDETAILS_GETMEMBERCOUNT_API_LATEST;
        const uint32 Count=EOS_LobbyDetails_GetMemberCount(D,&C);TSet<FString> Updated;
        for(uint32 I=0;I<FMath::Min(Count,uint32(EWSocial::Capacity));++I)
        {
            EOS_LobbyDetails_GetMemberByIndexOptions M={};M.ApiVersion=EOS_LOBBYDETAILS_GETMEMBERBYINDEX_API_LATEST;M.MemberIndex=I;
            const FString Member=ProductId(EOS_LobbyDetails_GetMemberByIndex(D,&M));if(!Member.IsEmpty())Updated.Add(Member);
        }
        EOS_LobbyDetails_Release(D);MemberIds=MoveTemp(Updated);
        if(!MemberIds.Contains(SelfText)){Leave();Message=TEXT("街から退出しました。");Changed();}
    }
    void CloseLobby(const FString& Id,bool Host)
    {
        if(!Lobby || !Self || Id.IsEmpty())return;FTCHARToUTF8 U(*Id);
        if(Host)
        {EOS_Lobby_DestroyLobbyOptions O={};O.ApiVersion=EOS_LOBBY_DESTROYLOBBY_API_LATEST;O.LocalUserId=Self;O.LobbyId=U.Get();EOS_Lobby_DestroyLobby(Lobby,&O,nullptr,[](const EOS_Lobby_DestroyLobbyCallbackInfo*){});}
        else
        {EOS_Lobby_LeaveLobbyOptions O={};O.ApiVersion=EOS_LOBBY_LEAVELOBBY_API_LATEST;O.LocalUserId=Self;O.LobbyId=U.Get();EOS_Lobby_LeaveLobby(Lobby,&O,nullptr,[](const EOS_Lobby_LeaveLobbyCallbackInfo*){});}
    }
    void Leave()
    {
        ++Epoch;bBusy=false;StopVoice();CloseLobby(CurrentLobby,bHost);
        if(P2P && Self)
        {EOS_P2P_CloseConnectionsOptions O={};O.ApiVersion=EOS_P2P_CLOSECONNECTIONS_API_LATEST;O.LocalUserId=Self;O.SocketId=&Socket;EOS_P2P_CloseConnections(P2P,&O);}
        CurrentLobby.Reset();OwnerText.Reset();MemberIds.Reset();Wire.Reset();bHost=false;Message=TEXT("街から退出しました。");Changed();
    }
    void StartVoice()
    {
        FModuleManager::LoadModuleChecked<IModuleInterface>("EOSVoiceChat");
        auto* F=FEOSVoiceChatFactory::Get();if(!F){VoiceMessage=TEXT("音声を起動できませんでした。");return;}
        Voice=F->CreateInstanceWithPlatform(Platform);
        if(!Voice || !Voice->Initialize()){VoiceMessage=TEXT("音声を初期化できませんでした。");Voice.Reset();return;}
        const TWeakPtr<FLife> Weak=Life;const uint64 Gen=Epoch;
        Voice->Connect(FOnVoiceChatConnectCompleteDelegate::CreateLambda([Weak,Gen](const FVoiceChatResult& R)
        {
            auto L=Weak.Pin();auto* S=L?L->Owner:nullptr;if(!S || S->Epoch!=Gen || !S->Voice)return;
            if(!R.IsSuccess()){S->VoiceMessage=TEXT("音声に接続できませんでした。");return;}
            S->VoiceUser=S->Voice->CreateUser();S->VoiceUser->SetAudioInputDeviceMuted(true);
            S->VoiceUser->Login(FPlatformUserId::CreateFromInternalId(0),S->SelfText,FString(),FOnVoiceChatLoginCompleteDelegate::CreateLambda([Weak,Gen](const FString&,const FVoiceChatResult& Result)
            {
                auto LL=Weak.Pin();auto* P=LL?LL->Owner:nullptr;if(!P || P->Epoch!=Gen || !P->VoiceUser)return;
                if(!Result.IsSuccess()){P->VoiceMessage=TEXT("音声の匿名接続に失敗しました。");return;}
                // Audio callbacks own only a bounded queue callback, never the connection or UObjects.
                const auto ReceivePCM=P->Outer.OnVoicePCM;
                P->Unmixed=P->VoiceUser->RegisterOnVoiceChatBeforeRecvUnmixedAudioRenderedDelegate(
                    FOnVoiceChatBeforeRecvAudioRenderedDelegate::FDelegate::CreateLambda([ReceivePCM](TArrayView<int16> Samples,int Rate,int Channels,bool,const FString&,const FString& Peer)
                {
                    if(ReceivePCM && Channels>0 && Channels<=2 && Rate>=8000 && Rate<=96000 && Samples.Num()<=19200)
                    {
                        TArray<int16> Mono;Mono.SetNumUninitialized(Samples.Num()/Channels);
                        for(int32 I=0;I<Mono.Num();++I)Mono[I]=Channels==1?Samples[I]:int16((int32(Samples[I*2])+Samples[I*2+1])/2);
                        ReceivePCM(Peer,Mono,Rate);
                    }
                    FMemory::Memzero(Samples.GetData(),Samples.Num()*sizeof(int16));
                }));
                P->Mixed=P->VoiceUser->RegisterOnVoiceChatBeforeRecvMixedAudioRenderedDelegate(
                    FOnVoiceChatBeforeRecvAudioRenderedDelegate::FDelegate::CreateLambda([](TArrayView<int16> Samples,int,int,bool,const FString&,const FString&)
                    {FMemory::Memzero(Samples.GetData(),Samples.Num()*sizeof(int16));}));
                if(!static_cast<FEOSVoiceChatUser*>(P->VoiceUser)->AddLobbyRoom(P->CurrentLobby))P->VoiceMessage=TEXT("街の音声に参加できませんでした。");
                else P->VoiceMessage=TEXT("Vを押している間だけ話せます。");
            }));
        }));
    }
    void StopVoice()
    {
        bMicSending=false;
        if(VoiceUser)
        {
            VoiceUser->SetAudioInputDeviceMuted(true);
            if(Unmixed.IsValid())VoiceUser->UnregisterOnVoiceChatBeforeRecvUnmixedAudioRenderedDelegate(Unmixed);
            if(Mixed.IsValid())VoiceUser->UnregisterOnVoiceChatBeforeRecvMixedAudioRenderedDelegate(Mixed);
            if(!CurrentLobby.IsEmpty())static_cast<FEOSVoiceChatUser*>(VoiceUser)->RemoveLobbyRoom(CurrentLobby);
            if(Voice)Voice->ReleaseUser(VoiceUser);VoiceUser=nullptr;
        }
        Unmixed.Reset();Mixed.Reset();VoiceBlocks.Reset();Voice.Reset();VoiceMessage.Reset();
    }
    bool Send(const FString& Peer,const FString& Text,bool Reliable)
    {
        if(!P2P || !Self || !MemberIds.Contains(Peer) || Peer==SelfText)return false;
        const auto Packets=EWSocial::Frame(Text,++SendSequence,Reliable);if(Packets.IsEmpty())return false;
        for(const auto& Data:Packets)
        {
            EOS_P2P_SendPacketOptions O={};O.ApiVersion=EOS_P2P_SENDPACKET_API_LATEST;O.LocalUserId=Self;O.RemoteUserId=Product(Peer);O.SocketId=&Socket;
            O.Channel=Reliable?1:0;O.Data=Data.GetData();O.DataLengthBytes=Data.Num();O.bAllowDelayedDelivery=Reliable;
            O.Reliability=Reliable?EOS_EPacketReliability::EOS_PR_ReliableOrdered:EOS_EPacketReliability::EOS_PR_UnreliableUnordered;O.bDisableAutoAcceptConnection=false;
            if(EOS_P2P_SendPacket(P2P,&O)!=EOS_EResult::EOS_Success)return false;
        }
        return true;
    }
    void Tick()
    {
        if(!Platform || !Self || CurrentLobby.IsEmpty())return;
        const double Now=FPlatformTime::Seconds();
        if(Now-LastMembership>.5){LastMembership=Now;RefreshMembers();}
        if(CurrentLobby.IsEmpty())return;
        Wire.Prune(Now);
        for(int32 I=0;I<256;++I)
        {
            uint8 Data[EOS_P2P_MAX_PACKET_SIZE];uint32 Size=0;uint8 Channel=0;EOS_ProductUserId From=nullptr;EOS_P2P_SocketId Incoming={};
            EOS_P2P_ReceivePacketOptions O={};O.ApiVersion=EOS_P2P_RECEIVEPACKET_API_LATEST;O.LocalUserId=Self;O.MaxDataSizeBytes=sizeof(Data);
            if(EOS_P2P_ReceivePacket(P2P,&O,&From,&Incoming,&Channel,Data,&Size)!=EOS_EResult::EOS_Success)break;
            const FString Peer=ProductId(From);
            if(!MemberIds.Contains(Peer) || (!bHost && Peer!=OwnerText) || FCStringAnsi::Strcmp(Incoming.SocketName,Socket.SocketName)!=0)continue;
            FString Text;if(Wire.Accept(Peer,Data,int32(Size),Channel,Now,Text) && Outer.OnPacket)Outer.OnPacket(Peer,Text);
        }
    }
};

FEWEOSConnection::FEWEOSConnection():Impl(MakeUnique<FImpl>(*this)){}
FEWEOSConnection::~FEWEOSConnection()=default;
bool FEWEOSConnection::Configure(const FString& P,const FString& C){return Impl->Configure(P,C);}
void FEWEOSConnection::Authenticate(){Impl->Authenticate();}
void FEWEOSConnection::Host(const FString& N,const FString& L,const FString& A){Impl->Host(N,L,A);}
void FEWEOSConnection::Find(){Impl->Find();}
void FEWEOSConnection::Join(const FString& Id){if(Id.Len()>=4 && Id.Len()<=64)Impl->Find(Id);else {Impl->Message=TEXT("招待コードが正しくありません。");Impl->Changed();}}
void FEWEOSConnection::Leave(){Impl->Leave();}
void FEWEOSConnection::Tick(){Impl->Tick();}
bool FEWEOSConnection::Send(const FString& P,const FString& T,bool R){return Impl->Send(P,T,R);}
bool FEWEOSConnection::Configured()const{return Impl->bConfigured;}
bool FEWEOSConnection::LoggedIn()const{return Impl->bLogged;}
bool FEWEOSConnection::Busy()const{return Impl->bBusy;}
bool FEWEOSConnection::InLobby()const{return !Impl->CurrentLobby.IsEmpty() && !Impl->bBusy;}
bool FEWEOSConnection::Hosting()const{return InLobby() && Impl->bHost;}
FString FEWEOSConnection::Status()const{return Impl->Message;}
FString FEWEOSConnection::SelfId()const{return Impl->SelfText;}
FString FEWEOSConnection::HostId()const{return Impl->OwnerText;}
FString FEWEOSConnection::LobbyId()const{return Impl->CurrentLobby;}
const TSet<FString>& FEWEOSConnection::Members()const{return Impl->MemberIds;}
const TArray<FEWCityListing>& FEWEOSConnection::Listings()const{return Impl->CityList;}
bool FEWEOSConnection::VoiceReady()const
{
    if(!Impl->VoiceUser || Impl->CurrentLobby.IsEmpty() || Impl->VoiceUser->GetChannels().IsEmpty())return false;
    FTCHARToUTF8 Id(*Impl->CurrentLobby);EOS_Lobby_IsRTCRoomConnectedOptions O={};O.ApiVersion=EOS_LOBBY_ISRTCROOMCONNECTED_API_LATEST;
    O.LocalUserId=Impl->Self;O.LobbyId=Id.Get();EOS_Bool Connected=EOS_FALSE;
    return EOS_Lobby_IsRTCRoomConnected(Impl->Lobby,&O,&Connected)==EOS_EResult::EOS_Success && Connected==EOS_TRUE;
}
FString FEWEOSConnection::VoiceStatus()const{return Impl->VoiceMessage;}
void FEWEOSConnection::SetMicrophone(bool Sending)
{if(Impl->VoiceUser && Impl->bMicSending!=Sending){Impl->bMicSending=Sending;Impl->VoiceUser->SetAudioInputDeviceMuted(!Sending);}}
void FEWEOSConnection::BlockVoice(const FString& Id,bool Blocked)
{
    if(!Impl->VoiceUser || Impl->VoiceBlocks.Contains(Id)==Blocked)return;
    if(Blocked){Impl->VoiceBlocks.Add(Id);Impl->VoiceUser->BlockPlayers({Id});}
    else{Impl->VoiceBlocks.Remove(Id);Impl->VoiceUser->UnblockPlayers({Id});}
}
TArray<TPair<FString,FString>> FEWEOSConnection::Microphones()const
{
    TArray<TPair<FString,FString>> Result;if(Impl->VoiceUser)for(const auto& D:Impl->VoiceUser->GetAvailableInputDeviceInfos())Result.Emplace(D.Id,D.DisplayName);return Result;
}
void FEWEOSConnection::SelectMicrophone(const FString& Id){if(Impl->VoiceUser)Impl->VoiceUser->SetInputDeviceId(Id);}
void FEWEOSConnection::Kick(const FString& Peer)
{
    if(!Hosting() || Peer==SelfId() || !Members().Contains(Peer))return;
    FTCHARToUTF8 Id(*Impl->CurrentLobby);EOS_Lobby_KickMemberOptions O={};O.ApiVersion=EOS_LOBBY_KICKMEMBER_API_LATEST;
    O.LobbyId=Id.Get();O.LocalUserId=Impl->Self;O.TargetUserId=Product(Peer);
    EOS_Lobby_KickMember(Impl->Lobby,&O,Impl->Call(),[](const EOS_Lobby_KickMemberCallbackInfo* D)
    {FImpl::Complete(D,[D](FImpl& S,const FImpl::FCall&){if(D->ResultCode!=EOS_EResult::EOS_Success)S.Error(TEXT("退出操作に失敗しました。"),D->ResultCode);else S.RefreshMembers();});});
}
void FEWEOSConnection::Report(const FString& Peer,const FString& Reason)
{
    if(!Impl->bLogged || !Members().Contains(Peer) || Peer==SelfId())return;
    const FString Safe=EWSocial::CleanText(Reason,240);if(Safe.IsEmpty())return;FTCHARToUTF8 Text(*Safe);
    EOS_Reports_SendPlayerBehaviorReportOptions O={};O.ApiVersion=EOS_REPORTS_SENDPLAYERBEHAVIORREPORT_API_LATEST;
    O.ReporterUserId=Impl->Self;O.ReportedUserId=Product(Peer);O.Category=EOS_EPlayerReportsCategory::EOS_PRC_Other;O.Message=Text.Get();
    EOS_Reports_SendPlayerBehaviorReport(EOS_Platform_GetReportsInterface(*Impl->Platform),&O,Impl->Call(),[](const EOS_Reports_SendPlayerBehaviorReportCompleteCallbackInfo* D)
    {FImpl::Complete(D,[D](FImpl& S,const FImpl::FCall&){if(D->ResultCode==EOS_EResult::EOS_Success){S.Message=TEXT("通報を受け付けました。");S.Changed();}else S.Error(TEXT("通報を送信できませんでした。"),D->ResultCode);});});
}
