#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>

/* Part 1: Console UI & OS Compatibility */
#ifdef _WIN32
#include <windows.h>
static HANDLE hCon;
/* Initializes the Windows console handle for color manipulation */
static void initConsole(void) { hCon = GetStdHandle(STD_OUTPUT_HANDLE); }
/* Changes the text color in the terminal using Win32 API */
static void setColor(int c)   { SetConsoleTextAttribute(hCon, (WORD)c); }
/* Resets the text color back to default white (7) */
static void resetColor(void)  { SetConsoleTextAttribute(hCon, 7);       }
/* Clears the console screen */
static void clrscr(void)      { system("cls");                           }
#else
/* Fallback functions for Mac/Linux environments */
static void initConsole(void) {}
static void setColor(int c)   { (void)c; }
static void resetColor(void)  {}
static void clrscr(void)      { system("clear"); }
#endif

/* Game Configuration Constants */
#define GS       15     // Grid Size (15x15)
#define MAX_W    12     // Maximum number of words allowed in a puzzle
#define MAX_L    14     // Maximum length of a single word
#define TRIES   1000    // Max attempts (Monte Carlo) to place a word randomly
#define NDIRS    8      // Number of possible directions (8-way movement)

/*
Vector Mathematics Arrays (DR = Delta Row, DC = Delta Column)
These vectors allow us to traverse the 2D grid in all 8 directions
without hardcoding specific paths.
*/

static const int DR[NDIRS] = {  0,  0,  1, -1,  1,  1, -1, -1 };
static const int DC[NDIRS] = {  1, -1,  0,  0,  1, -1,  1, -1 };
static const char *DNAME[NDIRS] = {
"Right", "Left", "Down", "Up",
"Down-Right", "Down-Left", "Up-Right", "Up-Left"
};

/* Color palette array used to uniquely color-code found words */
static const int PAL[8] = { 10, 14, 11, 13, 12, 9, 2, 6 };

/* Global State Tracking Arrays & Matrices */
static char grid [GS][GS];    // The raw ASCII character map shown to the player
static int  fmask[GS][GS];    // Found mask: stores color IDs of user-found words
static int  smask[GS][GS];    // Solution mask: stores underlying solution IDs

/* Arrays tracking the status and location of each word in the puzzle */
static char W      [MAX_W][MAX_L+1]; // Array storing the target puzzle words
static int  wRow   [MAX_W];          // Starting row of the placed word
static int  wCol   [MAX_W];          // Starting column of the placed word
static int  wDir   [MAX_W];          // Direction vector ID of the placed word
static int  wPlaced[MAX_W];          // Boolean flag: 1 if successfully placed on grid
static int  wFound [MAX_W];          // Boolean flag: 1 if spotted by the user

static int  nW      = 0;             // Total number of words selected for this round
static int  nPlaced = 0;             // Number of words successfully placed on the grid
static int  nFound  = 0;             // Number of words the user has found so far

/* Data Structures
Encapsulates player data for persistent session tracking */

typedef struct {
char username[32];       // Stores the player's inputted name
int totalSessionScore;   // Cumulative words found across all games played
int puzzlesCompleted;    // Total number of full puzzle victories
} PlayerProfile;

static PlayerProfile player; // Global instance of the active player

/* File I/O Module */
#define MAX_BANK_SZ 100
static char externalBank[MAX_BANK_SZ][MAX_L + 1];
static int externalBankSz = 0;

/* Parses words.txt to build the word bank, auto-generating it if missing */
static void loadWordBankFromFile(void) {
FILE *fp = fopen("words.txt", "r");

/* Failsafe: If file doesn't exist, generate a default tech dictionary */
if (!fp) {
fp = fopen("words.txt", "w");
if (fp) {
fprintf(fp, "ALGORITHM\nCOMPILER\nDATABASE\nNETWORK\nPYTHON\nBINARY\nSYNTAX\n");
fprintf(fp, "FUNCTION\nPOINTER\nSTRUCT\nMEMORY\nKERNEL\nSERVER\nROUTER\nCACHE\n");
fprintf(fp, "STACK\nQUEUE\nGRAPH\nARRAY\nLOOP\nCLASS\nOBJECT\nMODULE\nTHREAD\nPROCESS\n");
fclose(fp);
}
fp = fopen("words.txt", "r");
if (!fp) return;
}

char buf[128];
externalBankSz = 0;

/* Read stream safely using fgets to prevent buffer overflow */
while (fgets(buf, sizeof(buf), fp) && externalBankSz < MAX_BANK_SZ) {
buf[strcspn(buf, "\r\n")] = '\0'; // Strip trailing carriage returns/newlines

/* Convert string to uppercase for normalization */
for (char *s = buf; *s; s++) *s = toupper((unsigned char)*s);

int len = (int)strlen(buf);
int duplicate = 0;

/* Check against already loaded words to prevent duplicates in the bank */
for (int i = 0; i < externalBankSz; i++) {
    if (strcmp(externalBank[i], buf) == 0) {
        duplicate = 1;
        break;
    }
}

/* Only add valid, non-duplicate words to the external bank */
if (len >= 2 && len <= MAX_L && !duplicate) {
    strncpy(externalBank[externalBankSz], buf, MAX_L);
    externalBank[externalBankSz][MAX_L] = '\0';
    externalBankSz++;
}


}
fclose(fp);
}

/* Utility & Input Sanitization Functions
Converts an entire string to uppercase */
static void toUpper(char *s) {
for (; *s; s++) *s = toupper((unsigned char)*s);
}

/* Validates if proposed coordinates sit safely inside the 15x15 boundaries */
static int inBounds(int r, int c) {
return r >= 0 && r < GS && c >= 0 && c < GS;
}

/* Clears residual characters/newlines from stdin to prevent infinite loops */
static void flushInput(void) {
int ch;
while ((ch = getchar()) != '\n' && ch != EOF);
}

/* Wipes all arrays and matrices clean before a new puzzle starts */
static void resetState(void) {
memset(grid,    0, sizeof grid);
memset(fmask,   0, sizeof fmask);
memset(smask,   0, sizeof smask);
memset(wPlaced, 0, sizeof wPlaced);
memset(wFound,  0, sizeof wFound);
nPlaced = 0;
nFound  = 0;
}

/* Grid Generation Algorithms (Procedural Matrix Generation)
Evaluates vector trajectory for boundary issues and letter collision */
static int canPlace(const char *w, int r0, int c0, int d) {
int len = (int)strlen(w);
for (int i = 0; i < len; i++) {
int r = r0 + i * DR[d]; // Calculate next row using offset vector
int c = c0 + i * DC[d]; // Calculate next col using offset vector

if (!inBounds(r, c))                    return 0; // Reject: Out of bounds
/* Reject: Cell occupied by a DIFFERENT letter (interlocking is permitted) */
if (grid[r][c] && grid[r][c] != w[i])  return 0;
}
return 1; // Placement is safe
}

/* Physically injects the word characters into the grid and solution mask */
static void doPlace(const char *w, int r0, int c0, int d, int id) {
int len = (int)strlen(w);
for (int i = 0; i < len; i++) {
grid [r0 + i * DR[d]][c0 + i * DC[d]] = w[i];
smask[r0 + i * DR[d]][c0 + i * DC[d]] = id;
}
}

/* Monte Carlo algorithm: tries placing a word randomly up to 1000 times */
static int tryPlaceWord(int idx) {
for (int t = 0; t < TRIES; t++) {
int d  = rand() % NDIRS; // Pick random direction
int r0 = rand() % GS;    // Pick random start row
int c0 = rand() % GS;    // Pick random start col

/* If safe, place it and record its metadata */
if (canPlace(W[idx], r0, c0, d)) {
doPlace(W[idx], r0, c0, d, idx + 1);
wRow[idx]    = r0;
wCol[idx]    = c0;
wDir[idx]    = d;
wPlaced[idx] = 1;
nPlaced++;
return 1; // Success
}
}
return 0; // Failed to place after TRIES attempts
}

/* Fills remaining empty grid cells with random uppercase ASCII noise */
static void fillRandom(void) {
for (int r = 0; r < GS; r++)
for (int c = 0; c < GS; c++)
if (!grid[r][c]) grid[r][c] = 'A' + rand() % 26;
}

/* Master function to generate a fresh puzzle */
static void generatePuzzle(void) {
resetState();
for (int i = 0; i < nW; i++) tryPlaceWord(i);
fillRandom();
}

/* Randomly selects 'n' words from the external bank to use in the puzzle */
static void pickFromBank(int n) {
if (externalBankSz == 0) loadWordBankFromFile();

if (externalBankSz == 0) {
printf("Error: words.txt is empty!\n");
exit(1);
}

/* Part 2: Select distinct words without repeats */
int used[MAX_BANK_SZ];
memset(used, 0, sizeof used);
nW = 0;

int limit = (n > externalBankSz) ? externalBankSz : n;

while (nW < limit) {
int idx = rand() % externalBankSz;
if (!used[idx]) {
used[idx] = 1;
strncpy(W[nW], externalBank[idx], MAX_L);
W[nW][MAX_L] = '\0';
nW++;
}
}
}

/* Search Engine & Validation
4-level nested O(R * C * D * L) matrix scan to verify user guesses */
static int findInGrid(const char *word, int *fr, int *fc, int *fd) {
int len = (int)strlen(word);

for (int r = 0; r < GS; r++)             // Level 1: Rows
for (int c = 0; c < GS; c++)             // Level 2: Cols
for (int d = 0; d < NDIRS; d++) {        // Level 3: Directions
int ok = 1;

/* Level 4: Trace the path and check for exact character matches */
for (int i = 0; i < len && ok; i++) {
int nr = r + i * DR[d];
int nc = c + i * DC[d];
if (!inBounds(nr, nc) || grid[nr][nc] != word[i]) ok = 0;
}
/* If match is fully continuous, return starting coords and direction via pointers */
if (ok) { *fr = r; *fc = c; *fd = d; return 1; }
}
return 0; // Word not found anywhere in grid
}

/* Updates fmask with a color ID so the UI knows to highlight this path */
static void markFoundCells(const char *word, int r0, int c0, int d, int id) {
int len = (int)strlen(word);
for (int i = 0; i < len; i++)
fmask[r0 + i * DR[d]][c0 + i * DC[d]] = id;
}

/* UI Rendering (Dynamic Board & Console Graphics)
Renders the top title banner */
static void printTitle(void) {
setColor(11); /* bright cyan */
printf("\n");
printf("  +=+\n");
printf("  |      *** WORD SEARCH PUZZLE GENERATOR ***       |\n");
printf("  |        8 Directions | Find All the Words        |\n");
printf("  +=+\n");
resetColor();
}

/* Renders the 15x15 board, interpreting masks to colorize specific characters */
static void printGrid(int showSol) {
setColor(14); /* bright yellow */

/* Print column headers */
printf("\n     ");
for (int c = 0; c < GS; c++) printf("%3d", c + 1);
printf("\n   +");
for (int c = 0; c < GS; c++) printf("---");
printf("+\n");

/* Print grid contents row by row */
for (int r = 0; r < GS; r++) {
setColor(14);
printf("%3d |", r + 1); // Row headers

for (int c = 0; c < GS; c++) {
    char ch = grid[r][c];
    int  fm = fmask[r][c]; // Check if cell is found by user
    int  sm = smask[r][c]; // Check if cell is part of the solution

    if (fm > 0) {
        /* Highlight user-found words in their specific palette color */
        setColor(PAL[(fm - 1) % 8]);
        printf("[%c]", ch);
    } else if (showSol && sm > 0) {
        /* If cheat mode active, reveal un-found solution cells in dark yellow */
        setColor(6);
        printf("(%c)", ch);
    } else {
        /* Render standard background noise characters in white */
        resetColor();
        printf(" %c ", ch);
    }
}
setColor(14);
printf("|\n");


}

printf("   +");
for (int c = 0; c < GS; c++) printf("---");
printf("+\n");
resetColor();
}

/* Displays checklist of target words and their discovery status */
static void printWordList(void) {
setColor(11);
/* NEW: Display player name next to the word list using the Struct */
printf("\n  +--- %s'S PUZZLE --- FIND THESE %d WORDS (%d found) ---+\n",
player.username, nPlaced, nFound);

int col = 0;
for (int i = 0; i < nW; i++) {
if (!wPlaced[i]) continue; // Skip words that failed to generate

if (wFound[i]) {
    setColor(PAL[i % 8]);       // Highlight found words  
    printf("  (*) %-14s", W[i]);
} else {
    setColor(7);                // Gray out un-found words
    printf("  ( ) %-14s", W[i]);
}

/* Formatting: Print in 2 columns */
col++;
if (col % 2 == 0) printf("\n"); 


}
if (col % 2 != 0) printf("\n");

setColor(11);
printf("  +------------------------------------------------------------+\n");
resetColor();
}

/* Renders interactive options menu */
static void printMenu(void) {
setColor(13); /* bright magenta */
printf("\n  +----------- ACTIONS -----------+\n");
printf("  | [1] Guess / mark a word       |\n");
printf("  | [2] Reveal solution           |\n");
printf("  | [3] Get a hint                |\n");
printf("  | [4] Regenerate (same words)   |\n");
printf("  | [5] New puzzle (new words)    |\n");
printf("  | [0] Quit                      |\n");
printf("  +-------------------------------+\n");
resetColor();
printf("  Choice: ");
}

/*Interactive Gameplay Logic
Handles user string input, sanitizes it, and triggers search verification */
static void doGuessWord(void) {
char buf[MAX_L + 10];
setColor(14);
printf("\n  Type the word you spotted: ");
resetColor();

/* Buffer safety: reads input strictly bounded by buf size */
if (!fgets(buf, sizeof buf, stdin)) return;
if (strchr(buf, '\n') == NULL) flushInput(); // Clear stdin if overflow happened
buf[strcspn(buf, "\r\n")] = '\0';            // Strip newline
toUpper(buf);                                // Normalize case
if (!buf[0]) return;

/* Validate guess against the active dictionary of placed words */
int widx = -1;
for (int i = 0; i < nW; i++) {
if (wPlaced[i] && strcmp(W[i], buf) == 0) { widx = i; break; }
}

/* Catch duplicate guesses */
if (widx >= 0 && wFound[widx]) {
setColor(14);
printf("  "%s" has already been found!\n", buf);
resetColor();
return;
}

/* Trigger multidimensional search engine */
int fr, fc, fd;
if (!findInGrid(buf, &fr, &fc, &fd)) {
setColor(12); / red */
printf("  "%s" is not in the grid. Check your spelling!\n", buf);
resetColor();
return;
}

/* Determine color mapping and apply mask to UI */
int colorId = (widx >= 0) ? widx + 1 : nPlaced + nFound + 1;
markFoundCells(buf, fr, fc, fd, colorId);

if (widx >= 0) {
wFound[widx] = 1;
nFound++;

/* Update structure's global session score data */
player.totalSessionScore++; 

setColor(10); /* bright green */
printf("  FOUND!  \"%s\"  starts at Row %d, Col %d  [%s]\n",
       buf, fr + 1, fc + 1, DNAME[fd]);
resetColor();

/* Check Win Condition */
if (nFound == nPlaced) {
    /* Update completion count in struct */
    player.puzzlesCompleted++;

    setColor(10);
    printf("\n  **** PUZZLE COMPLETE!  All %d words found!  ****\n", nPlaced);
    resetColor();
}


} else {
/* Edge Case: User found a string that exists in the random noise by accident */
setColor(14);
printf("  "%s" found at Row %d, Col %d (%s) — but it is not a puzzle word.\n",
buf, fr + 1, fc + 1, DNAME[fd]);
resetColor();
}
}

/* Part 3 */
/* Analyzes tracking arrays to find a hidden word and reveals its coordinates */
static void doHint(void) {
int pool[MAX_W], n = 0;

/* Build candidate pool stack of un-found words */
for (int i = 0; i < nW; i++)
if (wPlaced[i] && !wFound[i]) pool[n++] = i;

if (n == 0) {
setColor(10);
printf("  All words found — no hints needed!\n");
resetColor();
return;
}

/* Select random candidate from pool and extract its coordinates */
int idx = pool[rand() % n];
setColor(14);
printf("  HINT: "%s"  starts at Row %d, Col %d  (going %s)\n",
W[idx], wRow[idx] + 1, wCol[idx] + 1, DNAME[wDir[idx]]);
resetColor();
}

/* Allows users to input their own word banks securely */
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

/* Robust Input Sanitization Pipeline */
char buf[64];
if (!fgets(buf, sizeof buf, stdin)) { i--; continue; }
if (strchr(buf, '\n') == NULL) flushInput();
buf[strcspn(buf, "\r\n")] = '\0';
toUpper(buf);

/* Validate Length Bounds */
int len = (int)strlen(buf);
if (len < 2 || len > MAX_L) {
    setColor(12);
    printf("  Length must be 2-%d — skipped.\n", MAX_L);
    resetColor();
    i--; continue;
}

/* Validate Alpha Characters Only */
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

/* Reject Duplicates via String Compare */
int duplicate = 0;
for (int j = 0; j < nW; j++) {
    if (strcmp(W[j], buf) == 0) {
        duplicate = 1;
        break;
    }
}
if (duplicate) {
    setColor(12);
    printf("  Duplicate word — skipped.\n");
    resetColor();
    i--; continue;
}

/* Push validated word to array */
strncpy(W[nW], buf, MAX_L);
W[nW][MAX_L] = '\0';
nW++;


}
}

/* Main Execution Loop */
int main(void) {
initConsole();
srand((unsigned)time(NULL)); // Seed RNG for unique puzzle grids

clrscr();
printTitle();

/* Setup the Player Profile Struct via standard input */
setColor(14);
printf("\n  Enter your Player Name: ");
resetColor();
if (fgets(player.username, sizeof(player.username), stdin)) {
if (strchr(player.username, '\n') == NULL) flushInput();
player.username[strcspn(player.username, "\r\n")] = '\0'; // Strip newline
}
if (player.username[0] == '\0') strcpy(player.username, "Guest");

player.totalSessionScore = 0;
player.puzzlesCompleted = 0;

/* Pre-game Setup Menu */
int useBank = 1;
setColor(13);
printf("\n  Choose word set:\n");
printf("  [1] Load words from external words.txt (File I/O)\n");
printf("  [2] Enter my own words\n");
resetColor();
printf("  Choice: ");
if (scanf("%d", &useBank) != 1) useBank = 1;
flushInput();

if (useBank == 2) {
getCustomWords();
/* Fallback if user failed to input valid strings */
if (nW < 2) {
setColor(12);
printf("  Too few words entered — using external file bank instead.\n");
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

/* Clamp constraints */
if (n < 4)     n = 4;
if (n > MAX_W) n = MAX_W;
pickFromBank(n);


}

/* Part 4 */
generatePuzzle(); // Build the first board

int showSol = 0;
int running = 1;

/* Primary Game State Loop */
while (running) {
clrscr();             // Clear old frame
printTitle();         // Render headers
printGrid(showSol);   // Render matrix
printWordList();      // Render side-targets
printMenu();          // Render interactive UI

int ch;
if (scanf("%d", &ch) != 1) { flushInput(); continue; }
flushInput();

/* Switch controller for game state routing */
switch (ch) {
    case 1:  
        doGuessWord();
        showSol = 0;
        printf("\n  Press Enter to continue...");
        flushInput();
        break;

    case 2:  
        showSol = 1; // Flips boolean to reveal hidden solution mask
        setColor(6);
        printf("\n  Solution shown — puzzle words appear as (X).\n");
        printf("  Words you find will still be highlighted [X].\n");
        resetColor();
        printf("  Press Enter to continue...");
        flushInput();
        break;

    case 3:  
        doHint();
        printf("\n  Press Enter to continue...");
        flushInput();
        break;

    case 4:  
        generatePuzzle(); // Re-runs placement engine on exact same word set
        showSol = 0;
        setColor(11);
        printf("\n  Same words, fresh grid generated!\n");
        resetColor();
        printf("  Press Enter to continue...");
        flushInput();
        break;

    case 5: {  
        /* Wipes word set and requests new words/file fetch */
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

    case 0:  
        running = 0; // Breaker for the while-loop
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

/* Post-Game Exit Sequence */
clrscr();
printTitle();
setColor(11);
printf("\n  Thanks for playing Word Search!\n");

/* OUTPUT: Display the persistent Struct Data at Game Over */
setColor(14);
printf("  ======================================\n");
printf("  PLAYER PROFILE SUMMARY:\n");
printf("  Name: %s\n", player.username);
printf("  Total Words Found: %d\n", player.totalSessionScore);
printf("  Puzzles Fully Completed: %d\n", player.puzzlesCompleted);
printf("  ======================================\n\n");
resetColor();

return 0;
}
