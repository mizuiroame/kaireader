// A growing string with a limit, for the HTML and XML the news writes
// (feeds.c, article.c). Past its limit it stops taking text (cut at a whole
// character) and says so; out of memory, it says that too. Pure.
#pragma once

#include <stdbool.h>
#include <stddef.h>

typedef struct {
    char *data;  // NUL-terminated once anything was added; NULL before.
    size_t size, cap, max;
    bool full;    // Something didn't fit under max (and was cut).
    bool failed;  // Memory ran out.
} TextBuf;

void textBufInit(TextBuf *b, size_t max);
void textBufAdd(TextBuf *b, const char *text, size_t n);
void textBufStr(TextBuf *b, const char *text);
// text with &, <, > and " written as XML entities (control characters XML can't hold left out).
void textBufEscaped(TextBuf *b, const char *text, size_t n);
// Its text (malloc'd; "" if nothing was added), its size in *size if not NULL; the buffer is left
// empty. NULL if memory ran out.
char *textBufTake(TextBuf *b, size_t *size);
void textBufFree(TextBuf *b);
