// The app's own folder next to its NRO, and moving the old one's files there.
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include "approot.h"
#include "check.h"

static bool folderIs(const char *nro, const char *expected, bool expected_ok)
{
    char out[APPROOT_PATH_MAX];
    bool ok = appRootFolder(nro, "kaireader", out, sizeof(out));
    return ok == expected_ok && strcmp(out, expected) == 0;
}

static void writeText(const char *path, const char *text)
{
    FILE *f = fopen(path, "w");
    if (f) {
        fputs(text, f);
        fclose(f);
    }
}

static bool readIs(const char *path, const char *text)
{
    char buf[64] = "";
    FILE *f = fopen(path, "r");
    if (!f)
        return false;
    size_t n = fread(buf, 1, sizeof(buf) - 1, f);
    buf[n] = '\0';
    fclose(f);
    return strcmp(buf, text) == 0;
}

void testAppRoot(void)
{
    CHECK(folderIs("sdmc:/brewstore/apps/kaireader/KaiReader.nro", "sdmc:/brewstore/apps/kaireader/config", true));
    CHECK(folderIs("sdmc:/switch/KaiReader/KaiReader.nro", "sdmc:/switch/KaiReader/config", true));  // Any case.
    CHECK(folderIs("sdmc:/switch/KaiReader.nro", "sdmc:/switch/kaireader/config", true));  // Never loose in /switch.
    CHECK(folderIs("sdmc:/KaiReader.nro", "sdmc:/kaireader/config", true));
    CHECK(folderIs("sdmc:/switch/apps/KaiReader.nro", "sdmc:/switch/apps/kaireader/config", true));
    // No usable path (launched over the network, or none): the old folder.
    CHECK(folderIs(NULL, "sdmc:/config/kaireader", false));
    CHECK(folderIs("", "sdmc:/config/kaireader", false));
    CHECK(folderIs("KaiReader.nro", "sdmc:/config/kaireader", false));
    CHECK(folderIs("sdmc:/switch/../x/KaiReader.nro", "sdmc:/config/kaireader", false));

    // Making the folders, then moving: what's there stays, the rest is copied, once.
    const char *old = TMP_ROOT "/old/kaireader", *now = TMP_ROOT "/switch/kaireader/config";
    CHECK(appRootMakeFolder(old) && appRootMakeFolder(TMP_ROOT "/old/kaireader/Backgrounds"));
    writeText(TMP_ROOT "/old/kaireader/resume.txt", "old resume");
    writeText(TMP_ROOT "/old/kaireader/settings.txt", "old settings");
    writeText(TMP_ROOT "/old/kaireader/Backgrounds/sea.png", "png");
    CHECK(appRootMakeFolder(now));
    writeText(TMP_ROOT "/switch/kaireader/config/settings.txt", "new settings");
    CHECK(appRootMigrate(old, now) == 2);
    CHECK(readIs(TMP_ROOT "/switch/kaireader/config/resume.txt", "old resume"));
    CHECK(readIs(TMP_ROOT "/switch/kaireader/config/settings.txt", "new settings"));  // Not overwritten.
    CHECK(readIs(TMP_ROOT "/switch/kaireader/config/Backgrounds/sea.png", "png"));
    CHECK(readIs(TMP_ROOT "/old/kaireader/resume.txt", "old resume"));  // The old folder is left.
    // Once: a file added to the old folder later isn't copied.
    writeText(TMP_ROOT "/old/kaireader/later.txt", "later");
    CHECK(appRootMigrate(old, now) == 0);
    struct stat st;
    CHECK(stat(TMP_ROOT "/switch/kaireader/config/later.txt", &st) != 0);
    CHECK(appRootMigrate(now, now) == 0);
    CHECK(appRootMigrate(TMP_ROOT "/nowhere", TMP_ROOT "/switch/other/config") == 0);
}
