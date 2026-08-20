/* =====================================================================
 * game.c
 * ---------------------------------------------------------------------
 * The interactive gameplay loop. The player types the row,col of a
 * word's first and last letter; we validate it, match it against the
 * placed words by POSITION (robust - no risk of a coincidental filler
 * letter run being mistaken for a real word), animate the find, and
 * track hints/time/score until every placed word is found.
 * =====================================================================
 */
#include "game.h"
#include "grid.h"
#include "ui.h"
#include "fileio.h"

/* CALL BY REFERENCE example: writes three outputs (the sign) back
 * through nothing but its return value here, but is used below to
 * feed pointer-based output parameters in parseSelection(). */
static int sign(int x) {
    return (x > 0) - (x < 0);
}

/* CALL BY REFERENCE: r1/c1/r2/c2 are written back through pointers
 * rather than returned, since we need four output values at once. */
int parseSelection(const char *input, int *r1, int *c1, int *r2, int *c2) {
    int matched = sscanf(input, "%d,%d %d,%d", r1, c1, r2, c2);
    return matched == 4;
}

/* RECURSION #6: collectLetters - walks from (row,col) toward the
 * selection's other end, recording one letter per call. Used purely
 * for the "traced: C-O-D-E"-style feedback shown when a word is
 * found; the actual match check below uses the more robust
 * position comparison instead of comparing strings. */
void collectLetters(const Puzzle *puzzle, int row, int col, int dRowStep, int dColStep,
                     int remaining, char *buffer, int index) {
    buffer[index] = puzzle->grid[row][col];
    buffer[index + 1] = '\0';
    if (remaining == 0) {
        return;
    }
    collectLetters(puzzle, row + dRowStep, col + dColStep, dRowStep, dColStep,
                    remaining - 1, buffer, index + 1);
}

/* Matches the player's selected endpoints against every placed word's
 * stored start/end (in either order, since players may trace a word
 * back-to-front). Comparing stored coordinates - not grid letters -
 * means a coincidental run of filler letters can never be mistaken
 * for a real word. */
WordEntry *findMatchingWord(Puzzle *puzzle, int r1, int c1, int r2, int c2) {
    for (int i = 0; i < puzzle->wordCount; i++) {
        WordEntry *entry = &puzzle->words[i];
        if (!entry->placed) {
            continue;
        }
        int forward  = (entry->start.row == r1 && entry->start.col == c1 &&
                         entry->end.row   == r2 && entry->end.col   == c2);
        int backward = (entry->start.row == r2 && entry->start.col == c2 &&
                         entry->end.row   == r1 && entry->end.col   == c1);
        if (forward || backward) {
            return entry;
        }
    }
    return NULL;
}

void giveHint(const Puzzle *puzzle) {
    int unfoundIdx[MAX_WORDS];
    int count = 0;

    for (int i = 0; i < puzzle->wordCount; i++) {
        if (puzzle->words[i].placed && !puzzle->words[i].found) {
            unfoundIdx[count++] = i;
        }
    }

    if (count == 0) {
        printf("\nNo hints left - every word is already found!\n");
        return;
    }

    const WordEntry *entry = &puzzle->words[unfoundIdx[rand() % count]];
    printf("\nHINT: a %d-letter word begins with '%c' at row %d, column %d (heading %s).\n",
           (int)strlen(entry->word), entry->word[0],
           entry->start.row + 1, entry->start.col + 1, DIR_NAMES[entry->dir]);
}

void playPuzzle(AppState *state) {
    Puzzle *puzzle = &state->puzzle;

    if (puzzle->size == 0 || puzzle->wordCount == 0) {
        clearScreen();
        printError(state, "No puzzle yet. Generate one first (Main Menu > New Puzzle).");
        pauseForUser();
        return;
    }

    int totalPlaced = countPlacedWords(puzzle);
    if (totalPlaced == 0) {
        clearScreen();
        printError(state, "No words were placed in this puzzle. Try generating a new one.");
        pauseForUser();
        return;
    }

    /* fresh round: clear any "found" flags left over from a previous play-through */
    for (int i = 0; i < puzzle->wordCount; i++) {
        puzzle->words[i].found = 0;
    }

    time_t startTime = time(NULL);
    int hintsUsed = 0;
    int wordsFound = 0;
    char input[128];

    while (wordsFound < totalPlaced) {
        clearScreen();
        printHeader(state, "FIND THE WORDS");
        printGrid(state, NULL, 0);
        printWordList(state, 1);
        printf("Progress: %d / %d words found\n\n", wordsFound, totalPlaced);
        printf("Enter 'row,col row,col' (e.g. 3,2 3,8), 'hint', 'solve', or 'quit'.\n> ");

        readLine(input, sizeof(input));

        if (input[0] == '\0') {
            continue;
        }
        if (caseInsensitiveEquals(input, "quit")) {
            return;
        }
        if (caseInsensitiveEquals(input, "solve")) {
            clearScreen();
            printHeader(state, "SOLUTION REVEALED");
            printGrid(state, NULL, 1);
            printf("You found %d of %d words before revealing the solution.\n",
                   wordsFound, totalPlaced);
            pauseForUser();
            return;
        }
        if (caseInsensitiveEquals(input, "hint")) {
            giveHint(puzzle);
            hintsUsed++;
            pauseForUser();
            continue;
        }

        int r1, c1, r2, c2;
        if (!parseSelection(input, &r1, &c1, &r2, &c2)) {
            printError(state, "Couldn't read that. Format: row,col row,col");
            pauseForUser();
            continue;
        }
        r1--; c1--; r2--; c2--;   /* user sees 1-indexed, we store 0-indexed */

        if (!isValidCoordinate(puzzle, r1, c1) || !isValidCoordinate(puzzle, r2, c2)) {
            printError(state, "Those coordinates are outside the grid.");
            pauseForUser();
            continue;
        }

        WordEntry *match = findMatchingWord(puzzle, r1, c1, r2, c2);
        if (match == NULL) {
            printError(state, "No word runs exactly between those two cells. Try again!");
            pauseForUser();
            continue;
        }
        if (match->found) {
            printInfo(state, "You've already found that one!");
            pauseForUser();
            continue;
        }

        match->found = 1;
        wordsFound++;

        char traced[MAX_WORD_LEN + 2];
        int dr = sign(r2 - r1);
        int dc = sign(c2 - c1);
        int steps = (int)strlen(match->word) - 1;
        collectLetters(puzzle, r1, c1, dr, dc, steps, traced, 0);

        printf("\a");   /* terminal bell - a little audio feedback */
        flashFoundWord(state, match, traced);
    }

    /* --- round complete: tally stats and offer to save the score --- */
    time_t endTime = time(NULL);
    double elapsed = difftime(endTime, startTime);

    int score = wordsFound * 100 - hintsUsed * 25 - (int)(elapsed / 2.0);
    if (score < 0) {
        score = 0;
    }

    GameStats stats;
    memset(&stats, 0, sizeof(stats));
    strcpy(stats.playerName, "Player");
    stats.wordsFound = wordsFound;
    stats.totalWords = totalPlaced;
    stats.hintsUsed  = hintsUsed;
    stats.timeSeconds = elapsed;
    stats.score = score;
    strncpy(stats.difficultyLabel, difficultyToString(puzzle->difficulty),
            sizeof(stats.difficultyLabel) - 1);
    stats.difficultyLabel[sizeof(stats.difficultyLabel) - 1] = '\0';

    clearScreen();
    printHeader(state, "PUZZLE COMPLETE!");
    printGrid(state, NULL, 1);
    typeText("Congratulations - you found every word!\n\n");

    printf("Time Taken  : %.0f seconds\n", elapsed);
    printf("Hints Used  : %d\n", hintsUsed);
    printf("Final Score : %d\n\n", score);

    printf("Save this score to the leaderboard? (y/n): ");
    readLine(input, sizeof(input));

    if (input[0] == 'y' || input[0] == 'Y') {
        printf("Enter your name: ");
        readLine(stats.playerName, sizeof(stats.playerName));
        if (stats.playerName[0] == '\0') {
            strcpy(stats.playerName, "Player");
        }
        for (int i = 0; stats.playerName[i] != '\0'; i++) {
            if (stats.playerName[i] == ',') {
                stats.playerName[i] = ' ';   /* keep the CSV file well-formed */
            }
        }
        if (saveScore(&stats)) {
            printSuccess(state, "Score saved to the leaderboard!");
        } else {
            printError(state, "Couldn't save the score file.");
        }
    }

    state->lastStats = stats;
    pauseForUser();
}
