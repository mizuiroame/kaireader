// A news article as a page KaiReader opens: its title, a line with its feed
// and date, and its text, as XHTML for MuPDF. The site's HTML is cleaned to
// what reads well (paragraphs, headings, lists, quotes, tables, emphasis):
// scripts, styles, frames, forms and menus go, links become their words, and
// pictures become their description or a "[picture]" placeholder. For
// "Read the full article", the article's own page: its <article>, else its
// <main>, else the part with the most paragraphs. Pure, tested on the PC.
#pragma once

#include <stdbool.h>
#include <stddef.h>

#define ARTICLE_MAX (4u << 20)  // A page written, at most (the text is cut past it).
#define ARTICLE_PICTURES_MAX 20  // Pictures shown in an article, at most (the rest: their placeholder).
#define ARTICLE_SRC_MAX 1024

// What a picture of the article is on the card (articlePage asks the app).
typedef enum {
    ARTICLE_PICTURE_NONE,  // Not there: its description, or the placeholder.
    ARTICLE_PICTURE_FILE,  // There: shown, from the file named.
    ARTICLE_PICTURE_OMIT,  // Not a picture to show (a tracking pixel): left out.
} ArticlePicture;

// A picture's src (as the article writes it): what it is on the card, and its file (next to the page).
typedef ArticlePicture (*ArticlePictureFile)(void *user, const char *src, char *file, size_t size);

typedef struct {
    const char *title;    // Plain text.
    const char *byline;   // "The Harbor Gazette · 2026-11-27"; may be "".
    const char *html;     // The article's HTML (a part of a page); may be NULL.
    const char *picture;  // A picture without a description: "[picture]", translated.
    const char *note;     // A line at the end (how to read the rest); may be NULL.
    ArticlePictureFile picture_file;  // May be NULL: every picture its placeholder.
    void *picture_user;
} ArticleParts;

// The page (malloc'd, NUL-terminated, its size in *size); NULL if memory ran out.
char *articlePage(const ArticleParts *parts, size_t *size);

// The article in a site's whole page: its main part, cleaned, as HTML for articlePage (malloc'd);
// NULL if no part of it reads as an article. Its pictures keep their src.
char *articleExtract(const char *page, size_t size);

// The src of an article's pictures (HTML), in order, each once, at most max (data: ones not):
// into out, how many.
int articleSources(const char *html, char (*out)[ARTICLE_SRC_MAX], int max);
