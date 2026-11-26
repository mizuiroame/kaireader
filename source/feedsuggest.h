// Suggested news feeds, by language: well-known sites free to read, picked in
// Settings > News > Suggested feeds so nothing long has to be typed on the
// console. Nothing is followed until one is picked. The table is in
// feedsuggest.c, easy to edit (each feed was checked to parse with feeds.c).
#pragma once

#define FEED_SUGGEST_MAX 7  // Per language: the page's rows, under the language's.

typedef struct {
    const char *name;
    const char *url;
} FeedSuggestion;

// The languages with suggestions ("en", "es"...), and how many.
int feedSuggestLanguages(void);
const char *feedSuggestLanguage(int index);
// A language's place among them; -1 if it has none.
int feedSuggestLanguageIndex(const char *id);
// A language's suggestions (index as above): into *out, how many (at most FEED_SUGGEST_MAX).
int feedSuggestions(int language, const FeedSuggestion **out);
