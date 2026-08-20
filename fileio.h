/* =====================================================================
 * fileio.h
 * ---------------------------------------------------------------------
 * All disk access lives here: loading a word list from a text file,
 * saving a generated puzzle (with an optional solution key), and
 * loading/appending the leaderboard.
 * =====================================================================
 */
#ifndef FILEIO_H
#define FILEIO_H

#include "common.h"

int isValidWord(const char *word);

/* Returns words loaded, or -1 if the file could not be opened. */
int loadWordsFromFile(Puzzle *puzzle, const char *filename);

int savePuzzleToFile(const Puzzle *puzzle, const char *filename, int includeSolution);

int loadScores(GameStats scores[], int maxScores);
int saveScore(const GameStats *stats);

#endif /* FILEIO_H */
