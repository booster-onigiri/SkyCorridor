#include "EWChessRules.h"
#include <algorithm>
#include <cmath>
#include <sstream>
#include <cctype>

namespace EWChess
{
namespace
{
const char* Pieces=" PNBRQK";
bool Inside(int X,int Y){return X>=0 && X<8 && Y>=0 && Y<8;}
int Sign(int P){return (P>0)-(P<0);}
std::string Square(int S){return std::string(1,char('a'+S%8))+char('1'+S/8);}
}
std::string Move::Uci() const
{std::string S=Square(From)+Square(To);if(Promotion)S+=char(std::tolower(Pieces[Promotion]));return S;}
Position Position::Initial()
{Position P;P.LoadFen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");return P;}
bool Position::LoadFen(const std::string& Text)
{
    if(Text.size()>256)return false;
    std::istringstream In(Text);std::string Map,Turn,Rights,Ep,Extra;Position P;
    if(!(In>>Map>>Turn>>Rights>>Ep>>P.Halfmove>>P.Fullmove) || (In>>Extra) || P.Halfmove<0 || P.Fullmove<1)return false;
    int X=0,Y=7;
    for(char C:Map)
    {
        if(C=='/'){if(X!=8 || Y==0)return false;X=0;--Y;continue;}
        if(C>='1' && C<='8'){X+=C-'0';if(X>8)return false;continue;}
        if(!Inside(X,Y))return false;
        int K=0;for(int I=1;I<=6;++I)if(std::toupper(C)==Pieces[I])K=I;
        if(!K)return false;P.Board[Y*8+X++]=(std::isupper(C)?1:-1)*K;
    }
    if(X!=8 || Y!=0 || (Turn!="w" && Turn!="b"))return false;
    if(std::count(P.Board.begin(),P.Board.end(),6)!=1 || std::count(P.Board.begin(),P.Board.end(),-6)!=1)return false;
    for(int I=0;I<8;++I)if(std::abs(P.Board[I])==1 || std::abs(P.Board[56+I])==1)return false;
    P.Side=Turn=="w"?1:-1;P.Castle=0;
    if(Rights!="-")for(char C:Rights){int B=C=='K'?1:C=='Q'?2:C=='k'?4:C=='q'?8:0;if(!B || (P.Castle&B))return false;P.Castle|=B;}
    P.EnPassant=-1;
    if(Ep!="-")
    {
        if(Ep.size()!=2 || Ep[0]<'a' || Ep[0]>'h' || Ep[1]!=(P.Side==1?'6':'3'))return false;
        P.EnPassant=(Ep[1]-'1')*8+Ep[0]-'a';
        if(P.Board[P.EnPassant]!=0 || P.Board[P.EnPassant-P.Side*8]!=-P.Side)return false;
    }
    if(P.Check(-P.Side))return false;
    *this=P;return true;
}
std::string Position::Fen() const
{
    std::string S;
    for(int Y=7;Y>=0;--Y)
    {
        int Empty=0;
        for(int X=0;X<8;++X){int P=Board[Y*8+X];if(!P){++Empty;continue;}if(Empty){S+=char('0'+Empty);Empty=0;}char C=Pieces[std::abs(P)];S+=P>0?C:char(std::tolower(C));}
        if(Empty)S+=char('0'+Empty);if(Y)S+='/';
    }
    S+=Side==1?" w ":" b ";if(!Castle)S+='-';
    if(Castle&1)S+='K';if(Castle&2)S+='Q';if(Castle&4)S+='k';if(Castle&8)S+='q';
    return S+" "+(EnPassant<0?"-":Square(EnPassant))+" "+std::to_string(Halfmove)+" "+std::to_string(Fullmove);
}
bool Position::Attacked(int S,int By) const
{
    const int X=S%8,Y=S/8;
    for(int Dx:{-1,1})if(Inside(X+Dx,Y-By) && Board[(Y-By)*8+X+Dx]==By)return true;
    for(int Dx=-2;Dx<=2;++Dx)for(int Dy=-2;Dy<=2;++Dy)
        if(std::abs(Dx*Dy)==2 && Inside(X+Dx,Y+Dy) && Board[(Y+Dy)*8+X+Dx]==By*2)return true;
    for(int Dx=-1;Dx<=1;++Dx)for(int Dy=-1;Dy<=1;++Dy)if(Dx || Dy)
    {
        int Step=1;
        for(int Nx=X+Dx,Ny=Y+Dy;Inside(Nx,Ny);Nx+=Dx,Ny+=Dy,++Step)
        {
            const int P=Board[Ny*8+Nx];if(!P)continue;
            if(Sign(P)==By){int K=std::abs(P);if(K==5 || (K==6 && Step==1) || (K==3 && Dx && Dy) || (K==4 && (!Dx || !Dy)))return true;}
            break;
        }
    }
    return false;
}
bool Position::Check(int Colour) const
{for(int S=0;S<64;++S)if(Board[S]==Colour*6)return Attacked(S,-Colour);return true;}
Position Position::After(const Move& M) const
{
    Position P=*this;const int Piece=Board[M.From];const bool Capture=Board[M.To]!=0;
    P.Board[M.To]=M.Promotion?Side*M.Promotion:Piece;P.Board[M.From]=0;P.EnPassant=-1;
    if(std::abs(Piece)==1)
    {
        if(M.To==EnPassant && !Capture)P.Board[M.To-Side*8]=0;
        if(std::abs(M.To-M.From)==16)P.EnPassant=(M.From+M.To)/2;
    }
    if(std::abs(Piece)==6)
    {
        P.Castle&=Side==1?~3:~12;
        if(std::abs(M.To-M.From)==2){int R=M.To>M.From?M.From+3:M.From-4;P.Board[(M.From+M.To)/2]=P.Board[R];P.Board[R]=0;}
    }
    if(M.From==0 || M.To==0)P.Castle&=~2;if(M.From==7 || M.To==7)P.Castle&=~1;
    if(M.From==56 || M.To==56)P.Castle&=~8;if(M.From==63 || M.To==63)P.Castle&=~4;
    P.Halfmove=std::abs(Piece)==1 || Capture?0:Halfmove+1;P.Fullmove=Fullmove+(Side==-1);P.Side=-Side;return P;
}
std::vector<Move> Position::Legal() const
{
    std::vector<Move> Result;
    auto Add=[&](int A,int B,int Promotion=0){if(std::abs(Board[B])==6)return;Move M{A,B,Promotion};if(!After(M).Check(Side))Result.push_back(M);};
    auto Pawn=[&](int A,int B){if(B/8==0 || B/8==7){for(int K:{5,4,3,2})Add(A,B,K);}else Add(A,B);};
    for(int A=0;A<64;++A)
    {
        const int Piece=Board[A],K=std::abs(Piece),X=A%8,Y=A/8;if(Sign(Piece)!=Side)continue;
        if(K==1)
        {
            if(Inside(X,Y+Side) && !Board[A+Side*8]){Pawn(A,A+Side*8);if(Y==(Side==1?1:6) && !Board[A+Side*16])Add(A,A+Side*16);}
            for(int Dx:{-1,1})if(Inside(X+Dx,Y+Side)){int B=(Y+Side)*8+X+Dx;if(Sign(Board[B])==-Side || (B==EnPassant && Board[B-Side*8]==-Side))Pawn(A,B);}
            continue;
        }
        if(K==2)
        {for(int Dx=-2;Dx<=2;++Dx)for(int Dy=-2;Dy<=2;++Dy)if(std::abs(Dx*Dy)==2 && Inside(X+Dx,Y+Dy)){int B=(Y+Dy)*8+X+Dx;if(Sign(Board[B])!=Side)Add(A,B);}continue;}
        for(int Dx=-1;Dx<=1;++Dx)for(int Dy=-1;Dy<=1;++Dy)
        {
            if((!Dx && !Dy) || (K==3 && (!Dx || !Dy)) || (K==4 && Dx && Dy))continue;
            for(int Nx=X+Dx,Ny=Y+Dy;Inside(Nx,Ny);Nx+=Dx,Ny+=Dy){int B=Ny*8+Nx;if(Sign(Board[B])==Side)break;Add(A,B);if(Board[B] || K==6)break;}
        }
        if(K==6 && A==(Side==1?4:60) && !Check(Side))
        {
            int Bit=Side==1?1:4;
            for(int Dir:{1,-1})
            {
                int Right=Dir==1?Bit:Bit*2,R=A+(Dir==1?3:-4);
                if(!(Castle&Right) || Board[R]!=Side*4 || Board[A+Dir] || Board[A+Dir*2] || (Dir==-1 && Board[A-3]))continue;
                if(!After({A,A+Dir,0}).Check(Side))Add(A,A+Dir*2);
            }
        }
    }
    return Result;
}
std::string Position::Key() const
{
    Position P=*this;
    if(EnPassant>=0){bool Can=false;for(const auto& M:Legal())if(M.To==EnPassant && std::abs(Board[M.From])==1 && !Board[M.To])Can=true;if(!Can)P.EnPassant=-1;}
    const std::string S=P.Fen();size_t Cut=S.find_last_of(' ');return S.substr(0,S.find_last_of(' ',Cut-1));
}
bool Position::InsufficientMaterial() const
{
    int Minors=0,Knights=0,BishopColour=-1;
    for(int S=0;S<64;++S)
    {
        int K=std::abs(Board[S]);if(K==0 || K==6)continue;
        if(K==1 || K==4 || K==5)return false;++Minors;
        if(K==2)++Knights;
        if(K==3){int C=(S%8+S/8)%2;if(BishopColour>=0 && C!=BishopColour)return false;BishopColour=C;}
    }
    return Minors<=1 || (Knights==0);
}
bool Game::Play(const std::string& Uci)
{
    if(Moves.size()>=4096 || Finished())return false;
    for(const auto& M:At.Legal())if(M.Uci()==Uci){At=At.After(M);Moves.push_back(Uci);Keys.push_back(At.Key());return true;}return false;
}
bool Game::Undo()
{
    if(Moves.empty())return false;auto Replay=Moves;Replay.pop_back();Game G;
    for(const auto& M:Replay)if(!G.Play(M))return false;*this=G;return true;
}
int Game::Repetitions() const{return int(std::count(Keys.begin(),Keys.end(),At.Key()));}
bool Game::CanClaimDraw() const{return !Finished() && (At.Halfmove>=100 || Repetitions()>=3);}
bool Game::ClaimDraw(){if(!CanClaimDraw())return false;DrawClaimed=true;return true;}
std::string Game::Status() const
{
    if(At.Legal().empty())return At.Check(At.Side)?"checkmate":"stalemate";
    if(DrawClaimed)return "claimed";
    if(At.InsufficientMaterial())return "material";
    if(At.Halfmove>=150)return "seventy-five";
    if(Repetitions()>=5)return "repetition";
    return At.Check(At.Side)?"check":"active";
}
bool Game::Finished() const{const auto S=Status();return S!="active" && S!="check";}
}
