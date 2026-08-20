/* =====================================================================
 * main.c
 * ---------------------------------------------------------------------
 * Program entry point and the "flows" bound to each main-menu choice.
 * The menu itself is an array of {label, function pointer} pairs
 * (MenuItem) - picking an option looks the function pointer up and
 * calls it directly, no switch-statement needed.
 * =====================================================================
 */
#include "common.h"
#include "grid.h"
#include "fileio.h"
#include "game.h"
#include "ui.h"

/* ---- forward declarations for the menu's function pointers ---- */
static void actionNewPuzzle(AppState *state);
static void actionPlayPuzzle(AppState *state);
static void actionViewSolution(AppState *state);
static void actionSavePuzzle(AppState *state);
static void actionViewLeaderboard(AppState *state);
static void actionHelp(AppState *state);
static void actionToggleColors(AppState *state);
static void actionExit(AppState *state);

static void enterWordsManually(AppState *state, Puzzle *puzzle);

static void initAppState(AppState *state) {
    state->puzzle.grid = NULL;
    state->puzzle.size = 0;
    state->puzzle.words = NULL;
    state->puzzle.wordCount = 0;
    state->puzzle.wordCapacity = 0;
    state->puzzle.difficulty = DIFF_EASY;
    memset(&state->lastStats, 0, sizeof(GameStats));
    state->colorEnabled = 1;
    state->running = 1;
}

int main(void) {
    enableAnsiSupport();
    srand((unsigned int)time(NULL));

    AppState state;
    initAppState(&state);

    showBanner(&state);

    /* The main menu dispatch table - an ARRAY OF STRUCTS, each
     * holding a FUNCTION POINTER. This is the "libraries"/"function
     * pointers" showcase: adding a new menu item is just adding a
     * new row here. */
    MenuItem mainMenu[] = {
        {"New Puzzle",           actionNewPuzzle},
        {"Play Current Puzzle",  actionPlayPuzzle},
        {"View Solution",        actionViewSolution},
        {"Save Puzzle to File",  actionSavePuzzle},
        {"View Leaderboard",     actionViewLeaderboard},
        {"How to Play / Help",   actionHelp},
        {"Toggle Colors",        actionToggleColors},
        {"Exit",                 actionExit}
    };
    int menuCount = (int)(sizeof(mainMenu) / sizeof(mainMenu[0]));

    while (state.running) {
        int choice = displayMenuAndGetChoice("WORD SEARCH GENERATOR - MAIN MENU",
                                              mainMenu, menuCount, &state);
        mainMenu[choice - 1].action(&state);   /* <-- function pointer call */
    }

    freePuzzle(&state.puzzle);

    clearScreen();
    setColor(&state, C_CYAN);
    printf("\nThanks for playing Word Search Generator. Goodbye!\n\n");
    resetColor(&state);

    return 0;
}

/* ===================== Menu action implementations ===================== */

static void actionNewPuzzle(AppState *state) {
    clearScreen();
    printHeader(state, "NEW PUZZLE");

    freePuzzle(&state->puzzle);   /* release any previous puzzle first */

    printf("How would you like to build your word list?\n\n");
    printf("  1. Load from a file\n");
    printf("  2. Enter words manually\n");
    printf("  3. Use the built-in sample list (C programming terms)\n\n");
    int source = readIntInRange("Choice (1-3): ", 1, 3);
    printf("\n");

    if (source == 1) {
        char filename[100];
        printf("Filename (Enter for 'words.txt'): ");
        readLine(filename, sizeof(filename));
        if (filename[0] == '\0') {
            strcpy(filename, "words.txt");
        }

        int loaded = loadWordsFromFile(&state->puzzle, filename);
        if (loaded < 0) {
            printError(state, "Couldn't open that file. Using the sample list instead.");
            loadDefaultWords(&state->puzzle);
        } else if (loaded == 0) {
            printError(state, "No valid words in that file. Using the sample list instead.");
            loadDefaultWords(&state->puzzle);
        } else {
            printf("Loaded %d word(s) from %s\n", loaded, filename);
        }
    } else if (source == 2) {
        enterWordsManually(state, &state->puzzle);
    } else {
        loadDefaultWords(&state->puzzle);
        printf("Loaded the sample word list.\n");
    }

    if (state->puzzle.wordCount == 0) {
        printError(state, "No usable words - returning to the menu.");
        pauseForUser();
        return;
    }

    printf("\nChoose a difficulty:\n\n");
    printf("  1. Easy   - horizontal & vertical only\n");
    printf("  2. Medium - adds forward diagonals\n");
    printf("  3. Hard   - all 8 directions, forward & backward\n\n");
    int diffChoice = readIntInRange("Choice (1-3): ", 1, 3);

    int size, dirCount;
    Difficulty diff;
    switch (diffChoice) {
        case 1: size = 14; dirCount = 2; diff = DIFF_EASY;   break;
        case 2: size = 16; dirCount = 4; diff = DIFF_MEDIUM; break;
        default: size = 18; dirCount = 8; diff = DIFF_HARD;  break;
    }

    int maxLen = longestWordLength(&state->puzzle);
    if (size < maxLen + 1) {
        size = maxLen + 1;
    }
    if (size < MIN_GRID_SIZE) {
        size = MIN_GRID_SIZE;
    }
    if (size > MAX_GRID_SIZE) {
        size = MAX_GRID_SIZE;
    }

    state->puzzle.difficulty = diff;

    printf("\nBuilding your puzzle...\n");
    loadingBar(0, 100);

    int placed = generatePuzzle(&state->puzzle, size, dirCount);

    if (placed < 0) {
        printError(state, "Ran out of memory while building the grid.");
        pauseForUser();
        return;
    }

    printf("\nDone! %d of %d words placed on a %dx%d grid.\n",
           placed, state->puzzle.wordCount, size, size);
    if (placed < state->puzzle.wordCount) {
        printInfo(state, "Some words didn't fit. Try a larger grid or an easier word list next time.");
    }

    pauseForUser();

    printf("\nPlay now? (y/n): ");
    char resp[8];
    readLine(resp, sizeof(resp));
    if (resp[0] == 'y' || resp[0] == 'Y') {
        actionPlayPuzzle(state);
    }
}

static void enterWordsManually(AppState *state, Puzzle *puzzle) {
    printf("Enter one word per line (letters only, 2-%d characters).\n", MAX_WORD_LEN);
    printf("Type 'done' when you're finished.\n\n");

    char input[64];
    while (puzzle->wordCount < MAX_WORDS) {
        printf("Word #%d (or 'done'): ", puzzle->wordCount + 1);
        readLine(input, sizeof(input));

        if (caseInsensitiveEquals(input, "done")) {
            break;
        }

        toUpperString(input);

        if (!isValidWord(input)) {
            printError(state, "Letters only, 2-15 characters. Try again.");
            continue;
        }
        if (isDuplicateWord(puzzle, input)) {
            printError(state, "Already on the list.");
            continue;
        }

        addWordToPuzzle(puzzle, input);
        printf("  Added '%s' (%d word%s so far)\n", input, puzzle->wordCount,
               puzzle->wordCount == 1 ? "" : "s");
    }
}

static void actionPlayPuzzle(AppState *state) {
    playPuzzle(state);
}

static void actionViewSolution(AppState *state) {
    clearScreen();
    if (state->puzzle.size == 0) {
        printError(state, "No puzzle yet. Generate one first!");
        pauseForUser();
        return;
    }
    printHeader(state, "SOLUTION");
    printGrid(state, NULL, 1);
    printWordList(state, 0);
    pauseForUser();
}

static void actionSavePuzzle(AppState *state) {
    clearScreen();
    if (state->puzzle.size == 0) {
        printError(state, "No puzzle to save yet. Generate one first!");
        pauseForUser();
        return;
    }
    printHeader(state, "SAVE PUZZLE");

    char filename[100];
    printf("Save as (Enter for 'puzzle_output.txt'): ");
    readLine(filename, sizeof(filename));
    if (filename[0] == '\0') {
        strcpy(filename, "puzzle_output.txt");
    }

    printf("Include the solution key in the file? (y/n): ");
    char resp[8];
    readLine(resp, sizeof(resp));
    int includeSolution = (resp[0] == 'y' || resp[0] == 'Y');

    if (savePuzzleToFile(&state->puzzle, filename, includeSolution)) {
        char msg[140];
        snprintf(msg, sizeof(msg), "Saved to '%s'", filename);
        printSuccess(state, msg);
    } else {
        printError(state, "Couldn't write that file. Check the path and try again.");
    }

    pauseForUser();
}

static int compareScoreDesc(const void *a, const void *b) {
    const GameStats *sa = (const GameStats *)a;
    const GameStats *sb = (const GameStats *)b;
    return sb->score - sa->score;
}

static void actionViewLeaderboard(AppState *state) {
    GameStats scores[MAX_SCORES];
    int count = loadScores(scores, MAX_SCORES);

    clearScreen();
    printHeader(state, "LEADERBOARD");

    if (count == 0) {
        printInfo(state, "No scores yet - finish a puzzle to be the first on the board!");
    } else {
        qsort(scores, (size_t)count, sizeof(GameStats), compareScoreDesc);
        int show = count < 10 ? count : 10;

        printf("  %-4s %-18s %-7s %-9s %-7s\n", "Rank", "Name", "Score", "Time(s)", "Level");
        printf("  ------------------------------------------------\n");
        for (int i = 0; i < show; i++) {
            printf("  %-4d %-18s %-7d %-9.0f %-7s\n",
                   i + 1, scores[i].playerName, scores[i].score,
                   scores[i].timeSeconds, scores[i].difficultyLabel);
        }
    }

    pauseForUser();
}

static void actionHelp(AppState *state) {
    clearScreen();
    printHeader(state, "HOW TO PLAY");
    printf("  1. Build a puzzle from a file, typed words, or the sample list.\n");
    printf("  2. Pick a difficulty - it sets the grid size and which\n");
    printf("     directions words can run in.\n");
    printf("  3. In Play mode, spot a word and enter the row,col of its\n");
    printf("     first letter and last letter, e.g:  3,2 3,8\n");
    printf("  4. Type 'hint' for a clue, 'solve' to reveal everything and\n");
    printf("     end the round, or 'quit' to return to the menu.\n");
    printf("  5. Find every word to post your score on the leaderboard!\n");
    pauseForUser();
}

static void actionToggleColors(AppState *state) {
    state->colorEnabled = !state->colorEnabled;
    printf("\nColours are now %s.\n", state->colorEnabled ? "ON" : "OFF");
    sleepMs(500);
}

static void actionExit(AppState *state) {
    state->running = 0;
}
