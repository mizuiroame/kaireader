# Host builds of the tests, with sanitizers, in the kaireader-host image (MuPDF for the PC).
#   make -C tests run        unit tests
#   make -C tests coverage   the same, then line coverage per file
CC      ?= gcc
SRC     := ../source
SAN     := -fsanitize=address,undefined -fno-omit-frame-pointer
CFLAGS  := -DKAIREADER_VERSION=\"test\" -std=gnu11 -g -O1 -Wall -Wextra -fwrapv -pthread -I$(SRC) $(SAN)
LDFLAGS := $(SAN) -pthread -lmupdf -lmupdf-third -larchive -lm
OBJDIR  := build

TESTED  := util.c approot.c kinetic.c lang.c notice.c doc.c shelf.c reading.c prefs.c settings.c bookmarks.c dav.c md4.c folderlist.c netlist.c touch.c opds.c
TESTS   := test_main.c test_approot.c test_kinetic.c test_lang.c test_notice.c test_doc.c test_shelf.c test_reading.c test_prefs.c test_settings.c test_bookmarks.c test_dav.c test_folderlist.c test_netlist.c test_touch.c test_opds.c

# GCC 12's ASan can hang on kernels with high mmap entropy, so ASLR is off.
# In Docker this needs --security-opt seccomp=unconfined.
NOASLR  := setarch $$(uname -m) -R

.PHONY: run coverage clean fixtures

run: $(OBJDIR)/test_main fixtures
	$(NOASLR) ./$(OBJDIR)/test_main

fixtures: $(OBJDIR)/fixtures/quiet.epub

$(OBJDIR)/fixtures/quiet.epub: make_fixtures.py
	@mkdir -p $(OBJDIR)/fixtures
	python3 make_fixtures.py $(OBJDIR)/fixtures

coverage: run
	cd $(OBJDIR) && gcov -n *.gcda 2>/dev/null | grep -A1 "File '.*source/" | grep -v "^--"

$(OBJDIR)/test_main: $(TESTS) check.h $(addprefix $(SRC)/,$(TESTED))
	@mkdir -p $(OBJDIR)
	cd $(OBJDIR) && $(CC) $(CFLAGS) --coverage -I../$(SRC) \
		$(addprefix ../,$(TESTS)) $(addprefix ../$(SRC)/,$(TESTED)) \
		-o test_main $(LDFLAGS) --coverage

clean:
	rm -rf $(OBJDIR)
