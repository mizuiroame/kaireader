#include "feeds.h"

#include <mupdf/fitz.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "opds.h"
#include "textbuf.h"

#define ITEMS_GROWTH 32
#define ITEMS_READ_MAX 500   // Articles read from one feed at most.
#define OUTLINES_MAX 500
#define DEPTH_MAX 48         // Elements in elements written out; deeper ones give their text.
#define CACHE_MAX (8u << 20) // The feed kept on the card, at most.

static void quiet(void *user, const char *message)
{
    (void)user;
    (void)message;
}

// --- Walking MuPDF's tree ---

// A tag without its namespace prefix ("content:encoded" -> "encoded"); NULL for text.
static const char *localName(fz_xml *node)
{
    const char *tag = fz_xml_tag(node);
    if (!tag)
        return NULL;
    const char *colon = strrchr(tag, ':');
    return colon ? colon + 1 : tag;
}

static bool tagIs(fz_xml *node, const char *name)
{
    const char *tag = node ? localName(node) : NULL;
    return tag && !strcasecmp(tag, name);
}

static fz_xml *child(fz_xml *node, const char *name)
{
    for (fz_xml *c = node ? fz_xml_down(node) : NULL; c; c = fz_xml_next(c))
        if (tagIs(c, name))
            return c;
    return NULL;
}

// The node after n in document order, staying under top (NULL: the whole document).
static fz_xml *nextNode(fz_xml *n, fz_xml *top)
{
    fz_xml *down = fz_xml_down(n);
    if (down)
        return down;
    while (n && n != top) {
        fz_xml *next = fz_xml_next(n);
        if (next)
            return next;
        n = fz_xml_up(n);
    }
    return NULL;
}

// The first element called name at or after n (in document order).
static fz_xml *findDeep(fz_xml *n, const char *name)
{
    for (; n; n = nextNode(n, NULL))
        if (tagIs(n, name))
            return n;
    return NULL;
}

// All the text under node, as it is.
static void deepText(fz_xml *node, TextBuf *b)
{
    for (fz_xml *n = node ? fz_xml_down(node) : NULL; n; n = nextNode(n, node)) {
        const char *t = fz_xml_text(n);
        if (t)
            textBufStr(b, t);
    }
}

static void plainOf(fz_xml *node, char *out, size_t size)
{
    TextBuf b;
    textBufInit(&b, FEED_HTML_MAX);
    deepText(node, &b);
    char *text = textBufTake(&b, NULL);
    feedsPlainText(text ? text : "", out, size);
    free(text);
}

static bool hasElements(fz_xml *node)
{
    for (fz_xml *c = fz_xml_down(node); c; c = fz_xml_next(c))
        if (fz_xml_tag(c))
            return true;
    return false;
}

static bool voidTag(const char *name)
{
    return !strcasecmp(name, "br") || !strcasecmp(name, "hr") || !strcasecmp(name, "img");
}

// The elements under node written out as HTML again (an Atom xhtml content), with the
// attributes an article needs; text escaped.
static void serialize(fz_xml *node, TextBuf *b, int depth)
{
    static const char *const ATTS[] = {"href", "src", "alt", "title"};
    for (fz_xml *c = fz_xml_down(node); c; c = fz_xml_next(c)) {
        const char *text = fz_xml_text(c);
        if (text) {
            textBufEscaped(b, text, strlen(text));
            continue;
        }
        const char *name = localName(c);
        if (!name || !name[0])
            continue;
        if (depth >= DEPTH_MAX) {  // Too deep: its words only.
            TextBuf t;
            textBufInit(&t, FEED_HTML_MAX);
            deepText(c, &t);
            char *words = textBufTake(&t, NULL);
            if (words)
                textBufEscaped(b, words, strlen(words));
            free(words);
            continue;
        }
        textBufStr(b, "<");
        textBufStr(b, name);
        for (size_t i = 0; i < sizeof(ATTS) / sizeof(ATTS[0]); i++) {
            const char *v = fz_xml_att(c, ATTS[i]);
            if (!v)
                continue;
            textBufStr(b, " ");
            textBufStr(b, ATTS[i]);
            textBufStr(b, "=\"");
            textBufEscaped(b, v, strlen(v));
            textBufStr(b, "\"");
        }
        if (voidTag(name)) {
            textBufStr(b, "/>");
            continue;
        }
        textBufStr(b, ">");
        serialize(c, b, depth + 1);
        textBufStr(b, "</");
        textBufStr(b, name);
        textBufStr(b, ">");
    }
}

// An article's HTML from its content element: escaped HTML (or CDATA) as it is, XHTML written
// out again, Atom's plain text escaped. NULL if there's none.
static char *htmlOf(fz_xml *node, bool atom)
{
    if (!node)
        return NULL;
    const char *type = fz_xml_att(node, "type");
    TextBuf b;
    textBufInit(&b, FEED_HTML_MAX);
    if ((type && strstr(type, "xhtml")) || hasElements(node)) {
        serialize(node, &b, 0);
    } else if (atom && (!type || !strcasecmp(type, "text"))) {
        TextBuf t;
        textBufInit(&t, FEED_HTML_MAX);
        deepText(node, &t);
        char *words = textBufTake(&t, NULL);
        if (words)
            textBufEscaped(&b, words, strlen(words));
        free(words);
    } else {
        deepText(node, &b);
    }
    size_t size;
    char *html = textBufTake(&b, &size);
    size_t lead = html ? strspn(html, " \t\r\n") : 0;
    if (html && lead == size) {
        free(html);
        return NULL;
    }
    return html;
}

// --- Text ---

static size_t putUtf8(unsigned cp, char *out)
{
    if (cp < 0x80) {
        out[0] = (char)cp;
        return 1;
    }
    if (cp < 0x800) {
        out[0] = (char)(0xC0 | (cp >> 6));
        out[1] = (char)(0x80 | (cp & 0x3F));
        return 2;
    }
    if (cp < 0x10000) {
        out[0] = (char)(0xE0 | (cp >> 12));
        out[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
        out[2] = (char)(0x80 | (cp & 0x3F));
        return 3;
    }
    out[0] = (char)(0xF0 | (cp >> 18));
    out[1] = (char)(0x80 | ((cp >> 12) & 0x3F));
    out[2] = (char)(0x80 | ((cp >> 6) & 0x3F));
    out[3] = (char)(0x80 | (cp & 0x3F));
    return 4;
}

// HTML's named entities that feeds' titles use.
static const struct {
    const char *name;
    unsigned cp;
} ENTITIES[] = {
    {"amp", '&'},      {"lt", '<'},       {"gt", '>'},       {"quot", '"'},     {"apos", '\''},
    {"nbsp", 0xA0},    {"hellip", 0x2026}, {"mdash", 0x2014}, {"ndash", 0x2013}, {"lsquo", 0x2018},
    {"rsquo", 0x2019}, {"ldquo", 0x201C}, {"rdquo", 0x201D}, {"sbquo", 0x201A}, {"bdquo", 0x201E},
    {"laquo", 0xAB},   {"raquo", 0xBB},   {"copy", 0xA9},    {"reg", 0xAE},     {"trade", 0x2122},
    {"euro", 0x20AC},  {"pound", 0xA3},   {"yen", 0xA5},     {"cent", 0xA2},    {"deg", 0xB0},
    {"middot", 0xB7},  {"bull", 0x2022},  {"times", 0xD7},   {"iexcl", 0xA1},   {"iquest", 0xBF},
    {"ordf", 0xAA},    {"ordm", 0xBA},    {"szlig", 0xDF},   {"aacute", 0xE1},  {"eacute", 0xE9},
    {"iacute", 0xED},  {"oacute", 0xF3},  {"uacute", 0xFA},  {"Aacute", 0xC1},  {"Eacute", 0xC9},
    {"Iacute", 0xCD},  {"Oacute", 0xD3},  {"Uacute", 0xDA},  {"ntilde", 0xF1},  {"Ntilde", 0xD1},
    {"agrave", 0xE0},  {"egrave", 0xE8},  {"igrave", 0xEC},  {"ograve", 0xF2},  {"ugrave", 0xF9},
    {"Agrave", 0xC0},  {"Egrave", 0xC8},  {"acirc", 0xE2},   {"ecirc", 0xEA},   {"icirc", 0xEE},
    {"ocirc", 0xF4},   {"ucirc", 0xFB},   {"auml", 0xE4},    {"euml", 0xEB},    {"iuml", 0xEF},
    {"ouml", 0xF6},    {"uuml", 0xFC},    {"Auml", 0xC4},    {"Ouml", 0xD6},    {"Uuml", 0xDC},
    {"ccedil", 0xE7},  {"Ccedil", 0xC7},  {"atilde", 0xE3},  {"otilde", 0xF5},  {"aring", 0xE5},
    {"oslash", 0xF8},  {"aelig", 0xE6},   {"oelig", 0x153},  {"thinsp", 0x2009}, {"ensp", 0x2002},
    {"emsp", 0x2003},  {"shy", 0xAD},     {"zwnj", 0x200C},  {"zwj", 0x200D},
};

// An entity at s ("&amp;", "&#233;", "&#xE9;"): its character in *cp, how many bytes it takes; 0 if it isn't one.
static size_t entityAt(const char *s, unsigned *cp)
{
    if (s[0] != '&')
        return 0;
    if (s[1] == '#') {
        bool hex = s[2] == 'x' || s[2] == 'X';
        const char *d = s + (hex ? 3 : 2);
        unsigned v = 0;
        int digits = 0;
        for (; digits < 8; digits++, d++) {
            char c = *d;
            int x = c >= '0' && c <= '9' ? c - '0' : hex && c >= 'a' && c <= 'f' ? c - 'a' + 10
                    : hex && c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1;
            if (x < 0)
                break;
            v = v * (hex ? 16u : 10u) + (unsigned)x;
        }
        if (!digits || *d != ';' || v == 0 || v > 0x10FFFF || (v >= 0xD800 && v <= 0xDFFF))
            return 0;
        *cp = v;
        return (size_t)(d - s) + 1;
    }
    size_t n = 0;
    while (n < 10 && ((s[1 + n] >= 'a' && s[1 + n] <= 'z') || (s[1 + n] >= 'A' && s[1 + n] <= 'Z')))
        n++;
    if (!n || s[1 + n] != ';')
        return 0;
    for (size_t i = 0; i < sizeof(ENTITIES) / sizeof(ENTITIES[0]); i++)
        if (strlen(ENTITIES[i].name) == n && !strncmp(ENTITIES[i].name, s + 1, n)) {
            *cp = ENTITIES[i].cp;
            return n + 2;
        }
    return 0;
}

// How much of s's first n bytes is whole UTF-8 characters.
static size_t wholeChars(const char *s, size_t n)
{
    size_t j = n;
    while (j > 0 && ((unsigned char)s[j - 1] & 0xC0) == 0x80)
        j--;
    if (j == 0)
        return 0;  // Nothing, or only continuation bytes: nothing whole.
    unsigned char lead = (unsigned char)s[j - 1];
    size_t need = lead >= 0xF0 ? 4 : lead >= 0xE0 ? 3 : lead >= 0xC0 ? 2 : 1;
    return n - (j - 1) < need ? j - 1 : n;
}

static bool isTagStart(const char *s)
{
    char c = s[1];
    return s[0] == '<' && ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '/' || c == '!' || c == '?');
}

void feedsPlainText(const char *in, char *out, size_t size)
{
    if (!size)
        return;
    size_t o = 0;
    bool space = false, closes = true;
    for (const char *s = in ? in : ""; *s && o + 1 < size;) {
        if (closes && isTagStart(s)) {
            const char *end = strchr(s, '>');
            if (end) {
                s = end + 1;
                continue;
            }
            closes = false;  // No '>' after here: no tag either (never looked for again).
        }
        char bytes[4];
        size_t n = 1;
        unsigned cp;
        size_t taken = entityAt(s, &cp);
        if (taken) {
            if (cp == 0xA0 || cp < 0x20 || cp == 0x7F)
                cp = ' ';
            n = putUtf8(cp, bytes);
            s += taken;
        } else {
            bytes[0] = *s++;
        }
        if (n == 1 && ((unsigned char)bytes[0] <= ' ' || bytes[0] == 0x7F)) {
            space = o > 0;
            continue;
        }
        if (space) {
            if (o + 2 >= size)
                break;
            out[o++] = ' ';
            space = false;
        }
        if (o + n >= size)
            break;
        memcpy(out + o, bytes, n);
        o += n;
    }
    o = wholeChars(out, o);
    out[o] = '\0';
}

// --- Dates ---

static int64_t daysFromCivil(int64_t y, int m, int d)
{
    y -= m <= 2;
    int64_t era = (y >= 0 ? y : y - 399) / 400;
    int64_t yoe = y - era * 400;
    int64_t doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    int64_t doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + doe - 719468;
}

static int64_t epochOf(int y, int mo, int d, int h, int mi, int s, int offset_s)
{
    static const int DAYS[12] = {31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (y < 1970 || y > 2200 || mo < 1 || mo > 12 || d < 1 || d > DAYS[mo - 1] || h < 0 || h > 24 || mi < 0 ||
        mi > 59 || s < 0 || s > 60)
        return 0;
    int64_t t = daysFromCivil(y, mo, d) * 86400 + h * 3600 + mi * 60 + s - offset_s;
    return t > 0 ? t : 0;
}

// Up to max digits at *s as a number (advancing); -1 if there's none.
static int digitsAt(const char **s, int max)
{
    int v = 0, n = 0;
    while (n < max && **s >= '0' && **s <= '9') {
        v = v * 10 + (**s - '0');
        (*s)++;
        n++;
    }
    return n ? v : -1;
}

static void skipSpaces(const char **s)
{
    while (**s == ' ' || **s == '\t' || **s == '\r' || **s == '\n')
        (*s)++;
}

// "+02:00", "-0500", "+02", "Z", or a name; seconds east of UTC. False if it's none of them.
static bool zoneAt(const char *s, int *offset)
{
    static const struct {
        const char *name;
        int hours;
    } NAMES[] = {{"GMT", 0}, {"UT", 0}, {"UTC", 0}, {"Z", 0}, {"EST", -5}, {"EDT", -4}, {"CST", -6},
                 {"CDT", -5}, {"MST", -7}, {"MDT", -6}, {"PST", -8}, {"PDT", -7}, {"CET", 1}, {"CEST", 2}};
    *offset = 0;
    skipSpaces(&s);
    if (*s == '+' || *s == '-') {
        int sign = *s == '-' ? -1 : 1;
        s++;
        int h = digitsAt(&s, 2), m = 0;
        if (h < 0)
            return false;
        if (*s == ':')
            s++;
        if (*s >= '0' && *s <= '9')
            m = digitsAt(&s, 2);
        if (h > 14 || m < 0 || m > 59)
            return false;
        *offset = sign * (h * 3600 + m * 60);
        return true;
    }
    for (size_t i = 0; i < sizeof(NAMES) / sizeof(NAMES[0]); i++) {
        size_t n = strlen(NAMES[i].name);
        if (!strncasecmp(s, NAMES[i].name, n) && !((s[n] >= 'a' && s[n] <= 'z') || (s[n] >= 'A' && s[n] <= 'Z'))) {
            *offset = NAMES[i].hours * 3600;
            return true;
        }
    }
    return *s == '\0';  // None said: UTC.
}

// "2003-06-10T04:00:00.5+02:00", "2003-06-10 04:00", "2003-06-10".
static int64_t rfc3339(const char *s)
{
    int y = digitsAt(&s, 4);
    if (y < 0 || *s++ != '-')
        return 0;
    int mo = digitsAt(&s, 2);
    if (mo < 0 || *s++ != '-')
        return 0;
    int d = digitsAt(&s, 2), h = 0, mi = 0, sec = 0, offset = 0;
    if (d < 0)
        return 0;
    if (*s == 'T' || *s == 't' || *s == ' ') {
        s++;
        h = digitsAt(&s, 2);
        if (h < 0 || *s++ != ':')
            return 0;
        mi = digitsAt(&s, 2);
        if (mi < 0)
            return 0;
        if (*s == ':') {
            s++;
            sec = digitsAt(&s, 2);
            if (sec < 0)
                return 0;
            if (*s == '.' || *s == ',') {
                s++;
                while (*s >= '0' && *s <= '9')
                    s++;
            }
        }
        if (!zoneAt(s, &offset))
            return 0;
    }
    return epochOf(y, mo, d, h, mi, sec, offset);
}

// "Tue, 10 Jun 2003 04:00:00 GMT", "10 June 2003 04:00 +0200", "Tue, 10 Jun 03 04:00:00 EST".
static int64_t rfc822(const char *s)
{
    static const char *const MONTHS[12] = {"jan", "feb", "mar", "apr", "may", "jun",
                                           "jul", "aug", "sep", "oct", "nov", "dec"};
    skipSpaces(&s);
    const char *comma = strchr(s, ',');
    if (comma && comma - s <= 12)  // Its day's name.
        s = comma + 1;
    skipSpaces(&s);
    int d = digitsAt(&s, 2);
    if (d < 0)
        return 0;
    skipSpaces(&s);
    if (*s == '-')
        s++;
    int mo = 0;
    for (int i = 0; i < 12; i++)
        if (!strncasecmp(s, MONTHS[i], 3))
            mo = i + 1;
    if (!mo)
        return 0;
    while ((*s >= 'a' && *s <= 'z') || (*s >= 'A' && *s <= 'Z') || *s == '.')
        s++;
    skipSpaces(&s);
    if (*s == '-')
        s++;
    const char *year_at = s;
    int y = digitsAt(&s, 4);
    if (y < 0)
        return 0;
    if (s - year_at <= 2)
        y += y < 70 ? 2000 : 1900;
    skipSpaces(&s);
    int h = 0, mi = 0, sec = 0, offset = 0;
    if (*s >= '0' && *s <= '9') {
        h = digitsAt(&s, 2);
        if (*s++ != ':')
            return 0;
        mi = digitsAt(&s, 2);
        if (mi < 0)
            return 0;
        if (*s == ':') {
            s++;
            sec = digitsAt(&s, 2);
            if (sec < 0)
                return 0;
        }
        if (!zoneAt(s, &offset))
            offset = 0;  // A zone not known: as UTC (a few hours off at worst).
    }
    return epochOf(y, mo, d, h, mi, sec, offset);
}

int64_t feedsParseDate(const char *text)
{
    if (!text)
        return 0;
    while (*text == ' ' || *text == '\t' || *text == '\r' || *text == '\n')
        text++;
    bool iso = text[0] >= '0' && text[0] <= '9' && text[1] >= '0' && text[1] <= '9' && text[2] >= '0' &&
               text[2] <= '9' && text[3] >= '0' && text[3] <= '9' && text[4] == '-';
    return iso ? rfc3339(text) : rfc822(text);
}

static void civilFromDays(int64_t z, int *y, int *m, int *d)
{
    z += 719468;
    int64_t era = (z >= 0 ? z : z - 146096) / 146097;
    int64_t doe = z - era * 146097;
    int64_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    int64_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    int64_t mp = (5 * doy + 2) / 153;
    *d = (int)(doy - (153 * mp + 2) / 5 + 1);
    *m = (int)(mp < 10 ? mp + 3 : mp - 9);
    *y = (int)(yoe + era * 400 + (*m <= 2));
}

static void isoTime(int64_t t, char *out, size_t size)
{
    int y, m, d;
    civilFromDays(t / 86400, &y, &m, &d);
    int64_t s = t % 86400;
    snprintf(out, size, "%04d-%02d-%02dT%02d:%02d:%02dZ", y, m, d, (int)(s / 3600), (int)(s / 60 % 60), (int)(s % 60));
}

void feedsDateText(int64_t date, char *out, size_t size)
{
    if (date <= 0) {
        if (size)
            out[0] = '\0';
        return;
    }
    int y, m, d;
    civilFromDays(date / 86400, &y, &m, &d);
    snprintf(out, size, "%04d-%02d-%02d", y, m, d);
}

void feedsDateTimeText(int64_t date, char *out, size_t size)
{
    char day[16];
    feedsDateText(date, day, sizeof(day));
    if (!day[0]) {
        if (size)
            out[0] = '\0';
        return;
    }
    int64_t s = date % 86400;
    snprintf(out, size, "%s %02d:%02d", day, (int)(s / 3600), (int)(s / 60 % 60));
}

// --- Articles ---

// needle in hay, ignoring ASCII case; NULL if it isn't there.
static const char *findCase(const char *hay, const char *needle)
{
    size_t n = strlen(needle);
    for (; *hay; hay++)
        if (!strncasecmp(hay, needle, n))
            return hay;
    return NULL;
}

static bool isImageType(const char *type)
{
    return type && !strncasecmp(type, "image/", 6);
}

static bool looksLikeImage(const char *url)
{
    static const char *const EXTS[] = {".jpg", ".jpeg", ".png", ".gif", ".webp"};
    size_t n = strcspn(url, "?#");
    for (size_t i = 0; i < sizeof(EXTS) / sizeof(EXTS[0]); i++) {
        size_t e = strlen(EXTS[i]);
        if (n >= e && !strncasecmp(url + n - e, EXTS[i], e))
            return true;
    }
    return false;
}

// How good a picture looks to be: its width when it says (pixels), else by what it is.
#define SCORE_THUMBNAIL 100  // A thumbnail that doesn't say its size.
#define SCORE_INLINE 600     // The first picture of the article's text, without a size.
#define SCORE_FULL 800       // A media:content, an enclosure, itunes:image: the picture itself.
#define WIDTH_MAX 20000

// An attribute of a tag's text (from "<img" to its ">"), as written, into out. False if it hasn't it.
static bool tagAttr(const char *tag, const char *end, const char *name, char *out, size_t size)
{
    size_t len = strlen(name);
    for (const char *a = tag + 4; a + len + 1 < end; a++) {
        if (strncasecmp(a, name, len) || a[len] != '=' || (a[-1] != ' ' && a[-1] != '\t' && a[-1] != '\n'))
            continue;
        a += len + 1;
        char quote = *a == '"' || *a == '\'' ? *a++ : 0;
        size_t n = quote ? strcspn(a, quote == '"' ? "\">" : "'>") : strcspn(a, " \t\r\n>");
        if (!n || n >= size || (size_t)(end - a) < n)
            return false;
        memcpy(out, a, n);
        out[n] = '\0';
        return true;
    }
    return false;
}

static int widthText(const char *text)
{
    int v = text ? atoi(text) : 0;
    return v > 0 && v < WIDTH_MAX ? v : 0;
}

// The largest of a srcset ("a.jpg 320w, b.jpg 1024w") into out, its width; 0 if it says none.
static int largestSrcset(const char *set, char *out, size_t size)
{
    int best = 0;
    for (const char *s = set; *s;) {
        s += strspn(s, " \t\r\n,");
        size_t n = strcspn(s, " \t\r\n,");
        if (!n)
            break;
        const char *url = s;
        s += n;
        s += strspn(s, " \t\r\n");
        int w = 0;
        if (*s >= '0' && *s <= '9') {
            w = widthText(s);
            s += strspn(s, "0123456789.");
            if (*s != 'w')
                w = 0;  // "2x": no width.
        }
        s += strcspn(s, ",");
        if (w > best && n < size) {
            memcpy(out, url, n);
            out[n] = '\0';
            best = w;
        }
    }
    return best;
}

// The first picture of an article's HTML (its srcset's largest if it has one), into out (as
// written), and how good it looks (its width when it says). 0 if it has none.
static int firstImage(const char *html, char *out, size_t size)
{
    for (const char *s = html; s && (s = findCase(s, "<img")); s += 4) {
        const char *end = strchr(s, '>');
        if (!end)
            return 0;
        char src[FEED_URL_MAX], attr[2048], large[FEED_URL_MAX];
        if (!tagAttr(s, end, "src", src, sizeof(src)) || !strncasecmp(src, "data:", 5))
            continue;
        int width = tagAttr(s, end, "width", attr, sizeof(attr)) ? widthText(attr) : 0;
        int set = tagAttr(s, end, "srcset", attr, sizeof(attr)) ? largestSrcset(attr, large, sizeof(large)) : 0;
        if (set > width && strncasecmp(large, "data:", 5)) {
            snprintf(out, size, "%s", large);
            return set;
        }
        snprintf(out, size, "%s", src);
        return width ? width : SCORE_INLINE;
    }
    return 0;
}

// A picture for the list, kept if it looks better than the one found before.
static void offerImage(FeedItem *it, const char *base, const char *url, int score)
{
    char resolved[FEED_URL_MAX];
    if (!url || !url[0] || score <= it->image_score || !opdsResolve(base, url, resolved, sizeof(resolved)))
        return;
    snprintf(it->image, sizeof(it->image), "%s", resolved);
    it->image_score = score;
}

// A picture element (enclosure, media:content, media:thumbnail, itunes:image, media:group's), if
// it's one, offered with how good it looks (the largest wins).
static void takeImage(FeedItem *it, fz_xml *n, const char *base)
{
    int width = widthText(fz_xml_att(n, "width"));
    if (tagIs(n, "enclosure") && isImageType(fz_xml_att(n, "type"))) {
        offerImage(it, base, fz_xml_att(n, "url"), SCORE_FULL);
    } else if (tagIs(n, "thumbnail")) {
        offerImage(it, base, fz_xml_att(n, "url"), width ? width : SCORE_THUMBNAIL);
    } else if (tagIs(n, "content") && fz_xml_att(n, "url")) {  // media:content (an Atom content has no url).
        const char *medium = fz_xml_att(n, "medium"), *u = fz_xml_att(n, "url");
        if ((medium && !strcasecmp(medium, "image")) || isImageType(fz_xml_att(n, "type")) ||
            (!medium && !fz_xml_att(n, "type") && looksLikeImage(u)))
            offerImage(it, base, u, width ? width : SCORE_FULL);
        for (fz_xml *c = fz_xml_down(n); c; c = fz_xml_next(c))  // Its own media:thumbnail.
            if (tagIs(c, "thumbnail"))
                takeImage(it, c, base);
    } else if (tagIs(n, "image") && fz_xml_att(n, "href")) {  // itunes:image.
        offerImage(it, base, fz_xml_att(n, "href"), SCORE_FULL);
    } else if (tagIs(n, "group")) {
        for (fz_xml *c = fz_xml_down(n); c; c = fz_xml_next(c))
            takeImage(it, c, base);
    }
}

// A picture's address asking for a larger one, where the site's addresses are known to say a size:
// the BBC's ichef (".../ws/240/..." -> 800), and WordPress's resized copies ("photo-150x150.jpg" ->
// the original "photo.jpg"). Others are left as they are.
static void largerImage(char *url, size_t size)
{
    static const char *const BBC[] = {"://ichef.bbci.co.uk/ace/ws/", "://ichef.bbci.co.uk/news/"};
    for (size_t i = 0; i < sizeof(BBC) / sizeof(BBC[0]); i++) {
        char *at = strstr(url, BBC[i]);
        if (!at)
            continue;
        char *digits = at + strlen(BBC[i]);
        size_t n = strspn(digits, "0123456789");
        if (n < 2 || n > 4 || digits[n] != '/' || atoi(digits) >= 800)
            return;
        char rest[FEED_URL_MAX];
        snprintf(rest, sizeof(rest), "%s", digits + n);
        if ((size_t)(digits - url) + 3 + strlen(rest) < size)
            snprintf(digits, size - (size_t)(digits - url), "800%s", rest);
        return;
    }
    size_t end = strcspn(url, "?#");  // "-150x150.jpg" at the path's end.
    char *dot = NULL;
    for (size_t i = end; i > 0; i--)
        if (url[i - 1] == '.') {
            dot = url + i - 1;
            break;
        } else if (url[i - 1] == '/') {
            break;
        }
    if (!dot)
        return;
    static const char *const EXTS[] = {".jpg", ".jpeg", ".png", ".gif", ".webp"};
    bool picture = false;
    for (size_t i = 0; i < sizeof(EXTS) / sizeof(EXTS[0]); i++)
        picture |= (size_t)(url + end - dot) == strlen(EXTS[i]) && !strncasecmp(dot, EXTS[i], strlen(EXTS[i]));
    char *x = dot;
    while (x > url && x[-1] >= '0' && x[-1] <= '9')
        x--;
    if (!picture || x == dot || x - url < 2 || x[-1] != 'x')
        return;
    char *w = x - 1;
    while (w > url && w[-1] >= '0' && w[-1] <= '9')
        w--;
    if (w == x - 1 || w - url < 1 || w[-1] != '-' || atoi(w) >= 600)
        return;  // Not "-WxH", or already large.
    memmove(w - 1, dot, strlen(dot) + 1);  // "-150x150" cut out.
}

// "jane@example.com (Jane Doe)" -> "Jane Doe".
static void authorName(char *author)
{
    char *open = strchr(author, '('), *close = open ? strrchr(open, ')') : NULL;
    if (!open || !close || !strchr(author, '@') || close - open < 2)
        return;
    size_t n = (size_t)(close - open - 1);
    memmove(author, open + 1, n);
    author[n] = '\0';
}

static void finishItem(Feed *f, FeedItem *it, const char *base)
{
    if (it->html) {  // Its text's first picture, if it looks better than the feed's own.
        char found[FEED_URL_MAX];
        int score = firstImage(it->html, found, sizeof(found));
        if (score)
            offerImage(it, base, found, score);
    }
    largerImage(it->image, sizeof(it->image));
    if (!it->id[0])
        snprintf(it->id, sizeof(it->id), "%s", it->link);
    if (!it->id[0]) {  // Nothing to know it by: its title and date.
        int n = snprintf(it->id, sizeof(it->id), "%s#%lld", it->title, (long long)it->date);
        if (n < 0)
            it->id[0] = '\0';
    }
    if (!it->title[0] && !it->html && !it->link[0]) {  // Empty: not an article.
        free(it->html);
        return;
    }
    if (f->count % ITEMS_GROWTH == 0) {
        FeedItem *grown = realloc(f->items, (size_t)(f->count + ITEMS_GROWTH) * sizeof(*grown));
        if (!grown) {
            free(it->html);
            return;
        }
        f->items = grown;
    }
    f->items[f->count++] = *it;
}

static void resolveInto(const char *base, const char *href, char *out, size_t size)
{
    if (!href || !href[0] || !opdsResolve(base, href, out, size))
        out[0] = '\0';
}

// An RSS 2.0 or RSS 1.0 item.
static void readRssItem(Feed *f, fz_xml *item, const char *base)
{
    FeedItem it;
    memset(&it, 0, sizeof(it));
    char text[FEED_URL_MAX];
    plainOf(child(item, "guid"), it.id, sizeof(it.id));
    if (!it.id[0]) {
        const char *about = fz_xml_att(item, "rdf:about");
        snprintf(it.id, sizeof(it.id), "%s", about ? about : "");
    }
    plainOf(child(item, "title"), it.title, sizeof(it.title));
    fz_xml *link = child(item, "link");
    plainOf(link, text, sizeof(text));
    if (!text[0] && link && fz_xml_att(link, "href"))  // An atom:link in an RSS item.
        snprintf(text, sizeof(text), "%s", fz_xml_att(link, "href"));
    if (!text[0] && !strncasecmp(it.id, "http", 4))  // A permalink guid.
        snprintf(text, sizeof(text), "%s", it.id);
    resolveInto(base, text, it.link, sizeof(it.link));
    plainOf(child(item, "creator"), it.author, sizeof(it.author));
    if (!it.author[0]) {
        plainOf(child(item, "author"), it.author, sizeof(it.author));
        authorName(it.author);
    }
    static const char *const DATES[] = {"pubDate", "date", "published", "updated", "issued", "modified"};
    for (size_t i = 0; i < sizeof(DATES) / sizeof(DATES[0]) && !it.date; i++) {
        char date[96];
        plainOf(child(item, DATES[i]), date, sizeof(date));
        it.date = feedsParseDate(date);
    }
    it.html = htmlOf(child(item, "encoded"), false);
    if (!it.html)
        it.html = htmlOf(child(item, "description"), false);
    for (fz_xml *c = fz_xml_down(item); c; c = fz_xml_next(c))
        takeImage(&it, c, base);
    finishItem(f, &it, base);
}

// An Atom entry's link to the article (rel alternate, or none), into out.
static void atomLink(fz_xml *node, const char *base, char *out, size_t size)
{
    out[0] = '\0';
    for (fz_xml *c = fz_xml_down(node); c; c = fz_xml_next(c)) {
        if (!tagIs(c, "link"))
            continue;
        const char *rel = fz_xml_att(c, "rel");
        if (!rel || !strcasecmp(rel, "alternate")) {
            resolveInto(base, fz_xml_att(c, "href"), out, size);
            if (out[0])
                return;
        }
    }
}

static void readAtomEntry(Feed *f, fz_xml *entry, const char *base)
{
    FeedItem it;
    memset(&it, 0, sizeof(it));
    plainOf(child(entry, "id"), it.id, sizeof(it.id));
    plainOf(child(entry, "title"), it.title, sizeof(it.title));
    atomLink(entry, base, it.link, sizeof(it.link));
    plainOf(child(child(entry, "author"), "name"), it.author, sizeof(it.author));
    static const char *const DATES[] = {"published", "updated", "issued", "modified", "date"};
    for (size_t i = 0; i < sizeof(DATES) / sizeof(DATES[0]) && !it.date; i++) {
        char date[96];
        plainOf(child(entry, DATES[i]), date, sizeof(date));
        it.date = feedsParseDate(date);
    }
    it.html = htmlOf(child(entry, "content"), true);
    if (!it.html)
        it.html = htmlOf(child(entry, "summary"), true);
    for (fz_xml *c = fz_xml_down(entry); c; c = fz_xml_next(c)) {
        const char *rel = tagIs(c, "link") ? fz_xml_att(c, "rel") : NULL;
        if (rel && !strcasecmp(rel, "enclosure") && isImageType(fz_xml_att(c, "type")))
            offerImage(&it, base, fz_xml_att(c, "href"), SCORE_FULL);
        else
            takeImage(&it, c, base);
    }
    finishItem(f, &it, base);
}

// The feed's element: rss, feed or RDF (the first, at the top or, from an HTML parse, anywhere).
static fz_xml *feedRoot(fz_xml *root, bool anywhere)
{
    for (fz_xml *n = root; n; n = anywhere ? nextNode(n, NULL) : fz_xml_next(n))
        if (tagIs(n, "rss") || tagIs(n, "feed") || tagIs(n, "RDF"))
            return n;
    return NULL;
}

static void readFeed(fz_xml *top, const char *base, Feed *out)
{
    if (tagIs(top, "feed")) {
        plainOf(child(top, "title"), out->title, sizeof(out->title));
        atomLink(top, base, out->link, sizeof(out->link));
        for (fz_xml *n = fz_xml_down(top); n && out->count < ITEMS_READ_MAX; n = fz_xml_next(n))
            if (tagIs(n, "entry"))
                readAtomEntry(out, n, base);
        return;
    }
    fz_xml *channel = child(top, "channel");
    char link[FEED_URL_MAX];
    plainOf(child(channel, "title"), out->title, sizeof(out->title));
    plainOf(child(channel, "link"), link, sizeof(link));
    resolveInto(base, link, out->link, sizeof(out->link));
    // RSS 2.0's items are in its channel; RSS 1.0's beside it.
    fz_xml *holder = tagIs(top, "rss") ? channel : top;
    for (fz_xml *n = holder ? fz_xml_down(holder) : NULL; n && out->count < ITEMS_READ_MAX; n = fz_xml_next(n))
        if (tagIs(n, "item"))
            readRssItem(out, n, base);
}

// Parses data as XML, or (html) with HTML5's rules. NULL if it can't be.
static fz_xml *parse(fz_context *ctx, const char *data, size_t size, bool html)
{
    fz_buffer *buf = NULL;
    fz_xml *doc = NULL;
    fz_var(buf);
    fz_var(doc);
    fz_try(ctx)
    {
        buf = fz_new_buffer_from_copied_data(ctx, (const unsigned char *)data, size);
        doc = html ? fz_parse_xml_from_html5(ctx, buf) : fz_parse_xml(ctx, buf, 1);  // Spaces between tags kept.
    }
    fz_always(ctx)
    {
        fz_drop_buffer(ctx, buf);
    }
    fz_catch(ctx)
    {
        doc = NULL;
    }
    return doc;
}

static fz_context *newContext(void)
{
    fz_context *ctx = fz_new_context(NULL, NULL, 16u << 20);
    if (ctx) {
        fz_set_warning_callback(ctx, quiet, NULL);
        fz_set_error_callback(ctx, quiet, NULL);
    }
    return ctx;
}

bool feedsParse(const char *data, size_t size, const char *base_url, Feed *out)
{
    memset(out, 0, sizeof(*out));
    if (!data || !size)
        return false;
    fz_context *ctx = newContext();
    if (!ctx)
        return false;
    fz_xml *doc = parse(ctx, data, size, false);
    fz_xml *top = doc ? feedRoot(fz_xml_root(doc), false) : NULL;
    if (!doc) {  // Not well-formed XML (an & left bare...): read as HTML reads it.
        doc = parse(ctx, data, size, true);
        top = doc ? feedRoot(fz_xml_root(doc), true) : NULL;
    }
    if (top)
        readFeed(top, base_url ? base_url : "", out);
    fz_drop_xml(ctx, doc);
    fz_drop_context(ctx);
    return top != NULL;
}

void feedsFree(Feed *f)
{
    for (int i = 0; i < f->count; i++)
        free(f->items[i].html);
    free(f->items);
    memset(f, 0, sizeof(*f));
}

// --- Discovery and OPML ---

static bool feedType(const char *type)
{
    return type && (strstr(type, "rss+xml") || strstr(type, "atom+xml") || strstr(type, "rdf+xml") ||
                    strstr(type, "x-rss"));
}

bool feedsDiscover(const char *html, size_t size, const char *base_url, char *out, size_t out_size)
{
    if (out_size)
        out[0] = '\0';
    if (!html || !size)
        return false;
    fz_context *ctx = newContext();
    if (!ctx)
        return false;
    fz_xml *doc = parse(ctx, html, size, true);
    bool found = false;
    for (fz_xml *n = doc ? findDeep(fz_xml_root(doc), "link") : NULL; n && !found; n = findDeep(nextNode(n, NULL), "link")) {
        const char *rel = fz_xml_att(n, "rel"), *href = fz_xml_att(n, "href");
        if (rel && findCase(rel, "alternate") && feedType(fz_xml_att(n, "type")) && href)
            found = opdsResolve(base_url, href, out, out_size);
    }
    fz_drop_xml(ctx, doc);
    fz_drop_context(ctx);
    return found;
}

int feedsParseOpml(const char *data, size_t size, FeedOutline **out)
{
    *out = NULL;
    if (!data || !size)
        return -1;
    fz_context *ctx = newContext();
    if (!ctx)
        return -1;
    fz_xml *doc = parse(ctx, data, size, false);
    fz_xml *top = doc ? fz_xml_root(doc) : NULL;
    while (top && !tagIs(top, "opml"))
        top = fz_xml_next(top);
    int count = top ? 0 : -1, cap = 0;
    for (fz_xml *n = top ? findDeep(top, "outline") : NULL; n && count < OUTLINES_MAX;
         n = findDeep(nextNode(n, NULL), "outline")) {
        const char *url = fz_xml_att(n, "xmlUrl");
        if (!url || strncasecmp(url, "http", 4))
            continue;
        if (count == cap) {
            cap = cap ? cap * 2 : 16;
            FeedOutline *grown = realloc(*out, (size_t)cap * sizeof(*grown));
            if (!grown)
                break;
            *out = grown;
        }
        FeedOutline *o = &(*out)[count++];
        snprintf(o->url, sizeof(o->url), "%s", url);
        const char *title = fz_xml_att(n, "title");
        if (!title || !title[0])
            title = fz_xml_att(n, "text");
        feedsPlainText(title ? title : "", o->title, sizeof(o->title));
    }
    fz_drop_xml(ctx, doc);
    fz_drop_context(ctx);
    return count;
}

// --- Keeping a feed ---

// True when a should come before b: newer first, the ones without a date last.
static bool before(const FeedItem *a, const FeedItem *b)
{
    return a->date && (!b->date || a->date > b->date);
}

static bool hasId(const FeedItem *items, int count, const char *id)
{
    for (int i = 0; i < count; i++)
        if (!strcmp(items[i].id, id))
            return true;
    return false;
}

bool feedsMerge(Feed *kept, Feed *fresh, int max)
{
    int total = kept->count + fresh->count;
    FeedItem *all = total ? malloc((size_t)total * sizeof(*all)) : NULL;
    if (total && !all)
        return false;
    int n = 0;
    for (int i = 0; i < fresh->count; i++) {
        if (hasId(all, n, fresh->items[i].id))
            free(fresh->items[i].html);
        else
            all[n++] = fresh->items[i];
    }
    for (int i = 0; i < kept->count; i++) {
        if (hasId(all, n, kept->items[i].id))
            free(kept->items[i].html);  // The fresh one's newer.
        else
            all[n++] = kept->items[i];
    }
    for (int i = 1; i < n; i++) {  // Stable: the same dates keep the feed's order.
        FeedItem it = all[i];
        int j = i;
        while (j > 0 && before(&it, &all[j - 1])) {
            all[j] = all[j - 1];
            j--;
        }
        all[j] = it;
    }
    if (max < 0)
        max = 0;
    for (int i = max; i < n; i++)
        free(all[i].html);
    free(kept->items);
    kept->items = all;
    kept->count = n < max ? n : max;
    if (fresh->title[0])
        snprintf(kept->title, sizeof(kept->title), "%s", fresh->title);
    if (fresh->link[0])
        snprintf(kept->link, sizeof(kept->link), "%s", fresh->link);
    free(fresh->items);
    fresh->items = NULL;
    fresh->count = 0;
    return true;
}

static void element(TextBuf *b, const char *name, const char *text)
{
    if (!text || !text[0])
        return;
    textBufStr(b, "<");
    textBufStr(b, name);
    textBufStr(b, ">");
    textBufEscaped(b, text, strlen(text));
    textBufStr(b, "</");
    textBufStr(b, name);
    textBufStr(b, ">");
}

static void linkElement(TextBuf *b, const char *tag, const char *att, const char *url)
{
    if (!url || !url[0])
        return;
    textBufStr(b, "<");
    textBufStr(b, tag);
    textBufStr(b, " ");
    textBufStr(b, att);
    textBufStr(b, "=\"");
    textBufEscaped(b, url, strlen(url));
    textBufStr(b, "\"/>");
}

char *feedsWrite(const Feed *f, size_t *size)
{
    TextBuf b;
    textBufInit(&b, CACHE_MAX);
    textBufStr(&b, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<feed xmlns=\"http://www.w3.org/2005/Atom\" "
                   "xmlns:media=\"http://search.yahoo.com/mrss/\">\n");
    element(&b, "title", f->title);
    linkElement(&b, "link", "href", f->link);
    textBufStr(&b, "\n");
    for (int i = 0; i < f->count; i++) {  // Each article whole, or (past the size kept) none.
        const FeedItem *it = &f->items[i];
        TextBuf e;
        textBufInit(&e, CACHE_MAX);
        textBufStr(&e, "<entry>");
        element(&e, "id", it->id);
        element(&e, "title", it->title);
        linkElement(&e, "link", "href", it->link);
        if (it->author[0]) {
            textBufStr(&e, "<author>");
            element(&e, "name", it->author);
            textBufStr(&e, "</author>");
        }
        if (it->date) {
            char when[32];
            isoTime(it->date, when, sizeof(when));
            element(&e, "published", when);
        }
        linkElement(&e, "media:thumbnail", "url", it->image);
        if (it->html) {
            textBufStr(&e, "<content type=\"html\">");
            textBufEscaped(&e, it->html, strlen(it->html));
            textBufStr(&e, "</content>");
        }
        textBufStr(&e, "</entry>\n");
        bool fits = !e.full && !e.failed && e.size + 16 < CACHE_MAX - b.size;
        if (fits)
            textBufAdd(&b, e.data, e.size);
        textBufFree(&e);
        if (!fits)
            break;
    }
    textBufStr(&b, "</feed>\n");
    if (b.full) {  // Can't happen (room was kept for the end), but never a cut feed.
        textBufFree(&b);
        return NULL;
    }
    return textBufTake(&b, size);
}

void feedsHash(const char *text, char out[FEED_HASH_LEN + 1])
{
    uint64_t h = 14695981039346656037ull;  // FNV-1a.
    for (const unsigned char *s = (const unsigned char *)(text ? text : ""); *s; s++) {
        h ^= *s;
        h *= 1099511628211ull;
    }
    snprintf(out, FEED_HASH_LEN + 1, "%016llx", (unsigned long long)h);
}
