/* ============================================================
 * common.h
 *
 * Shared constants, enums, structures and small string helpers
 * used by every module in the project. Every other header
 * includes this one, so it is kept dependency-free (standard
 * library only) to avoid circular includes.
 * ============================================================ */
#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

/* ---------------- Limits & constants ---------------- */
#define MAX_WORD_LEN        15   /* longest word we accept (not counting '\0') */
#define MAX_WORDS            40   /* hard cap on words per puzzle              */
#define MIN_GRID_SIZE         8
#define MAX_GRID_SIZE        22
#define MAX_PLACEMENT_TRIES 300   /* random attempts before we give up on a word */
#define MAX_SCORES          100   /* leaderboard entries kept in memory at once */
#define SCORES_FILE          "scores.txt"

/* ---------------- ANSI colour codes (graphics) ---------------- */
#define RESET            "\033[0m"
#define BOLD             "\033[1m"
#define C_RED            "\033[31m"
#define C_GREEN          "\033[32m"
#define C_YELLOW         "\033[33m"
#define C_BLUE           "\033[34m"
#define C_MAGENTA        "\033[35m"
#define C_CYAN           "\033[36m"
#define C_WHITE          "\033[37m"
#define C_BRIGHT_GREEN   "\033[92m"
#define C_BRIGHT_YELLOW  "\033[93m"
#define C_BRIGHT_CYAN    "\033[96m"

/* ---------------- Enums ---------------- */

/* All 8 compass directions a word can run in. The order matters:
 * it lets Easy/Medium/Hard difficulty simply use the first N of them. */
typedef enum {
    DIR_RIGHT = 0,
    DIR_DOWN,
    DIR_DOWN_RIGHT,
    DIR_DOWN_LEFT,
    DIR_LEFT,
    DIR_UP,
    DIR_UP_RIGHT,
    DIR_UP_LEFT,
    DIR_COUNT
} Direction;

typedef enum {
    DIFF_EASY   = 1,
    DIFF_MEDIUM = 2,
    DIFF_HARD   = 3
} Difficulty;

/* ---------------- Structures ---------------- */

typedef struct {
    int row;
    int col;
} Position;

typedef struct {
    char word[MAX_WORD_LEN + 1];
    Position start;
    Position end;
    Direction dir;
    int placed;   /* 1 once successfully placed in the grid   */
    int found;    /* 1 once the player has located it in-game */
} WordEntry;

typedef struct {
    char **grid;            /* dynamically allocated size x size letters   */
    int size;
    WordEntry *words;        /* dynamically allocated array (realloc-grown) */
    int wordCount;
    int wordCapacity;
    Difficulty difficulty;
} Puzzle;

typedef struct {
    char playerName[50];
    int wordsFound;
    int totalWords;
    int hintsUsed;
    double timeSeconds;
    int score;
    char difficultyLabel[10];
} GameStats;

typedef struct {
    Puzzle puzzle;
    GameStats lastStats;
    int colorEnabled;
    int running;
} AppState;

/* Function-pointer type used to drive the main menu dispatch table. */
typedef void (*MenuAction)(AppState *state);

typedef struct {
    char label[60];
    MenuAction action;
} MenuItem;

/* ---------------- Direction lookup tables (defined in grid.c) ---------------- */
extern const int DIR_ROW[DIR_COUNT];
extern const int DIR_COL[DIR_COUNT];
extern const char *DIR_NAMES[DIR_COUNT];

/* ---------------- Shared string utilities (defined in common.c) ---------------- */
void trimString(char *str);                              /* strips \r\n and surrounding whitespace */
void toUpperString(char *str);                            /* in-place uppercase                     */
int  caseInsensitiveEquals(const char *a, const char *b); /* case-insensitive string compare        */

#endif /* COMMON_H */
