/* =====================================================================
 * game.h
 * ---------------------------------------------------------------------
 * The interactive "find the words" gameplay loop: reading the
 * player's row,col selections, matching them against placed words,
 * hints, scoring and timing.
 * =====================================================================
 */
#ifndef GAME_H
#define GAME_H

#include "common.h"

void playPuzzle(AppState *state);

/* Exposed for potential reuse / unit testing of the input format. */
int parseSelection(const char *input, int *r1, int *c1, int *r2, int *c2);
WordEntry *findMatchingWord(Puzzle *puzzle, int r1, int c1, int r2, int c2);
void giveHint(const Puzzle *puzzle);
void collectLetters(const Puzzle *puzzle, int row, int col, int dRowStep, int dColStep,
                     int remaining, char *buffer, int index);

#endif /* GAME_H */
