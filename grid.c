/* =====================================================================
 * grid.c
 * ---------------------------------------------------------------------
 * Core puzzle engine: dynamic 2D grid allocation, the dynamic word
 * array (malloc + realloc), and the recursive placement algorithm
 * that drops words into the grid horizontally, vertically and
 * diagonally (forwards and backwards).
 * =====================================================================
 */
#include "grid.h"

/* Row/column deltas for each of the 8 directions, and their labels.
 * Order matches the Direction enum in common.h. Easy mode uses only
 * the first 2 entries, Medium the first 4, Hard all 8 - see the
 * "dirCount" parameter threaded through tryPlaceWord/generatePuzzle. */
const int DIR_ROW[DIR_COUNT] = { 0,  1,  1,  1,  0, -1, -1, -1 };
const int DIR_COL[DIR_COUNT] = { 1,  0,  1, -1, -1,  0,  1, -1 };
const char *DIR_NAMES[DIR_COUNT] = {
    "Right", "Down", "Down-Right", "Down-Left",
    "Left",  "Up",   "Up-Right",   "Up-Left"
};

/* ---------------------------------------------------------------------
 * allocateGrid - DYNAMIC MEMORY ALLOCATION (2D array of char via an
 * array of pointers). Each row is also a valid null-terminated C
 * string (filled with ' ' for "empty"), which lets savePuzzleToFile
 * print rows with a plain %s later if it ever needs to.
 * ------------------------------------------------------------------- */
int allocateGrid(Puzzle *puzzle, int size) {
    puzzle->grid = (char **)malloc((size_t)size * sizeof(char *));
    if (puzzle->grid == NULL) {
        return 0;
    }

    for (int i = 0; i < size; i++) {
        puzzle->grid[i] = (char *)malloc((size_t)(size + 1) * sizeof(char));
        if (puzzle->grid[i] == NULL) {
            /* allocation failed partway through - clean up what we
             * already grabbed so we don't leak memory */
            for (int j = 0; j < i; j++) {
                free(puzzle->grid[j]);
            }
            free(puzzle->grid);
            puzzle->grid = NULL;
            return 0;
        }
        for (int c = 0; c < size; c++) {
            puzzle->grid[i][c] = ' ';   /* ' ' means "empty cell" */
        }
        puzzle->grid[i][size] = '\0';
    }

    puzzle->size = size;
    return 1;
}

void freeGrid(Puzzle *puzzle) {
    if (puzzle->grid != NULL) {
        for (int i = 0; i < puzzle->size; i++) {
            free(puzzle->grid[i]);
        }
        free(puzzle->grid);
        puzzle->grid = NULL;
    }
    puzzle->size = 0;
}

void freePuzzle(Puzzle *puzzle) {
    freeGrid(puzzle);
    if (puzzle->words != NULL) {
        free(puzzle->words);
        puzzle->words = NULL;
    }
    puzzle->wordCount = 0;
    puzzle->wordCapacity = 0;
}

/* ---------------------------------------------------------------------
 * addWordToPuzzle - grows puzzle->words with realloc, doubling
 * capacity each time it runs out of room (classic dynamic array).
 * ------------------------------------------------------------------- */
int addWordToPuzzle(Puzzle *puzzle, const char *word) {
    if (puzzle->wordCount >= MAX_WORDS) {
        return 0;
    }

    if (puzzle->wordCount >= puzzle->wordCapacity) {
        int newCapacity = (puzzle->wordCapacity == 0) ? 4 : puzzle->wordCapacity * 2;
        if (newCapacity > MAX_WORDS) {
            newCapacity = MAX_WORDS;
        }
        WordEntry *grown = (WordEntry *)realloc(puzzle->words,
                                                  (size_t)newCapacity * sizeof(WordEntry));
        if (grown == NULL) {
            return 0;   /* original block is untouched on failure */
        }
        puzzle->words = grown;
        puzzle->wordCapacity = newCapacity;
    }

    WordEntry *entry = &puzzle->words[puzzle->wordCount];
    strncpy(entry->word, word, MAX_WORD_LEN);
    entry->word[MAX_WORD_LEN] = '\0';
    entry->placed = 0;
    entry->found = 0;
    entry->start.row = entry->start.col = -1;
    entry->end.row = entry->end.col = -1;
    entry->dir = DIR_RIGHT;

    puzzle->wordCount++;
    return 1;
}

int isDuplicateWord(const Puzzle *puzzle, const char *word) {
    for (int i = 0; i < puzzle->wordCount; i++) {
        if (strcmp(puzzle->words[i].word, word) == 0) {
            return 1;
        }
    }
    return 0;
}

int longestWordLength(const Puzzle *puzzle) {
    int maxLen = 0;
    for (int i = 0; i < puzzle->wordCount; i++) {
        int len = (int)strlen(puzzle->words[i].word);
        if (len > maxLen) {
            maxLen = len;
        }
    }
    return maxLen;
}

int countPlacedWords(const Puzzle *puzzle) {
    int count = 0;
    for (int i = 0; i < puzzle->wordCount; i++) {
        if (puzzle->words[i].placed) {
            count++;
        }
    }
    return count;
}

void loadDefaultWords(Puzzle *puzzle) {
    static const char *defaults[] = {
        "FUNCTION", "POINTER", "ARRAY", "STRUCT", "LOOP",
        "STRING", "MEMORY", "COMPILER", "VARIABLE", "HEADER",
        "LIBRARY", "RECURSION", "DYNAMIC", "INTEGER", "ALGORITHM"
    };
    int n = (int)(sizeof(defaults) / sizeof(defaults[0]));
    for (int i = 0; i < n; i++) {
        addWordToPuzzle(puzzle, defaults[i]);
    }
}

/* ---------------------------------------------------------------------
 * RECURSION #1: canPlaceWord
 * Walks one letter at a time in direction (dRowStep, dColStep),
 * checking bounds and letter conflicts. Base case: index reached the
 * word's terminator -> the whole word fits.
 * ------------------------------------------------------------------- */
int canPlaceWord(const Puzzle *puzzle, const char *word, int row, int col,
                  int dRowStep, int dColStep, int index) {
    if (word[index] == '\0') {
        return 1;   /* base case: every letter checked out fine */
    }
    if (row < 0 || row >= puzzle->size || col < 0 || col >= puzzle->size) {
        return 0;    /* walked off the grid */
    }
    char cell = puzzle->grid[row][col];
    if (cell != ' ' && cell != word[index]) {
        return 0;    /* a different letter is already sitting here */
    }
    return canPlaceWord(puzzle, word, row + dRowStep, col + dColStep,
                         dRowStep, dColStep, index + 1);
}

/* ---------------------------------------------------------------------
 * RECURSION #2: writeWordToGrid
 * Mirrors canPlaceWord's recursion but actually writes the letters.
 * Only ever called after canPlaceWord has already said yes.
 * ------------------------------------------------------------------- */
void writeWordToGrid(Puzzle *puzzle, const char *word, int row, int col,
                      int dRowStep, int dColStep, int index) {
    if (word[index] == '\0') {
        return;
    }
    puzzle->grid[row][col] = word[index];
    writeWordToGrid(puzzle, word, row + dRowStep, col + dColStep,
                     dRowStep, dColStep, index + 1);
}

/* ---------------------------------------------------------------------
 * RECURSION #3: tryPlaceWord
 * Picks a random direction/start cell; if canPlaceWord rejects it,
 * recurses to try again with one fewer attempt left (classic
 * "retry via recursion" backtracking pattern). Bounded by
 * MAX_PLACEMENT_TRIES so it can never recurse unboundedly.
 * ------------------------------------------------------------------- */
int tryPlaceWord(Puzzle *puzzle, WordEntry *entry, int dirCount, int attemptsLeft) {
    if (attemptsLeft <= 0) {
        return 0;   /* gave it its best shot - grid must be too full */
    }

    int dirIndex  = rand() % dirCount;
    int wordLen   = (int)strlen(entry->word);
    int startRow  = rand() % puzzle->size;
    int startCol  = rand() % puzzle->size;

    if (canPlaceWord(puzzle, entry->word, startRow, startCol,
                      DIR_ROW[dirIndex], DIR_COL[dirIndex], 0)) {
        writeWordToGrid(puzzle, entry->word, startRow, startCol,
                         DIR_ROW[dirIndex], DIR_COL[dirIndex], 0);
        entry->start.row = startRow;
        entry->start.col = startCol;
        entry->end.row   = startRow + DIR_ROW[dirIndex] * (wordLen - 1);
        entry->end.col   = startCol + DIR_COL[dirIndex] * (wordLen - 1);
        entry->dir       = (Direction)dirIndex;
        entry->placed    = 1;
        return 1;
    }

    return tryPlaceWord(puzzle, entry, dirCount, attemptsLeft - 1);
}

/* Comparator used with qsort() so longer words are placed first -
 * they are harder to fit, so giving them first pick of the empty
 * grid produces noticeably better packing. Demonstrates a standard
 * library algorithm driven by a function pointer. */
static int compareWordLengthDesc(const void *a, const void *b) {
    const WordEntry *wa = (const WordEntry *)a;
    const WordEntry *wb = (const WordEntry *)b;
    return (int)strlen(wb->word) - (int)strlen(wa->word);
}

void fillRandomLetters(Puzzle *puzzle) {
    for (int r = 0; r < puzzle->size; r++) {
        for (int c = 0; c < puzzle->size; c++) {
            if (puzzle->grid[r][c] == ' ') {
                puzzle->grid[r][c] = (char)('A' + (rand() % 26));
            }
        }
    }
}

int generatePuzzle(Puzzle *puzzle, int size, int dirCount) {
    if (!allocateGrid(puzzle, size)) {
        return -1;
    }

    qsort(puzzle->words, (size_t)puzzle->wordCount, sizeof(WordEntry), compareWordLengthDesc);

    int placedCount = 0;
    for (int i = 0; i < puzzle->wordCount; i++) {
        if ((int)strlen(puzzle->words[i].word) > size) {
            puzzle->words[i].placed = 0;   /* cannot possibly fit - skip */
            continue;
        }
        if (tryPlaceWord(puzzle, &puzzle->words[i], dirCount, MAX_PLACEMENT_TRIES)) {
            placedCount++;
        }
    }

    fillRandomLetters(puzzle);
    return placedCount;
}

int isCellOnWordPath(const WordEntry *entry, int row, int col) {
    int len = (int)strlen(entry->word);
    int dr = DIR_ROW[entry->dir];
    int dc = DIR_COL[entry->dir];
    for (int i = 0; i < len; i++) {
        if (entry->start.row + dr * i == row && entry->start.col + dc * i == col) {
            return 1;
        }
    }
    return 0;
}

int isCellHighlighted(const Puzzle *puzzle, int row, int col, int showAllSolution) {
    for (int i = 0; i < puzzle->wordCount; i++) {
        const WordEntry *entry = &puzzle->words[i];
        if (!entry->placed) {
            continue;
        }
        if ((showAllSolution || entry->found) && isCellOnWordPath(entry, row, col)) {
            return 1;
        }
    }
    return 0;
}

int isValidCoordinate(const Puzzle *puzzle, int row, int col) {
    return row >= 0 && row < puzzle->size && col >= 0 && col < puzzle->size;
}

const char *difficultyToString(Difficulty d) {
    switch (d) {
        case DIFF_EASY:   return "EASY";
        case DIFF_MEDIUM: return "MEDIUM";
        case DIFF_HARD:   return "HARD";
        default:          return "UNKNOWN";
    }
}
