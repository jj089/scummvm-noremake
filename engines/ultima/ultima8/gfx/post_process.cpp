/* ScummVM - Graphic Adventure Engine
 *
 * ScummVM is the legal property of its developers, whose names
 * are too numerous to list here. Please refer to the COPYRIGHT
 * file distributed with this source distribution.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "ultima/ultima8/gfx/post_process.h"
#include "common/system.h"
#include "common/util.h"

#if defined(__SSE2__) || defined(_M_X64)
#include <emmintrin.h>
#define POSTPROCESS_SSE2
#endif

namespace Ultima {
namespace Ultima8 {

// Strengths, in 1/256ths.
static const int SHARPEN = 72;     // unsharp amount on a 1 pixel radius
static const int SATURATION = 282; // 1.10x colour
static const int CONTRAST = 266;   // 1.04x around mid grey

PostProcess::PostProcess() {
	for (int i = 0; i < 256; i++)
		_contrast[i] = CLIP(128 + (((i - 128) * CONTRAST) >> 8), 0, 255);
}

PostProcess::~PostProcess() {
	_out.free();
}

static inline int clampByte(int v) {
	return v < 0 ? 0 : (v > 255 ? 255 : v);
}

bool PostProcess::present(const Graphics::Surface &frame) {
	const Graphics::PixelFormat &fmt = frame.format;
	if (fmt.bytesPerPixel != 4 || fmt.rBits() != 8 || fmt.gBits() != 8 || fmt.bBits() != 8 || frame.h < 3 || frame.w < 3)
		return false;
	if (_out.w != frame.w || _out.h != frame.h || _out.format != frame.format) {
		_out.free();
		_out.create(frame.w, frame.h, frame.format);
	}
	// Edge rows and columns are copied as they are.
	memcpy(_out.getBasePtr(0, 0), frame.getBasePtr(0, 0), frame.w * 4);
	memcpy(_out.getBasePtr(0, frame.h - 1), frame.getBasePtr(0, frame.h - 1), frame.w * 4);

	const uint rs = fmt.rShift, gs = fmt.gShift, bs = fmt.bShift;
	const uint32 amask = fmt.aBits() ? (0xFFu << fmt.aShift) : 0;
	const int w = frame.w;

	for (int y = 1; y < frame.h - 1; y++) {
		const uint32 *up = static_cast<const uint32 *>(frame.getBasePtr(0, y - 1));
		const uint32 *row = static_cast<const uint32 *>(frame.getBasePtr(0, y));
		const uint32 *down = static_cast<const uint32 *>(frame.getBasePtr(0, y + 1));
		uint32 *out = static_cast<uint32 *>(_out.getBasePtr(0, y));
		out[0] = row[0];
		out[w - 1] = row[w - 1];
		int x = 1;
#ifdef POSTPROCESS_SSE2
		// Same maths, four pixels at a time (channels must be whole bytes).
		if (!(rs % 8) && !(gs % 8) && !(bs % 8)) {
			const __m128i zero = _mm_setzero_si128();
			int16 wt[4] = { 0, 0, 0, 0 };
			wt[rs / 8] = 77;
			wt[gs / 8] = 150;
			wt[bs / 8] = 29;
			const __m128i lumWeights = _mm_setr_epi16(wt[0], wt[1], wt[2], wt[3], wt[0], wt[1], wt[2], wt[3]);
			const __m128i sharpen = _mm_set1_epi16(SHARPEN * 256);
			const __m128i saturation = _mm_set1_epi16((SATURATION - 256) * 256);
			const __m128i contrast = _mm_set1_epi16((CONTRAST - 256) * 256);
			const __m128i grey = _mm_set1_epi16(128);
			const __m128i maxv = _mm_set1_epi16(255);
			const __m128i alpha = _mm_set1_epi32((int)amask);
			for (; x + 4 <= w - 1; x += 4) {
				const __m128i c4 = _mm_loadu_si128(reinterpret_cast<const __m128i *>(row + x));
				const __m128i n4 = _mm_loadu_si128(reinterpret_cast<const __m128i *>(up + x));
				const __m128i s4 = _mm_loadu_si128(reinterpret_cast<const __m128i *>(down + x));
				const __m128i e4 = _mm_loadu_si128(reinterpret_cast<const __m128i *>(row + x + 1));
				const __m128i w4 = _mm_loadu_si128(reinterpret_cast<const __m128i *>(row + x - 1));
				__m128i res[2];
				for (int half = 0; half < 2; half++) {
					__m128i c, n, sv, e, wv;
					if (half == 0) {
						c = _mm_unpacklo_epi8(c4, zero); n = _mm_unpacklo_epi8(n4, zero); sv = _mm_unpacklo_epi8(s4, zero);
						e = _mm_unpacklo_epi8(e4, zero); wv = _mm_unpacklo_epi8(w4, zero);
					} else {
						c = _mm_unpackhi_epi8(c4, zero); n = _mm_unpackhi_epi8(n4, zero); sv = _mm_unpackhi_epi8(s4, zero);
						e = _mm_unpackhi_epi8(e4, zero); wv = _mm_unpackhi_epi8(w4, zero);
					}
					const __m128i lap = _mm_sub_epi16(_mm_slli_epi16(c, 2),
						_mm_add_epi16(_mm_add_epi16(n, sv), _mm_add_epi16(e, wv)));
					__m128i v = _mm_add_epi16(c, _mm_mulhi_epi16(lap, sharpen));
					v = _mm_max_epi16(_mm_min_epi16(v, maxv), zero);
					// Luminance per pixel, spread over its four channels.
					__m128i m = _mm_madd_epi16(v, lumWeights);
					m = _mm_add_epi32(m, _mm_shuffle_epi32(m, _MM_SHUFFLE(2, 3, 0, 1)));
					m = _mm_srai_epi32(m, 8);
					__m128i lum = _mm_packs_epi32(m, m);
					lum = _mm_unpacklo_epi16(lum, lum);
					v = _mm_add_epi16(v, _mm_mulhi_epi16(_mm_sub_epi16(v, lum), saturation));
					v = _mm_max_epi16(_mm_min_epi16(v, maxv), zero);
					v = _mm_add_epi16(v, _mm_mulhi_epi16(_mm_sub_epi16(v, grey), contrast));
					res[half] = v;
				}
				const __m128i packed = _mm_or_si128(_mm_packus_epi16(res[0], res[1]), alpha);
				_mm_storeu_si128(reinterpret_cast<__m128i *>(out + x), packed);
			}
		}
#endif
		for (; x < w - 1; x++) {
			const uint32 c = row[x], n = up[x], s = down[x], e = row[x + 1], wv = row[x - 1];
			const uint32 orAll = c | n | s | e | wv;
			// Black areas (letterbox, menus' backgrounds) stay as they are.
			if (!((orAll >> rs) & 0xFF) && !((orAll >> gs) & 0xFF) && !((orAll >> bs) & 0xFF)) {
				out[x] = c;
				continue;
			}
#define CH(p, sh) (int)(((p) >> (sh)) & 0xFF)
#define SHARP(sh) clampByte(CH(c, sh) + (((4 * CH(c, sh) - CH(n, sh) - CH(s, sh) - CH(e, sh) - CH(wv, sh)) * SHARPEN) >> 8))
			int r = SHARP(rs), g = SHARP(gs), b = SHARP(bs);
#undef SHARP
#undef CH
			const int lum = (77 * r + 150 * g + 29 * b) >> 8;
			r = _contrast[clampByte(lum + (((r - lum) * SATURATION) >> 8))];
			g = _contrast[clampByte(lum + (((g - lum) * SATURATION) >> 8))];
			b = _contrast[clampByte(lum + (((b - lum) * SATURATION) >> 8))];
			out[x] = ((uint32)r << rs) | ((uint32)g << gs) | ((uint32)b << bs) | amask;
		}
	}
	g_system->copyRectToScreen(_out.getPixels(), _out.pitch, 0, 0, _out.w, _out.h);
	g_system->updateScreen();
	return true;
}

} // End of namespace Ultima8
} // End of namespace Ultima
