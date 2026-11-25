// A book of the network folder as a MuPDF stream: only the parts MuPDF asks
// for are read (its covers and titles come from a few blocks of each book),
// in blocks of NET_BLOCK, the last few kept. SMB, FTP and SFTP read with the
// protocol's own reads (davFileRead), WebDAV and web folders with HTTP ranges.
#include "netstream.h"

#include <mupdf/fitz.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "davnet.h"
#include "doc.h"
#include "net.h"

#define NET_BLOCK (64 * 1024)
#define NET_BLOCKS 24  // 1.5 MB kept: the zip's end, an OPF, a cover.
#define URL_MAX 2048
#define AUTH_MAX 512
#define RANGE_STALL_S 10

typedef struct {
    int64_t start;  // -1: empty.
    size_t len;
    unsigned used;  // The last time it was read, for keeping the newest.
    uint8_t *data;
} Block;

typedef struct {
    DavFile *file;  // SMB, FTP or SFTP; NULL for HTTP.
    NetClient *client;
    char url[URL_MAX], auth[AUTH_MAX];
    bool any_certificate;  // HTTPS with the folder's Certificate check off.
    int64_t size;  // -1 until an HTTP answer says it.
    int64_t pos;   // Where the next read starts.
    unsigned tick;
    int block_size, block_count;
    Block blocks[NET_BLOCKS];
} NetStream;

typedef struct {
    uint8_t *to;
    size_t room, got;
} RangeSink;

static bool intoBlock(const void *data, size_t size, void *user)
{
    RangeSink *s = user;
    size_t n = size < s->room - s->got ? size : s->room - s->got;
    memcpy(s->to + s->got, data, n);
    s->got += n;
    return s->got < s->room;
}

// Bytes at start into to (want at most); how many, -1 if it failed.
static int64_t readAt(NetStream *s, int64_t start, uint8_t *to, size_t want)
{
    if (s->file)
        return davFileRead(s->file, start, to, want, NULL);
    char range[64];
    snprintf(range, sizeof(range), "%lld-%lld", (long long)start, (long long)(start + (int64_t)want - 1));
    long long total = -1;
    RangeSink sink = {to, want, 0};
    NetRequest r = {.url = s->url, .authorization = s->auth, .range = range, .total_size = &total,
                    .stall_s = RANGE_STALL_S, .any_certificate = s->any_certificate};
    long status = 0;
    char why[NET_ERROR_MAX];
    bool ok = netFetch(s->client, &r, intoBlock, &sink, &status, why, sizeof(why));
    if (!ok && sink.got == 0)
        return -1;
    if (total >= 0)
        s->size = total;
    else if (status == 200 && s->size < 0)  // No ranges: the whole file came (cut at want).
        s->size = (int64_t)sink.got < (int64_t)want ? (int64_t)sink.got : -1;
    return (int64_t)sink.got;
}

// The block that starts at start (a multiple of NET_BLOCK), read if it isn't kept.
static Block *blockAt(NetStream *s, int64_t start)
{
    Block *oldest = &s->blocks[0];
    for (int i = 0; i < s->block_count; i++) {
        Block *b = &s->blocks[i];
        if (b->start == start && b->data) {
            b->used = ++s->tick;
            return b;
        }
        if (b->start < 0 || b->used < oldest->used)
            oldest = b;
    }
    if (!oldest->data && !(oldest->data = malloc((size_t)s->block_size)))
        return NULL;
    int64_t n = readAt(s, start, oldest->data, (size_t)s->block_size);
    if (n < 0) {
        oldest->start = -1;
        return NULL;
    }
    oldest->start = start;
    oldest->len = (size_t)n;
    oldest->used = ++s->tick;
    return oldest;
}

static int next(fz_context *ctx, fz_stream *stm, size_t max)
{
    (void)max;
    NetStream *s = stm->state;
    if (s->size >= 0 && s->pos >= s->size)
        return EOF;
    int64_t start = s->pos - s->pos % s->block_size;
    Block *b = blockAt(s, start);
    if (!b)
        fz_throw(ctx, FZ_ERROR_SYSTEM, "The book couldn't be read from the network folder.");
    size_t off = (size_t)(s->pos - start);
    if (off >= b->len)
        return EOF;
    stm->rp = b->data + off;
    stm->wp = b->data + b->len;
    s->pos = start + (int64_t)b->len;
    stm->pos = s->pos;
    return *stm->rp++;
}

static void seekTo(fz_context *ctx, fz_stream *stm, int64_t offset, int whence)
{
    NetStream *s = stm->state;
    int64_t target = offset;
    if (whence == SEEK_CUR)
        target = stm->pos - (stm->wp - stm->rp) + offset;
    else if (whence == SEEK_END) {
        if (s->size < 0)
            fz_throw(ctx, FZ_ERROR_SYSTEM, "The book's size isn't known.");
        target = s->size + offset;
    }
    target = target < 0 ? 0 : s->size >= 0 && target > s->size ? s->size : target;
    s->pos = target;
    stm->pos = target;
    stm->rp = stm->wp = NULL;
}

static void drop(fz_context *ctx, void *state)
{
    (void)ctx;
    NetStream *s = state;
    if (s->file)
        davFileClose(s->file);
    if (s->client)
        netClientClose(s->client);
    for (int i = 0; i < NET_BLOCKS; i++)
        free(s->blocks[i].data);
    memset(s->auth, 0, sizeof(s->auth));
    free(s);
}

static fz_stream *netStreamOpen(fz_context *ctx, const char *location)
{
    NetStream *s = calloc(1, sizeof(*s));
    if (!s)
        fz_throw(ctx, FZ_ERROR_SYSTEM, "Out of memory.");
    // One size for all: big blocks were tried for RAR and 7z (2026-11-24) and were slower,
    // libarchive skipping from header to header rather than reading on.
    s->block_size = NET_BLOCK;
    s->block_count = NET_BLOCKS;
    for (int i = 0; i < NET_BLOCKS; i++)
        s->blocks[i].start = -1;
    s->size = -1;
    char why[256] = "";
    bool ok;
    if (davReadsFiles()) {
        s->file = davFileOpenKept(location, why, sizeof(why));  // One sign-in for all the covers.
        ok = s->file != NULL;
        if (ok)
            s->size = davFileSize(s->file);
    } else {
        s->any_certificate = davAnyCertificate();
        ok = davStreamRequest(location, s->url, sizeof(s->url), s->auth, sizeof(s->auth)) &&
             (s->client = netClientOpen()) != NULL && blockAt(s, 0) != NULL;  // Its first block: and its size.
        if (!ok && !why[0])
            snprintf(why, sizeof(why), "%s", "The book couldn't be read from the network folder.");
    }
    if (!ok) {
        drop(ctx, s);
        fz_throw(ctx, FZ_ERROR_SYSTEM, "%s", why);
    }
    fz_stream *stm = NULL;
    fz_try(ctx)
    {
        stm = fz_new_stream(ctx, s, next, drop);
        stm->seek = seekTo;
    }
    fz_catch(ctx)
    {
        drop(ctx, s);
        fz_rethrow(ctx);
    }
    return stm;
}

void netStreamInstall(void)
{
    docSetRemoteOpener(netStreamOpen);
}
