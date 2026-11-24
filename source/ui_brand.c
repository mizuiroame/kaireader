#include "ui_brand.h"

#include <stdlib.h>

#include "brand_seal_png.h"
#include "brand_word_png.h"
#include "font.h"
#include "gfx.h"
#include "image_config.h"
#include "lang.h"
#include "stb_image.h"
#include "ui_common.h"

typedef struct {
    uint32_t *pixels;  // 0xAABBGGRR.
    int w, h;
    bool tried;
} BrandImage;

// Decoded once; kept for as long as KaiReader runs (a few hundred KB).
static const BrandImage *decoded(BrandImage *img, const unsigned char *png, size_t size)
{
    if (!img->tried) {
        img->tried = true;
        int comp;
        img->pixels = (uint32_t *)stbi_load_from_memory(png, (int)size, &img->w, &img->h, &comp, 4);
    }
    return img->pixels ? img : NULL;
}

int uiDrawBrand(int x, int y, int seal_h)
{
    static BrandImage seal, word;
    const BrandImage *s = decoded(&seal, brand_seal_png, brand_seal_png_size);
    const BrandImage *w = decoded(&word, brand_word_png, brand_word_png_size);
    if (!s || !w)
        return 0;
    float seal_w = (float)seal_h * s->w / s->h;
    gfxDrawImageScaledFb(x * GFX_SCALE, y * GFX_SCALE, seal_w * GFX_SCALE, seal_h * GFX_SCALE, s->w, s->h, s->pixels);
    int tx = x + (int)seal_w + 22;
    float word_h = seal_h * 0.2f, word_w = word_h * w->w / w->h;
    fontDraw(FONT_REGULAR, 16, tx, y + (int)(seal_h * 0.5f - word_h - 22), T("Made by"), COLOR_DIM);
    gfxDrawImageScaledFb(tx * GFX_SCALE, (y + seal_h * 0.5f - word_h / 2) * GFX_SCALE, word_w * GFX_SCALE,
                         word_h * GFX_SCALE, w->w, w->h, w->pixels);
    return seal_h;
}
