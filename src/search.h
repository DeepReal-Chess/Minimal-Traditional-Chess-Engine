#ifndef SEARCH_H_INCLUDED
#define SEARCH_H_INCLUDED

#include <string>
#include "types.h"

namespace Stockfish {

class Position;

namespace Search {

struct SearchResult {
    Move  bestMove;
    Value score;
    int   depth;
    uint64_t nodes;
};

SearchResult search(Position& pos, int maxDepth, int timeMs);

}  // namespace Search

}  // namespace Stockfish

#endif // SEARCH_H_INCLUDED
