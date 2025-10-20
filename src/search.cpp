#include "search.h"

#include <algorithm>
#include <chrono>
#include <iostream>
#include <vector>
#include <cstring>
#include "evaluate.h"
#include "movegen.h"
#include "position.h"
#include "types.h"

namespace Stockfish {
    // Minimal TranspositionTable class for null move compatibility
    class TranspositionTable {
    public:
        void prefetch([[maybe_unused]] Key key) const {}
    };
}

namespace Stockfish::Search {

namespace {
    static Stockfish::TranspositionTable dummyTT;
    uint64_t nodeCount;
    std::chrono::steady_clock::time_point searchStart;
    int searchTimeMs;
    bool stopSearch;
    
    // Killer moves for move ordering
    Move killerMoves[MAX_PLY][2];
    
    // History heuristic table
    int history[COLOR_NB][SQUARE_NB][SQUARE_NB];
    
    // Simple transposition table
    struct TTEntry {
        Key key;
        Move best_move;
        Value value;
        int depth;
        uint8_t flag; // 0=exact, 1=lower, 2=upper
    };
    
    constexpr int TT_SIZE = 1 << 20; // 1M entries
    TTEntry tt[TT_SIZE];
}

// MVV-LVA (Most Valuable Victim - Least Valuable Attacker) scores
constexpr int mvv_lva_scores[PIECE_TYPE_NB][PIECE_TYPE_NB] = {
    {0, 0, 0, 0, 0, 0, 0, 0},  // NO_PIECE_TYPE
    {0, 15, 14, 13, 12, 11, 10, 0},  // PAWN captures
    {0, 25, 24, 23, 22, 21, 20, 0},  // KNIGHT captures
    {0, 35, 34, 33, 32, 31, 30, 0},  // BISHOP captures
    {0, 45, 44, 43, 42, 41, 40, 0},  // ROOK captures
    {0, 55, 54, 53, 52, 51, 50, 0},  // QUEEN captures
    {0, 0, 0, 0, 0, 0, 0, 0},  // KING
    {0, 0, 0, 0, 0, 0, 0, 0}   // ALL_PIECES
};

// Score a move for ordering
int score_move(const Position& pos, Move m, Move tt_move, int ply) {
    if (m == tt_move)
        return 1000000;
    
    Piece moved = pos.moved_piece(m);
    Square to = m.to_sq();
    
    // Captures with MVV-LVA
    if (pos.capture(m)) {
        Piece captured = pos.piece_on(to);
        if (captured != NO_PIECE) {
            return 900000 + mvv_lva_scores[type_of(moved)][type_of(captured)] * 1000;
        }
    }
    
    // Killer moves
    if (m == killerMoves[ply][0])
        return 800000;
    if (m == killerMoves[ply][1])
        return 799000;
    
    // History heuristic
    return history[color_of(moved)][m.from_sq()][to];
}

// Check if we should stop searching
bool should_stop() {
    if (nodeCount % 2048 == 0) {
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - searchStart).count();
        if (elapsed >= searchTimeMs) {
            stopSearch = true;
        }
    }
    return stopSearch;
}

// Quiescence search with capture search
Value qsearch(Position& pos, Value alpha, Value beta, int ply) {
    if (ply > MAX_PLY - 1)
        return Eval::evaluate(pos);
        
    nodeCount++;
    
    Value stand_pat = Eval::evaluate(pos);
    
    if (stand_pat >= beta)
        return beta;
    if (alpha < stand_pat)
        alpha = stand_pat;
    
    // Generate captures/checks
    Move moveList[MAX_MOVES];
    Move* begin = moveList;
    Move* end;
    
    // In check, search all evasions
    if (pos.checkers()) {
        end = generate<EVASIONS>(pos, begin);
    } else {
        end = generate<CAPTURES>(pos, begin);
    }
    
    // Score and sort moves
    int scores[MAX_MOVES];
    for (Move* m = begin; m < end; ++m) {
        scores[m - begin] = score_move(pos, *m, Move::none(), ply);
    }
    
    for (Move* m = begin; m < end; ++m) {
        // Find best remaining move
        Move* best = m;
        for (Move* cur = m + 1; cur < end; ++cur) {
            if (scores[cur - begin] > scores[best - begin])
                best = cur;
        }
        if (best != m) {
            std::swap(*m, *best);
            std::swap(scores[m - begin], scores[best - begin]);
        }
        
        if (!pos.legal(*m))
            continue;
        
        StateInfo st;
        pos.do_move(*m, st, nullptr);
        Value score = -qsearch(pos, -beta, -alpha, ply + 1);
        pos.undo_move(*m);
        
        if (score >= beta)
            return beta;
        if (score > alpha)
            alpha = score;
    }
    
    return alpha;
}

// Alpha-beta search with TT, null move, and move ordering
Value alphabeta(Position& pos, int depth, Value alpha, Value beta, int ply, bool doNull = true) {
    if (should_stop())
        return VALUE_ZERO;
    
    if (ply > MAX_PLY - 1)
        return Eval::evaluate(pos);
        
    if (depth <= 0)
        return qsearch(pos, alpha, beta, ply);
    
    nodeCount++;
    
    // Check for draw
    if (ply > 0 && (pos.is_draw(pos.game_ply()) || pos.rule50_count() >= 100))
        return VALUE_DRAW;
    
    bool inCheck = pos.checkers();
    Value originalAlpha = alpha;
    
    // Probe transposition table
    Key posKey = pos.key();
    int ttIndex = posKey % TT_SIZE;
    TTEntry& tte = tt[ttIndex];
    Move ttMove = Move::none();
    
    if (tte.key == posKey && tte.depth >= depth) {
        ttMove = tte.best_move;
        if (tte.flag == 0) { // Exact
            return tte.value;
        } else if (tte.flag == 1 && tte.value >= beta) { // Lower bound
            return beta;
        } else if (tte.flag == 2 && tte.value <= alpha) { // Upper bound
            return alpha;
        }
    } else if (tte.key == posKey) {
        ttMove = tte.best_move;
    }
    
        // Null move pruning
        if (doNull && !inCheck && depth >= 3 && ply > 0) {
            StateInfo st;
            pos.do_null_move(st, dummyTT);
            Value nullScore = -alphabeta(pos, depth - 3, -beta, -beta + 1, ply + 1, false);
            pos.undo_null_move();
            
            if (nullScore >= beta)
                return beta;
        }    // Generate moves
    Move moveList[MAX_MOVES];
    Move* begin = moveList;
    Move* end = generate<LEGAL>(pos, begin);
    
    // Checkmate or stalemate
    if (begin == end) {
        return inCheck ? mated_in(ply) : VALUE_DRAW;
    }
    
    // Score and sort moves
    int scores[MAX_MOVES];
    for (Move* m = begin; m < end; ++m) {
        scores[m - begin] = score_move(pos, *m, ttMove, ply);
    }
    
    Value bestScore = -VALUE_INFINITE;
    Move bestMove = Move::none();
    
    for (Move* m = begin; m < end; ++m) {
        // Find best remaining move
        Move* best = m;
        for (Move* cur = m + 1; cur < end; ++cur) {
            if (scores[cur - begin] > scores[best - begin])
                best = cur;
        }
        if (best != m) {
            std::swap(*m, *best);
            std::swap(scores[m - begin], scores[best - begin]);
        }
        
        StateInfo st;
        pos.do_move(*m, st, nullptr);
        Value score = -alphabeta(pos, depth - 1, -beta, -alpha, ply + 1, true);
        pos.undo_move(*m);
        
        if (should_stop())
            return bestScore;
        
        if (score > bestScore) {
            bestScore = score;
            bestMove = *m;
            
            if (score > alpha) {
                alpha = score;
                
                if (alpha >= beta) {
                    // Beta cutoff - update killers and history
                    if (!pos.capture(*m)) {
                        // Update killer moves
                        if (killerMoves[ply][0] != *m) {
                            killerMoves[ply][1] = killerMoves[ply][0];
                            killerMoves[ply][0] = *m;
                        }
                        
                        // Update history heuristic
                        Piece moved = pos.moved_piece(*m);
                        history[color_of(moved)][m->from_sq()][m->to_sq()] += depth * depth;
                    }
                    break;
                }
            }
        }
    }
    
    // Store in transposition table
    tte.key = posKey;
    tte.best_move = bestMove;
    tte.value = bestScore;
    tte.depth = depth;
    if (bestScore <= originalAlpha)
        tte.flag = 2; // Upper bound
    else if (bestScore >= beta)
        tte.flag = 1; // Lower bound
    else
        tte.flag = 0; // Exact
    
    return bestScore;
}

// Iterative deepening search
SearchResult search(Position& pos, int maxDepth, int timeMs) {
    nodeCount = 0;
    searchStart = std::chrono::steady_clock::now();
    searchTimeMs = timeMs;
    stopSearch = false;
    
    // Clear killer moves and history
    std::memset(killerMoves, 0, sizeof(killerMoves));
    std::memset(history, 0, sizeof(history));
    
    SearchResult result;
    result.bestMove = Move::none();
    result.score = VALUE_ZERO;
    result.depth = 0;
    
    // Generate root moves
    Move rootMoves[MAX_MOVES];
    Move* begin = rootMoves;
    Move* end = generate<LEGAL>(pos, begin);
    int numMoves = end - begin;
    
    // No legal moves
    if (numMoves == 0) {
        result.nodes = nodeCount;
        return result;
    }
    
    // Only one legal move
    if (numMoves == 1) {
        result.bestMove = rootMoves[0];
        result.score = VALUE_ZERO;
        result.depth = 0;
        result.nodes = nodeCount;
        return result;
    }
    
    Move prevBestMove = Move::none();
    
    // Iterative deepening
    for (int depth = 1; depth <= maxDepth && depth <= 20; ++depth) {
        if (should_stop())
            break;
        
        // Score and sort root moves
        int scores[MAX_MOVES];
        for (int i = 0; i < numMoves; ++i) {
            scores[i] = score_move(pos, rootMoves[i], prevBestMove, 0);
        }
        
        Value alpha = -VALUE_INFINITE;
        Value beta = VALUE_INFINITE;
        Move bestMove = Move::none();
        Value bestScore = -VALUE_INFINITE;
        
        for (int i = 0; i < numMoves; ++i) {
            // Find best remaining move
            int best = i;
            for (int j = i + 1; j < numMoves; ++j) {
                if (scores[j] > scores[best])
                    best = j;
            }
            if (best != i) {
                std::swap(rootMoves[i], rootMoves[best]);
                std::swap(scores[i], scores[best]);
            }
            
            StateInfo st;
            pos.do_move(rootMoves[i], st, nullptr);
            Value score = -alphabeta(pos, depth - 1, -beta, -alpha, 1, true);
            pos.undo_move(rootMoves[i]);
            
            if (should_stop())
                break;
            
            if (score > bestScore) {
                bestScore = score;
                bestMove = rootMoves[i];
                
                if (score > alpha)
                    alpha = score;
            }
        }
        
        if (!should_stop() && bestMove != Move::none()) {
            result.bestMove = bestMove;
            result.score = bestScore;
            result.depth = depth;
            prevBestMove = bestMove;
        }
        
        // Stop if we found a mate
        if (bestScore >= VALUE_MATE_IN_MAX_PLY || bestScore <= -VALUE_MATE_IN_MAX_PLY)
            break;
    }
    
    result.nodes = nodeCount;
    return result;
}

}  // namespace Stockfish::Search
