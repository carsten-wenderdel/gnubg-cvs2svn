/*
 * render.c
 *
 * by Gary Wong <gtw@gnu.org>, 1997-2002.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * $Id$
 */

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <assert.h>
#include <stdio.h>
#include <string.h>
#if HAVE_FREETYPE
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_GLYPH_H
#endif
#include <isaac.h>
#include <math.h>
#include <stdlib.h>

#if HAVE_LIBART
#include <libart_lgpl/art_misc.h>
#include <libart_lgpl/art_affine.h>
#include <libart_lgpl/art_point.h>
#include <libart_lgpl/art_vpath.h>
#include <libart_lgpl/art_bpath.h>
#include <libart_lgpl/art_vpath_bpath.h>
#include <libart_lgpl/art_svp.h>
#include <libart_lgpl/art_svp_vpath.h>
#include <libart_lgpl/art_gray_svp.h>
#include <libart_lgpl/art_rgb.h>
#include <libart_lgpl/art_rgb_svp.h>
#endif

#include "render.h"

static randctx rc;
#define RAND irand( &rc )

static int aaanPositions[ 2 ][ 30 ][ 3 ] = { {
    { 51, 25, 7 },
    { 90, 63, 6 }, { 84, 63, 6 }, { 78, 63, 6 }, { 72, 63, 6 }, { 66, 63, 6 },
    { 60, 63, 6 }, { 42, 63, 6 }, { 36, 63, 6 }, { 30, 63, 6 }, { 24, 63, 6 },
    { 18, 63, 6 }, { 12, 63, 6 },
    { 12, 3, -6 }, { 18, 3, -6 }, { 24, 3, -6 }, { 30, 3, -6 }, { 36, 3, -6 },
    { 42, 3, -6 }, { 60, 3, -6 }, { 66, 3, -6 }, { 72, 3, -6 }, { 78, 3, -6 },
    { 84, 3, -6 }, { 90, 3, -6 },
    { 51, 41, -7 }, { 99, 63, 6 }, { 99, 3, -6 }, { 3, 63, 6 }, { 3, 3, -6 }
}, {
    { 51, 25, 7 },
    { 12, 63, 6 }, { 18, 63, 6 }, { 24, 63, 6 }, { 30, 63, 6 }, { 36, 63, 6 },
    { 42, 63, 6 }, { 60, 63, 6 }, { 66, 63, 6 }, { 72, 63, 6 }, { 78, 63, 6 },
    { 84, 63, 6 }, { 90, 63, 6 },
    { 90, 3, -6 }, { 84, 3, -6 }, { 78, 3, -6 }, { 72, 3, -6 }, { 66, 3, -6 },
    { 60, 3, -6 }, { 42, 3, -6 }, { 36, 3, -6 }, { 30, 3, -6 }, { 24, 3, -6 },
    { 18, 3, -6 }, { 12, 3, -6 },
    { 51, 41, -7 }, { 3, 63, 6 }, { 3, 3, -6 }, { 99, 63, 6 }, { 99, 3, -6 }
} };

#if HAVE_FREETYPE
FT_Library ftl;

#if ( FREETYPE_MINOR < 1 ) || ( ( FREETYPE_MINOR == 1 ) && ( FREETYPE_PATCH < 3 ) )
#define FT_GLYPH_FORMAT_BITMAP ft_glyph_format_bitmap
#define FT_PIXEL_MODE_GRAY ft_pixel_mode_grays
#endif

extern unsigned char auchLuxiRB[], auchLuxiSB[], auchLuxiSR[];
extern unsigned int cbLuxiRB, cbLuxiSB, cbLuxiSR;

#endif

renderdata rdDefault = {
    WOOD_ALDER, /* wt */
    { { 1.0, 0.2, 0.2, 0.9 }, { 0.05, 0.05, 0.1, 0.5 } }, /* aarColour */
    { { 1.0, 0.2, 0.2, 0.9 }, { 0.05, 0.05, 0.1, 0.5 } }, /* aarDiceColour */
    { TRUE, TRUE }, /* afDieColour */
    { { 0.7, 0.7, 0.7, 1.0 }, { 0.7, 0.7, 0.7, 1.0 } }, /* aarDiceDotColour */
    { 0.9, 0.9, 0.9, 1.0 }, /* arCubeColour */
    { { 0x30, 0x60, 0x30, 0xFF }, { 0x00, 0x40, 0x00, 0xFF },
      { 0xFF, 0x60, 0x60, 0xFF }, { 0xC0, 0xC0, 0xC0, 0xFF } }, /* aanBoardC */
    { 25, 25, 25, 25 }, /* aSpeckle */
    { 1.5, 1.5 }, /* arRefraction */
    { 0.2, 1.0 }, /* arCoefficient */
    { 3.0, 30.0 }, /* arExponent */
    { 0.2, 1.0 }, /* arDiceCoefficient */
    { 3.0, 30.0 }, /* arDiceExponent */
    { -0.55667, 0.32139, 0.76604 }, /* arLight */
    0.5, /* rRound */
    -1, /* nSize */
    TRUE, /* fHinges */
    TRUE, /* fLabels */
    FALSE, /* fClockwise */
    TRUE  /* dynamic labels */
};

static inline unsigned char clamp( int n ) {

    if( n < 0 )
	return 0;
    else if( n > 0xFF )
	return 0xFF;
    else
	return n;
}

static int intersects( int x0, int y0, int cx0, int cy0,
		       int x1, int y1, int cx1, int cy1 ) {

    return ( y1 + cy1 > y0 ) && ( y1 < y0 + cy0 ) &&
	( x1 + cx1 > x0 ) && ( x1 < x0 + cx0 );
}

static inline double ssqrt( double x ) {

    return x < 0.0 ? 0.0 : sqrt( x );
}

extern void CopyArea( unsigned char *puchDest, int nDestStride,
		      unsigned char *puchSrc, int nSrcStride,
		      int cx, int cy ) {
    int x;
    
    nDestStride -= cx * 3;
    nSrcStride -= cx * 3;

    for( ; cy; cy-- ) {
	for( x = cx; x; x-- ) {
	    *puchDest++ = *puchSrc++;
	    *puchDest++ = *puchSrc++;
	    *puchDest++ = *puchSrc++;
	}
	puchDest += nDestStride;
	puchSrc += nSrcStride;
    }
}

static void CopyAreaClip( unsigned char *puchDest, int nDestStride,
			  int xDest, int yDest, int cxDest, int cyDest,
			  unsigned char *puchSrc, int nSrcStride,
			  int xSrc, int ySrc, int cx, int cy ) {
    if( xSrc < 0 ) {
	cx += xSrc;
	xDest -= xSrc;
	xSrc = 0;
    }

    if( ySrc < 0 ) {
	cy += ySrc;
	yDest -= ySrc;
	ySrc = 0;
    }

    if( xDest < 0 ) {
	cx += xDest;
	xSrc -= xDest;
	xDest = 0;
    }
    
    if( yDest < 0 ) {
	cy += yDest;
	ySrc -= yDest;
	yDest = 0;
    }
    
    if( xDest + cx > cxDest )
	cx = cxDest - xDest;
    
    if( yDest + cy > cyDest )
	cy = cyDest - yDest;
    
    if( cx <= 0 || cy <= 0 )
	return;

    return CopyArea( puchDest + yDest * nDestStride + xDest * 3, nDestStride,
		     puchSrc + ySrc * nSrcStride + xSrc * 3, nSrcStride,
		     cx, cy );
}

extern void CopyAreaRotateClip( unsigned char *puchDest, int nDestStride,
				int xDest, int yDest, int cxDest, int cyDest,
				unsigned char *puchSrc, int nSrcStride,
				int xSrc, int ySrc, int cx, int cy,
				int nTheta ) {

    int x, nSrcPixelStride = 0, nSrcRowStride = 0;
    
    if( !( nTheta %= 4 ) )
	return CopyAreaClip( puchDest, nDestStride, xDest, yDest, cxDest,
			     cyDest, puchSrc, nSrcStride, xSrc, ySrc, cx, cy );

    puchSrc += ySrc * nSrcStride + xSrc * 3;
    
    switch( nTheta % 4 ) {
    case 1:
	/* 90 deg anticlockwise */
	puchSrc += ( cy - 1 ) * 3; /* start at top right */
	nSrcPixelStride = nSrcStride; /* move down... */
	nSrcRowStride = -3; /* ...then left */
	break;
	
    case 2:
	/* 180 deg */
	puchSrc += ( cx - 1 ) * nSrcStride + ( cy - 1 ) * 3; /* start at
								bottom right */
	nSrcPixelStride = -3; /* move left... */
	nSrcRowStride = -nSrcStride; /* ...then up */
	break;
	
    case 3:
	/* 90 deg clockwise */
	puchSrc += ( cx - 1 ) * nSrcStride; /* start at bottom left */
	nSrcPixelStride = -nSrcStride; /* move up... */
	nSrcRowStride = 3; /* ...then right */
	break;
    }
    
    if( xDest < 0 ) {
	cx += xDest;
	puchSrc -= xDest * nSrcPixelStride;
	xDest = 0;
    }
    
    if( yDest < 0 ) {
	cy += yDest;
	puchSrc -= yDest * nSrcRowStride;
	yDest = 0;
    }
    
    if( xDest + cx > cxDest )
	cx = cxDest - xDest;
    
    if( yDest + cy > cyDest )
	cy = cyDest - yDest;
    
    if( cx <= 0 || cy <= 0 )
	return;

    puchDest += yDest * nDestStride + xDest * 3;
    
    nDestStride -= cx * 3;
    nSrcRowStride -= cx * nSrcPixelStride;
    
    for( ; cy; cy-- ) {
	for( x = cx; x; x-- ) {
	    *puchDest++ = puchSrc[ 0 ];
	    *puchDest++ = puchSrc[ 1 ];
	    *puchDest++ = puchSrc[ 2 ];
	    puchSrc += nSrcPixelStride;
	}
	puchDest += nDestStride;
	puchSrc += nSrcRowStride;
    }
}

static void FillArea( unsigned char *puchDest, int nDestStride,
		      int cx, int cy, unsigned char r, unsigned char g,
		      unsigned char b ) {
    int x;
    
    nDestStride -= cx * 3;

    for( ; cy; cy-- ) {
	for( x = cx; x; x-- ) {
	    *puchDest++ = r;
	    *puchDest++ = g;
	    *puchDest++ = b;
	}
	puchDest += nDestStride;
    }
}

extern void AlphaBlend( unsigned char *puchDest, int nDestStride,
			unsigned char *puchBack, int nBackStride,
			unsigned char *puchFore, int nForeStride,
			int cx, int cy ) {
    int x;
    
    nDestStride -= cx * 3;
    nBackStride -= cx * 3;
    nForeStride -= cx * 4;

    for( ; cy; cy-- ) {
	for( x = cx; x; x-- ) {
	    unsigned int a = puchFore[ 3 ];
	    
	    *puchDest++ = clamp( ( *puchBack++ * a ) / 0xFF + *puchFore++ );
	    *puchDest++ = clamp( ( *puchBack++ * a ) / 0xFF + *puchFore++ );
	    *puchDest++ = clamp( ( *puchBack++ * a ) / 0xFF + *puchFore++ );
	    puchFore++; /* skip the alpha channel */
	}
	puchDest += nDestStride;
	puchBack += nBackStride;
	puchFore += nForeStride;
    }
}

extern void AlphaBlendClip( unsigned char *puchDest, int nDestStride,
			    int xDest, int yDest, int cxDest, int cyDest,
			    unsigned char *puchBack, int nBackStride,
			    int xBack, int yBack,
			    unsigned char *puchFore, int nForeStride,
			    int xFore, int yFore, int cx, int cy ) {

    if( xFore < 0 ) {
	cx += xFore;
	xDest -= xFore;
	xBack -= xFore;
	xFore = 0;
    }

    if( yFore < 0 ) {
	cy += yFore;
	yDest -= yFore;
	yBack -= yFore;
	yFore = 0;
    }

    if( xDest < 0 ) {
	cx += xDest;
	xBack -= xDest;
	xFore -= xDest;
	xDest = 0;
    }
    
    if( yDest < 0 ) {
	cy += yDest;
	yBack -= yDest;
	yFore -= yDest;
	yDest = 0;
    }
    
    if( xDest + cx > cxDest )
	cx = cxDest - xDest;
    
    if( yDest + cy > cyDest )
	cy = cyDest - yDest;
    
    if( cx <= 0 || cy <= 0 )
	return;

    AlphaBlend( puchDest + yDest * nDestStride + xDest * 3, nDestStride,
		puchBack + yBack * nBackStride + xBack * 3, nBackStride,
		puchFore + yFore * nForeStride + xFore * 4, nForeStride,
		cx, cy );
}

extern void AlphaBlend2( unsigned char *puchDest, int nDestStride,
			 unsigned char *puchBack, int nBackStride,
			 unsigned char *puchFore, int nForeStride,
			 int cx, int cy ) {
/* draw *puchFore on top of *puchBack using the alpha channel as mask into *puchDest */

    int x;
    
    nDestStride -= cx * 3;
    nBackStride -= cx * 3;
    nForeStride -= cx * 4;

    for( ; cy; cy-- ) {
	for( x = cx; x; x-- ) {
	    unsigned int a = puchFore[ 3 ];

	    *puchDest++ = clamp( ( *puchBack++ * ( 0xFF - a ) ) / 0xFF
				 + ( *puchFore++ * a ) / 0xFF );
	    *puchDest++ = clamp( ( *puchBack++ * ( 0xFF - a ) ) / 0xFF
				 + ( *puchFore++ * a ) / 0xFF );
	    *puchDest++ = clamp( ( *puchBack++ * ( 0xFF - a ) ) / 0xFF
				 + ( *puchFore++ * a ) / 0xFF );
	    puchFore++; /* skip the alpha channel */
	}
	puchDest += nDestStride;
	puchBack += nBackStride;
	puchFore += nForeStride;
    }
}

extern void AlphaBlendClip2( unsigned char *puchDest, int nDestStride,
			     int xDest, int yDest, int cxDest, int cyDest,
			     unsigned char *puchBack, int nBackStride,
			     int xBack, int yBack,
			     unsigned char *puchFore, int nForeStride,
			     int xFore, int yFore, int cx, int cy ) {
/* draw *puchFore on top of *puchBack using the alpha channel as mask into *puchDest */

    if( xFore < 0 ) {
	cx += xFore;
	xDest -= xFore;
	xBack -= xFore;
	xFore = 0;
    }

    if( yFore < 0 ) {
	cy += yFore;
	yDest -= yFore;
	yBack -= yFore;
	yFore = 0;
    }

    if( xDest < 0 ) {
	cx += xDest;
	xBack -= xDest;
	xFore -= xDest;
	xDest = 0;
    }
    
    if( yDest < 0 ) {
	cy += yDest;
	yBack -= yDest;
	yFore -= yDest;
	yDest = 0;
    }
    
    if( xDest + cx > cxDest )
	cx = cxDest - xDest;
    
    if( yDest + cy > cyDest )
	cy = cyDest - yDest;

    if( cx <= 0 || cy <= 0 )
	return;

    AlphaBlend2( puchDest + yDest * nDestStride + xDest * 3, nDestStride,
		 puchBack + yBack * nBackStride + xBack * 3, nBackStride,
		 puchFore + yFore * nForeStride + xFore * 4, nForeStride,
		 cx, cy );
}

extern void RefractBlend( unsigned char *puchDest, int nDestStride,
			  unsigned char *puchBack, int nBackStride,
			  unsigned char *puchFore, int nForeStride,
			  unsigned short *psRefract, int nRefractStride,
			  int cx, int cy ) {
    int x;
	
    nDestStride -= cx * 3;
    nForeStride -= cx * 4;
    nRefractStride -= cx;
    
    for( ; cy; cy-- ) {
	for( x = cx; x; x-- ) {
	    unsigned int a = puchFore[ 3 ];
	    unsigned char *puch = puchBack +
		( *psRefract >> 8 ) * nBackStride +
		( *psRefract & 0xFF ) * 3;
	    
	    *puchDest++ = clamp( ( puch[ 0 ] * a ) / 0xFF + *puchFore++ );
	    *puchDest++ = clamp( ( puch[ 1 ] * a ) / 0xFF + *puchFore++ );
	    *puchDest++ = clamp( ( puch[ 2 ] * a ) / 0xFF + *puchFore++ );
	    puchFore++; /* skip the alpha channel */
	    psRefract++;
	}
	puchDest += nDestStride;
	puchFore += nForeStride;
	psRefract += nRefractStride;
    }
}

extern void RefractBlendClip( unsigned char *puchDest, int nDestStride,
			      int xDest, int yDest, int cxDest, int cyDest,
			      unsigned char *puchBack, int nBackStride,
			      int xBack, int yBack,
			      unsigned char *puchFore, int nForeStride,
			      int xFore, int yFore,
			      unsigned short *psRefract, int nRefractStride,
			      int cx, int cy ) {
    if( xFore < 0 ) {
	cx += xFore;
	xDest -= xFore;
	xFore = 0;
    }

    if( yFore < 0 ) {
	cy += yFore;
	yDest -= yFore;
	yFore = 0;
    }

    if( xDest < 0 ) {
	cx += xDest;
	xFore -= xDest;
	xDest = 0;
    }
    
    if( yDest < 0 ) {
	cy += yDest;
	yFore -= yDest;
	yDest = 0;
    }
    
    if( xDest + cx > cxDest )
	cx = cxDest - xDest;
    
    if( yDest + cy > cyDest )
	cy = cyDest - yDest;
    
    if( cx <= 0 || cy <= 0 )
	return;

    RefractBlend( puchDest + yDest * nDestStride + xDest * 3, nDestStride,
		  puchBack + yBack * nBackStride + xBack * 3, nBackStride,
		  puchFore + yFore * nForeStride + xFore * 4, nForeStride,
		  psRefract + yFore * nRefractStride + xFore, nRefractStride,
		  cx, cy );
}

static void RenderBorder( unsigned char *puch, int nStride, int x0, int y0,
			  int x1, int y1, int nSize, unsigned char *colours,
			  int fInvert ) {
    
#define COLOURS( ipix, edge, icol ) \
	( *( colours + ( (ipix) * 4 + (edge) ) * 3 + (icol) ) )

    int i, x, y, iCol;

    x0 *= nSize;
    x1 *= nSize;
    y0 *= nSize;
    y1 *= nSize;
    x1--;
    y1--;
    
    for( i = 0; i < nSize; i++ ) {
	for( x = x0; x < x1; x++ )
	    for( iCol = 0; iCol < 3; iCol++ ) 
		puch[ y0 * nStride + x * 3 + iCol ] = fInvert ?
		    COLOURS( nSize - i - 1, 1, iCol ) : COLOURS( i, 3, iCol );

	for( x = x0; x < x1; x++ )
	    for( iCol = 0; iCol < 3; iCol++ ) 
		puch[ y1 * nStride + x * 3 + iCol ] = fInvert ?
		    COLOURS( nSize - i - 1, 3, iCol ) : COLOURS( i, 1, iCol );

	for( y = y0; y < y1; y++ )
	    for( iCol = 0; iCol < 3; iCol++ ) 
		puch[ y * nStride + x0 * 3 + iCol ] = fInvert ?
		    COLOURS( nSize - i - 1, 0, iCol ) : COLOURS( i, 2, iCol );

	for( y = y0; y < y1; y++ )
	    for( iCol = 0; iCol < 3; iCol++ ) 
		puch[ y * nStride + x1 * 3 + iCol ] = fInvert ?
		    COLOURS( nSize - i - 1, 2, iCol ) : COLOURS( i, 0, iCol );

	x0++;
	x1--;
	y0++;
	y1--;
    }
}

static void RenderFramePainted( renderdata *prd, unsigned char *puch,
				int nStride ) {
    int i, ix;
    float x, z, cos_theta, diffuse, specular;
    unsigned char colours[ 4 * 3 * prd->nSize ];
    
    diffuse = 0.8 * prd->arLight[ 2 ] + 0.2;
    specular = pow( prd->arLight[ 2 ], 20 ) * 0.6;

    FillArea( puch, nStride, prd->nSize * 108, prd->nSize * 72,
	      clamp( specular * 0x100 +
		     diffuse * prd->aanBoardColour[ 1 ][ 0 ] ),
	      clamp( specular * 0x100 +
		     diffuse * prd->aanBoardColour[ 1 ][ 1 ] ),
	      clamp( specular * 0x100 +
		     diffuse * prd->aanBoardColour[ 1 ][ 2 ] ) );
    
    for( ix = 0; ix < prd->nSize; ix++ ) {
	x = 1.0 - ( (float) ix / prd->nSize );
	z = ssqrt( 1.0 - x * x );

	for( i = 0; i < 4; i++ ) {
	    cos_theta = prd->arLight[ 2 ] * z + prd->arLight[ i & 1 ] * x;
	    if( cos_theta < 0 )
		cos_theta = 0;
	    diffuse = 0.8 * cos_theta + 0.2;
	    cos_theta = 2 * z * cos_theta - prd->arLight[ 2 ];
	    specular = pow( cos_theta, 20 ) * 0.6;

	    COLOURS( ix, i, 0 ) = clamp( specular * 0x100 + diffuse *
					 prd->aanBoardColour[ 1 ][ 0 ] );
	    COLOURS( ix, i, 1 ) = clamp( specular * 0x100 + diffuse *
					 prd->aanBoardColour[ 1 ][ 1 ] );
	    COLOURS( ix, i, 2 ) = clamp( specular * 0x100 + diffuse *
					 prd->aanBoardColour[ 1 ][ 2 ] );

	    if( !( i & 1 ) )
		x = -x;
	}
    }
#undef COLOURS

    RenderBorder( puch, nStride, 0, 0, 54, 72, prd->nSize, colours, FALSE );
    RenderBorder( puch, nStride, 54, 0, 108, 72, prd->nSize, colours, FALSE );
    
    RenderBorder( puch, nStride, 2, 2, 10, 34, prd->nSize, colours, TRUE );
    RenderBorder( puch, nStride, 2, 38, 10, 70, prd->nSize, colours, TRUE );
    RenderBorder( puch, nStride, 98, 2, 106, 34, prd->nSize, colours, TRUE );
    RenderBorder( puch, nStride, 98, 38, 106, 70, prd->nSize, colours, TRUE );
		  
    RenderBorder( puch, nStride, 11, 2, 49, 70, prd->nSize, colours, TRUE );
    RenderBorder( puch, nStride, 59, 2, 97, 70, prd->nSize, colours, TRUE );
}

static float WoodHash( float r ) {
    /* A quick and dirty hash function for floating point numbers; returns
       a value in the range [0,1). */

    int n;
    float x;

    if( !r )
	return 0;
    
    x = frexp( r, &n );

    return fabs( frexp( x * 131073.1294427 + n, &n ) ) * 2 - 1;
}

static void WoodPixel( float x, float y, float z, unsigned char auch[ 3 ],
		       woodtype wt ) {
    float r;
    int grain, figure;

    r = sqrt( x * x + y * y );
    r -= z / 60;

    switch( wt ) {
    case WOOD_ALDER:
	r *= 3;
	grain = ( (int) r % 60 );

	if( grain < 10 ) {
	    auch[ 0 ] = 230 - grain * 2;
	    auch[ 1 ] = 100 - grain;
	    auch[ 2 ] = 20 - grain / 2;
	} else if( grain < 20 ) {
	    auch[ 0 ] = 210 + ( grain - 10 ) * 2;
	    auch[ 1 ] = 90 + ( grain - 10 );
	    auch[ 2 ] = 15 + ( grain - 10 ) / 2;
	} else {
	    auch[ 0 ] = 230 + ( grain % 3 );
	    auch[ 1 ] = 100 + ( grain % 3 );
	    auch[ 2 ] = 20 + ( grain % 3 );
	}
	
	figure = r / 29 + x / 15 + y / 17 + z / 29;
	
	if( figure % 3 == ( grain / 3 ) % 3 ) {
	    auch[ 0 ] -= WoodHash( figure + grain ) * 8;
	    auch[ 1 ] -= WoodHash( figure + grain ) * 4;
	    auch[ 2 ] -= WoodHash( figure + grain ) * 2;
	}
	
	break;
	
    case WOOD_ASH:
	r *= 3;
	grain = ( (int) r % 60 );

	if( grain > 40 )
	    grain = 120 - 2 * grain;

	grain *= WoodHash( (int) r / 60 ) * 0.7 + 0.3;
	
	auch[ 0 ] = 230 - grain;
	auch[ 1 ] = 125 - grain / 2;
	auch[ 2 ] = 20 - grain / 8;

	figure = r / 53 + x / 5 + y / 7 + z / 50;
	
	if( figure % 3 == ( grain / 3 ) % 3 ) {
	    auch[ 0 ] -= WoodHash( figure + grain ) * 16;
	    auch[ 1 ] -= WoodHash( figure + grain ) * 8;
	    auch[ 2 ] -= WoodHash( figure + grain ) * 4;
	}

	break;
	
    case WOOD_BASSWOOD:
	r *= 5;
	grain = ( (int) r % 60 );

	if( grain > 50 )
	    grain = 60 - grain;
	else if( grain > 40 )
	    grain = 10;
	else if( grain > 30 )
	    grain -= 30;
	else
	    grain = 0;
 
	auch[ 0 ] = 230 - grain;
	auch[ 1 ] = 205 - grain;
	auch[ 2 ] = 150 - grain;
	
	break;
	
    case WOOD_BEECH:
	r *= 3;
	grain = ( (int) r % 60 );

	if( grain > 40 )
	    grain = 120 - 2 * grain;

	auch[ 0 ] = 230 - grain;
	auch[ 1 ] = 125 - grain / 2;
	auch[ 2 ] = 20 - grain / 8;

	figure = r / 29 + x / 15 + y / 17 + z / 29;
	
	if( figure % 3 == ( grain / 3 ) % 3 ) {
	    auch[ 0 ] -= WoodHash( figure + grain ) * 16;
	    auch[ 1 ] -= WoodHash( figure + grain ) * 8;
	    auch[ 2 ] -= WoodHash( figure + grain ) * 4;
	}

	break;
	
    case WOOD_CEDAR:
	r *= 3;
	grain = ( (int) r % 60 );

	if( grain < 10 ) {
	    auch[ 0 ] = 230 + grain;
	    auch[ 1 ] = 135 + grain;
	    auch[ 2 ] = 85 + grain / 2;
	} else if( grain < 20 ) {
	    auch[ 0 ] = 240 - ( grain - 10 ) * 3;
	    auch[ 1 ] = 145 - ( grain - 10 ) * 3;
	    auch[ 2 ] = 90 - ( grain - 10 ) * 3 / 2;
	} else if( grain < 30 ) {
	    auch[ 0 ] = 200 + grain;
	    auch[ 1 ] = 105 + grain;
	    auch[ 2 ] = 70 + grain / 2;
	} else {
	    auch[ 0 ] = 230 + ( grain % 3 );
	    auch[ 1 ] = 135 + ( grain % 3 );
	    auch[ 2 ] = 85 + ( grain % 3 );
	}
	
	break;
	
    case WOOD_EBONY:
	r *= 3;
	grain = ( (int) r % 60 );

	if( grain > 40 )
	    grain = 120 - 2 * grain;

	auch[ 0 ] = 30 + grain / 4;
	auch[ 1 ] = 10 + grain / 8;
	auch[ 2 ] = 0;

	break;
	
    case WOOD_FIR:
	r *= 5;
	grain = ( (int) r % 60 );

	if( grain < 10 ) {
	    auch[ 0 ] = 230 - grain * 2 + grain % 3 * 3;
	    auch[ 1 ] = 100 - grain * 2 + grain % 3 * 3;
	    auch[ 2 ] = 20 - grain + grain % 3 * 3;
	} else if( grain < 30 ) {
	    auch[ 0 ] = 210 + grain % 3 * 3;
	    auch[ 1 ] = 80 + grain % 3 * 3;
	    auch[ 2 ] = 10 + grain % 3 * 3;
	} else if( grain < 40 ) {
	    auch[ 0 ] = 210 + ( grain - 30 ) * 2 + grain % 3 * 3;
	    auch[ 1 ] = 80 + ( grain - 30 ) * 2 + grain % 3 * 3;
	    auch[ 2 ] = 10 + ( grain - 30 ) + grain % 3 * 3;
	} else {
	    auch[ 0 ] = 230 + grain % 3 * 5;
	    auch[ 1 ] = 100 + grain % 3 * 5;
	    auch[ 2 ] = 20 + grain % 3 * 5;
	}
	
	break;
	
    case WOOD_MAPLE:
	r *= 3;
	grain = ( (int) r % 60 );

	if( grain < 10 ) {
	    auch[ 0 ] = 230 - grain * 2 + grain % 3;
	    auch[ 1 ] = 180 - grain * 2 + grain % 3;
	    auch[ 2 ] = 50 - grain + grain % 3;
	} else if( grain < 20 ) {
	    auch[ 0 ] = 210 + grain % 3;
	    auch[ 1 ] = 160 + grain % 3;
	    auch[ 2 ] = 40 + grain % 3;
	} else if( grain < 30 ) {
	    auch[ 0 ] = 210 + ( grain - 20 ) * 2 + grain % 3;
	    auch[ 1 ] = 160 + ( grain - 20 ) * 2 + grain % 3;
	    auch[ 2 ] = 40 + ( grain - 20 ) + grain % 3;
	} else {
	    auch[ 0 ] = 230 + grain % 3;
	    auch[ 1 ] = 180 + grain % 3;
	    auch[ 2 ] = 50 + grain % 3;
	}

	break;
	    
    case WOOD_OAK:
	r *= 4;
	grain = ( (int) r % 60 );

	if( grain > 40 )
	    grain = 120 - 2 * grain;

	grain *= WoodHash( (int) r / 60 ) * 0.7 + 0.3;
	
	auch[ 0 ] = 230 + grain / 2;
	auch[ 1 ] = 125 + grain / 3;
	auch[ 2 ] = 20 + grain / 8;

	figure = r / 53 + x / 5 + y / 7 + z / 30;
	
	if( figure % 3 == ( grain / 3 ) % 3 ) {
	    auch[ 0 ] -= WoodHash( figure + grain ) * 32;
	    auch[ 1 ] -= WoodHash( figure + grain ) * 16;
	    auch[ 2 ] -= WoodHash( figure + grain ) * 8;
	}

	break;
	
    case WOOD_PINE:
	r *= 2;
	grain = ( (int) r % 60 );

	if( grain < 10 ) {
	    auch[ 0 ] = 230 + grain * 2 + grain % 3 * 3;
	    auch[ 1 ] = 160 + grain * 2 + grain % 3 * 3;
	    auch[ 2 ] = 50 + grain + grain % 3 * 3;
	} else if( grain < 20 ) {
	    auch[ 0 ] = 250 + grain % 3;
	    auch[ 1 ] = 180 + grain % 3;
	    auch[ 2 ] = 60 + grain % 3;
	} else if( grain < 30 ) {
	    auch[ 0 ] = 250 - ( grain - 20 ) * 2 + grain % 3;
	    auch[ 1 ] = 180 - ( grain - 20 ) * 2 + grain % 3;
	    auch[ 2 ] = 50 - ( grain - 20 ) + grain % 3;
	} else {
	    auch[ 0 ] = 230 + grain % 3 * 3;
	    auch[ 1 ] = 160 + grain % 3 * 3;
	    auch[ 2 ] = 50 + grain % 3 * 3;
	}

	break;
	
    case WOOD_REDWOOD:
	r *= 5;
	grain = ( (int) r % 60 );

	if( grain > 40 )
	    grain = 120 - 2 * grain;

	auch[ 0 ] = 220 - grain;
	auch[ 1 ] = 70 - grain / 2;
	auch[ 2 ] = 40 - grain / 4;

	break;
	    
    case WOOD_WALNUT:
	r *= 3;
	grain = ( (int) r % 60 );

	if( grain > 40 )
	    grain = 120 - 2 * grain;

	grain *= WoodHash( (int) r / 60 ) * 0.7 + 0.3;
 
	auch[ 0 ] = 80 + ( grain * 3 / 2 );
	auch[ 1 ] = 40 + grain;
	auch[ 2 ] = grain / 2;

	break;
	
    case WOOD_WILLOW:
	r *= 3;
	grain = ( (int) r % 60 );

	if( grain > 40 )
	    grain = 120 - 2 * grain;

	auch[ 0 ] = 230 + grain / 3;
	auch[ 1 ] = 100 + grain / 5;
	auch[ 2 ] = 20 + grain / 10;

	figure = r / 60 + z / 30;
	
	if( figure % 3 == ( grain / 3 ) % 3 ) {
	    auch[ 0 ] -= WoodHash( figure + grain ) * 16;
	    auch[ 1 ] -= WoodHash( figure + grain ) * 8;
	    auch[ 2 ] -= WoodHash( figure + grain ) * 4;
	}

	break;
	
    default:
	assert( FALSE );
    }
}

static void RenderFrameWood( renderdata *prd, unsigned char *puch,
			     int nStride ) {

#define BUF( y, x, i ) ( puch[ (y) * nStride + (x) * 3 + (i) ] )
    
    int i, x, y, nSpecularTop, anSpecular[ 4 ][ prd->nSize ], nSpecular,
	s = prd->nSize;
    unsigned char a[ 3 ];
    float rx, rz, arDiffuse[ 4 ][ s ], cos_theta, rDiffuseTop,
	arHeight[ s ], rHeight, rDiffuse;

    nSpecularTop = pow( prd->arLight[ 2 ], 20 ) * 0.6 * 0x100;
    rDiffuseTop = 0.8 * prd->arLight[ 2 ] + 0.2;
	
    for( x = 0; x < s; x++ ) {
	rx = 1.0 - ( (float) x / s );
	rz = ssqrt( 1.0 - rx * rx );
	arHeight[ x ] = rz * s;
	    
	for( i = 0; i < 4; i++ ) {
	    cos_theta = prd->arLight[ 2 ] * rz + prd->arLight[ i & 1 ] * rx;
	    if( cos_theta < 0 )
		cos_theta = 0;
	    arDiffuse[ i ][ x ] = 0.8 * cos_theta + 0.2;
	    cos_theta = 2 * rz * cos_theta - prd->arLight[ 2 ];
	    anSpecular[ i ][ x ] = pow( cos_theta, 20 ) * 0.6 * 0x100;

	    if( !( i & 1 ) )
		rx = -rx;
	}
    }

    /* Top and bottom edges */
    for( y = 0; y < s * 3; y++ )
	for( x = 0; x < s * 108; x++ ) {
	    if( y < s ) {
		rDiffuse = arDiffuse[ 3 ][ y ];
		nSpecular = anSpecular[ 3 ][ y ];
		rHeight = arHeight[ y ];
	    } else if( y < 2 * s ) {
		rDiffuse = rDiffuseTop;
		nSpecular = nSpecularTop;
		rHeight = s;
	    } else {
		rDiffuse = arDiffuse[ 1 ][ 3 * s - y - 1 ];
		nSpecular = anSpecular[ 1 ][ 3 * s - y - 1 ];
		rHeight = arHeight[ 3 * s - y - 1 ];
	    }
	    
	    WoodPixel( 100 - y * 0.85 + x * 0.1, rHeight - x * 0.11,
			200 + x * 0.93 + y * 0.16, a, prd->wt );
	    
	    for( i = 0; i < 3; i++ )
		BUF( y, x, i ) = clamp( a[ i ] * rDiffuse +
					nSpecular );
	    
	    WoodPixel( 123 + y * 0.87 - x * 0.08, rHeight + x * 0.06,
			-100 - x * 0.94 - y * 0.11, a, prd->wt );
	    
	    for( i = 0; i < 3; i++ )
		BUF( y + 69 * s, x, i ) =
		    clamp( a[ i ] * rDiffuse + nSpecular );
	}

    /* Left and right edges */
    for( y = 0; y < s * 72; y++ )
	for( x = 0; x < s * 3; x++ ) {
	    if( x < s ) {
		rDiffuse = arDiffuse[ 2 ][ x ];
		nSpecular = anSpecular[ 2 ][ x ];
		rHeight = arHeight[ x ];
	    } else if( x < 2 * s ) {
		rDiffuse = rDiffuseTop;
		nSpecular = nSpecularTop;
		rHeight = s;
	    } else {
		rDiffuse = arDiffuse[ 0 ][ 3 * s - x - 1 ];
		nSpecular = anSpecular[ 0 ][ 3 * s - x - 1 ];
		rHeight = arHeight[ 3 * s - x - 1 ];
	    }
    
	    WoodPixel( 300 + x * 0.9 + y * 0.1, rHeight + y * 0.06,
			200 - y * 0.9 + x * 0.1, a, prd->wt );

	    if( x < y && x + y < s * 72 )
		for( i = 0; i < 3; i++ )
		    BUF( y, x, i ) = clamp( a[ i ] * rDiffuse + nSpecular );
	    
	    WoodPixel( -100 - x * 0.86 + y * 0.13, rHeight - y * 0.07,
			300 + y * 0.92 + x * 0.08, a, prd->wt );

	    if( s * 3 - x <= y && s * 3 - x + y < s * 72 ) 
		for( i = 0; i < 3; i++ )
		    BUF( y, x + 105 * s, i ) = clamp( a[ i ] * rDiffuse +
						      nSpecular );
	}

    /* Bar */
    for( y = 0; y < s * 72; y++ )
	for( x = 0; x < s * 6; x++ ) {
	    if( y < s && y < x && y < s * 6 - x - 1 ) {
		rDiffuse = arDiffuse[ 3 ][ y ];
		nSpecular = anSpecular[ 3 ][ y ];
		rHeight = arHeight[ y ];
	    } else if( y > 71 * s && s * 72 - y - 1 < x &&
		       s * 72 - y - 1 < s * 6 - x - 1 ) {
		rDiffuse = arDiffuse[ 1 ][ 72 * s - y - 1 ];
		nSpecular = anSpecular[ 1 ][ 72 * s - y - 1 ];
		rHeight = arHeight[ 72 * s - y - 1 ];
	    } else if( x < s ) {
		rDiffuse = arDiffuse[ 2 ][ x ];
		nSpecular = anSpecular[ 2 ][ x ];
		rHeight = arHeight[ x ];
	    } else if( x < 5 * s ) {
		rDiffuse = rDiffuseTop;
		nSpecular = nSpecularTop;
		rHeight = s;
	    } else {
		rDiffuse = arDiffuse[ 0 ][ 6 * s - x - 1 ];
		nSpecular = anSpecular[ 0 ][ 6 * s - x - 1 ];
		rHeight = arHeight[ 6 * s - x - 1 ];
	    }
    
	    WoodPixel( 100 - x * 0.88 + y * 0.08, 50 + rHeight - y * 0.1,
			-200 + y * 0.99 - x * 0.12, a, prd->wt );

	    if( y + x >= s * 3 && y - x <= s * 69 )
		for( i = 0; i < 3; i++ )
		    BUF( y, x + 48 * s , i ) = clamp( a[ i ] * rDiffuse +
						      nSpecular );
	    
	    WoodPixel( 100 - x * 0.86 + y * 0.02, 50 + rHeight - y * 0.07,
			200 - y * 0.92 + x * 0.03, a, prd->wt );

	    if( y + s * 6 - x >= s * 3 && y - s * 6 + x <= s * 69 )
		for( i = 0; i < 3; i++ )
		    BUF( y, x + 54 * s, i ) = clamp( a[ i ] * rDiffuse +
						     nSpecular );
	}
    
    /* Left and right separators (between board and bearoff tray) */
    for( y = 0; y < s * 68; y++ )
	for( x = 0; x < s * 3; x++ )
	    if( x + y >= s && y - x <= s * 67 &&
		y + s * 3 - x >= s && x + y <= s * 70 ) {
		if( x < s ) {
		    rDiffuse = arDiffuse[ 2 ][ x ];
		    nSpecular = anSpecular[ 2 ][ x ];
		    rHeight = arHeight[ x ];
		} else if( x < 2 * s ) {
		    rDiffuse = rDiffuseTop;
		    nSpecular = nSpecularTop;
		    rHeight = s;
		} else {
		    rDiffuse = arDiffuse[ 0 ][ 3 * s - x - 1 ];
		    nSpecular = anSpecular[ 0 ][ 3 * s - x - 1 ];
		    rHeight = arHeight[ 3 * s - x - 1 ];
		}
		
		WoodPixel( -300 - x * 0.91 + y * 0.1, rHeight + y * 0.02,
			   -200 + y * 0.94 - x * 0.06, a, prd->wt );
		
		for( i = 0; i < 3; i++ )
		    BUF( y + 2 * s, x + 9 * s, i ) = clamp( a[ i ] * rDiffuse +
							    nSpecular );
		
		WoodPixel( 100 - x * 0.89 - y * 0.07, rHeight + y * 0.05,
			   300 - y * 0.94 + x * 0.11, a, prd->wt );
		
		for( i = 0; i < 3; i++ )
		    BUF( y + 2 * s, x + 96 * s, i ) = clamp( a[ i ] *
							     rDiffuse +
							     nSpecular );
	    }

    /* Left and right dividers (between the bearoff trays) */
    for( y = 0; y < s * 6; y++ )
	for( x = 0; x < s * 8; x++ )
	    if( x + y >= s && y - x <= s * 5 &&
		y + s * 8 - x >= s && x + y <= s * 13 ) {
		if( y < s ) {
		    rDiffuse = arDiffuse[ 3 ][ y ];
		    nSpecular = anSpecular[ 3 ][ y ];
		    rHeight = arHeight[ y ];
		} else if( y < 5 * s ) {
		    rDiffuse = rDiffuseTop;
		    nSpecular = nSpecularTop;
		    rHeight = s;
		} else {
		    rDiffuse = arDiffuse[ 1 ][ 6 * s - y - 1 ];
		    nSpecular = anSpecular[ 1 ][ 6 * s - y - 1 ];
		    rHeight = arHeight[ 6 * s - y - 1 ];
		}
		
		WoodPixel( -100 - y * 0.85 + x * 0.11, rHeight - x * 0.04,
			   -100 - x * 0.93 + y * 0.08, a, prd->wt );
		
		for( i = 0; i < 3; i++ )
		    BUF( y + 33 * s, x + 2 * s, i ) = clamp( a[ i ] *
							     rDiffuse +
							     nSpecular );
		
		WoodPixel( -123 - y * 0.93 - x * 0.12, rHeight + x * 0.11,
			   -150 + x * 0.88 - y * 0.07, a, prd->wt );
		
		for( i = 0; i < 3; i++ )
		    BUF( y + 33 * s, x + 98 * s, i ) =
			clamp( a[ i ] * rDiffuse + nSpecular );
	    }
#undef BUF
}

static void HingePixel( renderdata *prd, float xNorm, float yNorm,
			float xEye, float yEye, unsigned char auch[ 3 ] ) {

    float arReflection[ 3 ], arAuxLight[ 2 ][ 3 ] = {
	{ 0.6, 0.7, 0.5 },
	{ 0.5, -0.6, 0.7 } };
    float *arLight[ 3 ] = { prd->arLight, arAuxLight[ 0 ], arAuxLight[ 1 ] };
    float zNorm, zEye;
    float diffuse, specular = 0, cos_theta;
    float l;
    int i;

    zNorm = ssqrt( 1.0 - xNorm * xNorm - yNorm * yNorm );
    
    if( ( cos_theta = xNorm * arLight[ 0 ][ 0 ] +
	  yNorm * arLight[ 0 ][ 1 ] +
	  zNorm * arLight[ 0 ][ 2 ] ) < 0 )
	diffuse = 0.2;
    else {
	diffuse = cos_theta * 0.8 + 0.2;

	for( i = 0; i < 3; i++ ) {
	    if( ( cos_theta = xNorm * arLight[ i ][ 0 ] +
		  yNorm * arLight[ i ][ 1 ] +
		  zNorm * arLight[ i ][ 2 ] ) < 0 )
		cos_theta = 0;
	    
	    arReflection[ 0 ] = arLight[ i ][ 0 ] - 2 * xNorm * cos_theta;
	    arReflection[ 1 ] = arLight[ i ][ 1 ] - 2 * yNorm * cos_theta;
	    arReflection[ 2 ] = arLight[ i ][ 2 ] - 2 * zNorm * cos_theta;
	    
	    l = sqrt( arReflection[ 0 ] * arReflection[ 0 ] +
		      arReflection[ 1 ] * arReflection[ 1 ] +
		      arReflection[ 2 ] * arReflection[ 2 ] );
	    
	    arReflection[ 0 ] /= l;
	    arReflection[ 1 ] /= l;
	    arReflection[ 2 ] /= l;
	    
	    zEye = ssqrt( 1.0 - xEye * xEye - yEye * yEye );
	    cos_theta = arReflection[ 0 ] * xEye + arReflection[ 1 ] * yEye +
		arReflection[ 2 ] * zEye;
	    
	    specular += pow( cos_theta, 30 ) * 0.7;
	}
    }
    
    auch[ 0 ] = clamp( 200 * diffuse + specular * 0x100 );
    auch[ 1 ] = clamp( 230 * diffuse + specular * 0x100 );
    auch[ 2 ] = clamp( 20 * diffuse + specular * 0x100 );
}

static void RenderHinges( renderdata *prd, unsigned char *puch, int nStride ) {
    
    int x, y, s = prd->nSize;
    float xNorm, yNorm;
    
    for( y = 0; y < 12 * s; y++ )
	for( x = 0; x < 2 * s; x++ ) {
	    if( s < 5 && y && !( y % ( 2 * s ) ) )
		yNorm = 0.5;
	    else if( y % ( 2 * s ) < s / 5 )
		yNorm = ( s / 5 - y % ( 2 * s ) ) * ( 2.5 / s );
	    else if( y % ( 2 * s ) >= ( 2 * s - s / 5 ) )
		yNorm = ( y % ( 2 * s ) - ( 2 * s - s / 5 - 1 ) ) *
		    ( -2.5 / s );
	    else
		yNorm = 0;

	    xNorm = ( x - s ) / (float) s * ( 1.0 - yNorm * yNorm );
		    
	    HingePixel( prd, xNorm, yNorm,
			( s - x ) / ( 40 * s ), ( y - 20 * s ) / ( 40 * s ),
			puch + ( y + 12 * s ) * nStride + ( x + 53 * s ) * 3 );
	    
	    HingePixel( prd, xNorm, yNorm,
			 ( s - x ) / ( 40 * s ), ( y + 20 * s ) / ( 40 * s ),
			puch + ( y + 48 * s ) * nStride + ( x + 53 * s ) * 3 );
	}
}

static void RenderBasicGlyph( unsigned char *puch, int nStride,
			      int nSize, unsigned n, int xOff, int yOff,
			      unsigned char r, unsigned char g, unsigned
			      char b ) {

    int i, x0, x1, y0, y1, y2;

    x0 = xOff + nSize / 12 + 1;
    x1 = xOff + nSize * 5 / 12;
    y0 = yOff;
    y1 = yOff - nSize / 2;
    y2 = yOff - nSize;

#define PUT( x, y ) { puch[ (y) * nStride + (x) * 3 + 0 ] = r; \
		      puch[ (y) * nStride + (x) * 3 + 1 ] = g; \
    		      puch[ (y) * nStride + (x) * 3 + 2 ] = b; }
    
    if( n == 0 || n == 2 || n == 3 || n == 5 || n == 6 || n == 7 || n == 8 ||
	n == 9 )
	/* top */
	for( i = x0; i <= x1; i++ )
	    PUT( i, y2 );

    if( n == 2 || n == 3 || n == 4 || n == 5 || n == 6 || n == 8 || n == 9 )
	/* middle */
	for( i = x0; i <= x1; i++ )
	    PUT( i, y1 );

    if( n == 0 || n == 2 || n == 3 || n == 5 || n == 6 || n == 8 || n == 9 )
	/* bottom */
	for( i = x0; i <= x1; i++ )
	    PUT( i, y0 );

    if( n == 0 || n == 4 || n == 5 || n == 6 || n == 8 || n == 9 )
	/* top left */
	for( i = y2; i <= y1; i++ )
	    PUT( x0, i );

    if( n == 0 || n == 2 || n == 6 || n == 8 )
	/* bottom left */
	for( i = y1; i <= y0; i++ )
	    PUT( x0, i );

    if( n == 0 || n == 1 || n == 2 || n == 3 || n == 4 || n == 7 || n == 8 ||
	n == 9 )
	/* top right */
	for( i = y2; i <= y1; i++ )
	    PUT( x1, i );

    if( n == 0 || n == 1 || n == 3 || n == 4 || n == 5 || n == 6 || n == 7 ||
	n == 8 || n == 9 )
	/* bottom right */
	for( i = y1; i <= y0; i++ )
	    PUT( x1, i );
#undef PUT
}


static void RenderBasicNumber( unsigned char *puch, int nStride,
			       int nSize, unsigned n, int xOff, int yOff,
			       unsigned char r, unsigned char g, unsigned
			       char b ) {
    int x, c;

    for( c = 0, x = n; x; x /= 10 )
	c += nSize / 2;

    xOff += c / 2;

    for( ; n; n /= 10 ) {
	xOff -= nSize / 2;
	RenderBasicGlyph( puch, nStride, nSize, n % 10, xOff, yOff, r, g, b );
    }
}

#if HAVE_FREETYPE
static void RenderGlyph( unsigned char *puch, int nStride, FT_Glyph pftg,
			 int xOff, int yOff, unsigned char r, unsigned char g,
			 unsigned char b ) {

    FT_BitmapGlyph pftbg;
    FT_Bitmap *pb;
    int x, y, x0 = 0, y0 = 0;
    
    assert( pftg->format == FT_GLYPH_FORMAT_BITMAP );

    pftbg = (FT_BitmapGlyph) pftg;
    pb = &pftbg->bitmap;

    xOff += pftbg->left;
    yOff -= pftbg->top;

    if( xOff < 0 ) {
	x0 = -xOff;
	xOff = 0;
    }

    if( yOff < 0 ) {
	y0 = -yOff;
	yOff = 0;
    }
    
    assert( pb->pixel_mode == FT_PIXEL_MODE_GRAY );

    puch += yOff * nStride + xOff * 3;
    nStride -= 3 * pb->width;
    
    for( y = y0; y < pb->rows; y++ ) {
	for( x = x0; x < pb->width; x++ ) {
	    *puch = ( *puch * ( pb->num_grays -
				pb->buffer[ y * pb->pitch + x ] ) +
		      r * pb->buffer[ y * pb->pitch + x ] ) /
		pb->num_grays;
	    puch++;
	    *puch = ( *puch * ( pb->num_grays -
				pb->buffer[ y * pb->pitch + x ] ) +
		      g * pb->buffer[ y * pb->pitch + x ] ) /
		pb->num_grays;
	    puch++;
	    *puch = ( *puch * ( pb->num_grays -
				pb->buffer[ y * pb->pitch + x ] ) +
		      b * pb->buffer[ y * pb->pitch + x ] ) /
		pb->num_grays;
	    puch++;
	}
	puch += nStride;
    }
}

static void RenderNumber( unsigned char *puch, int nStride, FT_Glyph *aftg,
			  unsigned n, int xOff, int yOff, unsigned char r,
			  unsigned char g, unsigned char b ) {
    int x, c;

    for( c = 0, x = n; x; x /= 10 )
	c += ( aftg[ x % 10 ]->advance.x + 0x8000 ) >> 16;

    xOff += c / 2;

    for( ; n; n /= 10 ) {
	xOff -= ( aftg[ n % 10 ]->advance.x + 0x8000 ) >> 16;
	RenderGlyph( puch, nStride, aftg[ n % 10 ], xOff, yOff, r, g, b );
    }
}


#endif

static void
RenderBasicLabels( renderdata *prd, unsigned char *puch, int nStride,
                    const int iStart, const int iEnd, const int iDelta ) {

  int i;

  for ( i = 0; i < ( 1 + abs( iStart - iEnd ) ); ++i ) 
    RenderBasicNumber( puch, nStride, prd->nSize, iStart + i * iDelta,
                       ( aaanPositions[ prd->fClockwise ][ i + 1 ][ 0 ] + 3 ) *
                       prd->nSize, 2 * prd->nSize,
                       0xFF, 0xFF, 0xFF );


}


static void
RenderLabels( renderdata *prd, unsigned char *puch, int nStride,
               const int iStart, const int iEnd, const int iDelta ) {


#if HAVE_FREETYPE
    FT_Face ftf;
    int i;
    FT_Glyph aftg[ 10 ];

    if( FT_New_Memory_Face( ftl, auchLuxiSB, cbLuxiSB, 0, &ftf ) )
	return RenderBasicLabels( prd, puch, nStride, iStart, iEnd, iDelta );
    
    if( FT_Set_Pixel_Sizes( ftf, 0, prd->nSize * 5 / 2 ) )
	return RenderBasicLabels( prd, puch, nStride, iStart, iEnd, iDelta );

    for( i = 0; i < 10; i++ ) {
	FT_Load_Char( ftf, '0' + i, FT_LOAD_RENDER );
	FT_Get_Glyph( ftf->glyph, aftg + i );
    }

    FT_Done_Face( ftf );

    for ( i = 0; i < ( 1 + abs( iStart - iEnd ) ); ++i ) 
	RenderNumber( puch, nStride, aftg, iStart + i * iDelta,
		      ( aaanPositions[ prd->fClockwise ][ i + 1 ][ 0 ] + 3 ) *
		      prd->nSize, 7 * prd->nSize / 3,
		      0xFF, 0xFF, 0xFF );
    
    for( i = 0; i < 10; i++ )
	FT_Done_Glyph( aftg[ i ] );
#else
    RenderBasicLabels( prd, puch, nStride, iStart, iEnd, iDelta );
#endif

}

static unsigned char BoardPixel( renderdata *prd, int i, int antialias,
				  int j ) {

    return clamp( ( ( (int) prd->aanBoardColour[ 0 ][ j ] -
		      (int) prd->aSpeckle[ 0 ] / 2 +
		      (int) RAND % ( prd->aSpeckle[ 0 ] + 1 ) ) *
		    ( 20 - antialias ) +
		    ( (int) prd->aanBoardColour[ i ][ j ] -
		      (int) prd->aSpeckle[ i ] / 2 +
		      (int) RAND % ( prd->aSpeckle[ i ] + 1 ) ) *
		    antialias ) * ( prd->arLight[ 2 ] * 0.8 + 0.2 ) / 20 );
}

extern void RenderBoard( renderdata *prd, unsigned char *puch, int nStride ) {

    int ix, iy, antialias;

#define BUF( y, x, i ) ( puch[ (y) * nStride + (x) * 3 + (i) ] )

    if( prd->wt == WOOD_PAINT )
	RenderFramePainted( prd, puch, nStride );
    else
	RenderFrameWood( prd, puch, nStride );

    if( prd->fHinges )
	RenderHinges( prd, puch, nStride );

    /*
    if( prd->fLabels )
    RenderLabels( prd, puch, nStride );*/
        
    for( iy = 0; iy < 30 * prd->nSize; iy++ )
	for( ix = 0; ix < 6 * prd->nSize; ix++ ) {
	    /* <= 0 is board; >= 20 is on a point; interpolate in between */
	    antialias = 2 * ( 30 * prd->nSize - iy ) + 1 - 20 *
		abs( 3 * prd->nSize - ix );

	    if( antialias < 0 )
		antialias = 0;
	    else if( antialias > 20 )
		antialias = 20;

	    BUF( iy + 3 * prd->nSize, ix + 18 * prd->nSize, 0 ) =
		BUF( 69 * prd->nSize - iy - 1, ix + 12 * prd->nSize, 0 ) =
		BoardPixel( prd, 2, antialias, 0 );
	    
	    BUF( iy + 3 * prd->nSize, ix + 18 * prd->nSize, 1 ) =
		BUF( 69 * prd->nSize - iy - 1, ix + 12 * prd->nSize, 1 ) =
		BoardPixel( prd, 2, antialias, 1 );
	    
	    BUF( iy + 3 * prd->nSize, ix + 18 * prd->nSize, 2 ) =
		BUF( 69 * prd->nSize - iy - 1, ix + 12 * prd->nSize, 2 ) =
		BoardPixel( prd, 2, antialias, 2 );
	    
	    BUF( iy + 3 * prd->nSize, ix + 12 * prd->nSize, 0 ) =
		BUF( 69 * prd->nSize - iy - 1, ix + 18 * prd->nSize,
		     0 ) = BoardPixel( prd, 3, antialias, 0 );

	    BUF( iy + 3 * prd->nSize, ix + 12 * prd->nSize, 1 ) =
		BUF( 69 * prd->nSize - iy - 1, ix + 18 * prd->nSize,
		     1 ) = BoardPixel( prd, 3, antialias, 1 );

	    BUF( iy + 3 * prd->nSize, ix + 12 * prd->nSize, 2 ) =
		BUF( 69 * prd->nSize - iy - 1, ix + 18 * prd->nSize,
		     2 ) = BoardPixel( prd, 3, antialias, 2 );
	}
    
    for( iy = 0; iy < 6 * prd->nSize; iy++ )
	for( ix = 0; ix < 12 * prd->nSize; ix++ ) {
	    BUF( 33 * prd->nSize + iy, 12 * prd->nSize + ix, 0 ) =
		BoardPixel( prd, 0, 0, 0 );
	    BUF( 33 * prd->nSize + iy, 12 * prd->nSize + ix, 1 ) =
		BoardPixel( prd, 0, 0, 1 );
	    BUF( 33 * prd->nSize + iy, 12 * prd->nSize + ix, 2 ) =
		BoardPixel( prd, 0, 0, 2 );
	}

    CopyArea( &BUF( 3 * prd->nSize, 24 * prd->nSize, 0 ), nStride,
	      &BUF( 3 * prd->nSize, 12 * prd->nSize, 0 ), nStride,
	      12 * prd->nSize, 66 * prd->nSize );
    CopyArea( &BUF( 3 * prd->nSize, 36 * prd->nSize, 0 ), nStride,
	      &BUF( 3 * prd->nSize, 12 * prd->nSize, 0 ), nStride,
	      12 * prd->nSize, 66 * prd->nSize );
    CopyArea( &BUF( 3 * prd->nSize, 60 * prd->nSize, 0 ), nStride,
	      &BUF( 3 * prd->nSize, 12 * prd->nSize, 0 ), nStride,
	      36 * prd->nSize, 66 * prd->nSize );
    
    for( iy = 0; iy < 30 * prd->nSize; iy++ )
	for( ix = 0; ix < 6 * prd->nSize; ix++ ) {
	    BUF( iy + 3 * prd->nSize, ix + 3 * prd->nSize, 0 ) =
		BoardPixel( prd, 0, 0, 0 );
	    BUF( iy + 3 * prd->nSize, ix + 3 * prd->nSize, 1 ) =
		BoardPixel( prd, 0, 0, 1 );
	    BUF( iy + 3 * prd->nSize, ix + 3 * prd->nSize, 2 ) =
		BoardPixel( prd, 0, 0, 2 );
	}

    CopyArea( &BUF( 39 * prd->nSize, 3 * prd->nSize, 0 ), nStride,
	      &BUF( 3 * prd->nSize, 3 * prd->nSize, 0 ), nStride,
	      6 * prd->nSize, 30 * prd->nSize );
    CopyArea( &BUF( 3 * prd->nSize, 99 * prd->nSize, 0 ), nStride,
	      &BUF( 3 * prd->nSize, 3 * prd->nSize, 0 ), nStride,
	      6 * prd->nSize, 30 * prd->nSize );
    CopyArea( &BUF( 39 * prd->nSize, 99 * prd->nSize, 0 ), nStride,
	      &BUF( 3 * prd->nSize, 3 * prd->nSize, 0 ), nStride,
	      6 * prd->nSize, 30 * prd->nSize );

#undef BUF
}

extern void RenderChequers( renderdata *prd, unsigned char *puch0,
			    unsigned char *puch1, unsigned short *psRefract0,
			    unsigned short *psRefract1, int nStride ) {
    
    int size = 6 * prd->nSize;
    int ix, iy, in, fx, fy, i;
    float x, y, z, x_loop, y_loop, diffuse, specular_x, specular_o, cos_theta,
	r, x1, y1, len;
    
#define BUFX( y, x, i ) puch0[ ( (y) * size + (x) ) * 4 + (i) ]
#define BUFO( y, x, i ) puch1[ ( (y) * size + (x) ) * 4 + (i) ]

    for( iy = 0, y_loop = -1.0; iy < size; iy++ ) {
	for( ix = 0, x_loop = -1.0; ix < size; ix++ ) {
	    in = 0;
	    diffuse = specular_x = specular_o = 0.0;
	    fy = 0;
	    y = y_loop;
	    do {
		fx = 0;
		x = x_loop;
		do {
		    r = sqrt( x * x + y * y );
		    if( r < prd->rRound )
			x1 = y1 = 0.0;
		    else {
			x1 = x * ( r / ( 1 - prd->rRound ) -
				   1 / ( 1 - prd->rRound ) + 1 );
			y1 = y * ( r / ( 1 - prd->rRound ) -
				   1 / ( 1 - prd->rRound ) + 1 );
		    }
		    if( ( z = 1.0 - x1 * x1 - y1 * y1 ) > 0.0 ) {
			in++;
			diffuse += 0.3;
			z = sqrt( z ) * 1.5;
			len = sqrt( x1 * x1 + y1 * y1 + z * z );
			if( ( cos_theta = ( prd->arLight[ 0 ] * x1 +
					    prd->arLight[ 1 ] * -y1 +
					    prd->arLight[ 2 ] * z ) / len )
			    > 0 ) {
			    diffuse += cos_theta;
			    if( ( cos_theta = 2 * z * cos_theta / len -
				  prd->arLight[ 2 ] ) > 0 ) {
				specular_x += pow( cos_theta,
						   prd->arExponent[ 0 ] ) *
				    prd->arCoefficient[ 0 ];
				specular_o += pow( cos_theta,
						   prd->arExponent[ 1 ] ) *
				    prd->arCoefficient[ 1 ];
			    }
			}
		    }
		    x += 1.0 / ( size );
		} while( !fx++ );
		y += 1.0 / ( size );
	    } while( !fy++ );

	    if( !in ) {
		/* pixel is outside chequer */
		for( i = 0; i < 3; i++ )
		    BUFX( iy, ix, i ) = BUFO( iy, ix, i ) = 0;

		*psRefract0++ = *psRefract1++ = ( iy << 8 ) | ix;
		
		BUFX( iy, ix, 3 ) = BUFO( iy, ix, 3 ) = 0xFF;
	    } else {
		/* pixel is inside chequer */
		float r, s, r1, s1, theta, a, b, p, q;
		int f;
		    
		r = sqrt( x_loop * x_loop + y_loop * y_loop );
		if( r < prd->rRound )
		    r1 = 0.0;
		else
		    r1 = r / ( 1 - prd->rRound ) -
			1 / ( 1 - prd->rRound ) + 1;
		s = ssqrt( 1 - r * r );
		s1 = ssqrt( 1 - r1 * r1 );
		
		theta = atanf( r1 / s1 );
		
		for( f = 0; f < 2; f++ ) {
		    b = asinf( sinf( theta ) / prd->arRefraction[ f ] );
		    a = theta - b;
		    p = r - s * tanf( a );
		    q = p / r;
			    
		    /* write the comparison this strange way to pick up
		       NaNs as well */
		    if( !( q >= -1.0f && q <= 1.0f ) )
			q = 1.0f;
			
		    *( f ? psRefract1++ : psRefract0++ ) =
			( lrint( iy * q + size / 2 * ( 1.0 - q ) ) << 8 ) |
			lrint( ix * q + size / 2 * ( 1.0 - q ) );
		}
		    
		BUFX( iy, ix, 0 ) = clamp( ( diffuse *
					     prd->aarColour[ 0 ][ 0 ] *
					     prd->aarColour[ 0 ][ 3 ] +
					     specular_x ) * 64.0 );
		BUFX( iy, ix, 1 ) = clamp( ( diffuse *
					     prd->aarColour[ 0 ][ 1 ] *
					     prd->aarColour[ 0 ][ 3 ] +
					     specular_x ) * 64.0 );
		BUFX( iy, ix, 2 ) = clamp( ( diffuse *
					     prd->aarColour[ 0 ][ 2 ] *
					     prd->aarColour[ 0 ][ 3 ] +
					     specular_x ) * 64.0 );
		    
		BUFO( iy, ix, 0 ) = clamp( ( diffuse *
					     prd->aarColour[ 1 ][ 0 ] *
					     prd->aarColour[ 1 ][ 3 ] +
					     specular_o ) * 64.0 );
		BUFO( iy, ix, 1 ) = clamp( ( diffuse *
					     prd->aarColour[ 1 ][ 1 ] *
					     prd->aarColour[ 1 ][ 3 ] +
					     specular_o ) * 64.0 );
		BUFO( iy, ix, 2 ) = clamp( ( diffuse *
					     prd->aarColour[ 1 ][ 2 ] *
					     prd->aarColour[ 1 ][ 3 ] +
					     specular_o ) * 64.0 );
		    
		BUFX( iy, ix, 3 ) = clamp(
		    0xFF * 0.25 * ( ( 4 - in ) +
				    ( ( 1.0 - prd->aarColour[ 0 ][ 3 ] ) *
				      diffuse ) ) );
		BUFO( iy, ix, 3 ) = clamp(
		    0xFF * 0.25 * ( ( 4 - in ) +
				    ( ( 1.0 - prd->aarColour[ 1 ][ 3 ] ) *
				      diffuse ) ) );
	    }
	    x_loop += 2.0 / ( size );
	}
	y_loop += 2.0 / ( size );
    }
#undef BUFX
#undef BUFO
}

extern void RenderChequerLabels( renderdata *prd, unsigned char *puch,
				 int nStride ) {
    int i, ip;
#if HAVE_FREETYPE
    FT_Face ftf;
    FT_Glyph aftg[ 10 ];
    int fFreetype = FALSE;
    
    if( !FT_New_Memory_Face( ftl, auchLuxiSR, cbLuxiSR, 0, &ftf ) &&
	!FT_Set_Pixel_Sizes( ftf, 0, 2 * prd->nSize ) ) {
	fFreetype = TRUE;
	for( i = 0; i < 10; i++ ) {
	    FT_Load_Char( ftf, '0' + i, FT_LOAD_RENDER );
	    FT_Get_Glyph( ftf->glyph, aftg + i );
	}

	FT_Done_Face( ftf );
    }
#endif

    for( i = 0; i < 12; i++ ) {
	FillArea( puch, nStride, 4 * prd->nSize, 4 * prd->nSize,
		  0xC0, 0xC0, 0xC0 );

	for( ip = 0; ip < 4 * prd->nSize; ip++ ) {
	    puch[ 3 * ip + nStride + 0 ] = 0xE0;
	    puch[ 3 * ip + nStride + 1 ] = 0xE0;
	    puch[ 3 * ip + nStride + 2 ] = 0xE0;
	    
	    puch[ 3 * ip + ( 4 * prd->nSize - 2 ) * nStride + 0 ] = 0x80;
	    puch[ 3 * ip + ( 4 * prd->nSize - 2 ) * nStride + 1 ] = 0x80;
	    puch[ 3 * ip + ( 4 * prd->nSize - 2 ) * nStride + 2 ] = 0x80;

	    puch[ ip * nStride + 3 + 0 ] = 0xE0;
	    puch[ ip * nStride + 3 + 1 ] = 0xE0;
	    puch[ ip * nStride + 3 + 2 ] = 0xE0;
	    
	    puch[ ip * nStride + ( 4 * prd->nSize - 2 ) * 3 + 0 ] = 0x80;
	    puch[ ip * nStride + ( 4 * prd->nSize - 2 ) * 3 + 1 ] = 0x80;
	    puch[ ip * nStride + ( 4 * prd->nSize - 2 ) * 3 + 2 ] = 0x80;
	}
	
	for( ip = 0; ip < 4 * prd->nSize; ip++ ) {
	    puch[ 3 * ip + 0 ] = 0xFF;
	    puch[ 3 * ip + 1 ] = 0xFF;
	    puch[ 3 * ip + 2 ] = 0xFF;
	    
	    puch[ 3 * ip + ( 4 * prd->nSize - 1 ) * nStride + 0 ] = 0;
	    puch[ 3 * ip + ( 4 * prd->nSize - 1 ) * nStride + 1 ] = 0;
	    puch[ 3 * ip + ( 4 * prd->nSize - 1 ) * nStride + 2 ] = 0;

	    puch[ ip * nStride + 0 ] = 0xFF;
	    puch[ ip * nStride + 1 ] = 0xFF;
	    puch[ ip * nStride + 2 ] = 0xFF;

	    puch[ ip * nStride + ( 4 * prd->nSize - 1 ) * 3 + 0 ] = 0;
	    puch[ ip * nStride + ( 4 * prd->nSize - 1 ) * 3 + 1 ] = 0;
	    puch[ ip * nStride + ( 4 * prd->nSize - 1 ) * 3 + 2 ] = 0;
	}

#if HAVE_FREETYPE
	if( fFreetype )
	    RenderNumber( puch, nStride, aftg, i + 4, 2 * prd->nSize,
			  45 * prd->nSize / 16, 0, 0, 0 );
	else
#endif
	    RenderBasicNumber( puch, nStride, 2 * prd->nSize, i + 4,
			       2 * prd->nSize, 3 * prd->nSize, 0, 0, 0 );
	    
	puch += 4 * prd->nSize * nStride;
    }

#if HAVE_FREETYPE
    if( fFreetype )
	for( i = 0; i < 10; i++ )
	    FT_Done_Glyph( aftg[ i ] );
#endif
}

static void
RenderBasicCube ( const float arLight[ 3 ], const int nSize, 
                  const double arColour[ 4 ], unsigned char *puch,
                  int nStride ) {

    int ix, iy, in, fx, fy, i;
    float x, y, x_loop, y_loop, diffuse, specular, cos_theta,
	x_norm, y_norm, z_norm;

    nStride -= 8 * nSize * 4;
    
    for( iy = 0, y_loop = -1.0; iy < 8 * nSize; iy++ ) {
	for( ix = 0, x_loop = -1.0; ix < 8 * nSize; ix++ ) {
	    in = 0;
	    diffuse = specular = 0.0;
	    fy = 0;
	    y = y_loop;
	    do {
		fx = 0;
		x = x_loop;
		do {
		    if( fabs( x ) < 7.0 / 8.0 &&
			fabs( y ) < 7.0 / 8.0 ) {
			/* flat surface */
			in++;
			diffuse += arLight[ 2 ] * 0.8 + 0.2;
			specular += pow( arLight[ 2 ], 10 ) * 0.4;
		    } else {
			if( fabs( x ) < 7.0 / 8.0 ) {
			    /* top/bottom edge */
			    x_norm = 0.0;
			    y_norm = -7.0 * y - ( y > 0.0 ? -6.0 : 6.0 );
			    z_norm = ssqrt( 1.0 - y_norm * y_norm );
			} else if( fabs( y ) < 7.0 / 8.0 ) {
			    /* left/right edge */
			    x_norm = 7.0 * x + ( x > 0.0 ? -6.0 : 6.0 );
			    y_norm = 0.0;
			    z_norm = ssqrt( 1.0 - x_norm * x_norm );
			} else {
			    /* corner */
			    x_norm = 7.0 * x + ( x > 0.0 ? -6.0 : 6.0 );
			    y_norm = -7.0 * y - ( y > 0.0 ? -6.0 : 6.0 );
			    if( ( z_norm = 1 - x_norm * x_norm -
				  y_norm * y_norm ) < 0.0 )
				goto missed;
			    z_norm = sqrt( z_norm );
			}
			
			in++;
			diffuse += 0.2;
			if( ( cos_theta = arLight[ 0 ] * x_norm +
			      arLight[ 1 ] * y_norm +
			      arLight[ 2 ] * z_norm ) > 0.0 ) {
			    diffuse += cos_theta * 0.8;
			    cos_theta = 2 * z_norm * cos_theta -
				arLight[ 2 ];
			    specular += pow( cos_theta, 10 ) * 0.4;
			}
		    }
		missed:		    
		    x += 1.0 / ( 8 * nSize );
		} while( !fx++ );
		y += 1.0 / ( 8 * nSize );
	    } while( !fy++ );

	    for( i = 0; i < 3; i++ )
		*puch++ = clamp( ( diffuse * arColour[ i ] +
				   specular ) * 64.0 );

	    *puch++ = 255 * ( 4 - in ) / 4; /* alpha channel */

	    x_loop += 2.0 / ( 8 * nSize );
	}
	y_loop += 2.0 / ( 8 * nSize );
	puch += nStride;
    }
}



extern void
RenderResign( renderdata *prd, unsigned char *puch, int nStride ) {

  const double arColour[ 4 ] = { 1.0, 1.0, 1.0, 0.0 }; /* white */

  RenderBasicCube( prd->arLight, prd->nSize, arColour,
                   puch, nStride );

}

extern void RenderCube( renderdata *prd, unsigned char *puch, int nStride ) {

  RenderBasicCube( prd->arLight, prd->nSize, prd->arCubeColour,
                   puch, nStride );

}

extern void RenderCubeFaces( renderdata *prd, unsigned char *puch,
			     int nStride, unsigned char *puchCube,
			     int nStrideCube ) {
    int i;
#if HAVE_FREETYPE
    FT_Face ftf;
    FT_Glyph aftg[ 10 ], aftgSmall[ 10 ];
    int fFreetype = FALSE;
    
    if( !FT_New_Memory_Face( ftl, auchLuxiRB, cbLuxiRB, 0, &ftf ) &&
	!FT_Set_Pixel_Sizes( ftf, 0, 5 * prd->nSize ) ) {
	fFreetype = TRUE;
	
	for( i = 0; i < 10; i++ ) {
	    FT_Load_Char( ftf, '0' + i, FT_LOAD_RENDER );
	    FT_Get_Glyph( ftf->glyph, aftg + i );
	}
	
	FT_Set_Pixel_Sizes( ftf, 0, 21 * prd->nSize / 8 );
	
	for( i = 0; i < 10; i++ ) {
	    FT_Load_Char( ftf, '0' + i, FT_LOAD_RENDER );
	    FT_Get_Glyph( ftf->glyph, aftgSmall + i );
	}
	
	FT_Done_Face( ftf );
    }
#endif
    
    for( i = 0; i < 6; i++ ) {
	AlphaBlend( puch, nStride, puch, nStride, puchCube + prd->nSize * 4 +
		    prd->nSize * nStrideCube, nStrideCube, 6 * prd->nSize,
		    6 * prd->nSize );

#if HAVE_FREETYPE
	if( fFreetype )
	    RenderNumber( puch, nStride, aftg, 2 << i, 3 * prd->nSize,
			  78 * prd->nSize / 16, 0, 0, 0x80 );
	else
#endif
	    RenderBasicNumber( puch, nStride, 4 * prd->nSize, 2 << i,
			       3 * prd->nSize, 5 * prd->nSize, 0, 0, 0x80 );
	
	puch += 6 * prd->nSize * nStride;
    }
    
    for( ; i < 12; i++ ) {
	AlphaBlend( puch, nStride, puch, nStride, puchCube + prd->nSize * 4 +
		    prd->nSize * nStrideCube, nStrideCube, 6 * prd->nSize,
		    6 * prd->nSize );

#if HAVE_FREETYPE
	if( fFreetype )
	    RenderNumber( puch, nStride, aftgSmall, 2 << i, 3 * prd->nSize,
			  63 * prd->nSize / 16, 0, 0, 0x80 );
	else
#endif
	    RenderBasicNumber( puch, nStride, 3 * prd->nSize, 2 << i,
			       3 * prd->nSize, 4 * prd->nSize, 0, 0, 0x80 );

	puch += 6 * prd->nSize * nStride;
    }

#if HAVE_FREETYPE
    if( fFreetype )
	for( i = 0; i < 10; i++ ) {
	    FT_Done_Glyph( aftg[ i ] );
	    FT_Done_Glyph( aftgSmall[ i ] );
	}
#endif
}


extern void RenderResignFaces( renderdata *prd, unsigned char *puch,
			     int nStride, unsigned char *puchCube,
			     int nStrideCube ) {
    int i;

#if HAVE_FREETYPE
    FT_Face ftf;
    FT_Glyph aftg[ 10 ], aftgSmall[ 10 ];
    int fFreetype = FALSE;
    
    if( !FT_New_Memory_Face( ftl, auchLuxiRB, cbLuxiRB, 0, &ftf ) &&
	!FT_Set_Pixel_Sizes( ftf, 0, 5 * prd->nSize ) ) {
	fFreetype = TRUE;
	
	for( i = 0; i < 10; i++ ) {
	    FT_Load_Char( ftf, '0' + i, FT_LOAD_RENDER );
	    FT_Get_Glyph( ftf->glyph, aftg + i );
	}
	
	FT_Set_Pixel_Sizes( ftf, 0, 21 * prd->nSize / 8 );
	
	for( i = 0; i < 10; i++ ) {
	    FT_Load_Char( ftf, '0' + i, FT_LOAD_RENDER );
	    FT_Get_Glyph( ftf->glyph, aftgSmall + i );
	}
	
	FT_Done_Face( ftf );
    }
#endif
    
    for( i = 0; i < 3; i++ ) {
	AlphaBlend( puch, nStride, puch, nStride, puchCube + prd->nSize * 4 +
		    prd->nSize * nStrideCube, nStrideCube, 6 * prd->nSize,
		    6 * prd->nSize );

#if HAVE_FREETYPE
	if( fFreetype )
            RenderNumber( puch, nStride, aftg, i + 1,
                          3 * prd->nSize,
			  78 * prd->nSize / 16, 0, 0, 0x80 );
	else
#endif
            RenderBasicNumber( puch, nStride, 4 * prd->nSize, 
                               i + 1, 
                               3 * prd->nSize, 5 * prd->nSize, 0, 0, 0x80 );
	
	puch += 6 * prd->nSize * nStride;
    }
    
#if HAVE_FREETYPE
    if( fFreetype )
	for( i = 0; i < 10; i++ ) 
	    FT_Done_Glyph( aftg[ i ] );
#endif
}


extern void RenderDice( renderdata *prd, unsigned char *puch0,
			unsigned char *puch1, int nStride ) {
    
    int ix, iy, in, fx, fy, i;
    float x, y, x_loop, y_loop, diffuse, specular_x, specular_o, cos_theta,
	x_norm, y_norm, z_norm;
    double *aarDiceColour[ 2 ];
    float arDiceCoefficient[ 2 ], arDiceExponent[ 2 ];

    nStride -= 4 * 7 * prd->nSize;
    
    for( i = 0; i < 2; i++ )
	if( prd->afDieColour[ i ] ) {
	    /* die same color as chequers */
	    aarDiceColour[ i ] = prd->aarColour[ i ];
	    arDiceCoefficient[ i ] = prd->arCoefficient[ i ];
	    arDiceExponent[ i ] = prd->arExponent[ i ];
	} else {
	    /* user color */
	    aarDiceColour[ i ] = prd->aarDiceColour[ i ];
	    arDiceCoefficient[ i ] = prd->arDiceCoefficient[ i ];
	    arDiceExponent[ i ] = prd->arDiceExponent[ i ];
	}

    for( iy = 0, y_loop = -1.0; iy < 7 * prd->nSize; iy++ ) {
	for( ix = 0, x_loop = -1.0; ix < 7 * prd->nSize; ix++ ) {
	    in = 0;
	    diffuse = specular_x = specular_o = 0.0;
	    fy = 0;
	    y = y_loop;
	    do {
		fx = 0;
		x = x_loop;
		do {
		    if( fabs( x ) < 6.0 / 7.0 &&
			fabs( y ) < 6.0 / 7.0 ) {
			/* flat surface */
			in++;
			diffuse += prd->arLight[ 2 ] * 0.8 + 0.2;
			specular_x += pow( prd->arLight[ 2 ],
					   arDiceExponent[ 0 ] ) *
			    arDiceCoefficient[ 0 ];
			specular_o += pow( prd->arLight[ 2 ],
					   arDiceExponent[ 1 ] ) *
			    arDiceCoefficient[ 1 ];
		    } else {
			if( fabs( x ) < 6.0 / 7.0 ) {
			    /* top/bottom edge */
			    x_norm = 0.0;
			    y_norm = -7.0 * y - ( y > 0.0 ? -6.0 : 6.0 );
			    z_norm = ssqrt( 1.0 - y_norm * y_norm );
			} else if( fabs( y ) < 6.0 / 7.0 ) {
			    /* left/right edge */
			    x_norm = 7.0 * x + ( x > 0.0 ? -6.0 : 6.0 );
			    y_norm = 0.0;
			    z_norm = ssqrt( 1.0 - x_norm * x_norm );
			} else {
			    /* corner */
			    x_norm = 7.0 * x + ( x > 0.0 ? -6.0 : 6.0 );
			    y_norm = -7.0 * y - ( y > 0.0 ? -6.0 : 6.0 );
			    if( ( z_norm = 1 - x_norm * x_norm -
				  y_norm * y_norm ) < 0.0 )
				goto missed;
			    z_norm = sqrt( z_norm );
			}
			
			in++;
			diffuse += 0.2;
			if( ( cos_theta = prd->arLight[ 0 ] * x_norm +
			      prd->arLight[ 1 ] * y_norm +
			      prd->arLight[ 2 ] * z_norm ) > 0.0 ) {
			    diffuse += cos_theta * 0.8;
			    if( ( cos_theta = 2 * z_norm * cos_theta -
				  prd->arLight[ 2 ] ) > 0.0 ) {
				specular_x += pow( cos_theta,
						   arDiceExponent[ 0 ] ) *
				    arDiceCoefficient[ 0 ];
				specular_o += pow( cos_theta,
						   arDiceExponent[ 1 ] ) *
				    arDiceCoefficient[ 1 ];
			    }
			}
		    }
		missed:		    
		    x += 1.0 / ( 7 * prd->nSize );
		} while( !fx++ );
		y += 1.0 / ( 7 * prd->nSize );
	    } while( !fy++ );

	    for( i = 0; i < 3; i++ )
		*puch0++ = clamp( ( diffuse * aarDiceColour[ 0 ][ i ] +
				    specular_x ) * 64.0 );
	    *puch0++ = 255 * ( 4 - in ) / 4; /* alpha channel */
	    
	    for( i = 0; i < 3; i++ )
		*puch1++ = clamp( ( diffuse * aarDiceColour[ 1 ][ i ] +
				    specular_o ) * 64.0 );
	    *puch1++ = 255 * ( 4 - in ) / 4; /* alpha channel */
	    
	    x_loop += 2.0 / ( 7 * prd->nSize );
	}
	y_loop += 2.0 / ( 7 * prd->nSize );
	puch0 += nStride;
	puch1 += nStride;
    }
}

extern void RenderPips( renderdata *prd, unsigned char *puch0,
			unsigned char *puch1, int nStride ) {

    int ix, iy, in, fx, fy, i;
    float x, y, z, x_loop, y_loop, diffuse, specular_x, specular_o, cos_theta,
	dice_top[ 2 ][ 3 ];
    double *aarDiceColour[ 2 ];
    float arDiceCoefficient[ 2 ], arDiceExponent[ 2 ];

    nStride -= 3 * prd->nSize;
    
    for( i = 0; i < 2; i++ )
	if( prd->afDieColour[ i ] ) {
	    /* die same color as chequers */
	    aarDiceColour[ i ] = prd->aarColour[ i ];
	    arDiceCoefficient[ i ] = prd->arCoefficient[ i ];
	    arDiceExponent[ i ] = prd->arExponent[ i ];
	} else {
	    /* user color */
	    aarDiceColour[ i ] = prd->aarDiceColour[ i ];
	    arDiceCoefficient[ i ] = prd->arDiceCoefficient[ i ];
	    arDiceExponent[ i ] = prd->arDiceExponent[ i ];
	}

    diffuse = prd->arLight[ 2 ] * 0.8 + 0.2;
    specular_x = pow( prd->arLight[ 2 ], arDiceExponent[ 0 ] ) *
	arDiceCoefficient[ 0 ];
    specular_o = pow( prd->arLight[ 2 ], arDiceExponent[ 1 ] ) *
	arDiceCoefficient[ 1 ];
    dice_top[ 0 ][ 0 ] = 
      ( diffuse * aarDiceColour[ 0 ][ 0 ] + specular_x ) * 64.0;
    dice_top[ 0 ][ 1 ] = 
      ( diffuse * aarDiceColour[ 0 ][ 1 ] + specular_x ) * 64.0;
    dice_top[ 0 ][ 2 ] = 
      ( diffuse * aarDiceColour[ 0 ][ 2 ] + specular_x ) * 64.0;
    dice_top[ 1 ][ 0 ] = 
      ( diffuse * aarDiceColour[ 1 ][ 0 ] + specular_o ) * 64.0;
    dice_top[ 1 ][ 1 ] = 
      ( diffuse * aarDiceColour[ 1 ][ 1 ] + specular_o ) * 64.0;
    dice_top[ 1 ][ 2 ] = 
      ( diffuse * aarDiceColour[ 1 ][ 2 ] + specular_o ) * 64.0;

    for( iy = 0, y_loop = -1.0; iy < prd->nSize; iy++ ) {
	for( ix = 0, x_loop = -1.0; ix < prd->nSize; ix++ ) {
	    in = 0;
	    diffuse = specular_x = specular_o = 0.0;
	    fy = 0;
	    y = y_loop;
	    do {
		fx = 0;
		x = x_loop;
		do {
		    if( ( z = 1.0 - x * x - y * y ) > 0.0 ) {
			in++;
			diffuse += 0.2;
			z = sqrt( z ) * 5;
			if( ( cos_theta = ( -prd->arLight[ 0 ] * x +
					    prd->arLight[ 1 ] * y +
					    prd->arLight[ 2 ] * z ) /
			      sqrt( x * x + y * y + z * z ) ) > 0 ) {
			    diffuse += cos_theta * 0.8;
			    if( ( cos_theta = 2 * z / 5 * cos_theta -
				  prd->arLight[ 2 ] ) > 0.0 ) {
				specular_x += pow( cos_theta,
						   arDiceExponent[ 0 ] ) *
				    arDiceCoefficient[ 0 ];
				specular_o += pow( cos_theta,
						   arDiceExponent[ 1 ] ) *
				    arDiceCoefficient[ 1 ];
			    }
			}
		    }
		    x += 1.0 / ( prd->nSize );
		} while( !fx++ );
		y += 1.0 / ( prd->nSize );
	    } while( !fy++ );

	    for( i = 0; i < 3; i++ )
		*puch0++ =
		    clamp( ( diffuse * prd->aarDiceDotColour[ 0 ][ i ] +
			     specular_x ) * 64.0 + ( 4 - in ) *
			   dice_top[ 0 ][ i ] );
	    
	    for( i = 0; i < 3; i++ )
		*puch1++ =
		    clamp( ( diffuse * prd->aarDiceDotColour[ 1 ][ i ] +
			     specular_o ) * 64.0 + ( 4 - in ) *
			   dice_top[ 1 ][ i ] );

	    x_loop += 2.0 / ( prd->nSize );
	}
	y_loop += 2.0 / ( prd->nSize );
	puch0 += nStride;
	puch1 += nStride;
    }
}

static void Copy_RGB_to_RGBA( unsigned char *puchDest, int nDestStride,
		              unsigned char *puchSrc, int nSrcStride,
		              int cx, int cy, unsigned char uchAlpha ) {
/* copy an 24-bit RGB buffer into an 24+8-bit RGBA buffer, setting
   the alpha channel to uchAlpha */

    int x;

    nDestStride -= cx * 4;  /* 8 bit alpha + 24 packed rgb bits */
    nSrcStride -= cx * 3;   /* 24 packed rgb bits */

    for( ; cy; cy-- ) {
	for( x = cx; x; x-- ) {
	    *puchDest++ = *puchSrc++;
	    *puchDest++ = *puchSrc++;
	    *puchDest++ = *puchSrc++;
	    *puchDest++ = uchAlpha;
	}
	puchDest += nDestStride;
	puchSrc += nSrcStride;
    }
}

static void InsertAlpha( unsigned char *puchDest, int nDestStride,
		         unsigned char *puchAlpha, int nAlphaStride,
		         int cx, int cy ) {
/* insert an alpha channel mask into an 24+8-bit RGBA buffer of
   dimension cx x cy */

    int x;

    nDestStride -= cx * 4;  /* 8 bit alpha + 24 packed rgb bits */
    nAlphaStride -= cx * 1; /* 8 bit alpha */

    for( ; cy; cy-- ) {
	for( x = cx; x; x-- ) {
	    puchDest += 3;  /* advance to alpha */
	    *puchDest++ = *puchAlpha++;
	}
	puchDest += nDestStride;
	puchAlpha += nAlphaStride;
    }
}

#if HAVE_LIBART
static ArtBpath * Make_Path_Arrow( void ) {
/* set up a B-spline vector path representing an arrow */

    ArtBpath *vec = NULL;

    vec = art_new( ArtBpath, 10 );
    vec[0].code = ART_MOVETO;
    vec[0].y1 = 0.;
    vec[0].y1 = 0.;
    vec[0].y2 = 0.;
    vec[0].y2 = 0.;
    vec[0].x3 = 5.;
    vec[0].y3 = 0.;
    vec[1].code = ART_LINETO;
    vec[1].y1 = 0.;
    vec[1].y1 = 0.;
    vec[1].y2 = 0.;
    vec[1].y2 = 0.;
    vec[1].x3 = 0.;
    vec[1].y3 = 4.;
    vec[2].code = ART_LINETO;
    vec[2].y1 = 0.;
    vec[2].y1 = 0.;
    vec[2].y2 = 0.;
    vec[2].y2 = 0.;
    vec[2].x3 = 3.;
    vec[2].y3 = 4.;
    vec[3].code = ART_LINETO;
    vec[3].y1 = 0.;
    vec[3].y1 = 0.;
    vec[3].y2 = 0.;
    vec[3].y2 = 0.;
    vec[3].x3 = 3.;
    vec[3].y3 = 10.;
    vec[4].code = ART_LINETO;
    vec[4].y1 = 0.;
    vec[4].y1 = 0.;
    vec[4].y2 = 0.;
    vec[4].y2 = 0.;
    vec[4].x3 = 7.;
    vec[4].y3 = 10.;
    vec[5].code = ART_LINETO;
    vec[5].y1 = 0.;
    vec[5].y1 = 0.;
    vec[5].y2 = 0.;
    vec[5].y2 = 0.;
    vec[5].x3 = 7.;
    vec[5].y3 = 4.;
    vec[6].code = ART_LINETO;
    vec[6].y1 = 0.;
    vec[6].y1 = 0.;
    vec[6].y2 = 0.;
    vec[6].y2 = 0.;
    vec[6].x3 = 10.;
    vec[6].y3 = 4.;
    vec[7].code = ART_LINETO;
    vec[7].y1 = 0.;
    vec[7].y1 = 0.;
    vec[7].y2 = 0.;
    vec[7].y2 = 0.;
    vec[7].x3 = 5.;
    vec[7].y3 = 0.;
    vec[8].code = ART_END;

    return vec;
}
#endif /* HAVE_LIBART */

#if HAVE_LIBART
static void Render_Path( art_u8 *puchRGBAbuf, const ArtBpath* bpPath,
			 int iWidth, int iHeight,
			 art_u32 fg_color, art_u32 bg_color ) {
/* render an anti-aliased vector path filled with fg_color into an RGBA buffer
   of dimension iWidth x iHeight, make background transparent (alpha mask) */

    art_u8 *puchRGBbuf = NULL;
    art_u8 *puchAlphabuf = NULL;
    double adAffine[6];
    ArtBpath *bpTransformed = NULL;
    ArtVpath *vpTransformed = NULL;
    ArtSVP *svp = NULL;

    art_affine_scale( adAffine, iWidth/10.0, iHeight/10.0 ); /* assume a vector area of [0,10] */
    bpTransformed = art_bpath_affine_transform( bpPath, adAffine );

    vpTransformed = art_bez_path_to_vec( bpTransformed, 0. );
    svp = art_svp_from_vpath( vpTransformed );
    art_free( vpTransformed );

    puchRGBbuf = art_new( art_u8, iWidth*iHeight*3 );	/* 24 packed rgb bits */
    assert( puchRGBbuf );
    /* fill with background colour */
    art_rgb_fill_run( puchRGBbuf, 0x00, 0x00, 0x00, iWidth*iHeight );
    /* render foreground */
    art_rgb_svp_aa( svp, 0, 0, iWidth, iHeight, fg_color, bg_color,
		    puchRGBbuf, iWidth*3, NULL );
    /* convert */
    assert( puchRGBAbuf );
    Copy_RGB_to_RGBA( puchRGBAbuf, iWidth*4, puchRGBbuf, iWidth*3,
		      iWidth, iHeight, 0xFF );
    art_free( puchRGBbuf );
    /* render alpha */
    puchAlphabuf = art_new( art_u8, iWidth*iHeight*1 );	/* 8 bit alpha */
    assert( puchAlphabuf );
    art_gray_svp_aa( svp, 0, 0, iWidth, iHeight, puchAlphabuf, iWidth*1 );
    /* integrate alpha information from puchAlphabuf into puchRGBAbuf */
    InsertAlpha( puchRGBAbuf, iWidth*4,
		 puchAlphabuf, iWidth*1, iWidth, iHeight );
    art_free( puchAlphabuf );
    art_free( svp );
}
#endif /* HAVE_LIBART */

#if HAVE_LIBART
#define ARROW_SIZE 5
extern void RenderArrows( renderdata *prd, unsigned char* puch0,
			  unsigned char* puch1, int nStride ) {
/* render arrows for direction of play and player on turn */

    ArtBpath *bpArrow = NULL;
    ArtBpath *bpTmp = NULL;
    double adAffine[6];
    art_u32 fg_colour = (0xFF << 24) |
			( (art_u8) ( prd->aarColour[ 0 ][ 0 ] * 0xFF ) << 16 ) |
			( (art_u8) ( prd->aarColour[ 0 ][ 1 ] * 0xFF ) <<  8 ) |
			( (art_u8) ( prd->aarColour[ 0 ][ 2 ] * 0xFF ) );	/* AARRGGBB */
    art_u32 bg_colour = 0xFFC9CACB;

    bpArrow = Make_Path_Arrow();

    /* player 0 */
    Render_Path( (art_u8 *) puch0, bpArrow, prd->nSize * ARROW_SIZE, prd->nSize * ARROW_SIZE,
			 fg_colour, bg_colour );

    /* set up a 180� rotation around (5,5) */
    adAffine[0] = -1.;
    adAffine[1] = 0.;
    adAffine[2] = 0.;
    adAffine[3] = -1.;
    adAffine[4] = 10.;
    adAffine[5] = 10.;
    bpTmp = art_bpath_affine_transform( bpArrow, adAffine );
    art_free( bpArrow );

    fg_colour = (0xFF << 24) |
		( (art_u8) ( prd->aarColour[ 1 ][ 0 ] * 0xFF ) << 16 ) |
		( (art_u8) ( prd->aarColour[ 1 ][ 1 ] * 0xFF ) <<  8 ) |
		( (art_u8) ( prd->aarColour[ 1 ][ 2 ] * 0xFF ) );	/* AARRGGBB */

    /* player 1 */
    Render_Path( (art_u8 *) puch1, bpTmp, prd->nSize * ARROW_SIZE, prd->nSize * ARROW_SIZE,
			 fg_colour, bg_colour );

    art_free( bpTmp );
}
#endif /* HAVE_LIBART */

static void PointArea( renderdata *prd, int n, int *px, int *py,
		       int *pcx, int *pcy ) {
    
    int c = ( !n || n == 25 ) ? 3 : 5;
    
    *px = aaanPositions[ prd->fClockwise ][ n ][ 0 ] * prd->nSize;
    *py = aaanPositions[ prd->fClockwise ][ n ][ 1 ] * prd->nSize;
    *pcx = 6 * prd->nSize;
    *pcy = aaanPositions[ prd->fClockwise ][ n ][ 2 ] * prd->nSize;
    
    if( *pcy > 0 ) {
	*pcy = *pcy * ( c - 1 ) + 6 * prd->nSize;
	*py += 6 * prd->nSize - *pcy;
    } else
	*pcy = -*pcy * ( c - 1 ) + 6 * prd->nSize;
}

static void DrawChequers( renderdata *prd, unsigned char *puch, int nStride,
			  renderimages *pri, int iPoint, int n, int f,
			  int x, int y, int cx, int cy ) {
    int i, c, yChequer;

    c = ( !iPoint || iPoint == 25 ) ? 3 : 5;
    yChequer = aaanPositions[ prd->fClockwise ][ iPoint ][ 1 ] * prd->nSize;
    
    for( i = 0; i < n; i++ ) {
	RefractBlendClip( puch, nStride,
			  aaanPositions[ prd->fClockwise ][ iPoint ][ 0 ] *
			  prd->nSize - x, yChequer - y, cx, cy,
			  pri->ach, 108 * prd->nSize * 3,
			  aaanPositions[ prd->fClockwise ][ iPoint ][ 0 ] *
			  prd->nSize, yChequer, pri->achChequer[ f ],
			  6 * prd->nSize * 4, 0, 0, pri->asRefract[ f ],
			  6 * prd->nSize, 6 * prd->nSize, 6 * prd->nSize );

	if( i == c - 1 )
	    break;
	
	yChequer -= aaanPositions[ prd->fClockwise ][ iPoint ][ 2 ] *
	    prd->nSize;
    }

    if( n > c )
	CopyAreaClip( puch, nStride,
		      ( aaanPositions[ prd->fClockwise ][ iPoint ][ 0 ] + 1 ) *
		      prd->nSize - x, yChequer + prd->nSize - y, cx, cy,
		      pri->achChequerLabels, 4 * prd->nSize * 3,
		      0, 4 * prd->nSize * ( n - 4 ), 4 * prd->nSize,
		      4 * prd->nSize );
}

extern void CalculateArea( renderdata *prd, unsigned char *puch, int nStride,
			   renderimages *pri, int anBoard[ 2 ][ 25 ],
			   int *anOff, int anDice[ 2 ],
			   int anDicePosition[ 2 ][ 2 ],
			   int fDiceColour, int anCubePosition[ 2 ],
			   int nLogCube, int nCubeOrientation,
                           int anResignPosition[ 2 ],
                           int fResign, int nResignOrientation,
                           int anArrowPosition[ 2 ],
			   int fPlaying, int nPlayer,
			   int x, int y, int cx, int cy ) {
    
    int i, xPoint, yPoint, cxPoint, cyPoint, n;
    int anOffCalc[ 2 ];
    
    if( x < 0 ) {
	puch -= x * 3;
	cx += x;
	x = 0;
    }
    
    if( y < 0 ) {
	puch -= y * nStride;
	cy += y;
	y = 0;
    }

    if( x + cx > 108 * prd->nSize )
	cx = 108 * prd->nSize - x;
    
    if( y + cy > 72 * prd->nSize )
	cy = 72 * prd->nSize - y;    
    
    if( cx <= 0 || cy <= 0 )
	return;

    /* draw board */
    CopyArea( puch, nStride, pri->ach + x * 3 + y * 108 * prd->nSize * 3,
	      108 * prd->nSize * 3, cx, cy );

    if( !anOff ) {
	anOff = anOffCalc;
	anOff[ 0 ] = anOff[ 1 ] = 15;
	for( i = 0; i < 25; i++ ) {
	    anOff[ 0 ] -= anBoard[ 0 ][ i ];
	    anOff[ 1 ] -= anBoard[ 1 ][ i ];
	}
    }

    /* draw labels */

    if ( intersects( x, y, cx, cy, 0, 0, 108 * prd->nSize, 3 * prd->nSize ) ) {

      AlphaBlendClip( puch, nStride,
                      -x, -y, 
                      cx, cy, 
                      puch, nStride,
                      -x, -y,
                      pri->achLabels[ prd->fDynamicLabels ? nPlayer : 1 ], 
                      108 * prd->nSize * 4,
                      0, 0, 108 * prd->nSize, 3 * prd->nSize );

    }

    if ( intersects( x, y, cx, cy, 0, 69 * prd->nSize, 
                     108 * prd->nSize, 3 * prd->nSize ) ) {

      AlphaBlendClip( puch, nStride,
                      -x, 69 * prd->nSize  - y, 
                      cx, cy, 
                      puch, nStride,
                      -x, 69 * prd->nSize - y,
                      pri->achLabels[ prd->fDynamicLabels ? ! nPlayer : 0 ], 
                      108 * prd->nSize * 4,
                      0, 0, 108 * prd->nSize, 3 * prd->nSize );

    }

    /* draw points */

    for( i = 0; i < 28; i++ ) {
	PointArea( prd, i, &xPoint, &yPoint, &cxPoint, &cyPoint );
	if( intersects( x, y, cx, cy, xPoint, yPoint, cxPoint,
			cyPoint ) ) {
	    switch( i ) {
	    case 0:
		/* top player on bar */
		n = -anBoard[ 0 ][ 24 ];
		break;
	    case 25:
		/* bottom player on bar */
		n = anBoard[ 1 ][ 24 ];
		break;
	    case 26:
		/* bottom player borne off */
		n = anOff[ 1 ];
		break;
	    case 27:
		/* top player borne off */
		n = -anOff[ 0 ];
		break;
	    default:
		/* ordinary point */
		n = anBoard[ 1 ][ i - 1 ] - anBoard[ 0 ][ 24 - i ];
		break;
	    }
	    if( n )
		DrawChequers( prd, puch, nStride, pri, i, abs( n ),
			      n > 0, x, y, cx, cy );
	}
    }
    
    /* draw dice */
    for( i = 0; i < 2; i++ )
	if( intersects( x, y, cx, cy, anDicePosition[ i ][ 0 ] * prd->nSize,
			anDicePosition[ i ][ 1 ] * prd->nSize, 7 * prd->nSize,
			7 * prd->nSize ) ) {
	    int ix, iy, afPip[ 9 ], n = anDice[ i ];
	    
	    AlphaBlendClip( puch, nStride,
			    anDicePosition[ i ][ 0 ] * prd->nSize - x,
			    anDicePosition[ i ][ 1 ] * prd->nSize - y,
			    cx, cy, puch, nStride,
			    anDicePosition[ i ][ 0 ] * prd->nSize - x,
			    anDicePosition[ i ][ 1 ] * prd->nSize - y,
			    pri->achDice[ fDiceColour ], prd->nSize * 7 * 4,
			    0, 0, prd->nSize * 7, prd->nSize * 7 );

	    afPip[ 0 ] = afPip[ 8 ] = ( n == 4 ) || ( n == 5 ) || ( n == 6 ) ||
		( ( ( n == 2 ) || ( n == 3 ) ) &&
		  anDicePosition[ i ][ 0 ] & 1 );
	    afPip[ 1 ] = afPip[ 7 ] = n == 6 &&
		!( anDicePosition[ i ][ 0 ] & 1 );
	    afPip[ 2 ] = afPip[ 6 ] = ( n == 4 ) || ( n == 5 ) || ( n == 6 ) ||
		( ( ( n == 2 ) || ( n == 3 ) ) &&
		  !( anDicePosition[ i ][ 0 ] & 1 ) );
	    afPip[ 3 ] = afPip[ 5 ] = n == 6 && anDicePosition[ i ][ 0 ] & 1;
	    afPip[ 4 ] = n & 1;

	    for( iy = 0; iy < 3; iy++ )
		for( ix = 0; ix < 3; ix++ )
		    if( afPip[ iy * 3 + ix ] )
			CopyAreaClip( puch, nStride,
				      ( anDicePosition[ i ][ 0 ] + 1 + 2 * ix )
				      * prd->nSize - x,
				      ( anDicePosition[ i ][ 1 ] + 1 + 2 * iy )
				      * prd->nSize - y,
				      cx, cy, pri->achPip[ fDiceColour ],
				      prd->nSize * 3, 0, 0,
				      prd->nSize, prd->nSize );
	}

    /* draw cube */
    if( nLogCube >= 0 && intersects( x, y, cx, cy,
				     anCubePosition[ 0 ] * prd->nSize, 
				     anCubePosition[ 1 ] * prd->nSize,
				     8 * prd->nSize, 8 * prd->nSize ) ) {
	AlphaBlendClip( puch, nStride,
			anCubePosition[ 0 ] * prd->nSize - x,
			anCubePosition[ 1 ] * prd->nSize - y,
			cx, cy, puch, nStride,
			anCubePosition[ 0 ] * prd->nSize - x,
			anCubePosition[ 1 ] * prd->nSize - y,
			pri->achCube, prd->nSize * 8 * 4,
			0, 0, prd->nSize * 8, prd->nSize * 8 );

	if( nLogCube < 1 )
	    nLogCube = 6; /* 64 */
	else if( nLogCube > 11 )
	    nLogCube = 12; /* 4096 */
	
	CopyAreaRotateClip( puch, nStride,
			    ( anCubePosition[ 0 ] + 1 ) * prd->nSize - x,
			    ( anCubePosition[ 1 ] + 1 ) * prd->nSize - y,
			    cx, cy, pri->achCubeFaces, prd->nSize * 6 * 3,
			    0, prd->nSize * 6 * ( nLogCube - 1 ),
			    prd->nSize * 6, prd->nSize * 6,
			    nCubeOrientation + 1 );
    }

    /* draw resignation */

    if( fResign && intersects( x, y, cx, cy,
                               anResignPosition[ 0 ] * prd->nSize, 
                               anResignPosition[ 1 ] * prd->nSize,
                               8 * prd->nSize, 8 * prd->nSize ) ) {

	AlphaBlendClip( puch, nStride,
			anResignPosition[ 0 ] * prd->nSize - x,
			anResignPosition[ 1 ] * prd->nSize - y,
			cx, cy, puch, nStride,
			anResignPosition[ 0 ] * prd->nSize - x,
			anResignPosition[ 1 ] * prd->nSize - y,
			pri->achResign, prd->nSize * 8 * 4,
			0, 0, prd->nSize * 8, prd->nSize * 8 );

	CopyAreaRotateClip( puch, nStride,
			    ( anResignPosition[ 0 ] + 1 ) * prd->nSize - x,
			    ( anResignPosition[ 1 ] + 1 ) * prd->nSize - y,
			    cx, cy, pri->achResignFaces, prd->nSize * 6 * 3,
			    0, prd->nSize * 6 * ( fResign - 1 ),
			    prd->nSize * 6, prd->nSize * 6,
			    nResignOrientation + 1 );
    }

    /* draw arrow for direction of play */

#if HAVE_LIBART
    if( fPlaying && intersects( x, y, cx, cy,
                    anArrowPosition[ 0 ], anArrowPosition[ 1 ],
                    ARROW_SIZE * prd->nSize, ARROW_SIZE * prd->nSize ) ) {

	assert( anArrowPosition );

	AlphaBlendClip2( puch, nStride,
			 anArrowPosition[ 0 ] - x, anArrowPosition[ 1 ] - y,
			 cx, cy,
			 puch, nStride,
			 anArrowPosition[ 0 ] - x, anArrowPosition[ 1 ] - y,
			 (unsigned char *) pri->auchArrow[ nPlayer ],
			 prd->nSize * ARROW_SIZE * 4,
			 0, 0,
			 prd->nSize * ARROW_SIZE,
			 prd->nSize * ARROW_SIZE );
    }
#endif /* HAVE_LIBART */

}

extern void
RenderBoardLabels( renderdata *prd, 
                   unsigned char *achLo, unsigned char *achHi, int nStride ) {

    unsigned char *achTemp = malloc( 108 * prd->nSize * 3 * prd->nSize * 3 );

    /* 12 11 10 9 8 7 - 6 5 4 3 2 1 */
    
    memset( achTemp, 0, 108 * prd->nSize * 3 * prd->nSize * 3 );
    RenderLabels( prd, achTemp, prd->nSize * 108 * 3, 1, 12, 1 );
    
    Copy_RGB_to_RGBA( achLo, prd->nSize * 108 * 4,
                      achTemp, prd->nSize * 108 * 3,
                      prd->nSize * 108, prd->nSize * 3, 0xFF );

    /* 13 14 15 16 17 18 - 19 20 21 22 24 24 */
    
    memset( achTemp, 0, 108 * prd->nSize * 3 * prd->nSize * 3 );
    RenderLabels( prd, achTemp, prd->nSize * 108 * 3, 24, 13, -1 );
    
    Copy_RGB_to_RGBA( achHi, prd->nSize * 108 * 4,
                      achTemp, prd->nSize * 108 * 3,
                      prd->nSize * 108, prd->nSize * 3, 0xFF );

    free( achTemp );

}

extern void RenderImages( renderdata *prd, renderimages *pri ) {
  
    int i;
    int nSize = prd->nSize;

    pri->ach = malloc( nSize * nSize * 108 * 72 * 3 );
    pri->achChequer[ 0 ] = malloc( nSize * nSize * 6 * 6 * 4 );
    pri->achChequer[ 1 ] = malloc( nSize * nSize * 6 * 6 * 4 );
    pri->achChequerLabels = malloc( nSize * nSize * 4 * 4 * 3 * 12 );
    pri->achDice[ 0 ] = malloc( nSize * nSize * 7 * 7 * 4 );
    pri->achDice[ 1 ] = malloc( nSize * nSize * 7 * 7 * 4 );
    pri->achPip[ 0 ] = malloc( nSize * nSize * 3 );
    pri->achPip[ 1 ] = malloc( nSize * nSize * 3 );
    pri->achCube = malloc( nSize * nSize * 8 * 8 * 4 );
    pri->achCubeFaces = malloc( nSize * nSize * 6 * 6 * 3 * 12 );
    pri->asRefract[ 0 ] = malloc( nSize * nSize * 6 * 6 *
				  sizeof (unsigned short) );
    pri->asRefract[ 1 ] = malloc( nSize * nSize * 6 * 6 *
				  sizeof (unsigned short) );
    pri->achResign = malloc ( nSize * nSize * 8 * 8 * 4 );
    pri->achResignFaces = malloc ( nSize * nSize * 6 * 6 * 3 * 3 );
#if HAVE_LIBART
    pri->auchArrow[0] = art_new( art_u8, nSize * nSize * ARROW_SIZE * ARROW_SIZE * 4 );
    pri->auchArrow[1] = art_new( art_u8, nSize * nSize * ARROW_SIZE * ARROW_SIZE * 4 );
#else
    pri->auchArrow[0] = NULL;
    pri->auchArrow[1] = NULL;
#endif /* HAVE_LIBART */
    for ( i = 0; i < 2; ++i )
      pri->achLabels[ i ] = malloc( nSize * nSize * 108 * 3 * 4 );
    
    RenderBoard( prd, pri->ach, 108 * nSize * 3 );
    RenderChequers( prd, pri->achChequer[ 0 ], pri->achChequer[ 1 ],
		    pri->asRefract[ 0 ], pri->asRefract[ 1 ], nSize * 6 * 4 );
    RenderChequerLabels( prd, pri->achChequerLabels, nSize * 4 * 3 );
    RenderDice( prd, pri->achDice[ 0 ], pri->achDice[ 1 ], nSize * 7 * 4 );
    RenderPips( prd, pri->achPip[ 0 ], pri->achPip[ 1 ], nSize * 3 );
    RenderCube( prd, pri->achCube, nSize * 8 * 4 );
    RenderCubeFaces( prd, pri->achCubeFaces, nSize * 6 * 3, pri->achCube,
		     nSize * 8 * 4 );
    RenderResign( prd, pri->achResign, nSize * 8 * 4 );
    RenderResignFaces( prd, pri->achResignFaces, nSize * 6 * 3, pri->achResign,
                       nSize * 8 * 4 );
#if HAVE_LIBART
    RenderArrows( prd, pri->auchArrow[0], pri->auchArrow[1], nSize * ARROW_SIZE * 4 );
#undef ARROW_SIZE
#endif /* HAVE_LIBART */

    RenderBoardLabels( prd, pri->achLabels[ 0 ], pri->achLabels[ 1 ],
                       108 * nSize * 4 );

}

extern void FreeImages( renderimages *pri ) {

    int i;

    free( pri->ach );
    free( pri->achChequer[ 0 ] );
    free( pri->achChequer[ 1 ] );
    free( pri->achChequerLabels );
    free( pri->achDice[ 0 ] );
    free( pri->achDice[ 1 ] );
    free( pri->achPip[ 0 ] );
    free( pri->achPip[ 1 ] );
    free( pri->achCube );
    free( pri->achCubeFaces );
    free( pri->asRefract[ 0 ] );
    free( pri->asRefract[ 1 ] );
    free( pri->achResign );
    free( pri->achResignFaces );
#if HAVE_LIBART
    art_free( pri->auchArrow[0] );
    art_free( pri->auchArrow[1] );
#endif /* HAVE_LIBART */
    for ( i = 0; i < 2; ++i )
      free( pri->achLabels[ i ] );
}

extern void RenderInitialise( void ) {
    
    irandinit( &rc, FALSE );

#if HAVE_FREETYPE
    FT_Init_FreeType( &ftl );
#endif
}
