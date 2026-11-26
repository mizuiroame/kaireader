#include "feedsuggest.h"

#include <string.h>

// Checked on 2026-11-26: each one fetched and read by feeds.c. Keep at most FEED_SUGGEST_MAX each.
static const FeedSuggestion EN[] = {
    {"BBC News", "https://feeds.bbci.co.uk/news/rss.xml"},
    {"The Guardian: World", "https://www.theguardian.com/world/rss"},
    {"NPR News", "https://feeds.npr.org/1001/rss.xml"},
    {"Al Jazeera", "https://www.aljazeera.com/xml/rss/all.xml"},
    {"Ars Technica", "https://feeds.arstechnica.com/arstechnica/index"},
    {"The Verge", "https://www.theverge.com/rss/index.xml"},
    {"Hacker News", "https://hnrss.org/frontpage"},
};
static const FeedSuggestion ES[] = {
    {"El País", "https://feeds.elpais.com/mrss-s/pages/ep/site/elpais.com/portada"},
    {"BBC News Mundo", "https://feeds.bbci.co.uk/mundo/rss.xml"},
    {"DW Español", "https://rss.dw.com/rdf/rss-sp-all"},
    {"Infobae", "https://www.infobae.com/arc/outboundfeeds/rss/"},
    {"20minutos", "https://www.20minutos.es/rss/"},
    {"Xataka", "https://www.xataka.com/feedburner.xml"},
    {"Vandal", "https://vandal.elespanol.com/xml.cgi"},
};
static const FeedSuggestion DE[] = {
    {"tagesschau", "https://www.tagesschau.de/index~rss2.xml"},
    {"DW Deutsch", "https://rss.dw.com/rdf/rss-de-all"},
    {"DER SPIEGEL", "https://www.spiegel.de/schlagzeilen/index.rss"},
    {"heise online", "https://www.heise.de/rss/heise-atom.xml"},
    {"Golem.de", "https://rss.golem.de/rss.php?feed=RSS2.0"},
};
static const FeedSuggestion FR[] = {
    {"franceinfo", "https://www.francetvinfo.fr/titres.rss"},
    {"RFI", "https://www.rfi.fr/fr/rss"},
    {"France 24", "https://www.france24.com/fr/rss"},
    {"Numerama", "https://www.numerama.com/feed/"},
    {"Jeuxvideo.com", "https://www.jeuxvideo.com/rss/rss.xml"},
};
static const FeedSuggestion IT[] = {
    {"ANSA", "https://www.ansa.it/sito/ansait_rss.xml"},
    {"RaiNews", "https://www.rainews.it/rss/tutti"},
    {"Corriere della Sera", "https://xml2.corriereobjects.it/rss/homepage.xml"},
    {"Wired Italia", "https://www.wired.it/feed/rss"},
    {"Everyeye.it", "https://www.everyeye.it/feed/"},
};
static const FeedSuggestion PT[] = {
    {"g1", "https://g1.globo.com/rss/g1/"},
    {"BBC News Brasil", "https://feeds.bbci.co.uk/portuguese/rss.xml"},
    {"DW Brasil", "https://rss.dw.com/rdf/rss-br-all"},
    {"RTP Notícias", "https://www.rtp.pt/noticias/rss"},
    {"TecMundo", "https://rss.tecmundo.com.br/feed"},
};
static const FeedSuggestion JA[] = {
    {"NHKニュース", "https://www3.nhk.or.jp/rss/news/cat0.xml"},
    {"ITmedia", "https://rss.itmedia.co.jp/rss/2.0/itmedia_all.xml"},
    {"GIGAZINE", "https://gigazine.net/news/rss_2.0/"},
    {"4Gamer.net", "https://www.4gamer.net/rss/index.xml"},
};
static const FeedSuggestion KO[] = {
    {"연합뉴스", "https://www.yna.co.kr/rss/news.xml"},
    {"BBC 코리아", "https://feeds.bbci.co.uk/korean/rss.xml"},
    {"경향신문", "https://www.khan.co.kr/rss/rssdata/total_news.xml"},
    {"ZDNet Korea", "https://feeds.feedburner.com/zdkorea"},
};
static const FeedSuggestion ZH[] = {
    {"BBC 中文", "https://feeds.bbci.co.uk/zhongwen/simp/rss.xml"},
    {"DW 中文", "https://rss.dw.com/rdf/rss-chi-all"},
    {"RFI 中文", "https://www.rfi.fr/cn/rss"},
    {"少数派", "https://sspai.com/feed"},
    {"IT之家", "https://www.ithome.com/rss/"},
};

#define LIST(id, a) {id, a, (int)(sizeof(a) / sizeof(a[0]))}
static const struct {
    const char *id;
    const FeedSuggestion *list;
    int count;
} LANGS[] = {LIST("en", EN), LIST("es", ES), LIST("de", DE), LIST("fr", FR), LIST("it", IT),
             LIST("pt", PT), LIST("ja", JA), LIST("ko", KO), LIST("zh", ZH)};
#define LANG_COUNT ((int)(sizeof(LANGS) / sizeof(LANGS[0])))

int feedSuggestLanguages(void)
{
    return LANG_COUNT;
}

const char *feedSuggestLanguage(int index)
{
    return index >= 0 && index < LANG_COUNT ? LANGS[index].id : "";
}

int feedSuggestLanguageIndex(const char *id)
{
    for (int i = 0; id && i < LANG_COUNT; i++)
        if (!strcmp(LANGS[i].id, id))
            return i;
    return -1;
}

int feedSuggestions(int language, const FeedSuggestion **out)
{
    if (language < 0 || language >= LANG_COUNT) {
        *out = NULL;
        return 0;
    }
    *out = LANGS[language].list;
    return LANGS[language].count < FEED_SUGGEST_MAX ? LANGS[language].count : FEED_SUGGEST_MAX;
}
