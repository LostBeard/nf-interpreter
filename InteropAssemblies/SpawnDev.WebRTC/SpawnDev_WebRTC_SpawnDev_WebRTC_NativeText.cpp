//-----------------------------------------------------------------------------
//
//  SpawnWear NativeText interop - real .tinyfnt font rendering.
//  (Hand-implemented on the tool-generated stub. Signatures must match the managed
//   NativeText class + the _mshl.cpp; only the method bodies are ours. If the interop
//   is re-generated, re-apply these bodies.)
//
//  We parse the .tinyfnt ourselves (format from CLR_GFX_Font::CreateInstance) and blit
//  1bpp glyphs into a 24bpp Windows BMP. The managed side wraps the BMP with
//  new Bitmap(buf, BitmapImageType.Bmp), MakeTransparent(Black), DrawImage - so we never
//  touch the managed framebuffer or a CLR_GFX_Font heap object (no GC roots).
//-----------------------------------------------------------------------------

#include "SpawnDev_WebRTC.h"
#include "SpawnDev_WebRTC_SpawnDev_WebRTC_NativeText.h"
#include <string.h>
#include <stdint.h>

using namespace SpawnDev_WebRTC::SpawnDev_WebRTC;

// ---- font slot table: static buffers hold a copy of the raw .tinyfnt bytes per handle.
// Static (not malloc - nanoFramework bans malloc/free in native code); a couple of small
// fonts is all the watch UI needs. s_fontLen[i] == 0 means the slot is free. ----
#define SW_MAX_FONTS 2
#define SW_FONT_CAP 7168
static uint8_t s_fontData[SW_MAX_FONTS][SW_FONT_CAP];
static int s_fontLen[SW_MAX_FONTS];

static inline uint16_t sw_rd16(const uint8_t *p) { return (uint16_t)(p[0] | (p[1] << 8)); }
static inline uint32_t sw_rd32(const uint8_t *p) { return (uint32_t)(p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24)); }

// .tinyfnt layout: FontDescription(24) + BitmapDescription(12) + Ranges((n+1)*12) +
//   Chars((m+1)*4) + 1bpp atlas ((atlasW+31)/32)*4*height bytes (LSB-first bit per px).
struct SwFontView
{
    const uint8_t *rangesP; // ranges section (n+1 entries, 12B each)
    const uint8_t *charsP;  // chars section  (m+1 entries, 4B each)
    const uint8_t *atlas;   // 1bpp glyph atlas
    int ranges, chars, atlasW, atlasH, wiw;
};

static bool sw_font_view(int handle, SwFontView &v)
{
    if (handle < 0 || handle >= SW_MAX_FONTS || s_fontLen[handle] <= 0)
        return false;
    const uint8_t *d = s_fontData[handle];
    v.ranges = sw_rd16(d + 16);
    v.chars = sw_rd16(d + 18);
    v.atlasW = (int)sw_rd32(d + 24);
    v.atlasH = (int)sw_rd32(d + 28);
    v.wiw = (v.atlasW + 31) / 32;
    v.rangesP = d + 36;
    v.charsP = v.rangesP + (v.ranges + 1) * 12;
    v.atlas = v.charsP + (v.chars + 1) * 4;
    return true;
}

// Map a character to its chars[] index; returns -1 if outside every range.
static int sw_char_index(const SwFontView &v, int c, uint32_t &rangeOffset)
{
    for (int r = 0; r < v.ranges; r++)
    {
        const uint8_t *rp = v.rangesP + r * 12;
        uint32_t idxFirst = sw_rd32(rp);
        int firstChar = sw_rd16(rp + 4);
        int lastChar = sw_rd16(rp + 6);
        if (c >= firstChar && c <= lastChar)
        {
            rangeOffset = sw_rd32(rp + 8);
            return (int)idxFirst + (c - firstChar);
        }
    }
    return -1;
}

// Absolute atlas x + glyph width for a chars[] index (widths = delta of consecutive offsets).
static void sw_glyph(const SwFontView &v, int chrIndex, uint32_t rangeOffset, int &ax, int &w)
{
    const uint8_t *cp = v.charsP + chrIndex * 4;
    int off0 = sw_rd16(cp);
    int off1 = sw_rd16(cp + 4);
    ax = (int)rangeOffset + off0;
    w = off1 - off0;
    if (w < 0) w = 0;
}

static int sw_measure(const SwFontView &v, const char *text)
{
    int total = 0;
    for (const char *p = text; *p; p++)
    {
        uint32_t ro;
        int ci = sw_char_index(v, (unsigned char)*p, ro);
        if (ci < 0) continue;
        int ax, w;
        sw_glyph(v, ci, ro, ax, w);
        total += w;
    }
    return total;
}

signed int NativeText::CreateFont(CLR_RT_TypedArray_UINT8 param0, HRESULT &hr)
{
    (void)hr;
    int len = (int)param0.GetSize();
    if (len < 48 || len > SW_FONT_CAP) return -1; // descriptors minimum; cap = static buffer size
    for (int i = 0; i < SW_MAX_FONTS; i++)
    {
        if (s_fontLen[i] == 0)
        {
            memcpy(s_fontData[i], param0.GetBuffer(), len);
            s_fontLen[i] = len;
            return i;
        }
    }
    return -1;
}

signed int NativeText::MeasureText(signed int param0, const char *param1, HRESULT &hr)
{
    (void)hr;
    SwFontView v;
    if (!sw_font_view(param0, v)) return -1;
    return sw_measure(v, param1 ? param1 : "");
}

signed int NativeText::FontHeight(signed int param0, HRESULT &hr)
{
    (void)hr;
    SwFontView v;
    if (!sw_font_view(param0, v)) return -1;
    return v.atlasH;
}

signed int NativeText::RenderText(signed int param0, const char *param1, signed int param2, CLR_RT_TypedArray_UINT8 param3, HRESULT &hr)
{
    (void)hr;
    SwFontView v;
    if (!sw_font_view(param0, v)) return -1;
    const char *text = param1 ? param1 : "";
    int w = sw_measure(v, text);
    int h = v.atlasH;
    if (w <= 0 || h <= 0) return -1;

    // 24bpp bottom-up Windows BMP (BGR, rows padded to 4 bytes).
    int rowBytes = (w * 3 + 3) & ~3;
    const int pixOff = 54; // 14 (file hdr) + 40 (info hdr)
    int imgSize = rowBytes * h;
    int total = pixOff + imgSize;
    if ((int)param3.GetSize() < total) return -1;
    uint8_t *out = param3.GetBuffer();
    memset(out, 0, total);

    out[0] = 'B'; out[1] = 'M';
    out[2] = (uint8_t)total; out[3] = (uint8_t)(total >> 8); out[4] = (uint8_t)(total >> 16); out[5] = (uint8_t)(total >> 24);
    out[10] = (uint8_t)pixOff;
    out[14] = 40; // biSize
    out[18] = (uint8_t)w; out[19] = (uint8_t)(w >> 8); out[20] = (uint8_t)(w >> 16); out[21] = (uint8_t)(w >> 24);
    out[22] = (uint8_t)h; out[23] = (uint8_t)(h >> 8); out[24] = (uint8_t)(h >> 16); out[25] = (uint8_t)(h >> 24);
    out[26] = 1;  // planes
    out[28] = 24; // bpp
    out[34] = (uint8_t)imgSize; out[35] = (uint8_t)(imgSize >> 8); out[36] = (uint8_t)(imgSize >> 16); out[37] = (uint8_t)(imgSize >> 24);

    uint8_t cr = (uint8_t)((param2 >> 16) & 0xFF);
    uint8_t cg = (uint8_t)((param2 >> 8) & 0xFF);
    uint8_t cb = (uint8_t)(param2 & 0xFF);

    int penX = 0;
    for (const char *p = text; *p; p++)
    {
        uint32_t ro;
        int ci = sw_char_index(v, (unsigned char)*p, ro);
        if (ci < 0) continue;
        int ax, gw;
        sw_glyph(v, ci, ro, ax, gw);
        for (int gy = 0; gy < h; gy++)
        {
            const uint8_t *rowb = v.atlas + gy * v.wiw * 4; // atlas row bytes
            uint8_t *bmpRow = out + pixOff + (h - 1 - gy) * rowBytes; // BMP is bottom-up
            for (int gx = 0; gx < gw; gx++)
            {
                int sx = ax + gx;
                if (rowb[sx >> 3] & (1 << (sx & 7)))
                {
                    uint8_t *px = bmpRow + (penX + gx) * 3;
                    px[0] = cb; px[1] = cg; px[2] = cr;
                }
            }
        }
        penX += gw;
    }
    return total;
}

void NativeText::ReleaseFont(signed int param0, HRESULT &hr)
{
    (void)hr;
    if (param0 >= 0 && param0 < SW_MAX_FONTS)
        s_fontLen[param0] = 0;
}
