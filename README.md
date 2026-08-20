# Word Search Generator (C, terminal edition)

A colour, animated, multi-file word search generator and game written in
portable C11. It builds a puzzle from a word list (file, typed, or a
built-in sample), hides the words horizontally / vertically / diagonally
(forwards and backwards), and lets you play, get hints, save puzzles to
disk, and compete on a local leaderboard - all in a coloured terminal UI.

~1,670 lines across 11 source/header files. No external libraries -
just the C standard library plus the Windows console API on Windows.

## Features

- **8-direction word placement** with a recursive backtracking placement
  engine (right, down, both diagonals, and all four in reverse)
- **3 difficulties** (Easy/Medium/Hard) controlling grid size and which
  directions are allowed
- **Colour + animation**: ANSI-coloured grid and menus, a typewriter
  title banner, a live loading bar while the puzzle builds, and a
  flash/blink effect when you find a word (with a terminal bell)
- **3 ways to build a word list**: load from a `.txt` file, type words
  in by hand, or use the bundled sample list
- **Full gameplay loop**: enter `row,col row,col` to claim a word,
  `hint` for a clue, `solve` to reveal everything, `quit` to bail out
- **Persistent leaderboard** (`scores.txt`) and **puzzle export**
  (with an optional solution key) to a text file
- **Cross-platform**: the same source compiles and runs on Windows,
  Linux and macOS (conditional compilation handles the differences)

Compiling & running

Windows (MinGW / Code::Blocks / Dev-C++ / WSL)

If you have MinGW's `gcc` on your PATH (Code::Blocks and Dev-C++ both
ship one), open a terminal in this folder and run:

```
gcc -Wall -Wextra -std=c11 -O2 -o wordsearch.exe main.c common.c grid.c fileio.c game.c ui.c
wordsearch.exe
```

Or open the folder as a project in Code::Blocks / Dev-C++ and build as
usual - just make sure **all six `.c` files** are added to the project
(not just `main.c`).

If you have `mingw32-make` installed, `mingw32-make` and
`mingw32-make run` work the same as `make`/`make run` below.

### Linux / macOS

```
make
./wordsearch
```

or without `make`:

```
gcc -Wall -Wextra -std=c11 -O2 -o wordsearch main.c common.c grid.c fileio.c game.c ui.c
./wordsearch
```

Both the Linux/macOS build and a Windows cross-build of this exact
source were compiled and exercised (including a full play-through
finding every word, invalid-input handling, file loading, and manual
entry) with `-Wall -Wextra -Wpedantic` and AddressSanitizer/UBSan while
building this - all clean, no warnings, no leaks.

## How to play

1. **New Puzzle** - pick a word source (file / typed / sample) and a
   difficulty.
2. Spot a word in the grid and enter the **row,col of its first letter**
   and **row,col of its last letter**, e.g. `3,2 3,8`. Either direction
   (start-to-end or end-to-start) counts.
3. Type `hint` for a clue, `solve` to reveal the answers and end the
   round, or `quit` to return to the menu.
4. Find every word to post your score to the leaderboard.

Colours can be toggled off from the main menu if your terminal doesn't
render ANSI escape codes well.

## Project structure

```
common.h / common.c   shared constants, enums, structs, string helpers
grid.h    / grid.c     grid allocation, word placement engine, queries
fileio.h  / fileio.c   word-list loading, puzzle export, leaderboard I/O
game.h    / game.c     interactive play loop, matching, hints, scoring
ui.h      / ui.c       colours, animations, input handling, menus
main.c                 entry point + the function-pointer menu dispatch
words.txt              sample word list (try it via "New Puzzle" > 1)
Makefile               `make`, `make run`, `make clean`
```

Dependency direction is one-way: every module includes `common.h`;
`grid`/`fileio`/`game`/`ui` never include each other's headers back and
forth, so there are no circular includes.

## C concepts checklist

Everything on the assignment list, with exactly where to find it:

| Concept | Where |
|---|---|
| **Functions** | Every module - ~60 functions total, each with a single clear job |
| **Loops** | `for`/`while` throughout: grid fill (`grid.c`), menu rendering (`ui.c`), the play loop (`game.c`) |
| **Arrays** | The letter grid, the dynamic `WordEntry` array, `DIR_ROW`/`DIR_COL`/`DIR_NAMES` (`grid.c`), the `mainMenu[]` dispatch table (`main.c`) |
| **Structures** | `Position`, `WordEntry`, `Puzzle`, `GameStats`, `AppState`, `MenuItem` (`common.h`) |
| **File I/O** | `loadWordsFromFile`, `savePuzzleToFile`, `loadScores`, `saveScore` (`fileio.c`) - `fopen`/`fgets`/`fprintf`/`fclose` |
| **Strings** | `trimString`, `toUpperString`, `caseInsensitiveEquals` (`common.c`), `sscanf`-based parsing (`game.c`, `fileio.c`) |
| **Pointers** | Structs passed by pointer everywhere; `char **grid` (pointer-to-pointer); function pointers below |
| **Libraries** | `<stdio.h> <stdlib.h> <string.h> <ctype.h> <time.h>` plus `<windows.h>`/`<unistd.h>` behind `#ifdef _WIN32`; `qsort()` from the standard library |
| **Dynamic memory allocation** | `malloc`/`realloc`/`free` for the grid and the word list (`allocateGrid`, `addWordToPuzzle`, `freePuzzle` in `grid.c`) |
| **Parameters** | Value params (e.g. `int size`) vs. pointer/reference params (e.g. `Puzzle *puzzle`) side by side in almost every function signature |
| **Recursion** | Six separate uses - see below |
| **Call by reference** | `parseSelection(input, &r1, &c1, &r2, &c2)` in `game.c` writes four ints back through pointers; every struct-pointer parameter is the same idea |
| **Bonus concepts** | Enums (`Direction`, `Difficulty`), function pointers driving the menu (`MenuAction` in `common.h`), `qsort` with custom comparators, `#ifdef`/conditional compilation, header guards, `const`-correctness, multi-file separate compilation |

### The six recursive functions

| Function | File | What it recurses on |
|---|---|---|
| `canPlaceWord` | `grid.c` | Walks one letter at a time along a direction, checking bounds/conflicts |
| `writeWordToGrid` | `grid.c` | Mirrors `canPlaceWord` but writes the letters in |
| `tryPlaceWord` | `grid.c` | Retries a random position/direction on failure (backtracking), bounded by `MAX_PLACEMENT_TRIES` |
| `collectLetters` | `game.c` | Walks a player's selection to build the "traced: C-O-D-E" feedback string |
| `typePrint` (via `typeText`) | `ui.c` | Prints one character, sleeps, recurses for the next (typewriter effect) |
| `loadingBar` | `ui.c` | Recurses once per percentage point, redrawing the bar with `\r` |

All six have a clear base case and a bound tied to a word length, a
retry limit, or a string/percentage length, so none can recurse
unboundedly.

## Customizing

- Word length/count limits, grid size limits, and placement-attempt
  limits are all `#define`d at the top of `common.h`.
- `words.txt` can be edited directly (one word per line, `#` for
  comments) or replaced with your own file, loaded via "New Puzzle" >
  option 1.
- The default in-code word list lives in `loadDefaultWords()` in
  `grid.c` if you'd rather change that one instead.
