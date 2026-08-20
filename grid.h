/* =====================================================================
 * grid.h
 * ---------------------------------------------------------------------
 * Everything about the Puzzle's DATA and LOGIC: allocating the letter
 * grid, growing the word list, placing words (with recursive
 * backtracking), and answering questions about the grid. No printing
 * or colour code lives here - that is ui.c's job.
 * =====================================================================
 */
#ifndef GRID_H
#define GRID_H

#include "common.h"

/* --- allocation / lifetime --- */
int  allocateGrid(Puzzle *puzzle, int size);
void freeGrid(Puzzle *puzzle);
void freePuzzle(Puzzle *puzzle);

/* --- word list management (dynamic array, grows via realloc) --- */
int addWordToPuzzle(Puzzle *puzzle, const char *word);
int isDuplicateWord(const Puzzle *puzzle, const char *word);
int longestWordLength(const Puzzle *puzzle);
int countPlacedWords(const Puzzle *puzzle);
void loadDefaultWords(Puzzle *puzzle);

/* --- placement engine --- */
int generatePuzzle(Puzzle *puzzle, int size, int dirCount);
int tryPlaceWord(Puzzle *puzzle, WordEntry *entry, int dirCount, int attemptsLeft);
int canPlaceWord(const Puzzle *puzzle, const char *word, int row, int col,
                  int dRowStep, int dColStep, int index);
void writeWordToGrid(Puzzle *puzzle, const char *word, int row, int col,
                      int dRowStep, int dColStep, int index);
void fillRandomLetters(Puzzle *puzzle);

/* --- queries used by both game.c and ui.c --- */
int isCellOnWordPath(const WordEntry *entry, int row, int col);
int isCellHighlighted(const Puzzle *puzzle, int row, int col, int showAllSolution);
int isValidCoordinate(const Puzzle *puzzle, int row, int col);

const char *difficultyToString(Difficulty d);

#endif /* GRID_H */
