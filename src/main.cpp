#include <iostream>
#include <string>
#include <sstream>
#include <vector>
#include <random>
#include <chrono>
#include <iomanip>

#include "types.h"
#include "bitboard.h"
#include "position.h"
#include "movegen.h"
#include "search.h"
#include "evaluate.h"

using namespace Stockfish;

// Convert move to UCI string
std::string move_to_uci(Move m) {
    if (m == Move::none())
        return "0000";
    
    Square from = m.from_sq();
    Square to = m.to_sq();
    
    std::string uci;
    uci += char('a' + file_of(from));
    uci += char('1' + rank_of(from));
    uci += char('a' + file_of(to));
    uci += char('1' + rank_of(to));
    
    if (m.type_of() == PROMOTION) {
        char promo[] = " nbrq";
        uci += promo[m.promotion_type()];
    }
    
    return uci;
}

// Analyze command: analyze position and return best move
void cmd_analyze(const std::string& fen) {
    std::cout << "Analyzing FEN: " << fen << std::endl;
    
    Position pos;
    StateInfo si;
    
    try {
        pos.set(fen, false, &si);
    } catch (const std::exception& e) {
        std::cerr << "Error setting position: " << e.what() << std::endl;
        return;
    }
    
    std::cout << "Position set successfully" << std::endl;
    
    // Test simple evaluation first
    std::cout << "Testing evaluation..." << std::endl;
    Value eval = Eval::evaluate(pos);
    std::cout << "Static eval: " << eval << std::endl;
    
    // Search for 10ms (as per benchmark requirement)
    std::cout << "Starting search..." << std::endl;
    auto result = Search::search(pos, 10, 10);
    
    std::cout << "Search completed" << std::endl;
    
    // Print results
    std::cout << "Evaluation: ";
    if (result.score >= VALUE_MATE_IN_MAX_PLY)
        std::cout << "Mate in " << (VALUE_MATE - result.score + 1) / 2 << std::endl;
    else if (result.score <= -VALUE_MATE_IN_MAX_PLY)
        std::cout << "Mated in " << (VALUE_MATE + result.score) / 2 << std::endl;
    else
        std::cout << result.score << std::endl;
    
    std::cout << "Best move: " << move_to_uci(result.bestMove) << std::endl;
    std::cout << "Depth: " << result.depth << " Nodes: " << result.nodes << std::endl;
}

// Self-play command: generate games
void cmd_play(int gameCount, int maxPly, int whiteTimeMs, int blackTimeMs) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> opening_moves(0, 100);
    
    int totalDepth = 0;
    int totalMoves = 0;
    
    for (int game = 0; game < gameCount; ++game) {
        Position pos;
        StateInfo si;
        std::vector<StateInfo> states(maxPly + 10);
        
        // Start from initial position
        pos.set("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", false, &si);
        
        std::cout << "[Event \"Engine Self-Play\"]" << std::endl;
        std::cout << "[Site \"Minimal Traditional Engine\"]" << std::endl;
        
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        std::cout << "[Date \"" << std::put_time(std::localtime(&time), "%Y.%m.%d") << "\"]" << std::endl;
        
        std::cout << "[Round \"" << (game + 1) << "\"]" << std::endl;
        std::cout << "[White \"MinimalEngine\"]" << std::endl;
        std::cout << "[Black \"MinimalEngine\"]" << std::endl;
        
        std::string pgn;
        int ply = 0;
        std::string result = "*";
        
        while (ply < maxPly) {
            int timeMs = pos.side_to_move() == WHITE ? whiteTimeMs : blackTimeMs;
            
            // Add small randomization to opening moves
            if (ply < 6 && opening_moves(gen) < 30) {
                Move moveList[MAX_MOVES];
                Move* last = generate<LEGAL>(pos, moveList);
                
                if (moveList == last) break;
                
                int legalMoves = last - moveList;
                if (legalMoves == 0) break;
                
                std::uniform_int_distribution<> dist(0, legalMoves - 1);
                Move randomMove = moveList[dist(gen)];
                
                if (ply % 2 == 0) {
                    pgn += std::to_string(ply / 2 + 1) + ". ";
                }
                pgn += move_to_uci(randomMove) + " ";
                
                pos.do_move(randomMove, states[ply], nullptr);
                ply++;
                continue;
            }
            
            auto result_search = Search::search(pos, 10, timeMs);
            totalDepth += result_search.depth;
            totalMoves++;
            
            if (result_search.bestMove == Move::none()) {
                // Game over
                if (pos.checkers()) {
                    result = pos.side_to_move() == WHITE ? "0-1" : "1-0";
                } else {
                    result = "1/2-1/2";
                }
                break;
            }
            
            // Check for draw by fifty-move rule or repetition
            if (pos.rule50_count() >= 100 || pos.is_draw(pos.game_ply())) {
                result = "1/2-1/2";
                break;
            }
            
            if (ply % 2 == 0) {
                pgn += std::to_string(ply / 2 + 1) + ". ";
            }
            pgn += move_to_uci(result_search.bestMove) + " ";
            
            pos.do_move(result_search.bestMove, states[ply], nullptr);
            ply++;
        }
        
        if (ply >= maxPly) {
            result = "1/2-1/2";
        }
        
        std::cout << "[Result \"" << result << "\"]" << std::endl;
        std::cout << std::endl;
        std::cout << pgn << result << std::endl;
        std::cout << std::endl;
    }
    
    if (totalMoves > 0) {
        std::cout << "Average depth: " << (double)totalDepth / totalMoves << std::endl;
    }
}

int main(int argc, char* argv[]) {
    // Initialize bitboards and position
    Bitboards::init();
    Position::init();
    
    if (argc < 2) {
        std::cerr << "Usage:" << std::endl;
        std::cerr << "  engine --analyze <FEN>" << std::endl;
        std::cerr << "  engine --play <Game Count> <Max ply> <White Movetime(ms)> <Black Movetime(ms)>" << std::endl;
        return 1;
    }
    
    std::string command = argv[1];
    
    if (command == "--analyze") {
        if (argc < 3) {
            std::cerr << "Error: FEN string required" << std::endl;
            return 1;
        }
        
        // Reconstruct FEN from remaining arguments
        std::string fen;
        for (int i = 2; i < argc; ++i) {
            if (i > 2) fen += " ";
            fen += argv[i];
        }
        
        cmd_analyze(fen);
    }
    else if (command == "--play") {
        if (argc < 6) {
            std::cerr << "Error: Required arguments: <Game Count> <Max ply> <White Movetime> <Black Movetime>" << std::endl;
            return 1;
        }
        
        int gameCount = std::stoi(argv[2]);
        int maxPly = std::stoi(argv[3]);
        int whiteTimeMs = std::stoi(argv[4]);
        int blackTimeMs = std::stoi(argv[5]);
        
        cmd_play(gameCount, maxPly, whiteTimeMs, blackTimeMs);
    }
    else {
        std::cerr << "Unknown command: " << command << std::endl;
        return 1;
    }
    
    return 0;
}
