#include "article.h"

#include <mupdf/fitz.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "textbuf.h"

#define DEPTH_MAX 48        // Elements in elements kept; deeper ones give their text.
#define ARTICLE_MIN_TEXT 200  // An <article> with less text isn't the article (a teaser).
#define PARAGRAPHS_MIN 140    // The part with the most paragraphs needs this much text.
#define PARENTS_MAX 1024

static void quiet(void *user, const char *message)
{
    (void)user;
    (void)message;
}

static const char *nameOf(fz_xml *n)
{
    const char *tag = fz_xml_tag(n);
    if (!tag)
        return NULL;
    const char *colon = strrchr(tag, ':');
    return colon ? colon + 1 : tag;
}

static bool among(const char *name, const char *const *list, size_t count)
{
    for (size_t i = 0; i < count; i++)
        if (!strcasecmp(name, list[i]))
            return true;
    return false;
}

#define AMONG(name, list) among((name), (list), sizeof(list) / sizeof((list)[0]))

// Kept as they are.
static const char *const KEEP[] = {"p",     "h1",    "h2",    "h3",   "h4",   "h5",     "h6",   "ul",  "ol",
                                   "li",    "blockquote", "pre", "code", "em",  "i",      "strong", "b",   "u",
                                   "s",     "sub",   "sup",   "table", "thead", "tbody", "tfoot", "tr",  "td",
                                   "th",    "dl",    "dt",    "dd",   "figcaption", "small", "cite", "q",
                                   "abbr",  "mark",  "span",  "del",  "ins",  "kbd",    "var",  "samp", "caption"};
// Kept as a plain block.
static const char *const BLOCKS[] = {"div", "section", "article", "header", "main", "figure", "details",
                                     "summary", "center", "address", "body"};
// Left out with all they hold.
static const char *const DROP[] = {"script", "style",  "iframe", "object", "embed",  "noscript", "form",
                                   "button", "input",  "select", "textarea", "svg", "math",    "video",
                                   "audio",  "canvas", "template", "head",  "nav",  "aside",   "footer",
                                   "source", "track",  "link",   "meta",   "title", "map",     "frame",
                                   "frameset", "applet", "dialog", "label", "menu"};

static void text(TextBuf *b, const char *t)
{
    textBufEscaped(b, t, strlen(t));
}

static fz_xml *nextNode(fz_xml *n, fz_xml *top);

// The node after n's whole tree, staying under top.
static fz_xml *afterNode(fz_xml *n, fz_xml *top)
{
    while (n && n != top) {
        fz_xml *next = fz_xml_next(n);
        if (next)
            return next;
        n = fz_xml_up(n);
    }
    return NULL;
}

// All the words under top (none of a dropped element's): escaped into b, and counted.
static void wordsOf(fz_xml *top, TextBuf *b, size_t *length)
{
    fz_xml *n = fz_xml_down(top);
    while (n) {
        const char *t = fz_xml_text(n);
        if (t) {
            if (b)
                text(b, t);
            if (length && strspn(t, " \t\r\n") != strlen(t))
                *length += strlen(t);
            n = afterNode(n, top);
            continue;
        }
        const char *name = nameOf(n);
        n = name && !AMONG(name, DROP) ? nextNode(n, top) : afterNode(n, top);
    }
}

static void picture(fz_xml *img, TextBuf *b, const char *word)
{
    const char *alt = fz_xml_att(img, "alt");
    size_t lead = alt ? strspn(alt, " \t\r\n") : 0;
    if (!word) {  // Kept for articlePage, which says it in the reader's language.
        textBufStr(b, "<img alt=\"");
        text(b, alt ? alt : "");
        textBufStr(b, "\"/>");
        return;
    }
    textBufStr(b, "<i>");
    if (alt && alt[lead]) {
        textBufStr(b, "[");
        text(b, alt + lead);
        textBufStr(b, "]");
    } else {
        text(b, word[0] ? word : "[picture]");
    }
    textBufStr(b, "</i> ");
}

// n's children, cleaned, into b.
static void clean(fz_xml *n, TextBuf *b, const char *word, int depth)
{
    for (fz_xml *c = fz_xml_down(n); c; c = fz_xml_next(c)) {
        const char *t = fz_xml_text(c);
        if (t) {
            text(b, t);
            continue;
        }
        const char *name = nameOf(c);
        if (!name || AMONG(name, DROP))
            continue;
        if (!strcasecmp(name, "img")) {
            picture(c, b, word);
            continue;
        }
        if (!strcasecmp(name, "br")) {
            textBufStr(b, "<br/>");
            continue;
        }
        if (!strcasecmp(name, "hr")) {
            textBufStr(b, "<hr/>");
            continue;
        }
        const char *as = AMONG(name, KEEP) ? name : AMONG(name, BLOCKS) ? "div" : NULL;
        if (depth >= DEPTH_MAX) {  // Too deep: the words alone.
            wordsOf(c, b, NULL);
            continue;
        }
        if (!as) {  // Anything else (a link, a font...): what it holds.
            clean(c, b, word, depth + 1);
            continue;
        }
        char tag[16];
        snprintf(tag, sizeof(tag), "%s", as);
        for (char *p = tag; *p; p++)
            if (*p >= 'A' && *p <= 'Z')
                *p = (char)(*p - 'A' + 'a');
        textBufStr(b, "<");
        textBufStr(b, tag);
        textBufStr(b, ">");
        clean(c, b, word, depth + 1);
        textBufStr(b, "</");
        textBufStr(b, tag);
        textBufStr(b, ">");
    }
}

static fz_context *newContext(void)
{
    fz_context *ctx = fz_new_context(NULL, NULL, 32u << 20);
    if (ctx) {
        fz_set_warning_callback(ctx, quiet, NULL);
        fz_set_error_callback(ctx, quiet, NULL);
    }
    return ctx;
}

static fz_xml *parseHtml(fz_context *ctx, const char *html, size_t size)
{
    fz_buffer *buf = NULL;
    fz_xml *doc = NULL;
    fz_var(buf);
    fz_var(doc);
    fz_try(ctx)
    {
        buf = fz_new_buffer_from_copied_data(ctx, (const unsigned char *)html, size);
        doc = fz_parse_xml_from_html5(ctx, buf);
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

static fz_xml *bodyOf(fz_xml *doc)
{
    for (fz_xml *n = fz_xml_root(doc); n; n = nextNode(n, NULL))
        if (nameOf(n) && !strcasecmp(nameOf(n), "body"))
            return n;
    return NULL;
}

// html's text without its tags, escaped, in one paragraph.
static void plainWords(const char *html, TextBuf *b)
{
    textBufStr(b, "<p>");
    for (const char *s = html; *s;) {
        if (*s == '<') {
            const char *end = strchr(s, '>');
            if (!end)
                break;
            s = end + 1;
            continue;
        }
        size_t n = strcspn(s, "<");
        textBufEscaped(b, s, n);
        s += n;
    }
    textBufStr(b, "</p>");
}

char *articlePage(const ArticleParts *parts, size_t *size)
{
    TextBuf b;
    textBufInit(&b, ARTICLE_MAX);
    textBufStr(&b, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<html xmlns=\"http://www.w3.org/1999/xhtml\">"
                   "<head><meta charset=\"utf-8\"/><title>");
    text(&b, parts->title ? parts->title : "");
    textBufStr(&b, "</title><style>p.kr-by{font-style:italic}</style></head>\n<body><h1>");
    text(&b, parts->title ? parts->title : "");
    textBufStr(&b, "</h1>");
    if (parts->byline && parts->byline[0]) {
        textBufStr(&b, "<p class=\"kr-by\">");
        text(&b, parts->byline);
        textBufStr(&b, "</p>");
    }
    textBufStr(&b, "\n");
    if (parts->html && parts->html[0]) {
        fz_context *ctx = newContext();
        fz_xml *doc = ctx ? parseHtml(ctx, parts->html, strlen(parts->html)) : NULL;
        fz_xml *body = doc ? bodyOf(doc) : NULL;
        // Room is kept for the page's end, so a long article is cut, never the page.
        TextBuf content;
        textBufInit(&content, ARTICLE_MAX - 4096);
        if (body)
            clean(body, &content, parts->picture ? parts->picture : "", 0);
        else  // HTML even its parser gives up on (nested too deep): its words.
            plainWords(parts->html, &content);
        if (content.data && !content.failed)
            textBufAdd(&b, content.data, content.size);
        textBufFree(&content);
        if (ctx) {
            fz_drop_xml(ctx, doc);
            fz_drop_context(ctx);
        }
    }
    if (parts->note && parts->note[0]) {
        textBufStr(&b, "\n<hr/><p class=\"kr-by\">");
        text(&b, parts->note);
        textBufStr(&b, "</p>");
    }
    textBufStr(&b, "\n</body></html>\n");
    return textBufTake(&b, size);
}

// The words under n, counted (a dropped element's not).
static size_t textLength(fz_xml *n)
{
    size_t length = 0;
    wordsOf(n, NULL, &length);
    return length;
}

static fz_xml *findNamed(fz_xml *from, const char *name)
{
    for (fz_xml *n = from; n; n = nextNode(n, NULL))
        if (nameOf(n) && !strcasecmp(nameOf(n), name))
            return n;
    return NULL;
}

// The element with the most paragraph text in it (its own <p>'s), if there's enough.
static fz_xml *mostParagraphs(fz_xml *body)
{
    fz_xml **parents = malloc(PARENTS_MAX * sizeof(*parents));
    size_t *scores = malloc(PARENTS_MAX * sizeof(*scores));
    int count = 0;
    if (!parents || !scores) {
        free(parents);
        free(scores);
        return NULL;
    }
    for (fz_xml *p = findNamed(body, "p"); p; p = findNamed(nextNode(p, NULL), "p")) {
        fz_xml *up = fz_xml_up(p);
        int at = -1;
        for (int i = 0; i < count; i++)
            if (parents[i] == up)
                at = i;
        if (at < 0 && count < PARENTS_MAX) {
            at = count++;
            parents[at] = up;
            scores[at] = 0;
        }
        if (at >= 0)
            scores[at] += textLength(p);
    }
    int best = -1;
    for (int i = 0; i < count; i++)
        if (best < 0 || scores[i] > scores[best])
            best = i;
    fz_xml *found = best >= 0 && scores[best] >= PARAGRAPHS_MIN ? parents[best] : NULL;
    free(parents);
    free(scores);
    return found;
}

static fz_xml *mainPart(fz_xml *body)
{
    fz_xml *best = NULL;
    size_t best_length = 0;
    for (fz_xml *a = findNamed(body, "article"); a; a = findNamed(nextNode(a, NULL), "article")) {
        size_t length = textLength(a);
        if (length > best_length) {
            best = a;
            best_length = length;
        }
    }
    if (best && best_length >= ARTICLE_MIN_TEXT)
        return best;
    fz_xml *main_part = findNamed(body, "main");
    for (fz_xml *n = body; n && !main_part; n = nextNode(n, NULL)) {
        const char *role = fz_xml_tag(n) ? fz_xml_att(n, "role") : NULL;
        if (role && !strcasecmp(role, "main"))
            main_part = n;
    }
    if (main_part && textLength(main_part) >= ARTICLE_MIN_TEXT)
        return main_part;
    return mostParagraphs(body);
}

char *articleExtract(const char *page, size_t size)
{
    if (!page || !size)
        return NULL;
    fz_context *ctx = newContext();
    if (!ctx)
        return NULL;
    fz_xml *doc = parseHtml(ctx, page, size);
    fz_xml *body = doc ? bodyOf(doc) : NULL;
    fz_xml *part = body ? mainPart(body) : NULL;
    char *out = NULL;
    if (part) {
        TextBuf b;
        textBufInit(&b, ARTICLE_MAX - 4096);
        textBufStr(&b, "<div>");
        clean(part, &b, NULL, 0);
        textBufStr(&b, "</div>");  // Cut short, it's still read (articlePage parses it again).
        out = textBufTake(&b, NULL);
    }
    fz_drop_xml(ctx, doc);
    fz_drop_context(ctx);
    return out;
}
