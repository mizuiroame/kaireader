#include "netlist.h"

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "doc.h"
#include "util.h"

#define EXT_MAX 8  // ".epub", ".cbz": a longer "extension" is part of the name.

static int byKindThenName(const void *a, const void *b)
{
    const DavEntry *x = a, *y = b;
    if (x->is_dir != y->is_dir)
        return x->is_dir ? -1 : 1;
    int c = strcasecmp(x->name, y->name);
    return c ? c : strcmp(x->name, y->name);
}

size_t netListBooks(DavEntry *entries, size_t count)
{
    size_t kept = 0;
    for (size_t i = 0; i < count; i++) {
        const DavEntry *e = &entries[i];
        if (e->name[0] == '.' || !e->name[0] || (!e->is_dir && !docKnownName(e->name)))
            continue;
        if (kept != i)
            entries[kept] = *e;
        kept++;
    }
    if (kept > 1)
        qsort(entries, kept, sizeof(*entries), byKindThenName);
    return kept;
}

// Latin-1's letters (U+00C0 to U+00FF) without accents; "" for its signs (as KaiVideo's downloads).
static const char *const LATIN1[64] = {
    "A", "A", "A", "A", "A", "A", "AE", "C", "E", "E", "E", "E", "I", "I", "I", "I",
    "D", "N", "O", "O", "O", "O", "O", "",  "O", "U", "U", "U", "U", "Y", "Th", "ss",
    "a", "a", "a", "a", "a", "a", "ae", "c", "e", "e", "e", "e", "i", "i", "i", "i",
    "d", "n", "o", "o", "o", "o", "o", "",  "o", "u", "u", "u", "u", "y", "th", "y",
};

// What a character becomes: " " a space (folded with others), NULL left out.
static const char *plainOf(unsigned c, char ascii[2])
{
    if (c < 0x80) {
        if (c < 0x20 || c == 0x7F || strchr("\\/*?\"<>|", (int)c))
            return " ";
        if (c == ':')
            return " - ";
        ascii[0] = (char)c;
        ascii[1] = '\0';
        return ascii;
    }
    if (c >= 0xC0 && c <= 0xFF)
        return LATIN1[c - 0xC0][0] ? LATIN1[c - 0xC0] : NULL;
    switch (c) {
    case 0x152: return "OE";
    case 0x153: return "oe";
    case 0x160: return "S";
    case 0x161: return "s";
    case 0x17D: return "Z";
    case 0x17E: return "z";
    case 0x2018: case 0x2019: return "'";  // Curly quotes.
    case 0x201C: case 0x201D: return "'";
    case 0x2013: case 0x2014: return "-";  // Dashes.
    case 0x2026: return "...";
    case 0xA0: return " ";
    default: return NULL;
    }
}

// text as plain ASCII into out; true if something had to be left out.
static bool plainText(const char *text, size_t length, char *out, size_t size)
{
    size_t n = 0;
    bool space = false, lost = false;
    const char *p = text, *end = text + length;
    while (p < end && *p) {
        char ascii[2];
        const char *r = plainOf(utilNextCodePoint(&p), ascii);
        if (!r) {
            lost = true;
            continue;
        }
        for (; *r; r++) {
            if (*r == ' ') {
                space = n > 0;
            } else if (n == 0 && *r == '.') {
                // A leading dot would hide it.
            } else if (n + (space ? 2 : 1) < size) {
                if (space)
                    out[n++] = ' ';
                space = false;
                out[n++] = *r;
            } else {
                lost = true;
            }
        }
    }
    while (n > 0 && (out[n - 1] == '.' || out[n - 1] == ' '))
        n--;
    out[n] = '\0';
    return lost;
}

static uint32_t hashOf(const char *s)
{
    uint32_t h = 2166136261u;  // FNV-1a.
    for (; *s; s++)
        h = (h ^ (unsigned char)*s) * 16777619u;
    return h;
}

void netPlainName(const char *name, bool is_dir, char *out, size_t size)
{
    if (size == 0)
        return;
    const char *dot = is_dir ? NULL : strrchr(name, '.');
    char ext[EXT_MAX + 2] = "";
    if (dot && dot != name && strlen(dot) <= EXT_MAX) {
        size_t n = 0;
        for (const char *c = dot; *c && n + 1 < sizeof(ext); c++)
            ext[n++] = (char)(c == dot ? '.' : isalnum((unsigned char)*c) ? tolower((unsigned char)*c) : '_');
        ext[n] = '\0';
    } else {
        dot = NULL;
    }
    size_t stem_len = dot ? (size_t)(dot - name) : strlen(name);
    char stem[256], code[16] = "";
    bool lost = plainText(name, stem_len, stem, sizeof(stem));
    if (lost || !stem[0])
        snprintf(code, sizeof(code), "%s~%06x", stem[0] ? " " : "", (unsigned)(hashOf(name) & 0xFFFFFF));
    // The stem cut to leave room for the code and the extension.
    long keep = (long)size - 1 - (long)strlen(ext) - (long)strlen(code);
    keep = keep < 0 ? 0 : keep;
    if ((long)strlen(stem) > keep) {
        stem[keep] = '\0';
        for (size_t n = strlen(stem); n > 0 && (stem[n - 1] == ' ' || stem[n - 1] == '.'); n--)
            stem[n - 1] = '\0';
    }
    snprintf(out, size, "%s%s%s", stem, code, ext);
}

bool netLocalPath(const char *books_folder, const char *location, char *out, size_t size)
{
    if (!davIsLocation(location))
        return false;
    const char *rest = location + strlen(DAV_PREFIX);
    int n = snprintf(out, size, "%s/Network", books_folder);
    if (n < 0 || (size_t)n >= size || !*rest)
        return false;
    while (*rest) {
        const char *slash = strchr(rest, '/');
        size_t len = slash ? (size_t)(slash - rest) : strlen(rest);
        if (len > 0) {
            char part[DAV_NAME_MAX], plain[DAV_NAME_MAX];
            if (len >= sizeof(part))
                return false;
            memcpy(part, rest, len);
            part[len] = '\0';
            if (!strcmp(part, "..") || !strcmp(part, "."))
                return false;
            netPlainName(part, slash != NULL, plain, sizeof(plain));
            int m = snprintf(out + n, size - (size_t)n, "/%s", plain);
            if (m < 0 || (size_t)m >= size - (size_t)n)
                return false;
            n += m;
        }
        rest += len + (slash ? 1 : 0);
    }
    return true;
}

bool netParent(const char *location, char *out, size_t size)
{
    size_t top = strlen(DAV_PREFIX);
    if (!davIsLocation(location) || strlen(location) <= top)
        return false;
    const char *slash = strrchr(location, '/');
    size_t len = (size_t)(slash - location);
    if (len < top)
        len = top;  // "dav:/A" -> "dav:/".
    if (len >= size)
        return false;
    memcpy(out, location, len);
    out[len] = '\0';
    if (len < top)
        snprintf(out, size, "%s", DAV_ROOT);
    return true;
}

bool netChild(const char *location, const char *name, char *out, size_t size)
{
    size_t len = strlen(location);
    bool slash = len > 0 && location[len - 1] == '/';
    int n = snprintf(out, size, "%s%s%s", location, slash ? "" : "/", name);
    return n > 0 && (size_t)n < size;
}

const char *netLastName(const char *location)
{
    if (!davIsLocation(location) || strlen(location) <= strlen(DAV_PREFIX))
        return "";
    const char *slash = strrchr(location, '/');
    return slash ? slash + 1 : location;
}
