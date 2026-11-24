// A minimal check macro shared by the host test files.
#pragma once

#include <stdio.h>

extern int g_failures;
extern int g_checks;

#define CHECK(cond)                                                                  \
    do {                                                                             \
        g_checks++;                                                                  \
        if (!(cond)) {                                                               \
            g_failures++;                                                            \
            fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #cond); \
        }                                                                            \
    } while (0)

#define TMP_ROOT "/tmp/kaireader_test"

void testAppRoot(void);
void testKinetic(void);
void testLang(void);
void testNotice(void);
void testDoc(void);
void testShelf(void);
void testReading(void);
void testPrefs(void);
void testSettings(void);
void testBookmarks(void);
void testDav(void);
void testFolderList(void);
void testNetList(void);
void testTouch(void);
void testOpds(void);
