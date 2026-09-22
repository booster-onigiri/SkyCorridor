#pragma once
#include <array>
#include <string>
#include <vector>

// A bounded, data-driven concept selects this capability; package files never supply executable code.
namespace EWChess
{
struct Move
{
    int From=-1, To=-1, Promotion=0;
    std::string Uci() const;
    bool operator==(const Move& Other) const { return From==Other.From && To==Other.To && Promotion==Other.Promotion; }
};
struct Position
{
    std::array<int,64> Board{};
    int Side=1, Castle=15, EnPassant=-1, Halfmove=0, Fullmove=1;
    static Position Initial();
    bool LoadFen(const std::string& Fen);
    std::string Fen() const;
    bool Attacked(int Square,int By) const;
    bool Check(int Colour) const;
    std::vector<Move> Legal() const;
    Position After(const Move& M) const;
    std::string Key() const;
    bool InsufficientMaterial() const;
};
struct Game
{
    Position At=Position::Initial();
    std::vector<std::string> Moves;
    std::vector<std::string> Keys{At.Key()};
    bool DrawClaimed=false;
    bool Play(const std::string& Uci);
    bool Undo();
    int Repetitions() const;
    bool CanClaimDraw() const;
    bool ClaimDraw();
    // active, check, checkmate, stalemate, material, repetition, seventy-five, claimed
    std::string Status() const;
    bool Finished() const;
};
}
