#ifndef TR_IMAGE_BLUR_H
#define TR_IMAGE_BLUR_H

#include "../qcommon/q_shared.h"

#define R_BLUR_MAX_LINE 4096

static void R_BlurLine( byte *data, int count, int step, int radius ) {
	byte	src[R_BLUR_MAX_LINE * 4];
	int		sum[4];
	int		i, c, lo, hi, n = radius * 2 + 1;

	for ( i = 0; i < count; i++ ) {
		Com_Memcpy( src + i * 4, data + i * step, 4 );
	}
	for ( c = 0; c < 4; c++ ) {
		sum[c] = 0;
		for ( i = -radius; i <= radius; i++ ) {
			sum[c] += src[( i < 0 ? 0 : i >= count ? count - 1 : i ) * 4 + c];
		}
	}
	for ( i = 0; i < count; i++ ) {
		lo = i - radius < 0 ? 0 : i - radius;
		hi = i + radius + 1 >= count ? count - 1 : i + radius + 1;
		for ( c = 0; c < 4; c++ ) {
			data[i * step + c] = (byte)( ( sum[c] + n / 2 ) / n );
			sum[c] += src[hi * 4 + c] - src[lo * 4 + c];
		}
	}
}

static void R_BlurImage( byte *pic, int *width, int *height ) {
	int		f = *height / 256 > 1 ? *height / 256 : 1;
	int		w = *width / f, h = *height / f;
	int		x, y, i, j, c, sum, radius, pass;
	byte	out[4];

	if ( w < 1 || h < 1 || w > R_BLUR_MAX_LINE || h > R_BLUR_MAX_LINE ) {
		return;
	}
	if ( f > 1 ) {
		for ( y = 0; y < h; y++ ) {
			for ( x = 0; x < w; x++ ) {
				for ( c = 0; c < 4; c++ ) {
					sum = 0;
					for ( j = 0; j < f; j++ ) {
						for ( i = 0; i < f; i++ ) {
							sum += pic[( ( y * f + j ) * *width + x * f + i ) * 4 + c];
						}
					}
					out[c] = (byte)( sum / ( f * f ) );
				}
				Com_Memcpy( pic + ( y * w + x ) * 4, out, 4 );
			}
		}
	}
	radius = (int)( h * 0.024f + 0.5f );
	if ( radius < 1 ) {
		radius = 1;
	}
	for ( pass = 0; pass < 3; pass++ ) {
		for ( y = 0; y < h; y++ ) {
			R_BlurLine( pic + y * w * 4, w, 4, radius );
		}
		for ( x = 0; x < w; x++ ) {
			R_BlurLine( pic + x * 4, h, w * 4, radius );
		}
	}
	*width = w;
	*height = h;
}

#endif
