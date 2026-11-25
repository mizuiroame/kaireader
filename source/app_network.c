// The network folder (dav.h): its Settings page and its test, as in KaiVideo.
#include "app.h"

#include <pthread.h>
#include <stdio.h>
#include <string.h>

#include "davnet.h"
#include "keyboard_system.h"
#include "lang.h"
#include "net.h"
#include "netjob.h"
#include "netstream.h"
#include "opdsnet.h"
#include "roots_sslcom_pem.h"
#include "smbnet.h"

#define TEST_TEXT_MAX 256

static pthread_mutex_t g_test_lock = PTHREAD_MUTEX_INITIALIZER;
static bool g_testing, g_tested;
static char g_test_text[TEST_TEXT_MAX];
static bool g_test_ok;

// The console's network, started while there's a folder: without it, no socket opens.
static bool g_network;

static void accountPath(const App *app, char *out, size_t size)
{
    snprintf(out, size, "%s/%s", app->folder, DAV_ACCOUNT_FILE);
}

static bool g_free_wanted;  // Free books opened: the network stays.

void appNetworkWantFree(void)
{
    g_free_wanted = true;
    appNetworkUse();
}

void appNetworkUse(void)
{
    bool wanted = davConfigured() || opdsConfigured() || g_free_wanted;
    if (wanted && !g_network)
        g_network = netInit();
    else if (!wanted && g_network) {
        netExit();
        g_network = false;
    }
}

void appNetworkFeedSettings(void)
{
    char shown[DAV_ADDRESS_MAX];
    DavAccount a;
    davGetAccount(&a);
    davShownAddress(a.address, shown, sizeof(shown));
    settingsSetNetwork(shown, a.user, !a.forget_password, a.allow_unsigned, davIsSmbAddress(a.address),
                       !a.any_certificate);
    memset(&a, 0, sizeof(a));
}

// Project Gutenberg's certificate comes from SSL.com, whose roots the console's list lacks: those
// two roots (from Mozilla's list, as curl publishes it) written next to KaiReader's files, for it only.
static void addRoots(const App *app)
{
    char path[APP_FOLDER_MAX + 32];
    snprintf(path, sizeof(path), "%s/roots-sslcom.pem", app->folder);
    FILE *f = fopen(path, "rb");
    long have = -1;
    if (f) {
        fseek(f, 0, SEEK_END);
        have = ftell(f);
        fclose(f);
    }
    if (have != (long)roots_sslcom_pem_size) {  // Missing, or of another version: written anew.
        f = fopen(path, "wb");
        if (!f)
            return;
        bool ok = fwrite(roots_sslcom_pem, 1, roots_sslcom_pem_size, f) == roots_sslcom_pem_size;
        if (fclose(f) != 0 || !ok)
            return;
    }
    netAddRoots("gutenberg.org", path);
}

void appNetworkStart(App *app)
{
    addRoots(app);
    char path[APP_FOLDER_MAX + 32];
    accountPath(app, path, sizeof(path));
    davLoadAccount(path);
    netStreamInstall();  // "dav:/" books open over the network (covers, titles).
    appNetworkFeedSettings();
    appNetworkUse();
}

static void save(App *app, const DavAccount *a, uint64_t now)
{
    DavAccount before;
    davGetAccount(&before);
    bool another = strcmp(before.address, a->address) != 0;
    memset(&before, 0, sizeof(before));
    if (another)  // Another folder: the old one's library goes.
        appNetForget(app);
    davUseAccount(a);
    davForget();  // The listing's connections were the old account's.
    char path[APP_FOLDER_MAX + 32];
    accountPath(app, path, sizeof(path));
    if (!davSaveAccount(path))
        appNotify(app, NOTICE_ERROR, T("It couldn't be written to the card."), now);
    appNetworkUse();
    appNetworkFeedSettings();
}

static void typeAddress(App *app, uint64_t now)
{
    DavAccount a;
    davGetAccount(&a);
    char shown[DAV_ADDRESS_MAX], text[DAV_ADDRESS_MAX], address[DAV_ADDRESS_MAX];
    davShownAddress(a.address, shown, sizeof(shown));
    // Room for "https://" and the last '/' that may be added.
    SystemKeyboardResult result = systemKeyboardInput(
        T("Address: \\\\PC\\Books (shared folder), 192.168.0.10:8098 (WebDAV or web), ftp://nas or sftp://nas"), shown,
        text, sizeof(text), sizeof(text) - 10);
    if (result == SYSTEM_KEYBOARD_UNAVAILABLE) {
        appNotify(app, NOTICE_ERROR, T("The keyboard couldn't open."), now);
    } else if (result == SYSTEM_KEYBOARD_OK && !davNormalizeAddress(text, address, sizeof(address))) {
        appNotify(app, NOTICE_ERROR,
                  T("That isn't a network folder's address: \\\\PC\\Books, smb://, http://, https://, ftp://, ftps://, "
                    "ftpes:// or sftp://."),
                  now);
    } else if (result == SYSTEM_KEYBOARD_OK) {
        if (!davSameServer(address, a.address)) {  // Another server: the old one's password isn't sent there.
            a.user[0] = '\0';
            memset(a.password, 0, sizeof(a.password));
            a.host_key[0] = 0;  // And an SFTP server's key is met anew,
            a.any_certificate = false;  // and a certificate checked again.
        }
        memcpy(a.address, address, sizeof(a.address));
        save(app, &a, now);
        appNotify(app, NOTICE_DONE, T("Network folder set: test the connection, then press X in the library to see its books."), now);
    }
    memset(&a, 0, sizeof(a));
}

// After a password is typed: how the card keeps it.
static void sayHowKept(App *app, const DavAccount *a, uint64_t now)
{
    if (!a->user[0] || !a->password[0])
        return;
    if (a->forget_password)
        appNotify(app, NOTICE_DONE, T("Saved until KaiReader closes: the password isn't kept on the card."), now);
    else if (davIsSmbAddress(a->address))
        appNotify(app, NOTICE_DONE,
                  T("Saved: the card keeps the password's hash, which opens this share as the password does. "
                    "Use a user that can only read the books."),
                  now);
    else
        appNotify(app, NOTICE_DONE,
                  T("Saved: the password is only encoded on the card. Use a user that can only read the books."), now);
}

static void typeAccount(App *app, uint64_t now)
{
    DavAccount a;
    davGetAccount(&a);
    if (!a.address[0]) {
        appNotify(app, NOTICE_INFO, T("Enter the folder's address first."), now);
        memset(&a, 0, sizeof(a));
        return;
    }
    char user[DAV_TEXT_MAX], password[DAV_TEXT_MAX] = "";
    SystemKeyboardResult result = systemKeyboardInput(T("User name (empty: none; DOMAIN\\user for a domain)"), a.user,
                                                      user, sizeof(user), sizeof(user) - 1);
    if (result == SYSTEM_KEYBOARD_OK && user[0])
        result = systemKeyboardPassword(T("Password"), password, sizeof(password), sizeof(password) - 1);
    if (result == SYSTEM_KEYBOARD_UNAVAILABLE) {
        appNotify(app, NOTICE_ERROR, T("The keyboard couldn't open."), now);
    } else if (result == SYSTEM_KEYBOARD_OK) {
        snprintf(a.user, sizeof(a.user), "%s", user);
        if (davSetPassword(&a, password)) {
            save(app, &a, now);
            sayHowKept(app, &a, now);
        } else {
            appNotify(app, NOTICE_ERROR, T("That password can't be used."), now);
        }
    }
    memset(password, 0, sizeof(password));
    memset(&a, 0, sizeof(a));
}

bool appNetworkEnsurePassword(App *app, uint64_t now)
{
    if (!davPasswordMissing())
        return true;
    DavAccount a;
    davGetAccount(&a);
    char password[DAV_TEXT_MAX] = "", prompt[DAV_ADDRESS_MAX + 64], shown[DAV_ADDRESS_MAX];
    davShownAddress(a.address, shown, sizeof(shown));
    snprintf(prompt, sizeof(prompt), T("Password for %s (%s)"), a.user, shown);
    SystemKeyboardResult result = systemKeyboardPassword(prompt, password, sizeof(password), sizeof(password) - 1);
    bool ok = result == SYSTEM_KEYBOARD_OK && password[0] && davSetPassword(&a, password);
    if (ok) {
        davUseAccount(&a);  // Kept until KaiReader closes; the card's file stays without it.
        davForget();
    } else if (result == SYSTEM_KEYBOARD_UNAVAILABLE) {
        appNotify(app, NOTICE_ERROR, T("The keyboard couldn't open."), now);
    } else {
        appNotify(app, NOTICE_ERROR, T("Without the password the network folder can't be read."), now);
    }
    memset(password, 0, sizeof(password));
    memset(&a, 0, sizeof(a));
    return ok;
}

static void toggleRemember(App *app, uint64_t now)
{
    DavAccount a;
    davGetAccount(&a);
    a.forget_password = !a.forget_password;
    save(app, &a, now);  // Not remembered: the password leaves the card's file now.
    appNotify(app, NOTICE_INFO,
              a.forget_password ? T("The password won't be kept on the card: KaiReader asks for it when the folder is used.")
                                : T("The password is kept on the card."),
              now);
    memset(&a, 0, sizeof(a));
}

// Only an https folder's connections use it; the book server and the free catalogs have their own.
static void toggleCertificate(App *app, uint64_t now)
{
    DavAccount a;
    davGetAccount(&a);
    a.any_certificate = !a.any_certificate;
    save(app, &a, now);
    memset(&a, 0, sizeof(a));
}

static void toggleUnsigned(App *app, uint64_t now)
{
    DavAccount a;
    davGetAccount(&a);
    if (!davIsSmbAddress(a.address)) {
        appNotify(app, NOTICE_INFO, T("Signing is only for shared folders (SMB)."), now);
    } else {
        a.allow_unsigned = !a.allow_unsigned;
        save(app, &a, now);
        appNotify(app, NOTICE_INFO,
                  a.allow_unsigned ? T("Not signed: someone on your network could change what's read. Use it only for "
                                       "an old NAS.")
                                   : T("The connection is signed again."),
                  now);
    }
    memset(&a, 0, sizeof(a));
}

static void *testThread(void *arg)
{
    (void)arg;
    char text[TEST_TEXT_MAX];
    bool ok = davTest(text, sizeof(text));
    pthread_mutex_lock(&g_test_lock);
    snprintf(g_test_text, sizeof(g_test_text), "%s", text);
    g_test_ok = ok;
    g_testing = false;
    g_tested = true;
    pthread_mutex_unlock(&g_test_lock);
    return NULL;
}

static void test(App *app, uint64_t now)
{
    if (!davConfigured()) {
        appNotify(app, NOTICE_INFO, T("Enter the folder's address first."), now);
        return;
    }
    if (!appNetworkEnsurePassword(app, now))
        return;
    pthread_mutex_lock(&g_test_lock);
    bool busy = g_testing;
    pthread_t thread;
    if (!busy)
        g_testing = pthread_create(&thread, NULL, testThread, NULL) == 0;
    bool started = !busy && g_testing;
    pthread_mutex_unlock(&g_test_lock);
    if (started)
        pthread_detach(thread);
    appNotify(app, NOTICE_INFO, busy || started ? T("Testing the connection...") : T("The network can't be used."), now);
}

static void forget(App *app, uint64_t now)
{
    DavAccount none = {0};
    save(app, &none, now);
    appNotify(app, NOTICE_DONE, T("The network folder is forgotten."), now);
}

bool appNetworkActivate(App *app, SettingRow row, uint64_t now)
{
    switch (row) {
    case SETTING_NET_ADDRESS: typeAddress(app, now); return true;
    case SETTING_NET_ACCOUNT: typeAccount(app, now); return true;
    case SETTING_NET_TEST: test(app, now); return true;
    case SETTING_NET_FORGET: forget(app, now); return true;
    case SETTING_NET_REMEMBER: toggleRemember(app, now); return true;
    case SETTING_NET_UNSIGNED: toggleUnsigned(app, now); return true;
    case SETTING_NET_CERTIFICATE: toggleCertificate(app, now); return true;
    default: return false;
    }
}

void appNetworkUpdate(App *app, uint64_t now)
{
    char text[TEST_TEXT_MAX] = "";
    bool ok = false;
    pthread_mutex_lock(&g_test_lock);
    if (g_tested) {
        ok = g_test_ok;
        snprintf(text, sizeof(text), "%s", g_test_text);
        g_tested = false;
    }
    pthread_mutex_unlock(&g_test_lock);
    if (text[0])
        appNotify(app, ok ? NOTICE_DONE : NOTICE_ERROR, text, now);
}

void appNetworkExit(void)
{
    netJobStop();
    davForget();
    if (g_network)
        netExit();
    g_network = false;
}
