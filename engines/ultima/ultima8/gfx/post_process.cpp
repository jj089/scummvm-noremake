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

// Glow: in light sources, colours brighter than GLOW_START light up.
static const int GLOW_START = 110;
static const int GLOW_STRENGTH = 300; // 1/256ths
static const int GLOW_SCALE = 4;      // glow resolution divisor
static const int GLOW_RADIUS = 3;     // box blur radius (glow pixels), run twice

PostProcess::PostProcess() : _qw(0), _qh(0) {
	for (int i = 0; i < 256; i++)
		_contrast[i] = CLIP(128 + (((i - 128) * CONTRAST) >> 8), 0, 255);
}

PostProcess::~PostProcess() {
	_out.free();
}

static inline int clampByte(int v) {
	return v < 0 ? 0 : (v > 255 ? 255 : v);
}

bool PostProcess::present(const Graphics::Surface &frame, bool sharpen, bool glow,
						  const Common::Array<uint8> &glowMask, int maskW, int maskH) {
	const Graphics::PixelFormat &fmt = frame.format;
	if (fmt.bytesPerPixel != 4 || fmt.rBits() != 8 || fmt.gBits() != 8 || fmt.bBits() != 8 || frame.h < 3 || frame.w < 3)
		return false;
	if (!sharpen && !glow)
		return false;
	if (_out.w != frame.w || _out.h != frame.h || _out.format != frame.format) {
		_out.free();
		_out.create(frame.w, frame.h, frame.format);
	}
	if (sharpen)
		sharpenInto(frame);
	else
		_out.copyFrom(frame);
	if (glow && buildGlow(frame, glowMask, maskW, maskH))
		addGlow();
	g_system->copyRectToScreen(_out.getPixels(), _out.pitch, 0, 0, _out.w, _out.h);
	g_system->updateScreen();
	return true;
}

void PostProcess::sharpenInto(const Graphics::Surface &frame) {
	const Graphics::PixelFormat &fmt = frame.format;
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
}

bool PostProcess::buildGlow(const Graphics::Surface &frame, const Common::Array<uint8> &mask, int maskW, int maskH) {
	const Graphics::PixelFormat &fmt = frame.format;
	const uint rs = fmt.rShift, gs = fmt.gShift, bs = fmt.bShift;
	_qw = frame.w / GLOW_SCALE;
	_qh = frame.h / GLOW_SCALE;
	if (_qw < 2 || _qh < 2 || maskW != _qw || maskH != _qh || mask.size() != (uint)(_qw * _qh))
		return false;
	for (int c = 0; c < 3; c++)
		_glow[c].resize(_qw * _qh);
	_tmp.resize(_qw * _qh);

	// Source: in cells the game marked as light sources, how far pixels are
	// above GLOW_START, sampled at four points of each cell.
	bool any = false;
	for (int qy = 0; qy < _qh; qy++) {
		for (int qx = 0; qx < _qw; qx++) {
			int sr = 0, sg = 0, sb = 0;
			for (int k = 0; k < 4 && mask[qy * _qw + qx]; k++) {
				const int x = qx * GLOW_SCALE + 1 + (k & 1) * 2, y = qy * GLOW_SCALE + 1 + (k >> 1) * 2;
				const uint32 p = *static_cast<const uint32 *>(frame.getBasePtr(x, y));
				const int r = (p >> rs) & 0xFF, g = (p >> gs) & 0xFF, b = (p >> bs) & 0xFF;
				const int mx = MAX(r, MAX(g, b));
				if (mx <= GLOW_START)
					continue;
				// 0..256: brightness above the threshold
				const int wgt = (mx - GLOW_START) * 256 / (255 - GLOW_START);
				sr += r * wgt;
				sg += g * wgt;
				sb += b * wgt;
			}
			const int i = qy * _qw + qx;
			_glow[0][i] = MIN(255, sr >> 10);
			_glow[1][i] = MIN(255, sg >> 10);
			_glow[2][i] = MIN(255, sb >> 10);
			any = any || _glow[0][i] || _glow[1][i] || _glow[2][i];
		}
	}
	if (!any)
		return false;
	for (int c = 0; c < 3; c++) {
		blurPlane(_glow[c].data());
		blurPlane(_glow[c].data());
	}

	// Stretch each glow row to full width, in the screen's pixel layout.
	const int w = frame.w;
	_glowRows.resize(_qh * w);
	_glowRowUsed.resize(_qh);
	for (int qy = 0; qy < _qh; qy++) {
		const uint8 *pr = &_glow[0][qy * _qw], *pg = &_glow[1][qy * _qw], *pb = &_glow[2][qy * _qw];
		uint32 *row = &_glowRows[qy * w];
		bool used = false;
		for (int x = 0; x < w; x++) {
			const int fx = x * 256 / GLOW_SCALE - 128; // between sample centres
			const int q0 = CLIP(fx >> 8, 0, _qw - 1), q1 = MIN(q0 + 1, _qw - 1);
			const int f = fx < 0 ? 0 : (fx & 0xFF);
			const int r = ((pr[q0] * (256 - f) + pr[q1] * f) >> 8) * GLOW_STRENGTH >> 8;
			const int g = ((pg[q0] * (256 - f) + pg[q1] * f) >> 8) * GLOW_STRENGTH >> 8;
			const int b = ((pb[q0] * (256 - f) + pb[q1] * f) >> 8) * GLOW_STRENGTH >> 8;
			row[x] = ((uint32)MIN(255, r) << rs) | ((uint32)MIN(255, g) << gs) | ((uint32)MIN(255, b) << bs);
			used = used || row[x];
		}
		_glowRowUsed[qy] = used;
	}
	return true;
}

void PostProcess::blurPlane(uint8 *plane) {
	const int n = 2 * GLOW_RADIUS + 1;
	// Horizontal box blur into _tmp...
	for (int y = 0; y < _qh; y++) {
		const uint8 *src = plane + y * _qw;
		uint8 *dst = &_tmp[y * _qw];
		int sum = 0;
		for (int i = -GLOW_RADIUS; i <= GLOW_RADIUS; i++)
			sum += src[CLIP(i, 0, _qw - 1)];
		for (int x = 0; x < _qw; x++) {
			dst[x] = sum / n;
			sum += src[MIN(x + GLOW_RADIUS + 1, _qw - 1)] - src[MAX(x - GLOW_RADIUS, 0)];
		}
	}
	// ...then vertical back into the plane.
	for (int x = 0; x < _qw; x++) {
		int sum = 0;
		for (int i = -GLOW_RADIUS; i <= GLOW_RADIUS; i++)
			sum += _tmp[CLIP(i, 0, _qh - 1) * _qw + x];
		for (int y = 0; y < _qh; y++) {
			plane[y * _qw + x] = sum / n;
			sum += _tmp[MIN(y + GLOW_RADIUS + 1, _qh - 1) * _qw + x] - _tmp[MAX(y - GLOW_RADIUS, 0) * _qw + x];
		}
	}
}

void PostProcess::addGlow() {
	const int w = _out.w;
	for (int y = 0; y < _out.h; y++) {
		// Between the two nearest glow rows.
		const int fy = y * 256 / GLOW_SCALE - 128;
		const int q0 = CLIP(fy >> 8, 0, _qh - 1), q1 = MIN(q0 + 1, _qh - 1);
		const int f = fy < 0 ? 0 : (fy & 0xFF);
		if (!_glowRowUsed[q0] && !_glowRowUsed[q1])
			continue;
		const uint32 *a = &_glowRows[q0 * w], *b = &_glowRows[q1 * w];
		uint32 *out = static_cast<uint32 *>(_out.getBasePtr(0, y));
		int x = 0;
#ifdef POSTPROCESS_SSE2
		const __m128i zero = _mm_setzero_si128();
		const __m128i wa = _mm_set1_epi16(256 - f), wb = _mm_set1_epi16(f);
		for (; x + 4 <= w; x += 4) {
			const __m128i va = _mm_loadu_si128(reinterpret_cast<const __m128i *>(a + x));
			const __m128i vb = _mm_loadu_si128(reinterpret_cast<const __m128i *>(b + x));
			const __m128i lo = _mm_srli_epi16(_mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(va, zero), wa),
															_mm_mullo_epi16(_mm_unpacklo_epi8(vb, zero), wb)), 8);
			const __m128i hi = _mm_srli_epi16(_mm_add_epi16(_mm_mullo_epi16(_mm_unpackhi_epi8(va, zero), wa),
															_mm_mullo_epi16(_mm_unpackhi_epi8(vb, zero), wb)), 8);
			__m128i *o = reinterpret_cast<__m128i *>(out + x);
			_mm_storeu_si128(o, _mm_adds_epu8(_mm_loadu_si128(o), _mm_packus_epi16(lo, hi)));
		}
#endif
		for (; x < w; x++) {
			uint32 res = out[x];
			for (int sh = 0; sh < 32; sh += 8) {
				const int v = (((a[x] >> sh) & 0xFF) * (256 - f) + ((b[x] >> sh) & 0xFF) * f) >> 8;
				const int sum = MIN(255, (int)((res >> sh) & 0xFF) + v);
				res = (res & ~(0xFFu << sh)) | ((uint32)sum << sh);
			}
			out[x] = res;
		}
	}
}

} // End of namespace Ultima8
} // End of namespace Ultima
