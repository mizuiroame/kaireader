# MuPDF for Switch 2 under the AGPL-3.0, KaiReader's engine for EPUB, PDF, XPS,
# FB2, MOBI and comic books (CBZ). Built with the Switch 2 environment over the
# KaiRadio image (it has libcurl and the libounce devkit):
#   docker build -t kaireader-devkit mupdf
# Left out: JavaScript in PDFs (mujs), OCR (tesseract), barcodes, the document
# writers (extract), and the extra script fonts (tofu, tofu_cjk): books bring
# their fonts, and Chinese, Japanese and Korean come from the console's own
# fonts (main.c), not a 25 MB copy in the NRO.
# The Switch 1 portlibs carry an old MuPDF (1.17): its headers are kept out
# of the build, and this one goes to the Switch 2 portlibs, which come first.
ARG DEVKIT_IMAGE=kairadio-devkit:latest
FROM ${DEVKIT_IMAGE}

ARG MUPDF_VERSION=1.28.6
ARG MUPDF_SHA256=dc85000640a45d90ca9b77b061f6ca67cce0c4d4fbecf8819647baf573eef101

WORKDIR /tmp/mupdf
RUN curl -fsSL -o mupdf.tar.gz "https://mupdf.com/downloads/archive/mupdf-${MUPDF_VERSION}-source.tar.gz" \
 && echo "${MUPDF_SHA256}  mupdf.tar.gz" | sha256sum -c - \
 && tar xzf mupdf.tar.gz --strip-components=1 && rm mupdf.tar.gz

# What newlib does differently, in a header included first (switch_compat.h):
# its <sys/types.h> defines quad as quad_t, which breaks MuPDF's fz_quad fields;
# and it has no timegm (PDF dates), so here is one for UTC.
COPY switch_compat.h /tmp/mupdf/switch_compat.h

RUN . /opt/devkitpro/switch2vars.sh \
 && make -j"$(nproc)" OS=switch2 build=release \
      CC=aarch64-none-elf-gcc CXX=aarch64-none-elf-g++ AR=aarch64-none-elf-ar \
      HAVE_X11=no HAVE_GLUT=no HAVE_CURL=no HAVE_PTHREAD=no HAVE_OBJCOPY=no \
      mujs=no tesseract=no barcode=no extract=no tofu=yes tofu_cjk=yes \
      XCFLAGS="-include /tmp/mupdf/switch_compat.h -D__SWITCH__ -D__SWITCH2__ -isystem $DEVKITPRO/libounce/include $CFLAGS -DFZ_ENABLE_JS=0 -DFZ_ENABLE_ICC=1" \
      libs \
 && make OS=switch2 build=release prefix="$PORTLIBS_PREFIX" \
      mujs=no tesseract=no barcode=no extract=no tofu=yes tofu_cjk=yes install-libs \
 && cp COPYING "$PORTLIBS_PREFIX/include/mupdf/COPYING-AGPL-3.0.txt" \
 && rm -rf /tmp/mupdf
WORKDIR /src
