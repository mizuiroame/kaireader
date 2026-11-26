# Renders KaiReader's screens on the PC and checks every touch target, in the
# kaireader-host image (MuPDF for the PC):
#   make -C tests/preview
# Writes build/shots/*.png (from .ppm) with the made-up books of ../make_fixtures.py.
CC      ?= gcc
SRC     := ../../source
OUT     := build
CFLAGS  := -std=gnu11 -O1 -g -Wall -Wextra -pthread -DKAIREADER_VERSION=\"preview\" -I$(SRC) -I../../third_party -I$(OUT)
UI_SRC  := gfx.c font.c ui.c ui_common.c ui_buttons.c ui_hit.c ui_notice.c util.c app.c touch.c kinetic.c \
           image_impl.c lang.c notice.c approot.c shelf.c doc.c reading.c covers.c prefs.c settings.c            motion.c ui_brand.c bookmarks.c app_marks.c ui_marks.c app_zoom.c app_netlib.c ui_net.c netlist.c dav.c md4.c app_server.c opds.c app_free.c catalogs.c languages.c app_welcome.c theme.c background.c app_update.c update.c copybar.c app_news.c app_scroll.c feeds.c feedlist.c article.c textbuf.c feedsuggest.c

.PHONY: run clean

run: $(OUT)/preview
	@mkdir -p $(OUT)/books
	python3 ../make_fixtures.py $(OUT)/books --library
	rm -f $(OUT)/books/broken.epub $(OUT)/books/empty.pdf
	cd $(OUT) && ./preview shots
	python3 topng.py $(OUT)/shots

$(OUT)/font_regular_ttf.c: ../../data/font_regular.ttf embed.py
	python3 embed.py $< $(OUT) font_regular_ttf

$(OUT)/font_bold_ttf.c: ../../data/font_bold.ttf embed.py
	python3 embed.py $< $(OUT) font_bold_ttf

$(OUT)/about_icon_png.c: ../../data/about_icon.png embed.py
	python3 embed.py $< $(OUT) about_icon_png

$(OUT)/brand_seal_png.c: ../../data/brand_seal.png embed.py
	python3 embed.py $< $(OUT) brand_seal_png

$(OUT)/brand_word_png.c: ../../data/brand_word.png embed.py
	python3 embed.py $< $(OUT) brand_word_png

BRAND   := $(OUT)/about_icon_png.c $(OUT)/brand_seal_png.c $(OUT)/brand_word_png.c

LANGS   := de es fr it ja ko pt zh
LANG_C  := $(foreach l,$(LANGS),$(OUT)/lang_$(l)_txt.c)

$(OUT)/lang_%_txt.c: ../../data/lang_%.txt embed.py
	python3 embed.py $< $(OUT) lang_$*_txt

$(OUT)/preview: preview.c font_system_host.c net_host.c $(addprefix $(SRC)/,$(UI_SRC)) $(OUT)/font_regular_ttf.c $(OUT)/font_bold_ttf.c $(BRAND) $(LANG_C)
	$(CC) $(CFLAGS) preview.c font_system_host.c net_host.c $(addprefix $(SRC)/,$(UI_SRC)) \
		$(OUT)/font_regular_ttf.c $(OUT)/font_bold_ttf.c $(BRAND) $(LANG_C) -o $@ -lmupdf -lmupdf-third -larchive -lm -pthread

clean:
	rm -rf $(OUT)
