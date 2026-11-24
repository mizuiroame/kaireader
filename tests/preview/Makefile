# Renders KaiReader's screens on the PC and checks every touch target, in the
# kaireader-host image (MuPDF for the PC):
#   make -C tests/preview
# Writes build/shots/*.png (from .ppm) with the made-up books of ../make_fixtures.py.
CC      ?= gcc
SRC     := ../../source
OUT     := build
CFLAGS  := -std=gnu11 -O1 -g -Wall -Wextra -pthread -DKAIREADER_VERSION=\"preview\" -I$(SRC) -I../../third_party -I$(OUT)
UI_SRC  := gfx.c font.c ui.c ui_common.c ui_buttons.c ui_hit.c ui_notice.c util.c app.c touch.c kinetic.c \
           image_impl.c lang.c notice.c approot.c shelf.c doc.c reading.c covers.c prefs.c

.PHONY: run clean

run: $(OUT)/preview
	@mkdir -p $(OUT)/books
	python3 ../make_fixtures.py $(OUT)/books
	rm -f $(OUT)/books/broken.epub $(OUT)/books/empty.pdf
	cd $(OUT) && ./preview shots
	python3 topng.py $(OUT)/shots

$(OUT)/font_regular_ttf.c: ../../data/font_regular.ttf embed.py
	python3 embed.py $< $(OUT) font_regular_ttf

$(OUT)/font_bold_ttf.c: ../../data/font_bold.ttf embed.py
	python3 embed.py $< $(OUT) font_bold_ttf

$(OUT)/preview: preview.c font_system_host.c $(addprefix $(SRC)/,$(UI_SRC)) $(OUT)/font_regular_ttf.c $(OUT)/font_bold_ttf.c
	$(CC) $(CFLAGS) preview.c font_system_host.c $(addprefix $(SRC)/,$(UI_SRC)) \
		$(OUT)/font_regular_ttf.c $(OUT)/font_bold_ttf.c -o $@ -lmupdf -lmupdf-third -lm -pthread

clean:
	rm -rf $(OUT)
