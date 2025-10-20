#include "evaluate.h"

#include "bitboard.h"
#include "position.h"
#include "types.h"

namespace Stockfish::Eval {

// Piece values in centipawns
constexpr Value PieceValues[PIECE_TYPE_NB] = {
  VALUE_ZERO, 100, 320, 330, 500, 900, VALUE_ZERO, VALUE_ZERO
};

// Piece-square tables (from white's perspective)
// Values in centipawns
constexpr Value PawnTable[SQUARE_NB] = {
     0,  0,  0,  0,  0,  0,  0,  0,
    50, 50, 50, 50, 50, 50, 50, 50,
    10, 10, 20, 30, 30, 20, 10, 10,
     5,  5, 10, 25, 25, 10,  5,  5,
     0,  0,  0, 20, 20,  0,  0,  0,
     5, -5,-10,  0,  0,-10, -5,  5,
     5, 10, 10,-20,-20, 10, 10,  5,
     0,  0,  0,  0,  0,  0,  0,  0
};

constexpr Value KnightTable[SQUARE_NB] = {
   -50,-40,-30,-30,-30,-30,-40,-50,
   -40,-20,  0,  0,  0,  0,-20,-40,
   -30,  0, 10, 15, 15, 10,  0,-30,
   -30,  5, 15, 20, 20, 15,  5,-30,
   -30,  0, 15, 20, 20, 15,  0,-30,
   -30,  5, 10, 15, 15, 10,  5,-30,
   -40,-20,  0,  5,  5,  0,-20,-40,
   -50,-40,-30,-30,-30,-30,-40,-50
};

constexpr Value BishopTable[SQUARE_NB] = {
   -20,-10,-10,-10,-10,-10,-10,-20,
   -10,  0,  0,  0,  0,  0,  0,-10,
   -10,  0,  5, 10, 10,  5,  0,-10,
   -10,  5,  5, 10, 10,  5,  5,-10,
   -10,  0, 10, 10, 10, 10,  0,-10,
   -10, 10, 10, 10, 10, 10, 10,-10,
   -10,  5,  0,  0,  0,  0,  5,-10,
   -20,-10,-10,-10,-10,-10,-10,-20
};

constexpr Value RookTable[SQUARE_NB] = {
     0,  0,  0,  0,  0,  0,  0,  0,
     5, 10, 10, 10, 10, 10, 10,  5,
    -5,  0,  0,  0,  0,  0,  0, -5,
    -5,  0,  0,  0,  0,  0,  0, -5,
    -5,  0,  0,  0,  0,  0,  0, -5,
    -5,  0,  0,  0,  0,  0,  0, -5,
    -5,  0,  0,  0,  0,  0,  0, -5,
     0,  0,  0,  5,  5,  0,  0,  0
};

constexpr Value QueenTable[SQUARE_NB] = {
   -20,-10,-10, -5, -5,-10,-10,-20,
   -10,  0,  0,  0,  0,  0,  0,-10,
   -10,  0,  5,  5,  5,  5,  0,-10,
    -5,  0,  5,  5,  5,  5,  0, -5,
     0,  0,  5,  5,  5,  5,  0, -5,
   -10,  5,  5,  5,  5,  5,  0,-10,
   -10,  0,  5,  0,  0,  0,  0,-10,
   -20,-10,-10, -5, -5,-10,-10,-20
};

constexpr Value KingMiddleTable[SQUARE_NB] = {
   -30,-40,-40,-50,-50,-40,-40,-30,
   -30,-40,-40,-50,-50,-40,-40,-30,
   -30,-40,-40,-50,-50,-40,-40,-30,
   -30,-40,-40,-50,-50,-40,-40,-30,
   -20,-30,-30,-40,-40,-30,-30,-20,
   -10,-20,-20,-20,-20,-20,-20,-10,
    20, 20,  0,  0,  0,  0, 20, 20,
    20, 30, 10,  0,  0, 10, 30, 20
};

// Flip square for black's perspective
constexpr Square flip_square(Square s) {
    return Square(int(s) ^ int(SQ_A8));
}

// Get piece-square table value
Value psqt_value(Piece pc, Square s) {
    PieceType pt = type_of(pc);
    Color c = color_of(pc);
    
    // Flip square for black pieces
    Square sq = c == WHITE ? s : flip_square(s);
    
    Value value = PieceValues[pt];
    
    switch (pt) {
        case PAWN:   value += PawnTable[sq]; break;
        case KNIGHT: value += KnightTable[sq]; break;
        case BISHOP: value += BishopTable[sq]; break;
        case ROOK:   value += RookTable[sq]; break;
        case QUEEN:  value += QueenTable[sq]; break;
        case KING:   value += KingMiddleTable[sq]; break;
        default: break;
    }
    
    return c == WHITE ? value : -value;
}

// Simple evaluation: material + piece-square tables
Value evaluate(const Position& pos) {
    Value score = VALUE_ZERO;
    
    // Evaluate all pieces on the board
    for (Square s = SQ_A1; s <= SQ_H8; ++s) {
        Piece pc = pos.piece_on(s);
        if (pc != NO_PIECE) {
            score += psqt_value(pc, s);
        }
    }
    
    // Return score from side to move's perspective
    return pos.side_to_move() == WHITE ? score : -score;
}

}  // namespace Stockfish::Eval
