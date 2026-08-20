/* =====================================================================
 * ui.h
 * ---------------------------------------------------------------------
 * Everything about talking to the terminal: cross-platform screen
 * clearing/sleeping, ANSI colour helpers, robust keyboard input,
 * the coloured/animated puzzle display, and the function-pointer
 * driven menu system.
 * =====================================================================
 */
#ifndef UI_H
#define UI_H

#include "common.h"

/* --- terminal control (Windows vs Linux/Mac handled in ui.c) --- */
void clearScreen(void);
void sleepMs(int ms);
void enableAnsiSupport(void);

/* --- colour helpers (respect AppState.colorEnabled) --- */
void setColor(const AppState *state, const char *code);
void resetColor(const AppState *state);

/* --- robust keyboard input (no scanf pitfalls) --- */
void readLine(char *buffer, int size);
int  readIntInRange(const char *prompt, int min, int max);
void pauseForUser(void);

/* --- coloured status messages --- */
void printHeader(const AppState *state, const char *title);
void printError(const AppState *state, const char *msg);
void printInfo(const AppState *state, const char *msg);
void printSuccess(const AppState *state, const char *msg);

/* --- puzzle display --- */
void printGrid(const AppState *state, const WordEntry *flashEntry, int showAllSolution);
void printWordList(const AppState *state, int showFoundStatus);
void flashFoundWord(const AppState *state, const WordEntry *entry, const char *traced);

/* --- animation & menus --- */
void typeText(const char *str);              /* recursive typewriter effect */
void loadingBar(int progress, int total);      /* recursive progress bar      */
void showBanner(const AppState *state);
int  displayMenuAndGetChoice(const char *title, const MenuItem *items, int count,
                              const AppState *state);

#endif /* UI_H */
