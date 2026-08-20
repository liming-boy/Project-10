/* =====================================================================
 * ui.c
 * ---------------------------------------------------------------------
 * All terminal presentation: colours, the coloured/animated grid
 * display, the typewriter/loading-bar animations (recursive), robust
 * input reading, and the function-pointer-driven menu renderer.
 * =====================================================================
 */
#ifndef _WIN32
    /* Exposes usleep() under strict -std=c11 (glibc hides POSIX
     * extensions unless a feature-test macro is set). Must come
     * before any system header is pulled in. */
    #define _DEFAULT_SOURCE
#endif

#include "ui.h"
#include "grid.h"   /* isCellOnWordPath / isCellHighlighted */

#ifdef _WIN32
    #include <windows.h>
#else
    #include <unistd.h>
#endif

/* ---------------------------------------------------------------------
 * Cross-platform terminal control. Everything platform-specific is
 * isolated to this one block via conditional compilation so the rest
 * of the program never has to think about Windows vs Linux/Mac.
 * ------------------------------------------------------------------- */
void clearScreen(void) {
    /* result is intentionally unused - a failed screen clear is
     * purely cosmetic and not worth aborting the program over */
    int result;
#ifdef _WIN32
    result = system("cls");
#else
    result = system("clear");
#endif
    (void)result;
}

void sleepMs(int ms) {
#ifdef _WIN32
    Sleep((DWORD)ms);
#else
    usleep(ms * 1000);
#endif
}

/* On modern Windows terminals, ANSI escape codes are opt-in - this
 * flips the switch so colours/cursor codes work the same as they do
 * on Linux/macOS out of the box. Harmless no-op everywhere else. */
void enableAnsiSupport(void) {
#ifdef _WIN32
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut != INVALID_HANDLE_VALUE) {
        DWORD mode = 0;
        if (GetConsoleMode(hOut, &mode)) {
            SetConsoleMode(hOut, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
        }
    }
#endif
}

/* ---------------------------------------------------------------------
 * Colour helpers - every coloured print goes through these two so
 * toggling AppState.colorEnabled off truly disables all escape codes.
 * ------------------------------------------------------------------- */
void setColor(const AppState *state, const char *code) {
    if (state->colorEnabled) {
        printf("%s", code);
    }
}

void resetColor(const AppState *state) {
    if (state->colorEnabled) {
        printf("%s", RESET);
    }
}

/* ---------------------------------------------------------------------
 * Input helpers. fgets + manual parsing instead of scanf("%d") avoids
 * the classic leftover-newline / infinite-loop-on-bad-input pitfalls.
 * ------------------------------------------------------------------- */
void readLine(char *buffer, int size) {
    if (fgets(buffer, size, stdin) != NULL) {
        trimString(buffer);
    } else {
        buffer[0] = '\0';
        clearerr(stdin);
    }
}

int readIntInRange(const char *prompt, int min, int max) {
    char buffer[64];
    while (1) {
        printf("%s", prompt);
        readLine(buffer, sizeof(buffer));

        char *endPtr = NULL;
        long value = strtol(buffer, &endPtr, 10);

        if (endPtr != buffer && *endPtr == '\0' && value >= min && value <= max) {
            return (int)value;
        }
        printf("  Please enter a number between %d and %d.\n", min, max);
    }
}

void pauseForUser(void) {
    printf("\nPress Enter to continue...");
    fflush(stdout);
    int c;
    while ((c = getchar()) != '\n' && c != EOF) {
        /* discard until end of line */
    }
}

/* ---------------------------------------------------------------------
 * Coloured status messages.
 * ------------------------------------------------------------------- */
void printHeader(const AppState *state, const char *title) {
    int width = (int)strlen(title) + 10;
    if (width < 44) {
        width = 44;
    }

    printf("\n");
    setColor(state, C_CYAN);
    for (int i = 0; i < width; i++) putchar('=');
    printf("\n");
    setColor(state, BOLD);
    printf("   %s\n", title);
    resetColor(state);
    setColor(state, C_CYAN);
    for (int i = 0; i < width; i++) putchar('=');
    resetColor(state);
    printf("\n\n");
}

void printError(const AppState *state, const char *msg) {
    setColor(state, C_RED);
    printf("  [!] %s\n", msg);
    resetColor(state);
}

void printInfo(const AppState *state, const char *msg) {
    setColor(state, C_CYAN);
    printf("  (i) %s\n", msg);
    resetColor(state);
}

void printSuccess(const AppState *state, const char *msg) {
    setColor(state, C_GREEN);
    printf("  [OK] %s\n", msg);
    resetColor(state);
}

/* ---------------------------------------------------------------------
 * Puzzle display. Column headers / cell bodies are all a fixed 3
 * characters wide, and every row-prefix is a fixed 5 characters wide,
 * so the grid lines up cleanly even at 2-digit coordinates.
 * ------------------------------------------------------------------- */
void printGrid(const AppState *state, const WordEntry *flashEntry, int showAllSolution) {
    const Puzzle *puzzle = &state->puzzle;
    int size = puzzle->size;

    printf("     ");
    for (int c = 0; c < size; c++) {
        printf("%2d ", c + 1);
    }
    printf("\n");

    printf("    +");
    for (int c = 0; c < size; c++) printf("---");
    printf("+\n");

    for (int r = 0; r < size; r++) {
        printf("%3d |", r + 1);
        for (int c = 0; c < size; c++) {
            char ch = puzzle->grid[r][c];
            int flash = (flashEntry != NULL) && isCellOnWordPath(flashEntry, r, c);
            int found = isCellHighlighted(puzzle, r, c, showAllSolution);

            if (flash) {
                setColor(state, BOLD);
                setColor(state, C_BRIGHT_YELLOW);
            } else if (found) {
                setColor(state, BOLD);
                setColor(state, C_GREEN);
            }

            printf(" %c ", ch);

            if (flash || found) {
                resetColor(state);
            }
        }
        printf("|\n");
    }

    printf("    +");
    for (int c = 0; c < size; c++) printf("---");
    printf("+\n\n");
}

void printWordList(const AppState *state, int showFoundStatus) {
    const Puzzle *puzzle = &state->puzzle;
    printf("Words to find:\n  ");

    int printed = 0;
    for (int i = 0; i < puzzle->wordCount; i++) {
        const WordEntry *e = &puzzle->words[i];
        if (!e->placed) {
            continue;
        }
        if (showFoundStatus && e->found) {
            setColor(state, C_GREEN);
            printf("[%s]", e->word);
            resetColor(state);
            printf(" ");
        } else {
            printf("%s ", e->word);
        }
        printed++;
        if (printed % 5 == 0) {
            printf("\n  ");
        }
    }
    printf("\n\n");
}

/* A short flash/blink effect: redraw the grid a couple of times,
 * alternating a bright highlight on the just-found word with the
 * normal (still-green, since 'found' is already set) view. */
void flashFoundWord(const AppState *state, const WordEntry *entry, const char *traced) {
    for (int i = 0; i < 2; i++) {
        clearScreen();
        setColor(state, BOLD);
        setColor(state, C_BRIGHT_YELLOW);
        printf("\n   >>> FOUND \"%s\"  (traced: %s) <<<\n", entry->word, traced);
        resetColor(state);
        printGrid(state, entry, 0);
        sleepMs(180);

        clearScreen();
        printGrid(state, NULL, 0);
        sleepMs(120);
    }
}

/* ---------------------------------------------------------------------
 * RECURSION #4: typePrint - prints one character, sleeps, then calls
 * itself for the next character. Bounded by the string's own length.
 * Kept 'static' (private to this file); typeText() is the public
 * entry point so callers never have to pass the recursion state.
 * ------------------------------------------------------------------- */
static void typePrint(const char *str, int index, int delayMs) {
    if (str[index] == '\0') {
        return;
    }
    putchar(str[index]);
    fflush(stdout);
    sleepMs(delayMs);
    typePrint(str, index + 1, delayMs);
}

void typeText(const char *str) {
    typePrint(str, 0, 10);
}

/* RECURSION #5: loadingBar - recurses once per percentage point,
 * redrawing the bar in place with '\r'. Bounded by 'total'. */
void loadingBar(int progress, int total) {
    if (progress > total) {
        printf("\n");
        return;
    }
    int barWidth = 30;
    int filled = (progress * barWidth) / total;

    printf("\r  [");
    for (int i = 0; i < barWidth; i++) {
        putchar(i < filled ? '#' : '-');
    }
    printf("] %3d%%", (progress * 100) / total);
    fflush(stdout);

    sleepMs(15);
    loadingBar(progress + 1, total);
}

void showBanner(const AppState *state) {
    clearScreen();
    setColor(state, BOLD);
    setColor(state, C_CYAN);
    printf("\n");
    printf("  +--------------------------------------------+\n");
    printf("  |                                              |\n");
    printf("  |        W O R D   S E A R C H                 |\n");
    printf("  |            G E N E R A T O R                 |\n");
    printf("  |                                              |\n");
    printf("  +--------------------------------------------+\n");
    resetColor(state);
    printf("\n");

    setColor(state, C_YELLOW);
    typeText("   Find hidden words across, down, and diagonally!\n");
    resetColor(state);

    printf("\n");
    sleepMs(300);
}

int displayMenuAndGetChoice(const char *title, const MenuItem *items, int count,
                             const AppState *state) {
    clearScreen();
    printHeader(state, title);

    for (int i = 0; i < count; i++) {
        setColor(state, C_YELLOW);
        printf("   %d. ", i + 1);
        resetColor(state);
        printf("%s\n", items[i].label);
    }
    printf("\n");

    char prompt[48];
    snprintf(prompt, sizeof(prompt), "  Choice (1-%d): ", count);
    return readIntInRange(prompt, 1, count);
}
