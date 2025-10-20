#ifndef EVALUATE_H_INCLUDED
#define EVALUATE_H_INCLUDED

#include "types.h"

namespace Stockfish {

class Position;

namespace Eval {

Value evaluate(const Position& pos);

}  // namespace Eval

}  // namespace Stockfish

#endif // EVALUATE_H_INCLUDED
