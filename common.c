/* =====================================================================
 * common.c
 * ---------------------------------------------------------------------
 * Implements the small string-handling utilities declared in common.h.
 * Kept in one place so fileio.c (reading files) and ui.c (reading
 * keyboard input) don't each reinvent the same trimming/casing logic.
 * =====================================================================
 */
#include "common.h"

/* Strips a trailing '\n'/'\r' (left over from fgets) plus any other
 * leading/trailing whitespace, in place. Uses pointer arithmetic and
 * memmove to shift the string left when leading spaces are removed -
 * a classic pointer/array manipulation exercise. */
void trimString(char *str) {
    size_t len = strlen(str);

    /* trim trailing whitespace / newline characters */
    while (len > 0 &&
           (str[len - 1] == '\n' || str[len - 1] == '\r' ||
            isspace((unsigned char)str[len - 1]))) {
        str[--len] = '\0';
    }

    /* find first non-space character */
    size_t start = 0;
    while (str[start] != '\0' && isspace((unsigned char)str[start])) {
        start++;
    }

    /* shift the remaining text (including the null terminator) left */
    if (start > 0) {
        memmove(str, str + start, strlen(str + start) + 1);
    }
}

/* In-place uppercase conversion - walks the string via array indexing. */
void toUpperString(char *str) {
    for (int i = 0; str[i] != '\0'; i++) {
        str[i] = (char)toupper((unsigned char)str[i]);
    }
}

/* Case-insensitive comparison without relying on non-standard
 * strcasecmp (POSIX only, not guaranteed on every Windows toolchain),
 * so the whole project stays portable with nothing but standard C. */
int caseInsensitiveEquals(const char *a, const char *b) {
    while (*a != '\0' && *b != '\0') {
        if (toupper((unsigned char)*a) != toupper((unsigned char)*b)) {
            return 0;
        }
        a++;
        b++;
    }
    return *a == '\0' && *b == '\0';
}
