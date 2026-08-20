/* =====================================================================
 * fileio.c
 * ---------------------------------------------------------------------
 * FILE I/O: reading a word list (fopen/fgets/fclose), writing a
 * generated puzzle out to a text file (fprintf), and a simple CSV-ish
 * leaderboard file that is appended to after every finished game and
 * re-read (and re-sorted) whenever the player views it.
 * =====================================================================
 */
#include "fileio.h"
#include "grid.h"   /* addWordToPuzzle, isDuplicateWord, difficultyToString, DIR_NAMES */

int isValidWord(const char *word) {
    int len = (int)strlen(word);
    if (len < 2 || len > MAX_WORD_LEN) {
        return 0;
    }
    for (int i = 0; i < len; i++) {
        if (!isalpha((unsigned char)word[i])) {
            return 0;
        }
    }
    return 1;
}

int loadWordsFromFile(Puzzle *puzzle, const char *filename) {
    FILE *fp = fopen(filename, "r");
    if (fp == NULL) {
        return -1;
    }

    char line[128];
    int loaded = 0;
    int skipped = 0;

    while (fgets(line, sizeof(line), fp) != NULL) {
        trimString(line);

        if (line[0] == '\0' || line[0] == '#') {
            continue;   /* blank line or comment - ignore */
        }

        toUpperString(line);

        if (!isValidWord(line) || isDuplicateWord(puzzle, line)) {
            skipped++;
            continue;
        }

        if (addWordToPuzzle(puzzle, line)) {
            loaded++;
        } else {
            skipped++;   /* list is full (MAX_WORDS reached) */
        }
    }

    fclose(fp);

    if (skipped > 0) {
        printf("Note: %d entr%s skipped (invalid, duplicate, or list full).\n",
               skipped, skipped == 1 ? "y" : "ies");
    }

    return loaded;
}

int savePuzzleToFile(const Puzzle *puzzle, const char *filename, int includeSolution) {
    FILE *fp = fopen(filename, "w");
    if (fp == NULL) {
        return 0;
    }

    fprintf(fp, "WORD SEARCH PUZZLE (%dx%d, %s)\n", puzzle->size, puzzle->size,
            difficultyToString(puzzle->difficulty));
    fprintf(fp, "=====================================\n\n");

    for (int r = 0; r < puzzle->size; r++) {
        for (int c = 0; c < puzzle->size; c++) {
            fprintf(fp, "%c  ", puzzle->grid[r][c]);
        }
        fprintf(fp, "\n");
    }

    fprintf(fp, "\nWORDS TO FIND:\n");
    for (int i = 0; i < puzzle->wordCount; i++) {
        if (puzzle->words[i].placed) {
            fprintf(fp, "  - %s\n", puzzle->words[i].word);
        }
    }

    if (includeSolution) {
        fprintf(fp, "\nSOLUTION KEY\n");
        fprintf(fp, "============\n");
        for (int i = 0; i < puzzle->wordCount; i++) {
            const WordEntry *e = &puzzle->words[i];
            if (e->placed) {
                fprintf(fp, "  %-15s (%2d,%2d) -> (%2d,%2d)  [%s]\n",
                        e->word, e->start.row + 1, e->start.col + 1,
                        e->end.row + 1, e->end.col + 1, DIR_NAMES[e->dir]);
            }
        }
    }

    fclose(fp);
    return 1;
}

/* Leaderboard rows are stored one per line as:
 *   name,wordsFound,totalWords,hintsUsed,timeSeconds,score,DIFFICULTY
 * A plain-text CSV keeps this readable/editable and easy to parse
 * back with sscanf. */
int loadScores(GameStats scores[], int maxScores) {
    FILE *fp = fopen(SCORES_FILE, "r");
    if (fp == NULL) {
        return 0;   /* no leaderboard yet - not an error */
    }

    int count = 0;
    char line[200];

    while (count < maxScores && fgets(line, sizeof(line), fp) != NULL) {
        trimString(line);
        if (line[0] == '\0') {
            continue;
        }

        char name[50];
        char diff[10];
        int wf, tw, hu, sc;
        double t;

        int parsed = sscanf(line, "%49[^,],%d,%d,%d,%lf,%d,%9s",
                             name, &wf, &tw, &hu, &t, &sc, diff);

        if (parsed == 7) {
            GameStats *s = &scores[count];
            strncpy(s->playerName, name, sizeof(s->playerName) - 1);
            s->playerName[sizeof(s->playerName) - 1] = '\0';
            s->wordsFound  = wf;
            s->totalWords  = tw;
            s->hintsUsed   = hu;
            s->timeSeconds = t;
            s->score       = sc;
            strncpy(s->difficultyLabel, diff, sizeof(s->difficultyLabel) - 1);
            s->difficultyLabel[sizeof(s->difficultyLabel) - 1] = '\0';
            count++;
        }
        /* malformed lines are silently skipped rather than crashing */
    }

    fclose(fp);
    return count;
}

int saveScore(const GameStats *stats) {
    FILE *fp = fopen(SCORES_FILE, "a");   /* append, never overwrite */
    if (fp == NULL) {
        return 0;
    }

    fprintf(fp, "%s,%d,%d,%d,%.1f,%d,%s\n",
            stats->playerName, stats->wordsFound, stats->totalWords,
            stats->hintsUsed, stats->timeSeconds, stats->score, stats->difficultyLabel);

    fclose(fp);
    return 1;
}
