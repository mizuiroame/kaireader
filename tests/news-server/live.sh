#!/bin/sh
# In the kaireader-host image, with the made-up site served on port 8099 of the PC:
#   docker run --rm --add-host=host.docker.internal:host-gateway -v <repo>:/build/kaireader \
#     -w /build/kaireader/tests/news-server kaireader-host sh live.sh
set -e
command -v curl-config >/dev/null || (apt-get update -qq && apt-get install -y -qq libcurl4-openssl-dev >/dev/null)
mkdir -p build/inc && touch build/inc/switch.h && rm -rf build/kept && mkdir -p build/kept
S=../../source
cc -std=gnu11 -g -O1 -Wall -Wextra -fsanitize=address,undefined -DKAIREADER_VERSION=\"live\" -I$S -Ibuild/inc \
  live.c $S/newsjob.c $S/net.c $S/feeds.c $S/feedlist.c $S/article.c $S/textbuf.c $S/opds.c $S/lang.c $S/shelf.c \
  $S/doc.c $S/util.c $S/approot.c -o build/live -lcurl -lmupdf -lmupdf-third -larchive -lm -pthread
cd build && setarch "$(uname -m)" -R ./live "${1:-http://host.docker.internal:8099}"
