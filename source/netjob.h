// The network folder's slow work, off the screen's thread: listing a folder
// (netlist.c keeps its folders and books) and copying a book to the card.
// One listing and one copy at a time; the app asks each frame how they go.
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "dav.h"

#define NETJOB_WHY_MAX 256

typedef enum {
    NETJOB_IDLE,
    NETJOB_BUSY,
    NETJOB_DONE,
    NETJOB_FAILED,
} NetJobState;

// Lists location; a listing asked while one runs follows it (only the newest).
void netJobList(const char *location);
bool netJobListing(void);
// A listing come, once: its location, entries (malloc'd, the caller frees them),
// or why it failed. False if none is waiting.
bool netJobTakeList(char *location, size_t location_size, DavEntry **entries, size_t *count, bool *ok, char *why,
                    size_t why_size);

// Copies location to local (through local ".part", then renamed); size from the
// listing, -1 if unknown. False if a copy runs already.
bool netJobCopy(const char *location, const char *local, int64_t size);
void netJobCancelCopy(void);
// How the copy goes: bytes so far and in all (-1 unknown), why it failed.
NetJobState netJobCopyState(int64_t *done, int64_t *total, char *why, size_t why_size);
// After a copy's end is read: idle again.
void netJobForgetCopy(void);

// Waits for both to end (a copy is cancelled first). At exit.
void netJobStop(void);
