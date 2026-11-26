#include "feedlist.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LINE_MAX_LEN (FEED_URL_MAX + FEED_TEXT_MAX + 16)

// Text for a line of the file: tabs and line breaks made spaces.
static void oneLine(const char *in, char *out, size_t size)
{
    snprintf(out, size, "%s", in ? in : "");
    for (char *c = out; *c; c++)
        if (*c == '\t' || *c == '\n' || *c == '\r')
            *c = ' ';
}

bool feedListLoad(FeedList *l, const char *file)
{
    memset(l, 0, sizeof(*l));
    FILE *f = fopen(file, "r");
    if (!f)
        return true;
    char line[LINE_MAX_LEN];
    while (fgets(line, sizeof(line), f) && l->count < FEEDLIST_MAX) {
        line[strcspn(line, "\r\n")] = '\0';
        if (!line[0] || line[0] == '#')
            continue;
        char *check = strchr(line, '\t');
        char *name = check ? strchr(check + 1, '\t') : NULL;
        if (check)
            *check++ = '\0';
        if (name)
            *name++ = '\0';
        if (strncmp(line, "http://", 7) && strncmp(line, "https://", 8))
            continue;
        bool added;
        int i = feedListAdd(l, line, name ? name : "", &added);
        if (i >= 0 && added)
            l->items[i].any_certificate = check && check[0] == '0';
    }
    bool ok = !ferror(f);
    fclose(f);
    return ok;
}

bool feedListSave(const FeedList *l, const char *file)
{
    char tmp[1024];
    if (snprintf(tmp, sizeof(tmp), "%s.new", file) >= (int)sizeof(tmp))
        return false;
    FILE *f = fopen(tmp, "w");
    if (!f)
        return false;
    fprintf(f, "# KaiReader's news feeds: address, certificate checked (1) or not (0), name\n");
    for (int i = 0; i < l->count; i++)
        fprintf(f, "%s\t%d\t%s\n", l->items[i].url, l->items[i].any_certificate ? 0 : 1, l->items[i].name);
    bool ok = !ferror(f);
    ok = fclose(f) == 0 && ok;
    if (!ok) {
        remove(tmp);
        return false;
    }
    remove(file);  // newlib's rename doesn't replace a file on the SD.
    return rename(tmp, file) == 0;
}

int feedListAdd(FeedList *l, const char *url, const char *name, bool *added)
{
    *added = false;
    char hash[FEED_HASH_LEN + 1];
    feedsHash(url, hash);
    int had = feedListFind(l, hash);
    if (had >= 0)
        return had;
    if (l->count >= FEEDLIST_MAX || !url[0] || strlen(url) >= FEED_URL_MAX || strchr(url, '\t'))
        return -1;
    FeedEntry *e = &l->items[l->count];
    memset(e, 0, sizeof(*e));
    snprintf(e->url, sizeof(e->url), "%s", url);
    oneLine(name, e->name, sizeof(e->name));
    memcpy(e->hash, hash, sizeof(hash));
    *added = true;
    return l->count++;
}

void feedListRemove(FeedList *l, int index)
{
    if (index < 0 || index >= l->count)
        return;
    memmove(&l->items[index], &l->items[index + 1], (size_t)(l->count - index - 1) * sizeof(l->items[0]));
    l->count--;
}

void feedListRename(FeedList *l, int index, const char *name)
{
    if (index >= 0 && index < l->count)
        oneLine(name, l->items[index].name, sizeof(l->items[index].name));
}

int feedListFind(const FeedList *l, const char *hash)
{
    for (int i = 0; i < l->count; i++)
        if (!strcmp(l->items[i].hash, hash))
            return i;
    return -1;
}

bool newsPath(const char *feed_hash, const char *item_id, char *out, size_t size)
{
    char item[FEED_HASH_LEN + 1];
    feedsHash(item_id, item);
    int n = snprintf(out, size, "%s%s/%s", NEWS_PREFIX, feed_hash, item);
    return n > 0 && (size_t)n < size;
}

static bool isHash(const char *s, size_t n)
{
    if (n != FEED_HASH_LEN)
        return false;
    for (size_t i = 0; i < n; i++)
        if (!((s[i] >= '0' && s[i] <= '9') || (s[i] >= 'a' && s[i] <= 'f')))
            return false;
    return true;
}

bool newsPathParts(const char *path, char feed_hash[FEED_HASH_LEN + 1], char item_hash[FEED_HASH_LEN + 1])
{
    size_t prefix = strlen(NEWS_PREFIX);
    if (!path || strncmp(path, NEWS_PREFIX, prefix))
        return false;
    const char *feed = path + prefix, *slash = strchr(feed, '/');
    if (!slash || !isHash(feed, (size_t)(slash - feed)) || !isHash(slash + 1, strlen(slash + 1)))
        return false;
    memcpy(feed_hash, feed, FEED_HASH_LEN);
    feed_hash[FEED_HASH_LEN] = '\0';
    memcpy(item_hash, slash + 1, FEED_HASH_LEN);
    item_hash[FEED_HASH_LEN] = '\0';
    return true;
}

int newsForgetMarks(Shelf *s, int64_t now)
{
    int forgotten = 0;
    int64_t limit = now - (int64_t)NEWS_MARK_DAYS * 86400;
    for (int i = 0; i < s->count; i++) {
        ShelfBook *b = &s->items[i];
        if (b->present || b->percent < 0 || b->last_read >= limit || b->favorite)
            continue;
        b->percent = -1;
        b->last_read = 0;
        forgotten++;
    }
    return forgotten;
}
