#include "covers.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gfx.h"

#define MAGIC "KRC2"      // Then its size, the size it was asked for, and its pixels.
#define MAGIC_OLD "KRC1"  // Its size and pixels (the size asked: taken as its own).
#define COVER_SIDE_MAX 2048
#define SMALLER(made, asked) ((int64_t)(made) * 10 < (int64_t)(asked) * 9)  // Made for under 90% of it.

void coversInit(Covers *c, const char *folder)
{
    memset(c, 0, sizeof(*c));
    snprintf(c->folder, sizeof(c->folder), "%s", folder ? folder : "");
}

static void drop(Cover *cv)
{
    if (cv->pixels) {
        gfxImageChanged(cv->pixels);
        free(cv->pixels);
    }
    memset(cv, 0, sizeof(*cv));
}

void coversFree(Covers *c)
{
    for (int i = 0; i < COVERS_MAX; i++)
        drop(&c->items[i]);
}

void coversFileName(const char *folder, const char *path, int64_t size, char *out, size_t out_size)
{
    uint64_t h = 1469598103934665603ull;  // FNV-1a of the path, then the size.
    for (const unsigned char *s = (const unsigned char *)path; *s; s++)
        h = (h ^ *s) * 1099511628211ull;
    for (int i = 0; i < 8; i++)
        h = (h ^ ((uint64_t)size >> (i * 8) & 0xFF)) * 1099511628211ull;
    snprintf(out, out_size, "%s/%016llx.cover", folder, (unsigned long long)h);
}

static Cover *find(Covers *c, const char *path, int64_t size)
{
    for (int i = 0; i < COVERS_MAX; i++)
        if (c->items[i].path[0] && c->items[i].size == size && !strcmp(c->items[i].path, path))
            return &c->items[i];
    return NULL;
}

// A free slot, or the one drawn longest ago (not one being made).
static Cover *slot(Covers *c)
{
    Cover *oldest = NULL;
    for (int i = 0; i < COVERS_MAX; i++) {
        Cover *cv = &c->items[i];
        if (!cv->path[0])
            return cv;
        if (!cv->asked && (!oldest || cv->used < oldest->used))
            oldest = cv;
    }
    if (oldest)
        drop(oldest);
    return oldest;
}

static bool readFile(const char *file, Cover *cv)
{
    FILE *f = fopen(file, "rb");
    if (!f)
        return false;
    unsigned char head[20];
    bool ok = fread(head, 1, 12, f) == 12, old = ok && !memcmp(head, MAGIC_OLD, 4);
    ok = ok && (old || (!memcmp(head, MAGIC, 4) && fread(head + 12, 1, 8, f) == 8));
    uint32_t w = 0, h = 0, made_w = 0, made_h = 0;
    if (ok) {
        memcpy(&w, head + 4, 4);
        memcpy(&h, head + 8, 4);
        if (!old) {
            memcpy(&made_w, head + 12, 4);
            memcpy(&made_h, head + 16, 4);
        }
        ok = w > 0 && h > 0 && w <= COVER_SIDE_MAX && h <= COVER_SIDE_MAX && made_w <= 16384 && made_h <= 16384;
    }
    uint32_t *pixels = ok ? malloc((size_t)w * h * 4) : NULL;
    ok = pixels && fread(pixels, 4, (size_t)w * h, f) == (size_t)w * h;
    fclose(f);
    if (!ok) {
        free(pixels);
        return false;
    }
    cv->pixels = pixels;
    cv->width = (int)w;
    cv->height = (int)h;
    cv->made_w = old ? (int)w : (int)made_w;
    cv->made_h = old ? (int)h : (int)made_h;
    return true;
}

static void writeFile(const char *file, const Cover *cv)
{
    FILE *f = fopen(file, "wb");
    if (!f)
        return;
    uint32_t w = (uint32_t)cv->width, h = (uint32_t)cv->height;
    uint32_t made_w = (uint32_t)(cv->made_w > 0 ? cv->made_w : cv->width);
    uint32_t made_h = (uint32_t)(cv->made_h > 0 ? cv->made_h : cv->height);
    bool ok = fwrite(MAGIC, 1, 4, f) == 4 && fwrite(&w, 4, 1, f) == 1 && fwrite(&h, 4, 1, f) == 1 &&
              fwrite(&made_w, 4, 1, f) == 1 && fwrite(&made_h, 4, 1, f) == 1 &&
              fwrite(cv->pixels, 4, (size_t)w * h, f) == (size_t)w * h;
    ok = fclose(f) == 0 && ok;
    if (!ok)
        remove(file);  // Never a cover cut short.
}

const Cover *coversGet(Covers *c, const char *path, int64_t size, int w, int h, bool *failed)
{
    if (failed)
        *failed = false;
    Cover *cv = find(c, path, size);
    if (!cv) {
        cv = slot(c);
        if (!cv)
            return NULL;  // Every slot is being made: asked again next frame.
        snprintf(cv->path, sizeof(cv->path), "%s", path);
        cv->size = size;
        char file[COVERS_FOLDER_MAX + 32];
        if (c->folder[0]) {
            coversFileName(c->folder, path, size, file, sizeof(file));
            readFile(file, cv);
        }
        if (!cv->pixels) {
            cv->asked = true;
            readingWantCover(path, w, h);
        }
    } else if (cv->asked) {
        readingWantCover(path, w, h);  // Still wanted: to the front of the asks.
    }
    if (!cv->asked && cv->pixels && (SMALLER(cv->made_w, w) || SMALLER(cv->made_h, h))) {
        cv->asked = true;  // Made for a smaller size than it's drawn at now: made again (shown meanwhile).
        readingWantCover(path, w, h);
    }
    cv->used = ++c->tick;
    if (failed)
        *failed = cv->failed;
    return cv->pixels ? cv : NULL;
}

void coversPut(Covers *c, ReadingCover *made, int64_t size)
{
    Cover *cv = find(c, made->path, size);
    if (!cv) {
        // Asked for, then dropped (scrolled far away): kept on the SD only.
        if (made->ok && c->folder[0] && made->image.pixels) {
            Cover tmp = {.pixels = made->image.pixels, .width = made->image.width, .height = made->image.height,
                         .made_w = made->w, .made_h = made->h};
            char file[COVERS_FOLDER_MAX + 32];
            coversFileName(c->folder, made->path, size, file, sizeof(file));
            writeFile(file, &tmp);
        }
        docImageFree(&made->image);
        return;
    }
    cv->asked = false;
    if (!made->ok || !made->image.pixels || made->image.width > COVER_SIDE_MAX ||
        made->image.height > COVER_SIDE_MAX) {
        if (cv->pixels) {  // Made again larger and failed: the one before stays.
            docImageFree(&made->image);
            cv->made_w = made->w;
            cv->made_h = made->h;
            return;
        }
        cv->failed = true;
        docImageFree(&made->image);
        return;
    }
    if (cv->pixels) {
        gfxImageChanged(cv->pixels);
        free(cv->pixels);
    }
    cv->pixels = made->image.pixels;
    cv->width = made->image.width;
    cv->height = made->image.height;
    cv->made_w = made->w;
    cv->made_h = made->h;
    made->image.pixels = NULL;
    if (c->folder[0]) {
        char file[COVERS_FOLDER_MAX + 32];
        coversFileName(c->folder, cv->path, size, file, sizeof(file));
        writeFile(file, cv);
    }
}
