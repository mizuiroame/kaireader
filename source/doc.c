// MuPDF behind doc.h. Every call into MuPDF is inside fz_try: a broken book
// gives an error, never a crash.
#include "doc.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include <mupdf/fitz.h>
#include <mupdf/ucdn.h>

// Fewer than MuPDF's default 256 MB of decoded images and fonts kept around.
#define DOC_STORE_BYTES (96u << 20)

// Pages are laid out with no margin of their own: KaiReader leaves the room around the page.
static const char USER_CSS[] = "@page { margin: 0 } body { margin: 0; padding: 0 }";

struct Doc {
    fz_context *ctx;
    fz_document *doc;
    bool reflowable;
    int pages;  // -1 until counted for the current layout.
};

static const char *const KNOWN[] = {
    ".epub", ".pdf", ".xps", ".oxps", ".cbz", ".cbt", ".fb2", ".fbz", ".mobi", ".txt", ".xhtml", ".html", ".htm",
};

bool docKnownName(const char *name)
{
    const char *dot = name ? strrchr(name, '.') : NULL;
    if (!dot || dot == name)
        return false;
    for (size_t i = 0; i < sizeof(KNOWN) / sizeof(KNOWN[0]); i++) {
        const char *k = KNOWN[i];
        size_t n = strlen(k);
        if (strlen(dot) != n)
            continue;
        size_t j = 0;
        while (j < n && tolower((unsigned char)dot[j]) == k[j])
            j++;
        if (j == n)
            return true;
    }
    return false;
}

static DocFont g_fonts[DOC_FONT_COUNT];

void docSetFallbackFonts(const DocFont fonts[DOC_FONT_COUNT])
{
    memcpy(g_fonts, fonts, sizeof(g_fonts));
}

static fz_font *fontOf(fz_context *ctx, DocFontKind kind, const char *name)
{
    if (kind < 0 || kind >= DOC_FONT_COUNT || !g_fonts[kind].size)
        return NULL;
    return fz_new_font_from_memory(ctx, name, (const unsigned char *)g_fonts[kind].data, (int)g_fonts[kind].size, 0, 0);
}

// A PDF asks for a CJK font by its character collection.
static fz_font *loadCjk(fz_context *ctx, const char *name, int ordering, int serif)
{
    (void)serif;
    switch (ordering) {
    case FZ_ADOBE_JAPAN:
        return fontOf(ctx, DOC_FONT_JAPANESE, name);
    case FZ_ADOBE_GB:
        return fontOf(ctx, DOC_FONT_CHINESE_SIMPLIFIED, name);
    case FZ_ADOBE_CNS:
        return fontOf(ctx, DOC_FONT_CHINESE_TRADITIONAL, name);
    case FZ_ADOBE_KOREA:
        return fontOf(ctx, DOC_FONT_KOREAN, name);
    default:
        return NULL;
    }
}

// Text (an EPUB's) in a script none of its fonts have: by script, then by the text's language.
static fz_font *loadFallback(fz_context *ctx, int script, int language, int serif, int bold, int italic)
{
    (void)serif;
    (void)bold;
    (void)italic;
    switch (script) {
    case UCDN_SCRIPT_HIRAGANA:
    case UCDN_SCRIPT_KATAKANA:
        return fontOf(ctx, DOC_FONT_JAPANESE, "Japanese");
    case UCDN_SCRIPT_HANGUL:
        return fontOf(ctx, DOC_FONT_KOREAN, "Korean");
    case UCDN_SCRIPT_BOPOMOFO:
        return fontOf(ctx, DOC_FONT_CHINESE_TRADITIONAL, "Chinese");
    case UCDN_SCRIPT_HAN:
        if (language == FZ_LANG_ko)
            return fontOf(ctx, DOC_FONT_KOREAN, "Korean");
        if (language == FZ_LANG_zh_Hant)
            return fontOf(ctx, DOC_FONT_CHINESE_TRADITIONAL, "Chinese");
        if (language == FZ_LANG_zh || language == FZ_LANG_zh_Hans)
            return fontOf(ctx, DOC_FONT_CHINESE_SIMPLIFIED, "Chinese");
        return fontOf(ctx, DOC_FONT_JAPANESE, "Japanese");  // Its font has most kanji and hanzi.
    default:
        return NULL;
    }
}

static void quiet(void *user, const char *message)
{
    (void)user;
    (void)message;
}

Doc *docOpen(const char *path, char *error, size_t error_size)
{
    if (error_size)
        error[0] = 0;
    Doc *d = calloc(1, sizeof(*d));
    if (!d) {
        snprintf(error, error_size, "Out of memory.");
        return NULL;
    }
    d->ctx = fz_new_context(NULL, NULL, DOC_STORE_BYTES);
    if (!d->ctx) {
        free(d);
        snprintf(error, error_size, "Out of memory.");
        return NULL;
    }
    fz_set_warning_callback(d->ctx, quiet, NULL);
    fz_set_error_callback(d->ctx, quiet, NULL);
    bool ok = true;
    fz_var(ok);
    fz_var(d);
    fz_try(d->ctx)
    {
        fz_register_document_handlers(d->ctx);
        fz_install_load_system_font_funcs(d->ctx, NULL, loadCjk, loadFallback);
        fz_set_user_css(d->ctx, USER_CSS);
        fz_set_use_document_css(d->ctx, 1);
        d->doc = fz_open_document(d->ctx, path);
        if (fz_needs_password(d->ctx, d->doc))
            fz_throw(d->ctx, FZ_ERROR_ARGUMENT, "This book is locked with a password or DRM.");
        d->reflowable = fz_is_document_reflowable(d->ctx, d->doc) != 0;
        d->pages = -1;
    }
    fz_catch(d->ctx)
    {
        snprintf(error, error_size, "%s", fz_caught_message(d->ctx));
        ok = false;
    }
    if (!ok) {
        docClose(d);
        return NULL;
    }
    return d;
}

void docClose(Doc *d)
{
    if (!d)
        return;
    if (d->doc)
        fz_drop_document(d->ctx, d->doc);
    fz_drop_context(d->ctx);
    free(d);
}

bool docReflowable(const Doc *d)
{
    return d->reflowable;
}

bool docLayout(Doc *d, float w, float h, float em)
{
    if (!d->reflowable)
        return true;
    if (!(w >= 50 && h >= 50 && em >= 4 && w <= 5000 && h <= 5000 && em <= 200))
        return false;
    bool ok = true;
    fz_var(ok);
    fz_try(d->ctx)
    {
        fz_layout_document(d->ctx, d->doc, w, h, em);
    }
    fz_catch(d->ctx)
    {
        ok = false;
    }
    d->pages = -1;
    return ok;
}

int docPageCount(Doc *d)
{
    if (d->pages >= 0)
        return d->pages;
    int n = 0;
    fz_var(n);
    fz_try(d->ctx)
    {
        n = fz_count_pages(d->ctx, d->doc);
    }
    fz_catch(d->ctx)
    {
        n = 0;
    }
    d->pages = n > 0 ? n : 0;
    return d->pages;
}

bool docPageSize(Doc *d, int page, float *w, float *h)
{
    if (page < 0 || page >= docPageCount(d))
        return false;
    bool ok = true;
    fz_page *p = NULL;
    fz_var(p);
    fz_var(ok);
    fz_try(d->ctx)
    {
        p = fz_load_page(d->ctx, d->doc, page);
        fz_rect r = fz_bound_page(d->ctx, p);
        *w = r.x1 - r.x0;
        *h = r.y1 - r.y0;
    }
    fz_always(d->ctx)
    {
        fz_drop_page(d->ctx, p);
    }
    fz_catch(d->ctx)
    {
        ok = false;
    }
    return ok && *w > 0 && *h > 0;
}

// The pixmap's RGB as opaque RGBA.
static bool toImage(const fz_pixmap *pix, DocImage *out)
{
    int w = pix->w, h = pix->h, n = pix->n;
    if (w <= 0 || h <= 0 || n < 3)
        return false;
    uint32_t *pixels = malloc((size_t)w * (size_t)h * sizeof(uint32_t));
    if (!pixels)
        return false;
    for (int y = 0; y < h; y++) {
        const unsigned char *s = pix->samples + (size_t)y * (size_t)pix->stride;
        uint32_t *o = pixels + (size_t)y * (size_t)w;
        for (int x = 0; x < w; x++, s += n)
            o[x] = 0xFF000000u | ((uint32_t)s[2] << 16) | ((uint32_t)s[1] << 8) | s[0];
    }
    out->pixels = pixels;
    out->width = w;
    out->height = h;
    return true;
}

bool docRender(Doc *d, int page, float scale, const DocColors *colors, DocImage *out)
{
    memset(out, 0, sizeof(*out));
    float pw, ph;
    if (!(scale > 0) || !docPageSize(d, page, &pw, &ph))
        return false;
    float biggest = pw > ph ? pw : ph;
    if (biggest * scale > DOC_RENDER_MAX)
        scale = DOC_RENDER_MAX / biggest;
    bool ok = true;
    fz_pixmap *pix = NULL;
    fz_var(pix);
    fz_var(ok);
    fz_var(scale);
    fz_try(d->ctx)
    {
        pix = fz_new_pixmap_from_page_number(d->ctx, d->doc, page, fz_scale(scale, scale), fz_device_rgb(d->ctx), 0);
        if (d->reflowable && colors && (colors->ink != 0x000000 || colors->paper != 0xFFFFFF))
            fz_tint_pixmap(d->ctx, pix, (int)colors->ink, (int)colors->paper);
        ok = toImage(pix, out);
    }
    fz_always(d->ctx)
    {
        fz_drop_pixmap(d->ctx, pix);
    }
    fz_catch(d->ctx)
    {
        ok = false;
    }
    return ok;
}

void docImageFree(DocImage *image)
{
    free(image->pixels);
    memset(image, 0, sizeof(*image));
}

// Copies text without control characters, whitespace runs made one space.
static void cleanText(const char *in, char *out, size_t size)
{
    size_t o = 0;
    bool space = false;
    for (const unsigned char *s = (const unsigned char *)in; *s && o + 1 < size; s++) {
        if (*s < 0x20 || *s == 0x7F || *s == ' ') {
            space = o > 0;
            continue;
        }
        if (space && o + 2 < size)
            out[o++] = ' ';
        space = false;
        out[o++] = (char)*s;
    }
    // Never end inside a UTF-8 sequence cut short.
    size_t start = o;
    while (start > 0 && o - start < 4 && ((unsigned char)out[start - 1] & 0xC0) == 0x80)
        start--;
    if (start > 0 && start < o + 1) {
        unsigned char lead = (unsigned char)out[start - 1];
        size_t want = lead >= 0xF0 ? 4 : lead >= 0xE0 ? 3 : lead >= 0xC0 ? 2 : 1;
        if (o - (start - 1) < want)
            o = start - 1;
    }
    out[o] = 0;
}

static bool meta(Doc *d, const char *key, char *out, size_t size)
{
    if (size)
        out[0] = 0;
    char raw[DOC_TEXT_MAX * 2];
    int n = -1;
    fz_var(n);
    fz_try(d->ctx)
    {
        n = fz_lookup_metadata(d->ctx, d->doc, key, raw, sizeof(raw));
    }
    fz_catch(d->ctx)
    {
        n = -1;
    }
    if (n <= 0)
        return false;
    raw[sizeof(raw) - 1] = 0;
    cleanText(raw, out, size);
    return out[0] != 0;
}

bool docTitle(Doc *d, char *out, size_t size)
{
    return meta(d, FZ_META_INFO_TITLE, out, size);
}

bool docAuthor(Doc *d, char *out, size_t size)
{
    return meta(d, FZ_META_INFO_AUTHOR, out, size);
}

// The page an entry of the table of contents points to: its place, or (EPUB) its link.
static int pageOf(Doc *d, const fz_outline *o)
{
    fz_location loc = o->page;
    if ((loc.chapter < 0 || loc.page < 0) && o->uri && o->uri[0]) {
        float x, y;
        loc = fz_resolve_link(d->ctx, d->doc, o->uri, &x, &y);
    }
    if (loc.chapter < 0 || loc.page < 0)
        return -1;
    return fz_page_number_from_location(d->ctx, d->doc, loc);
}

static void addChapters(Doc *d, fz_outline *o, int level, DocChapter *out, int max, int *count)
{
    // Deep tables are cut at 8 levels; the walk goes on along each level.
    for (; o && *count < max; o = o->next) {
        DocChapter *c = &out[*count];
        cleanText(o->title ? o->title : "", c->title, sizeof(c->title));
        c->level = level;
        c->page = pageOf(d, o);
        (*count)++;
        if (o->down && level < 8)
            addChapters(d, o->down, level + 1, out, max, count);
    }
}

int docChapters(Doc *d, DocChapter *out, int max)
{
    int count = 0;
    fz_outline *outline = NULL;
    fz_var(outline);
    fz_var(count);
    fz_try(d->ctx)
    {
        outline = fz_load_outline(d->ctx, d->doc);
        addChapters(d, outline, 0, out, max, &count);
    }
    fz_always(d->ctx)
    {
        fz_drop_outline(d->ctx, outline);
    }
    fz_catch(d->ctx)
    {
        count = 0;
    }
    return count;
}

DocPlace docPlaceOf(Doc *d, int page)
{
    DocPlace place = {0, 0};
    if (page <= 0 || page >= docPageCount(d))
        return place;
    fz_var(place);
    fz_try(d->ctx)
    {
        fz_location loc = fz_location_from_page_number(d->ctx, d->doc, page);
        int in_chapter = fz_count_chapter_pages(d->ctx, d->doc, loc.chapter);
        place.chapter = loc.chapter;
        place.fraction = in_chapter > 0 ? (double)loc.page / in_chapter : 0;
    }
    fz_catch(d->ctx)
    {
        place = (DocPlace){0, 0};
    }
    return place;
}

int docPageOfPlace(Doc *d, DocPlace place)
{
    int pages = docPageCount(d);
    if (pages <= 0 || place.chapter < 0 || !(place.fraction >= 0 && place.fraction <= 1))
        return 0;
    int page = 0;
    fz_var(page);
    fz_try(d->ctx)
    {
        if (place.chapter < fz_count_chapters(d->ctx, d->doc)) {
            int in_chapter = fz_count_chapter_pages(d->ctx, d->doc, place.chapter);
            // The page whose start is at or before the place, so the text read last is on it.
            int at = (int)(place.fraction * in_chapter + 1e-6);
            if (at >= in_chapter)
                at = in_chapter - 1;
            page = fz_page_number_from_location(d->ctx, d->doc, fz_make_location(place.chapter, at < 0 ? 0 : at));
        }
    }
    fz_catch(d->ctx)
    {
        page = 0;
    }
    if (page < 0)
        page = 0;
    if (page >= pages)
        page = pages - 1;
    return page;
}

// --- An EPUB's cover picture ---

#define COVER_PIXELS_MAX (40u * 1000u * 1000u)  // A picture bigger than this isn't a cover: left alone.

// The part of a tag after its namespace prefix ("opf:item" is "item").
static bool tagIs(fz_xml *node, const char *name)
{
    const char *tag = fz_xml_tag(node);
    if (!tag)
        return false;
    const char *colon = strrchr(tag, ':');
    return !strcmp(colon ? colon + 1 : tag, name);
}

// The first element called name in node's tree (node itself included), depth first.
static fz_xml *findTag(fz_xml *node, const char *name, int depth)
{
    for (; node && depth < 32; node = fz_xml_next(node)) {
        if (tagIs(node, name))
            return node;
        fz_xml *in = findTag(fz_xml_down(node), name, depth + 1);
        if (in)
            return in;
    }
    return NULL;
}

// The next element called name after node, among node's siblings and their trees.
static fz_xml *nextTag(fz_xml *node, const char *name)
{
    for (node = fz_xml_next(node); node; node = fz_xml_next(node)) {
        fz_xml *found = findTag(node, name, 0);
        if (found)
            return found;
    }
    return NULL;
}

// True if words (space-separated) has word.
static bool hasWord(const char *words, const char *word)
{
    size_t n = strlen(word);
    for (const char *p = words; p && *p;) {
        while (*p == ' ')
            p++;
        size_t len = strcspn(p, " ");
        if (len == n && !strncmp(p, word, n))
            return true;
        p += len;
    }
    return false;
}

// The href of the manifest item that is the cover picture; NULL if none.
static const char *coverHref(fz_xml *package)
{
    fz_xml *manifest = findTag(package, "manifest", 0);
    if (!manifest)
        return NULL;
    // EPUB 3: properties="cover-image".
    for (fz_xml *item = findTag(fz_xml_down(manifest), "item", 0); item; item = nextTag(item, "item")) {
        const char *props = fz_xml_att(item, "properties");
        if (props && hasWord(props, "cover-image"))
            return fz_xml_att(item, "href");
    }
    // EPUB 2: <meta name="cover" content="the item's id"/>.
    fz_xml *metadata = findTag(package, "metadata", 0);
    const char *id = NULL;
    for (fz_xml *meta = metadata ? findTag(fz_xml_down(metadata), "meta", 0) : NULL; meta && !id;
         meta = nextTag(meta, "meta")) {
        const char *name = fz_xml_att(meta, "name");
        if (name && !strcmp(name, "cover"))
            id = fz_xml_att(meta, "content");
    }
    if (!id)
        return NULL;
    for (fz_xml *item = findTag(fz_xml_down(manifest), "item", 0); item; item = nextTag(item, "item")) {
        const char *item_id = fz_xml_att(item, "id");
        const char *type = fz_xml_att(item, "media-type");
        if (item_id && !strcmp(item_id, id) && type && !strncmp(type, "image/", 6))
            return fz_xml_att(item, "href");
    }
    return NULL;
}

// Turns "%20" and the like into their characters, in place.
static void urlDecode(char *s)
{
    char *o = s;
    for (; *s; s++) {
        if (*s == '%' && isxdigit((unsigned char)s[1]) && isxdigit((unsigned char)s[2])) {
            char hex[3] = {s[1], s[2], 0};
            *o++ = (char)strtol(hex, NULL, 16);
            s += 2;
        } else {
            *o++ = *s;
        }
    }
    *o = 0;
}

bool docEpubCover(const char *path, int max_w, int max_h, DocImage *out)
{
    memset(out, 0, sizeof(*out));
    const char *dot = path ? strrchr(path, '.') : NULL;
    if (!dot || strcasecmp(dot, ".epub") || max_w <= 0 || max_h <= 0)
        return false;
    fz_context *ctx = fz_new_context(NULL, NULL, 32u << 20);
    if (!ctx)
        return false;
    fz_set_warning_callback(ctx, quiet, NULL);
    fz_set_error_callback(ctx, quiet, NULL);
    bool ok = false;
    fz_archive *zip = NULL;
    fz_buffer *buf = NULL;
    fz_xml *xml = NULL;
    fz_image *image = NULL;
    fz_pixmap *pix = NULL, *rgb = NULL, *small = NULL;
    fz_var(ok);
    fz_var(zip);
    fz_var(buf);
    fz_var(xml);
    fz_var(image);
    fz_var(pix);
    fz_var(rgb);
    fz_var(small);
    fz_try(ctx)
    {
        fz_register_document_handlers(ctx);
        zip = fz_open_archive(ctx, path);
        buf = fz_read_archive_entry(ctx, zip, "META-INF/container.xml");
        xml = fz_parse_xml(ctx, buf, 0);
        fz_drop_buffer(ctx, buf);
        buf = NULL;
        fz_xml *rootfile = findTag(fz_xml_root(xml), "rootfile", 0);
        const char *full = rootfile ? fz_xml_att(rootfile, "full-path") : NULL;
        if (!full || strlen(full) >= 400)
            fz_throw(ctx, FZ_ERROR_FORMAT, "no package");
        char opf[400];
        snprintf(opf, sizeof(opf), "%s", full);
        fz_drop_xml(ctx, xml);
        xml = NULL;
        buf = fz_read_archive_entry(ctx, zip, opf);
        xml = fz_parse_xml(ctx, buf, 0);
        fz_drop_buffer(ctx, buf);
        buf = NULL;
        const char *href = coverHref(fz_xml_root(xml));
        if (!href || !href[0] || strlen(href) >= 400 || strstr(href, "://"))
            fz_throw(ctx, FZ_ERROR_FORMAT, "no cover");
        // The href is relative to the package's folder.
        char entry[820];
        const char *slash = strrchr(opf, '/');
        snprintf(entry, sizeof(entry), "%.*s%s", slash ? (int)(slash - opf + 1) : 0, opf, href);
        urlDecode(entry);
        fz_cleanname(entry);
        buf = fz_read_archive_entry(ctx, zip, entry);
        image = fz_new_image_from_buffer(ctx, buf);
        if (image->w <= 0 || image->h <= 0 || (uint64_t)image->w * (uint64_t)image->h > COVER_PIXELS_MAX)
            fz_throw(ctx, FZ_ERROR_LIMIT, "cover too big");
        float s = (float)max_w / image->w < (float)max_h / image->h ? (float)max_w / image->w : (float)max_h / image->h;
        if (s > 1)
            s = 1;
        int w = (int)(image->w * s + 0.5f), h = (int)(image->h * s + 0.5f);
        w = w < 1 ? 1 : w;
        h = h < 1 ? 1 : h;
        fz_matrix ctm = fz_scale((float)w, (float)h);  // Lets the decoder skip detail it won't need.
        pix = fz_get_pixmap_from_image(ctx, image, NULL, &ctm, NULL, NULL);
        if (pix->colorspace != fz_device_rgb(ctx) || pix->alpha) {
            rgb = fz_convert_pixmap(ctx, pix, fz_device_rgb(ctx), NULL, NULL, fz_default_color_params, 0);
        }
        fz_pixmap *src = rgb ? rgb : pix;
        if (src->w != w || src->h != h)
            small = fz_scale_pixmap(ctx, src, 0, 0, (float)w, (float)h, NULL);
        ok = toImage(small ? small : src, out);
    }
    fz_always(ctx)
    {
        fz_drop_pixmap(ctx, small);
        fz_drop_pixmap(ctx, rgb);
        fz_drop_pixmap(ctx, pix);
        fz_drop_image(ctx, image);
        fz_drop_xml(ctx, xml);
        fz_drop_buffer(ctx, buf);
        fz_drop_archive(ctx, zip);
    }
    fz_catch(ctx)
    {
        ok = false;
    }
    fz_drop_context(ctx);
    return ok;
}
