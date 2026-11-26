#include "textbuf.h"

#include <stdlib.h>
#include <string.h>

void textBufInit(TextBuf *b, size_t max)
{
    memset(b, 0, sizeof(*b));
    b->max = max;
}

void textBufAdd(TextBuf *b, const char *text, size_t n)
{
    if (b->failed || b->full || !n)
        return;
    if (n > b->max - b->size) {  // Cut to what fits, not inside a UTF-8 sequence.
        size_t fit = b->max - b->size;
        while (fit > 0 && ((unsigned char)text[fit] & 0xC0) == 0x80)
            fit--;
        n = fit;
        b->full = true;
        if (!n)
            return;
    }
    if (b->size + n + 1 > b->cap) {
        size_t cap = b->cap ? b->cap : 256;
        while (cap < b->size + n + 1)
            cap *= 2;
        char *grown = realloc(b->data, cap);
        if (!grown) {
            b->failed = true;
            return;
        }
        b->data = grown;
        b->cap = cap;
    }
    memcpy(b->data + b->size, text, n);
    b->size += n;
    b->data[b->size] = '\0';
}

void textBufStr(TextBuf *b, const char *text)
{
    if (text)
        textBufAdd(b, text, strlen(text));
}

void textBufEscaped(TextBuf *b, const char *text, size_t n)
{
    size_t from = 0;
    for (size_t i = 0; i < n; i++) {
        unsigned char c = (unsigned char)text[i];
        bool control = c < 0x20 && c != 0x09 && c != 0x0A && c != 0x0D;  // Never valid in XML: left out.
        const char *entity = c == '&' ? "&amp;" : c == '<' ? "&lt;" : c == '>' ? "&gt;" : c == '"' ? "&quot;"
                             : control ? "" : NULL;
        if (!entity)
            continue;
        textBufAdd(b, text + from, i - from);
        textBufStr(b, entity);
        from = i + 1;
    }
    textBufAdd(b, text + from, n - from);
}

char *textBufTake(TextBuf *b, size_t *size)
{
    char *out = NULL;
    if (!b->failed)
        out = b->data ? b->data : calloc(1, 1);
    else
        free(b->data);
    if (size)
        *size = out ? b->size : 0;
    size_t max = b->max;
    textBufInit(b, max);
    return out;
}

void textBufFree(TextBuf *b)
{
    free(b->data);
    size_t max = b->max;
    textBufInit(b, max);
}
