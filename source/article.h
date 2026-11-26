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

typedef struct {
    const char *title;    // Plain text.
    const char *byline;   // "The Harbor Gazette · 2026-11-27"; may be "".
    const char *html;     // The article's HTML (a part of a page); may be NULL.
    const char *picture;  // A picture without a description: "[picture]", translated.
    const char *note;     // A line at the end (how to read the rest); may be NULL.
} ArticleParts;

// The page (malloc'd, NUL-terminated, its size in *size); NULL if memory ran out.
char *articlePage(const ArticleParts *parts, size_t *size);

// The article in a site's whole page: its main part, cleaned, as HTML for articlePage (malloc'd);
// NULL if no part of it reads as an article.
char *articleExtract(const char *page, size_t size);
