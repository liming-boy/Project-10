# Project-10
/*=================================================================
 *  WORD SEARCH PUZZLE GENERATOR
 *  ----------------------------------------------------------------
 *  How to run in Code::Blocks:
 *    File -> New -> Project -> Console Application -> C
 *    Paste this code into main.c, then press F9 (Build & Run)
 *
 *  Features:
 *    - Random word placement in 8 directions
 *      (horizontal, vertical, all 4 diagonals — forward & reverse)
 *    - Colour-highlighted found words (Windows console colours)
 *    - Built-in technology word bank OR enter your own words
 *    - Interactive menu: guess, hints, solution reveal, regenerate
 *=================================================================*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>

/* ── Windows colour support (no-op on other OS) ─────────────────── */
#ifdef _WIN32
  #include <windows.h>
  static HANDLE hCon;
  static void initConsole(void) { hCon = GetStdHandle(STD_OUTPUT_HANDLE); }
  static void setColor(int c)   { SetConsoleTextAttribute(hCon, (WORD)c); }
  static void resetColor(void)  { SetConsoleTextAttribute(hCon, 7);       }
  static void clrscr(void)      { system("cls");                           }
#else
  static void initConsole(void) {}
  static void setColor(int c)   { (void)c; }
  static void resetColor(void)  {}
  static void clrscr(void)      { system("clear"); }
#endif

/* ── Configuration constants ────────────────────────────────────── */
#define GS       15        /* Grid dimension (GS x GS)         */
#define MAX_W    12        /* Maximum words in one puzzle       */
#define MAX_L    14        /* Maximum letters in a word         */
#define TRIES   1000       /* Placement attempts per word       */
#define NDIRS    8         /* Eight search directions           */

/* ── Direction vectors [row_delta][col_delta] ───────────────────── */
/*    E    W    S    N   SE   SW   NE   NW                           */
static const int DR[NDIRS] = {  0,  0,  1, -1,  1,  1, -1, -1 };
static const int DC[NDIRS] = {  1, -1,  0,  0,  1, -1,  1, -1 };
static const char *DNAME[NDIRS] = {
    "Right", "Left", "Down", "Up",
    "Down-Right", "Down-Left", "Up-Right", "Up-Left"
};

/* ── Highlight colour palette (Windows console attribute values) ─── */
/* bright: green=10, yellow=14, cyan=11, magenta=13,                 */
/*         red=12,   blue=9,    dk.green=2, dk.yellow=6              */
static const int PAL[8] = { 10, 14, 11, 13, 12, 9, 2, 6 };

/* ── Grid state ─────────────────────────────────────────────────── */
static char grid [GS][GS]; /* 0 = empty, else uppercase letter         */
static int  fmask[GS][GS]; /* 0 = not found;  K = highlighted by word K */
static int  smask[GS][GS]; /* 0 = filler;     K = belongs to word K    */

/* ── Word records ───────────────────────────────────────────────── */
static char W      [MAX_W][MAX_L+1]; /* word strings (uppercase)      */
static int  wRow   [MAX_W];          /* starting row in grid          */
static int  wCol   [MAX_W];          /* starting col in grid          */
static int  wDir   [MAX_W];          /* direction index 0-7           */
static int  wPlaced[MAX_W];          /* 1 = successfully placed       */
static int  wFound [MAX_W];          /* 1 = found by user             */
static int  nW      = 0;             /* total words in list           */
static int  nPlaced = 0;             /* words placed in grid          */
static int  nFound  = 0;             /* words found by user           */

/* ── Built-in word bank (technology theme) ──────────────────────── */
static const char *BANK[] = {
    "ALGORITHM", "COMPILER", "DATABASE", "NETWORK",  "PYTHON",
    "BINARY",    "SYNTAX",   "FUNCTION", "POINTER",  "STRUCT",
    "MEMORY",    "KERNEL",   "SERVER",   "ROUTER",   "CACHE",
    "STACK",     "QUEUE",    "GRAPH",    "ARRAY",    "LOOP",
    "CLASS",     "OBJECT",   "MODULE",   "THREAD",   "PROCESS"
};
#define BANK_SZ 25

/* ==================================================================
   UTILITY HELPERS
================================================================== */

static void toUpper(char *s) {
    for (; *s; s++) *s = toupper((unsigned char)*s);
}

static int inBounds(int r, int c) {
    return r >= 0 && r < GS && c >= 0 && c < GS;
}

/* Discard leftover characters in the input buffer up to newline */
static void flushInput(void) {
    int ch;
    while ((ch = getchar()) != '\n' && ch != EOF);
}

/* ==================================================================
   GRID GENERATION
================================================================== */

static void resetState(void) {
    memset(grid,    0, sizeof grid);
    memset(fmask,   0, sizeof fmask);
    memset(smask,   0, sizeof smask);
    memset(wPlaced, 0, sizeof wPlaced);
    memset(wFound,  0, sizeof wFound);
    nPlaced = 0;
    nFound  = 0;
}

/* Return 1 if word 'w' can be written starting at (r0,c0) in direction d */
static int canPlace(const char *w, int r0, int c0, int d) {
    int len = (int)strlen(w);
    for (int i = 0; i < len; i++) {
        int r = r0 + i * DR[d];
        int c = c0 + i * DC[d];
        if (!inBounds(r, c))                    return 0;
        if (grid[r][c] && grid[r][c] != w[i])  return 0; /* conflict */
    }
    return 1;
}

/* Write word 'w' into the grid and tag smask with word id */
static void doPlace(const char *w, int r0, int c0, int d, int id) {
    int len = (int)strlen(w);
    for (int i = 0; i < len; i++) {
        grid [r0 + i*DR[d]][c0 + i*DC[d]] = w[i];
        smask[r0 + i*DR[d]][c0 + i*DC[d]] = id;
    }
}

/* Try to place word W[idx]; return 1 on success */
static int tryPlaceWord(int idx) {
    for (int t = 0; t < TRIES; t++) {
        int d  = rand() % NDIRS;
        int r0 = rand() % GS;
        int c0 = rand() % GS;
        if (canPlace(W[idx], r0, c0, d)) {
            doPlace(W[idx], r0, c0, d, idx + 1);
            wRow[idx]    = r0;
            wCol[idx]    = c0;
            wDir[idx]    = d;
            wPlaced[idx] = 1;
            nPlaced++;
            return 1;
        }
    }
    return 0; /* could not place within TRIES attempts */
}

/* Fill empty cells with random letters */
static void fillRandom(void) {
    for (int r = 0; r < GS; r++)
        for (int c = 0; c < GS; c++)
            if (!grid[r][c]) grid[r][c] = 'A' + rand() % 26;
}

/* Build the complete puzzle */
static void generatePuzzle(void) {
    resetState();
    for (int i = 0; i < nW; i++) tryPlaceWord(i);
    fillRandom();
}

/* Pick 'n' unique words at random from BANK */
static void pickFromBank(int n) {
    int used[BANK_SZ];
    memset(used, 0, sizeof used);
    nW = 0;
    while (nW < n) {
        int idx = rand() % BANK_SZ;
        if (!used[idx]) {
            used[idx] = 1;
            strncpy(W[nW], BANK[idx], MAX_L);
            W[nW][MAX_L] = '\0';
            nW++;
        }
    }
}

/* ==================================================================
   SEARCH & MARKING
================================================================== */

/* Scan the entire grid for 'word' in all 8 directions.
   On success, fills *fr,*fc,*fd with the starting position/direction
   and returns 1. */
static int findInGrid(const char *word, int *fr, int *fc, int *fd) {
    int len = (int)strlen(word);
    for (int r = 0; r < GS; r++)
    for (int c = 0; c < GS; c++)
    for (int d = 0; d < NDIRS; d++) {
        int ok = 1;
        for (int i = 0; i < len && ok; i++) {
            int nr = r + i * DR[d];
            int nc = c + i * DC[d];
            if (!inBounds(nr, nc) || grid[nr][nc] != word[i]) ok = 0;
        }
        if (ok) { *fr = r; *fc = c; *fd = d; return 1; }
    }
    return 0;
}

/* Mark the cells of a found word in fmask with color-id */
static void markFoundCells(const char *word, int r0, int c0, int d, int id) {
    int len = (int)strlen(word);
    for (int i = 0; i < len; i++)
        fmask[r0 + i*DR[d]][c0 + i*DC[d]] = id;
}

/* ==================================================================
   DISPLAY
================================================================== */

static void printTitle(void) {
    setColor(11); /* bright cyan */
    printf("\n");
    printf("  +=================================================+\n");
    printf("  |      *** WORD SEARCH PUZZLE GENERATOR ***       |\n");
    printf("  |        8 Directions | Find All the Words        |\n");
    printf("  +=================================================+\n");
    resetColor();
}

/*  showSol = 0  -> normal puzzle view
    showSol = 1  -> placed-word cells shown in dark yellow           */
static void printGrid(int showSol) {
    /* Column number header */
    setColor(14); /* bright yellow */
    printf("\n     ");
    for (int c = 0; c < GS; c++) printf("%3d", c + 1);
    printf("\n    +");
    for (int c = 0; c < GS; c++) printf("---");
    printf("+\n");

    /* Grid rows */
    for (int r = 0; r < GS; r++) {
        setColor(14);
        printf("%3d |", r + 1);

        for (int c = 0; c < GS; c++) {
            char ch = grid[r][c];
            int  fm = fmask[r][c];
            int  sm = smask[r][c];

            if (fm > 0) {
                /* Word found by user — bright unique colour per word */
                setColor(PAL[(fm - 1) % 8]);
                printf("[%c]", ch);
            } else if (showSol && sm > 0) {
                /* Solution revealed — dark yellow */
                setColor(6);
                printf("(%c)", ch);
            } else {
                resetColor();
                printf(" %c ", ch);
            }
        }
        setColor(14);
        printf("|\n");
    }

    /* Bottom border */
    printf("    +");
    for (int c = 0; c < GS; c++) printf("---");
    printf("+\n");
    resetColor();
}

static void printWordList(void) {
    setColor(11); /* bright cyan */
    printf("\n  +-------------- FIND THESE %d WORDS  (%d found) ---------------+\n",
           nPlaced, nFound);

    int col = 0;
    for (int i = 0; i < nW; i++) {
        if (!wPlaced[i]) continue; /* skip words that failed to place */

        if (wFound[i]) {
            setColor(PAL[i % 8]);       /* same colour as on grid */
            printf("  (*) %-14s", W[i]);
        } else {
            setColor(7);                /* plain white */
            printf("  ( ) %-14s", W[i]);
        }
        col++;
        if (col % 2 == 0) printf("\n"); /* two columns */
    }
    if (col % 2 != 0) printf("\n");

    setColor(11);
    printf("  +------------------------------------------------------------+\n");
    resetColor();
}

static void printMenu(void) {
    setColor(13); /* bright magenta */
    printf("\n  +----------- ACTIONS -----------+\n");
    printf("  | [1] Guess / mark a word      |\n");
    printf("  | [2] Reveal solution          |\n");
    printf("  | [3] Get a hint               |\n");
    printf("  | [4] Regenerate (same words)  |\n");
    printf("  | [5] New puzzle (new words)   |\n");
    printf("  | [0] Quit                     |\n");
    printf("  +-------------------------------+\n");
    resetColor();
    printf("  Choice: ");
}

/* ==================================================================
   GAME ACTIONS
================================================================== */

/* Action 1 – user guesses a word they spotted in the grid */
static void doGuessWord(void) {
    char buf[MAX_L + 10];
    setColor(14);
    printf("\n  Type the word you spotted: ");
    resetColor();

    if (!fgets(buf, sizeof buf, stdin)) return;
    buf[strcspn(buf, "\r\n")] = '\0';
    toUpper(buf);
    if (!buf[0]) return;

    /* Check if it belongs to the puzzle word list */
    int widx = -1;
    for (int i = 0; i < nW; i++) {
        if (wPlaced[i] && strcmp(W[i], buf) == 0) { widx = i; break; }
    }

    /* Already found? */
    if (widx >= 0 && wFound[widx]) {
        setColor(14);
        printf("  \"%s\" has already been found!\n", buf);
        resetColor();
        return;
    }

    /* Search the grid */
    int fr, fc, fd;
    if (!findInGrid(buf, &fr, &fc, &fd)) {
        setColor(12); /* red */
        printf("  \"%s\" is not in the grid. Check your spelling!\n", buf);
        resetColor();
        return;
    }

    /* Mark those cells */
    int colorId = (widx >= 0) ? widx + 1 : nPlaced + nFound + 1;
    markFoundCells(buf, fr, fc, fd, colorId);

    if (widx >= 0) {
        /* It is a puzzle word */
        wFound[widx] = 1;
        nFound++;
        setColor(10); /* bright green */
        printf("  FOUND!  \"%s\"  starts at Row %d, Col %d  [%s]\n",
               buf, fr + 1, fc + 1, DNAME[fd]);
        resetColor();

        if (nFound == nPlaced) {
            setColor(10);
            printf("\n  **** PUZZLE COMPLETE!  All %d words found!  ****\n", nPlaced);
            resetColor();
        }
    } else {
        /* In the grid but not a puzzle target */
        setColor(14);
        printf("  \"%s\" found at Row %d, Col %d (%s) — but it is not a puzzle word.\n",
               buf, fr + 1, fc + 1, DNAME[fd]);
        resetColor();
    }
}

/* Action 3 – give a hint (reveal start position of one unfound word) */
static void doHint(void) {
    int pool[MAX_W], n = 0;
    for (int i = 0; i < nW; i++)
        if (wPlaced[i] && !wFound[i]) pool[n++] = i;

    if (n == 0) {
        setColor(10);
        printf("  All words found — no hints needed!\n");
        resetColor();
        return;
    }

    int idx = pool[rand() % n];
    setColor(14);
    printf("  HINT: \"%s\"  starts at Row %d, Col %d  (going %s)\n",
           W[idx], wRow[idx] + 1, wCol[idx] + 1, DNAME[wDir[idx]]);
    resetColor();
}

/* Helper: prompt user to type their own word list */
static void getCustomWords(void) {
    int n;
    printf("\n  How many words? (2-%d): ", MAX_W);
    if (scanf("%d", &n) != 1) { flushInput(); n = 4; }
    flushInput();
    if (n < 2)     n = 2;
    if (n > MAX_W) n = MAX_W;

    nW = 0;
    for (int i = 0; i < n; i++) {
        printf("  Word %d/%d  (2-%d letters, letters only): ", i + 1, n, MAX_L);
        char buf[64];
        if (!fgets(buf, sizeof buf, stdin)) { i--; continue; }
        buf[strcspn(buf, "\r\n")] = '\0';
        toUpper(buf);

        /* Validate */
        int len = (int)strlen(buf);
        if (len < 2 || len > MAX_L) {
            setColor(12);
            printf("  Length must be 2-%d — skipped.\n", MAX_L);
            resetColor();
            i--; continue;
        }
        int valid = 1;
        for (int k = 0; k < len; k++) {
            if (!isalpha((unsigned char)buf[k])) { valid = 0; break; }
        }
        if (!valid) {
            setColor(12);
            printf("  Letters only — skipped.\n");
            resetColor();
            i--; continue;
        }

        strncpy(W[nW], buf, MAX_L);
        W[nW][MAX_L] = '\0';
        nW++;
    }
}

/* ==================================================================
   MAIN
================================================================== */
int main(void) {
    initConsole();
    srand((unsigned)time(NULL));

    clrscr();
    printTitle();

    /* ── Choose word source ──────────────────────────────────────── */
    int useBank = 1;
    setColor(13);
    printf("\n  Choose word set:\n");
    printf("  [1] Built-in technology word bank\n");
    printf("  [2] Enter my own words\n");
    resetColor();
    printf("  Choice: ");
    if (scanf("%d", &useBank) != 1) useBank = 1;
    flushInput();

    if (useBank == 2) {
        getCustomWords();
        if (nW < 2) {
            setColor(12);
            printf("  Too few words entered — using built-in bank instead.\n");
            resetColor();
            pickFromBank(8);
        }
    } else {
        int n;
        setColor(14);
        printf("\n  How many words? (4-%d): ", MAX_W);
        resetColor();
        if (scanf("%d", &n) != 1) n = 8;
        flushInput();
        if (n < 4)     n = 4;
        if (n > MAX_W) n = MAX_W;
        pickFromBank(n);
    }

    generatePuzzle();

    /* ── Main game loop ──────────────────────────────────────────── */
    int showSol = 0;
    int running = 1;

    while (running) {
        clrscr();
        printTitle();
        printGrid(showSol);
        printWordList();
        printMenu();

        int ch;
        if (scanf("%d", &ch) != 1) { flushInput(); continue; }
        flushInput();

        switch (ch) {

            case 1:  /* Guess a word */
                doGuessWord();
                showSol = 0;
                printf("\n  Press Enter to continue...");
                flushInput();
                break;

            case 2:  /* Reveal solution */
                showSol = 1;
                setColor(6);
                printf("\n  Solution shown — puzzle words appear as (X).\n");
                printf("  Words you find will still be highlighted [X].\n");
                resetColor();
                printf("  Press Enter to continue...");
                flushInput();
                break;

            case 3:  /* Hint */
                doHint();
                printf("\n  Press Enter to continue...");
                flushInput();
                break;

            case 4:  /* Regenerate same word list */
                generatePuzzle();
                showSol = 0;
                setColor(11);
                printf("\n  Same words, fresh grid generated!\n");
                resetColor();
                printf("  Press Enter to continue...");
                flushInput();
                break;

            case 5: {  /* New word list + new grid */
                if (useBank == 1) {
                    int n;
                    setColor(14);
                    printf("\n  How many words? (4-%d): ", MAX_W);
                    resetColor();
                    if (scanf("%d", &n) != 1) n = 8;
                    flushInput();
                    if (n < 4)     n = 4;
                    if (n > MAX_W) n = MAX_W;
                    pickFromBank(n);
                } else {
                    getCustomWords();
                    if (nW < 2) pickFromBank(8);
                }
                generatePuzzle();
                showSol = 0;
                setColor(11);
                printf("\n  New puzzle ready!\n");
                resetColor();
                printf("  Press Enter to continue...");
                flushInput();
                break;
            }

            case 0:  /* Quit */
                running = 0;
                break;

            default:
                setColor(12);
                printf("  Invalid choice — please enter 0-5.\n");
                resetColor();
                printf("  Press Enter to continue...");
                flushInput();
                break;
        }
    }

    /* ── Goodbye screen ─────────────────────────────────────────── */
    clrscr();
    printTitle();
    setColor(11);
    printf("\n  Thanks for playing Word Search!\n");
    setColor(14);
    printf("  Final score: %d / %d words found.\n\n", nFound, nPlaced);
    resetColor();

    return 0;
}
