# Project Outline: Minimal Traditional Engine

## Objective

Derive from stockfish a minimal traditional engine consisting of limited amount of commands with high speed and quality for each command.

## Details

**Basic Architecture:**

- Cheap board representation and move generation
- Efficient and high quality search
- Simple evaluation function (Piece Value + Piece-Square Tables)
- Position analysis and game generation (with randomization)

**Download stockfish source code from the internet. Then prune the code for the commands below:**

1. Analyze and provide top move in a position.
```
//command
engine --analyze <FEN>
//return
Evaluation: <Eval(Centipawn)>
Best move: <Best move(UCI format)>
```
2. Self play and game generation
```
//command
engine --play <Game Count> <Max ply> <White Movetime(milisecond)> <Black Movetime>
//return
<PGN>
Average depth: <Average depth>
```
*different move time for creating decided games (creating a difference of skill level).*

**Benchmarks:** Reach depth=7 for movetime 10ms with decent game quality.

## Next Steps

- Download stockfish source code
- Prune the code, leaving core chess functions, search and eval
- Update evaluation to simple piece value & piece-square tables
- Create uci with listed commands
- compile and build
- Run `engine --play 5 200 10 5` and `engine --play 5 200 5 10`, test benchmark for white and black, respectively