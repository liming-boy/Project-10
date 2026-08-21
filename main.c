#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>

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

#define GS       15

#define MAX_W    12

#define MAX_L    14

#define TRIES   1000

#define NDIRS    8

static const int DR[NDIRS] = {  0,  0,  1, -1,  1,  1, -1, -1 };
static const int DC[NDIRS] = {  1, -1,  0,  0,  1, -1,  1, -1 };
static const char *DNAME[NDIRS] = {
"Right", "Left", "Down", "Up",
"Down-Right", "Down-Left", "Up-Right", "Up-Left"
};

static const int PAL[8] = { 10, 14, 11, 13, 12, 9, 2, 6 };

static char grid [GS][GS];
static int  fmask[GS][GS];
static int  smask[GS][GS];

static char W      [MAX_W][MAX_L+1];
static int  wRow   [MAX_W];

static int  wCol   [MAX_W];

static int  wDir   [MAX_W];

static int  wPlaced[MAX_W];

static int  wFound [MAX_W];

static int  nW      = 0;

static int  nPlaced = 0;

static int  nFound  = 0;

/* Structure for Player Profile & Score Entry */
typedef struct {
char username[32];
int totalSessionScore; /* Total words found across all puzzles played */
int puzzlesCompleted;
} PlayerProfile;

static PlayerProfile player; /* Global instance of our player */

/* File I/O for External Word Bank*/
#define MAX_BANK_SZ 100
static char externalBank[MAX_BANK_SZ][MAX_L + 1];
static int externalBankSz = 0;

static void loadWordBankFromFile(void) {
FILE *fp = fopen("words.txt", "r");

/* If words.txt is missing, generate a default one automatically 
   so the game doesn't crash on first run! */
if (!fp) {
    fp = fopen("words.txt", "w");
    if (fp) {
        fprintf(fp, "ALGORITHM\nCOMPILER\nDATABASE\nNETWORK\nPYTHON\nBINARY\nSYNTAX\n");
        fprintf(fp, "FUNCTION\nPOINTER\nSTRUCT\nMEMORY\nKERNEL\nSERVER\nROUTER\nCACHE\n");
        fprintf(fp, "STACK\nQUEUE\nGRAPH\nARRAY\nLOOP\nCLASS\nOBJECT\nMODULE\nTHREAD\nPROCESS\n");
        fclose(fp);
    }
    fp = fopen("words.txt", "r"); /* Re-open for reading */
    if (!fp) return; /* Failsafe */
}

char buf[128];
externalBankSz = 0;

/* Read line-by-line using standard File I/O */
while (fgets(buf, sizeof(buf), fp) && externalBankSz < MAX_BANK_SZ) {
    buf[strcspn(buf, "\r\n")] = '\0'; /* Strip newline character */
    
    /* toUpper logic directly here to ensure words are clean */
    for (char *s = buf; *s; s++) *s = toupper((unsigned char)*s);
    
    int len = (int)strlen(buf);
    int duplicate = 0;
    for (int i = 0; i < externalBankSz; i++) {
        if (strcmp(externalBank[i], buf) == 0) {
            duplicate = 1;
            break;
        }
    }
    if (len >= 2 && len <= MAX_L && !duplicate) {
        strncpy(externalBank[externalBankSz], buf, MAX_L);
        externalBank[externalBankSz][MAX_L] = '\0';
        externalBankSz++;
    }
}
fclose(fp);


}


static void toUpper(char *s) {
for (; *s; s++) *s = toupper((unsigned char)*s);
}

static int inBounds(int r, int c) {
return r >= 0 && r < GS && c >= 0 && c < GS;
}

static void flushInput(void) {
int ch;
while ((ch = getchar()) != '\n' && ch != EOF);
}

static void resetState(void) {
memset(grid,    0, sizeof grid);
memset(fmask,   0, sizeof fmask);
memset(smask,   0, sizeof smask);
memset(wPlaced, 0, sizeof wPlaced);
memset(wFound,  0, sizeof wFound);
nPlaced = 0;
nFound  = 0;
}

static int canPlace(const char *w, int r0, int c0, int d) {
int len = (int)strlen(w);
for (int i = 0; i < len; i++) {
int r = r0 + i * DR[d];
int c = c0 + i * DC[d];
if (!inBounds(r, c))                    return 0;
if (grid[r][c] && grid[r][c] != w[i])  return 0;
}
return 1;
}

static void doPlace(const char *w, int r0, int c0, int d, int id) {
int len = (int)strlen(w);
for (int i = 0; i < len; i++) {
grid [r0 + i * DR[d]][c0 + i * DC[d]] = w[i];
smask[r0 + i * DR[d]][c0 + i * DC[d]] = id;
}

}

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
return 0;
}

static void fillRandom(void) {
for (int r = 0; r < GS; r++)
for (int c = 0; c < GS; c++)
if (!grid[r][c]) grid[r][c] = 'A' + rand() % 26;
}

static void generatePuzzle(void) {
resetState();
for (int i = 0; i < nW; i++) tryPlaceWord(i);
fillRandom();
}

static void pickFromBank(int n) {
/* Ensure the bank is loaded from the file first */
if (externalBankSz == 0) loadWordBankFromFile();

/* Failsafe if file was completely empty */
if (externalBankSz == 0) {
    printf("Error: words.txt is empty!\n");
    exit(1);
}

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

static void markFoundCells(const char *word, int r0, int c0, int d, int id) {
int len = (int)strlen(word);
for (int i = 0; i < len; i++)
fmask[r0 + i * DR[d]][c0 + i * DC[d]] = id;
}

static void printTitle(void) {
setColor(11); /* bright cyan */
printf("\n");
printf("  +=+\n");
printf("  |      *** WORD SEARCH PUZZLE GENERATOR ***       |\n");
printf("  |        8 Directions | Find All the Words        |\n");
printf("  +=+\n");
resetColor();
}

static void printGrid(int showSol) {
setColor(14); /* bright yellow */
printf("\n     ");
for (int c = 0; c < GS; c++) printf("%3d", c + 1);
printf("\n    +");
for (int c = 0; c < GS; c++) printf("---");
printf("+\n");

for (int r = 0; r < GS; r++) {
    setColor(14);
    printf("%3d |", r + 1);

    for (int c = 0; c < GS; c++) {
        char ch = grid[r][c];
        int  fm = fmask[r][c];
        int  sm = smask[r][c];

        if (fm > 0) {
            setColor(PAL[(fm - 1) % 8]);
            printf("[%c]", ch);
        } else if (showSol && sm > 0) {
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

printf("    +");
for (int c = 0; c < GS; c++) printf("---");
printf("+\n");
resetColor();


}

static void printWordList(void) {
setColor(11);
/* NEW: Display player name next to the word list */
printf("\n  +--- %s'S PUZZLE --- FIND THESE %d WORDS (%d found) ---+\n",
player.username, nPlaced, nFound);

int col = 0;
for (int i = 0; i < nW; i++) {
    if (!wPlaced[i]) continue; 

    if (wFound[i]) {
        setColor(PAL[i % 8]);       
        printf("  (*) %-14s", W[i]);
    } else {
        setColor(7);                
        printf("  ( ) %-14s", W[i]);
    }
    col++;
    if (col % 2 == 0) printf("\n"); 
}
if (col % 2 != 0) printf("\n");

setColor(11);
printf("  +------------------------------------------------------------+\n");
resetColor();


}

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

static void doGuessWord(void) {
char buf[MAX_L + 10];
setColor(14);
printf("\n  Type the word you spotted: ");
resetColor();

if (!fgets(buf, sizeof buf, stdin)) return;
if (strchr(buf, '\n') == NULL) flushInput();
buf[strcspn(buf, "\r\n")] = '\0';
toUpper(buf);
if (!buf[0]) return;

int widx = -1;
for (int i = 0; i < nW; i++) {
    if (wPlaced[i] && strcmp(W[i], buf) == 0) { widx = i; break; }
}

if (widx >= 0 && wFound[widx]) {
    setColor(14);
    printf("  \"%s\" has already been found!\n", buf);
    resetColor();
    return;
}

int fr, fc, fd;
if (!findInGrid(buf, &fr, &fc, &fd)) {
    setColor(12); /* red */
    printf("  \"%s\" is not in the grid. Check your spelling!\n", buf);
    resetColor();
    return;
}

int colorId = (widx >= 0) ? widx + 1 : nPlaced + nFound + 1;
markFoundCells(buf, fr, fc, fd, colorId);

if (widx >= 0) {
    wFound[widx] = 1;
    nFound++;
    
    /*structure's global score data */
    player.totalSessionScore++; 

    setColor(10); /* bright green */
    printf("  FOUND!  \"%s\"  starts at Row %d, Col %d  [%s]\n",
           buf, fr + 1, fc + 1, DNAME[fd]);
    resetColor();

    if (nFound == nPlaced) {
        /* completion count in struct */
        player.puzzlesCompleted++;

        setColor(10);
        printf("\n  **** PUZZLE COMPLETE!  All %d words found!  ****\n", nPlaced);
        resetColor();
    }
} else {
    setColor(14);
    printf("  \"%s\" found at Row %d, Col %d (%s) — but it is not a puzzle word.\n",
           buf, fr + 1, fc + 1, DNAME[fd]);
    resetColor();
}


}

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
    if (strchr(buf, '\n') == NULL) flushInput();
    buf[strcspn(buf, "\r\n")] = '\0';
    toUpper(buf);

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

    strncpy(W[nW], buf, MAX_L);
    W[nW][MAX_L] = '\0';
    nW++;
}


}

int main(void) {
initConsole();
srand((unsigned)time(NULL));

clrscr();
printTitle();

/* Setup the Player Profile  */
setColor(14);
printf("\n  Enter your Player Name: ");
resetColor();
if (fgets(player.username, sizeof(player.username), stdin)) {
    if (strchr(player.username, '\n') == NULL) flushInput();
    player.username[strcspn(player.username, "\r\n")] = '\0';
}
if (player.username[0] == '\0') strcpy(player.username, "Guest");

player.totalSessionScore = 0;
player.puzzlesCompleted = 0;

/* ================================================================== */

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
    if (n < 4)     n = 4;
    if (n > MAX_W) n = MAX_W;
    pickFromBank(n);
}

generatePuzzle();

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
        case 1:  
            doGuessWord();
            showSol = 0;
            printf("\n  Press Enter to continue...");
            flushInput();
            break;

        case 2:  
            showSol = 1;
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
            generatePuzzle();
            showSol = 0;
            setColor(11);
            printf("\n  Same words, fresh grid generated!\n");
            resetColor();
            printf("  Press Enter to continue...");
            flushInput();
            break;

        case 5: {  
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

clrscr();
printTitle();
setColor(11);
printf("\n  Thanks for playing Word Search!\n");

/*OUTPUT: Display the Struct Data at Game Over*/
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