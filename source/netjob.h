// The network folder's slow work, off the screen's thread: looking through it
// for books (netlist.c keeps each folder's books) and copying a book to the
// card. One of each at a time; the app asks each frame how they go.
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "dav.h"
#include "opds.h"

#define NETJOB_WHY_MAX 256

typedef enum {
    NETJOB_IDLE,
    NETJOB_BUSY,
    NETJOB_DONE,
    NETJOB_FAILED,
} NetJobState;

// A book found in the network folder: its location and size (-1 unknown).
typedef struct {
    char location[DAV_ADDRESS_MAX];
    int64_t size;
} NetBook;

#define NETJOB_SCAN_DEPTH 4     // Folders in folders, as on the card (SHELF_SCAN_DEPTH).
#define NETJOB_SCAN_MAX 4000    // Books at most (SHELF_MAX_BOOKS).

// Looks through the whole folder (and its folders) for books. False if a scan runs already.
bool netJobScan(void);
// While a scan runs: true, and how many books it found so far.
bool netJobScanning(int *found);
// A scan ended, once: its books (malloc'd, the caller frees them) and whether the top could be read.
bool netJobTakeScan(NetBook **books, int *count, bool *ok, char *why, size_t why_size);

#define NETJOB_SERVER_FEEDS 300  // Feeds of a book server's catalog read at most in a look-through.

// Looks through a book server's catalog from catalog (its lists, their pages), each book
// once. False if a look-through of it runs already.
bool netJobServerScan(const char *catalog);
bool netJobServerScanning(int *found);
// Its end, once: the books (malloc'd, the caller frees them), and whether the catalog could be read.
bool netJobTakeServerScan(OpdsBook **books, int *count, bool *ok, char *why, size_t why_size);

// Downloads a book server's book (its link) to local, as netJobCopy does. False if a copy runs already.
bool netJobCopyLink(const char *link, const char *local, int64_t size);

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
