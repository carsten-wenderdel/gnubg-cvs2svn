/*
 * drawboard.c
 *
 * by Gary Wong <gtw@gnu.org>, 1999, 2000, 2001, 2002
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

#include "config.h"

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "drawboard.h"
#include "positionid.h"
#include <glib/gi18n.h>

int fClockwise = FALSE; /* Player 1 moves clockwise */

/*
 *  GNU Backgammon  Position ID: 0123456789ABCD 
 *                  Match ID   : 0123456789ABCD
 *  +13-14-15-16-17-18------19-20-21-22-23-24-+     O: gnubg (Cube: 2)
 *  |                  |   | O  O  O  O     O | OO  0 points
 *  |                  |   | O     O          | OO  Cube offered at 2
 *  |                  |   |       O          | O
 *  |                  |   |                  | O
 *  |                  |   |                  | O   
 * v|                  |BAR|                  |     Cube: 1 (7 point match)
 *  |                  |   |                  | X    
 *  |                  |   |                  | X
 *  |                  |   |                  | X
 *  |                  |   |       X  X  X  X | X   Rolled 11
 *  |                  |   |    X  X  X  X  X | XX  0 points
 *  +12-11-10--9--8--7-------6--5--4--3--2--1-+     X: Gary (Cube: 2)
 *
 */

static char *DrawBoardStd( char *sz, int anBoard[ 2 ][ 25 ], int fRoll,
                           char *asz[], char *szMatchID, 
                           int nChequers ) {

    char *pch = sz, *pchIn;
    int x, y, an[ 2 ][ 25 ], cOffO = nChequers, cOffX = nChequers;
    static char achX[ 16 ] = "     X6789ABCDEF",
        achO[ 16 ] = "     O6789ABCDEF";

    for( x = 0; x < 25; x++ ) {
        cOffO -= anBoard[ 0 ][ x ];
        cOffX -= anBoard[ 1 ][ x ];
    }
    
    strcpy( pch, _ (" GNU Backgammon  Position ID: ") );

    pch += 30;

    if( fRoll )
        strcpy( pch, PositionID( anBoard ) );
    else {
        for( x = 0; x < 25; x++ ) {
            an[ 0 ][ x ] = anBoard[ 1 ][ x ];
            an[ 1 ][ x ] = anBoard[ 0 ][ x ];
        }
        
        strcpy( pch, PositionID( an ) );
    }
    
    pch += 14;
    *pch++ = '\n';

    /* match id */

    if ( szMatchID && *szMatchID ) {
      sprintf ( pch, _("                 Match ID   : %s\n"), szMatchID );
      pch = strchr ( pch, 0 );
    }
            
    strcpy( pch, fRoll ? " +13-14-15-16-17-18------19-20-21-22-23-24-+     " :
            " +12-11-10--9--8--7-------6--5--4--3--2--1-+     " );
    pch += 49;

    if( asz[ 0 ] )
        for( pchIn = asz[ 0 ]; *pchIn; pchIn++ )
            *pch++ = *pchIn;

    *pch++ = '\n';
    
    for( y = 0; y < 4; y++ ) {
        *pch++ = ' ';
        *pch++ = '|';

        for( x = 12; x < 18; x++ ) {
            *pch++ = ' ';
            *pch++ = anBoard[ 1 ][ x ] > y ? 'X' :
                anBoard[ 0 ][ 23 - x ] > y ? 'O' : ' ';
            *pch++ = ' ';
        }

        *pch++ = '|';
        *pch++ = ' ';
        *pch++ = anBoard[ 0 ][ 24 ] > y ? 'O' : ' ';
        *pch++ = ' ';
        *pch++ = '|';
        
        for( ; x < 24; x++ ) {
            *pch++ = ' ';
            *pch++ = anBoard[ 1 ][ x ] > y ? 'X' :
                anBoard[ 0 ][ 23 - x ] > y ? 'O' : ' ';
            *pch++ = ' ';
        }

        *pch++ = '|';
        *pch++ = ' ';

        for( x = 0; x < 3; x++ )
            *pch++ = ( cOffO > 5 * x + y ) ? 'O' : ' ';

        *pch++ = ' ';
        
        if( y < 2 && asz[ y + 1 ] )
            for( pchIn = asz[ y + 1 ]; *pchIn; pchIn++ )
                *pch++ = *pchIn;
#if USE_TIMECONTROL
	if ((y==3) && asz[7])
            for( pchIn = asz[ 7 ]; *pchIn; pchIn++ )
                *pch++ = *pchIn;
#endif

        *pch++ = '\n';
    }

    *pch++ = ' ';
    *pch++ = '|';

    for( x = 12; x < 18; x++ ) {
        *pch++ = ' ';
        *pch++ = anBoard[ 1 ][ x ] ? achX[ anBoard[ 1 ][ x ] ] :
                achO[ anBoard[ 0 ][ 23 - x ] ];
        *pch++ = ' ';
    }

    *pch++ = '|';
    *pch++ = ' ';
    *pch++ = achO[ anBoard[ 0 ][ 24 ] ];
    *pch++ = ' ';
    *pch++ = '|';
        
    for( ; x < 24; x++ ) {
        *pch++ = ' ';
        *pch++ = anBoard[ 1 ][ x ] ? achX[ anBoard[ 1 ][ x ] ] :
                achO[ anBoard[ 0 ][ 23 - x ] ];
        *pch++ = ' ';
    }

    *pch++ = '|';

    *pch++ = ' ';
        
    for( x = 0; x < 3; x++ )
        *pch++ = ( cOffO > 5 * x + 4 ) ? 'O' : ' ';

    *pch++ = '\n';
    
    *pch++ = fRoll ? 'v' : '^';
    strcpy( pch, _("|                  |BAR|                  |     ") );
    pch += 48;
    
    if( asz[ 3 ] )
        for( pchIn = asz[ 3 ]; *pchIn; pchIn++ )
            *pch++ = *pchIn;

    *pch++ = '\n';

    *pch++ = ' ';
    *pch++ = '|';

    for( x = 11; x > 5; x-- ) {
        *pch++ = ' ';
        *pch++ = anBoard[ 1 ][ x ] ? achX[ anBoard[ 1 ][ x ] ] :
                achO[ anBoard[ 0 ][ 23 - x ] ];
        *pch++ = ' ';
    }

    *pch++ = '|';
    *pch++ = ' ';
    *pch++ = achX[ anBoard[ 1 ][ 24 ] ];
    *pch++ = ' ';
    *pch++ = '|';
        
    for( ; x >= 0; x-- ) {
        *pch++ = ' ';
        *pch++ = anBoard[ 1 ][ x ] ? achX[ anBoard[ 1 ][ x ] ] :
                achO[ anBoard[ 0 ][ 23 - x ] ];
        *pch++ = ' ';
    }

    *pch++ = '|';

    *pch++ = ' ';
        
    for( x = 0; x < 3; x++ )
        *pch++ = ( cOffX > 5 * x + 4 ) ? 'X' : ' ';

    *pch++ = '\n';
    
    for( y = 3; y >= 0; y-- ) {
        *pch++ = ' ';
        *pch++ = '|';

        for( x = 11; x > 5; x-- ) {
            *pch++ = ' ';
            *pch++ = anBoard[ 1 ][ x ] > y ? 'X' :
                anBoard[ 0 ][ 23 - x ] > y ? 'O' : ' ';
            *pch++ = ' ';
        }

        *pch++ = '|';
        *pch++ = ' ';
        *pch++ = anBoard[ 1 ][ 24 ] > y ? 'X' : ' ';
        *pch++ = ' ';
        *pch++ = '|';
        
        for( ; x >= 0; x-- ) {
            *pch++ = ' ';
            *pch++ = anBoard[ 1 ][ x ] > y ? 'X' :
                anBoard[ 0 ][ 23 - x ] > y ? 'O' : ' ';
            *pch++ = ' ';
        }
        
        *pch++ = '|';
        *pch++ = ' ';

        for( x = 0; x < 3; x++ )
            *pch++ = ( cOffX > 5 * x + y ) ? 'X' : ' ';

        *pch++ = ' ';
        
        if( y < 2 && asz[ 5 - y ] )
            for( pchIn = asz[ 5 - y ]; *pchIn; pchIn++ )
                *pch++ = *pchIn;
        
#if USE_TIMECONTROL
	if ((y==3) && asz[8])
            for( pchIn = asz[ 8 ]; *pchIn; pchIn++ )
                *pch++ = *pchIn;
#endif
        *pch++ = '\n';
    }

    strcpy( pch, fRoll ? " +12-11-10--9--8--7-------6--5--4--3--2--1-+     " :
            " +13-14-15-16-17-18------19-20-21-22-23-24-+     " );
    pch += 49;

    if( asz[ 6 ] )
        for( pchIn = asz[ 6 ]; *pchIn; pchIn++ )
            *pch++ = *pchIn;
    
    *pch++ = '\n';
    *pch = 0;

    return sz;
}


/*
 *     GNU Backgammon  Position ID: 0123456789ABCD
 *                     Match ID   : 0123456789ABCD
 *     +24-23-22-21-20-19------18-17-16-15-14-13-+  O: gnubg (Cube: 2)     
 *  OO | O     O  O  O  O |   |                  |  0 points               
 *  OO |          O     O |   |                  |  Cube offered at 2      
 *   O |          O       |   |                  |                         
 *   O |                  |   |                  |                         
 *   O |                  |   |                  |                         
 *     |                  |BAR|                  |v Cube: 1 (7 point match)
 *   X |                  |   |                  |                         
 *   X |                  |   |                  |                         
 *   X |                  |   |                  |                         
 *   X | X  X  X  X       |   |                  |  Rolled 11              
 *  XX | X  X  X  X  X    |   |                  |  0 points               
 *     +-1--2--3--4--5--6-------7--8--9-10-11-12-+  X: Gary (Cube: 2)      
 *
 */

static char *DrawBoardCls( char *sz, int anBoard[ 2 ][ 25 ], int fRoll,
                           char *asz[], char *szMatchID,
                           int nChequers) {

    char *pch = sz, *pchIn;
    int x, y, an[ 2 ][ 25 ], cOffO = nChequers, cOffX = nChequers;
    static char achX[ 16 ] = "     X6789ABCDEF",
        achO[ 16 ] = "     O6789ABCDEF";

    for( x = 0; x < 25; x++ ) {
        cOffO -= anBoard[ 0 ][ x ];
        cOffX -= anBoard[ 1 ][ x ];
    }
    
    strcpy( pch, _("    GNU Backgammon  Position ID: ") );

    pch += 33;

    if( fRoll )
        strcpy( pch, PositionID( anBoard ) );
    else {
        for( x = 0; x < 25; x++ ) {
            an[ 0 ][ x ] = anBoard[ 1 ][ x ];
            an[ 1 ][ x ] = anBoard[ 0 ][ x ];
        }
        
        strcpy( pch, PositionID( an ) );
    }
    
    pch += 14;
    *pch++ = '\n';
            
    /* match id */

    if ( szMatchID && *szMatchID ) {
      sprintf ( pch, _("                    Match ID   : %s\n"), szMatchID );
      pch = strchr ( pch, 0 );
    }
            
    strcpy( pch, fRoll ? "    +24-23-22-21-20-19------18-17-16-15-14-13-+  " :
            "    +-1--2--3--4--5--6-------7--8--9-10-11-12-+  " );
    pch += 49;

    if( asz[ 0 ] )
        for( pchIn = asz[ 0 ]; *pchIn; pchIn++ )
            *pch++ = *pchIn;

    *pch++ = '\n';

    for( y = 0; y < 4; y++ ) {
    
        for( x = 2; x >= 0; x-- )
            *pch++ = ( cOffO > 5 * x + y ) ? 'O' : ' ';

        *pch++ = ' ';
        *pch++ = '|';

        for( x = 23 ; x > 17; x-- ) {
            *pch++ = ' ';
            *pch++ = anBoard[ 1 ][ x ] > y ? 'X' :
                anBoard[ 0 ][ 23 - x ] > y ? 'O' : ' ';
            *pch++ = ' ';
        }

        *pch++ = '|';
        *pch++ = ' ';
        *pch++ = anBoard[ 0 ][ 24 ] > y ? 'O' : ' ';
        *pch++ = ' ';
        *pch++ = '|';
        
        for( ; x > 11; x-- ) {
            *pch++ = ' ';
            *pch++ = anBoard[ 1 ][ x ] > y ? 'X' :
                anBoard[ 0 ][ 23 - x ] > y ? 'O' : ' ';
            *pch++ = ' ';
        }

        *pch++ = '|';
        *pch++ = ' ';
	*pch++ = ' ';
        if( y < 2 && asz[ y + 1 ] )
            for( pchIn = asz[ y + 1 ]; *pchIn; pchIn++ )
                *pch++ = *pchIn;

        *pch++ = '\n';
    }

    for( x = 2; x >= 0; x-- )
        *pch++ = ( cOffO > 5 * x + 4 ) ? 'O' : ' ';

    *pch++ = ' ';
    *pch++ = '|';

    for( x = 23; x > 17; x-- ) {
        *pch++ = ' ';
        *pch++ = anBoard[ 1 ][ x ] ? achX[ anBoard[ 1 ][ x ] ] :
                achO[ anBoard[ 0 ][ 23 - x ] ];
        *pch++ = ' ';
    }

    *pch++ = '|';
    *pch++ = ' ';
    *pch++ = achO[ anBoard[ 0 ][ 24 ] ];
    *pch++ = ' ';
    *pch++ = '|';
        
    for( ; x > 11; x-- ) {
        *pch++ = ' ';
        *pch++ = anBoard[ 1 ][ x ] ? achX[ anBoard[ 1 ][ x ] ] :
                achO[ anBoard[ 0 ][ 23 - x ] ];
        *pch++ = ' ';
    }

    *pch++ = '|';

    *pch++ = ' ';
    *pch++ = ' ';

    *pch++ = '\n';
    
    strcpy( pch, _("    |                  |BAR|                  |") );
    pch += 47;
    *pch++ = fRoll ? 'v' : '^';
    *pch++ = ' ';   
    
    if( asz[ 3 ] )
        for( pchIn = asz[ 3 ]; *pchIn; pchIn++ )
            *pch++ = *pchIn;

    *pch++ = '\n';

    for( x = 2; x >= 0; x-- )
        *pch++ = ( cOffX > 5 * x + 4 ) ? 'X' : ' ';

    *pch++ = ' ';
    *pch++ = '|';

    for( x = 0; x < 6; x++ ) {
        *pch++ = ' ';
        *pch++ = anBoard[ 1 ][ x ] ? achX[ anBoard[ 1 ][ x ] ] :
                achO[ anBoard[ 0 ][ 23 - x ] ];
        *pch++ = ' ';
    }

    *pch++ = '|';
    *pch++ = ' ';
    *pch++ = achX[ anBoard[ 1 ][ 24 ] ];
    *pch++ = ' ';
    *pch++ = '|';
        
    for( ; x < 12; x++ ) {
        *pch++ = ' ';
        *pch++ = anBoard[ 1 ][ x ] ? achX[ anBoard[ 1 ][ x ] ] :
                achO[ anBoard[ 0 ][ 23 - x ] ];
        *pch++ = ' ';
    }

    *pch++ = '|';

    *pch++ = ' ';
    *pch++ = ' ';

    *pch++ = '\n';
    
    for( y = 3; y >= 0; y-- ) {

        for( x = 2; x >= 0; x-- )
            *pch++ = ( cOffX > 5 * x + y ) ? 'X' : ' ';

        *pch++ = ' ';
        *pch++ = '|';

        for( x = 0; x < 6; x++ ) {
            *pch++ = ' ';
            *pch++ = anBoard[ 1 ][ x ] > y ? 'X' :
                anBoard[ 0 ][ 23 - x ] > y ? 'O' : ' ';
            *pch++ = ' ';
        }

        *pch++ = '|';
        *pch++ = ' ';
        *pch++ = anBoard[ 1 ][ 24 ] > y ? 'X' : ' ';
        *pch++ = ' ';
        *pch++ = '|';
        
        for( ; x < 12; x++ ) {
            *pch++ = ' ';
            *pch++ = anBoard[ 1 ][ x ] > y ? 'X' :
                anBoard[ 0 ][ 23 - x ] > y ? 'O' : ' ';
            *pch++ = ' ';
        }
        
        *pch++ = '|';
        *pch++ = ' ';
        *pch++ = ' ';       
        if( y < 2 && asz[ 5 - y ] )
            for( pchIn = asz[ 5 - y ]; *pchIn; pchIn++ )
                *pch++ = *pchIn;
        
        *pch++ = '\n';
    }

    strcpy( pch, fRoll ? "    +-1--2--3--4--5--6-------7--8--9-10-11-12-+   " :
            "    +24-23-22-21-20-19------18-17-16-15-14-13-+  " );
    pch += 49;

    if( asz[ 6 ] )
        for( pchIn = asz[ 6 ]; *pchIn; pchIn++ )
            *pch++ = *pchIn;
    
    *pch++ = '\n';
    *pch = 0;

    return sz;
}

extern char *DrawBoard( char *sz, int anBoard[ 2 ][ 25 ], int fRoll,
                        char *asz[], char *szMatchID, int nChequers ) {
    if( fClockwise == FALSE ) 
        return ( DrawBoardStd( sz, anBoard, fRoll, asz, 
                               szMatchID, nChequers  ) ) ;

    return ( DrawBoardCls( sz, anBoard, fRoll, asz, 
                           szMatchID, nChequers ) ) ;   
}

static char *FormatPoint( char *pch, int n ) {

    assert( n >= 0 );
    
    if( !n ) {
        strcpy( pch, N_("off") );
        return pch + strlen(N_("off") );
    } else if( n == 25 ) {
        strcpy( pch, N_("bar") );
        return pch + strlen(N_("bar") );
    } else if( n > 9 )
        *pch++ = n / 10 + '0';

    *pch++ = ( n % 10 ) + '0';

    return pch;
}

static char *FormatPointPlain( char *pch, int n ) {

    assert( n >= 0 );
    
    if( n > 9 )
        *pch++ = n / 10 + '0';

    *pch++ = ( n % 10 ) + '0';

    return pch;
}

extern char *FormatMovePlain( char *sz, int anBoard[ 2 ][ 25 ],
                              int anMove[ 8 ] ) {

    char *pch = sz;
    int i, j;
    
    for( i = 0; i < 8 && anMove[ i ] >= 0; i += 2 ) {
        pch = FormatPointPlain( pch, anMove[ i ] + 1 );
        *pch++ = '/';
        pch = FormatPointPlain( pch, anMove[ i + 1 ] + 1 );

        if( anBoard && anMove[ i + 1 ] >= 0 &&
            anBoard[ 0 ][ 23 - anMove[ i + 1 ] ] ) {
            for( j = 1; ; j += 2 )
                if( j > i ) {
                    *pch++ = '*';
                    break;
                } else if( anMove[ i + 1 ] == anMove[ j ] )
                    break;
        }
        
        if( i < 6 )
            *pch++ = ' '; 
    }

    *pch = 0;

    return sz;
}

static int CompareMoves( const void *p0, const void *p1 ) {

    int n0 = *( (int *) p0 ), n1 = *( (int *) p1 );

    if( n0 != n1 )
        return n1 - n0;
    else
        return *( (int *) p1 + 1 ) - *( (int *) p0 + 1 );
}

extern void CanonicalMoveOrder( int an[] ) {

    int i;

    for( i = 0; i < 4 && an[ 2 * i ] > -1; i++ )
	;
    
    qsort( an, i, sizeof( int ) << 1, CompareMoves );
}

extern char *FormatMove( char *sz, int anBoard[ 2 ][ 25 ], int anMove[ 8 ] ) {

    char *pch = sz;
    int aanMove[ 4 ][ 4 ], *pnSource[ 4 ], *pnDest[ 4 ], i, j;
    int fl = 0;
    int anCount[4], nMoves, nDuplicate, k;
  
    /* Re-order moves into 2-dimensional array. */
    for( i = 0; i < 4 && anMove[ i << 1 ] >= 0; i++ ) {
        aanMove[ i ][ 0 ] = anMove[ i << 1 ] + 1;
        aanMove[ i ][ 1 ] = anMove[ ( i << 1 ) | 1 ] + 1;
        pnSource[ i ] = aanMove[ i ];
        pnDest[ i ] = aanMove[ i ] + 1;
    }

    while( i < 4 ) {
        aanMove[ i ][ 0 ] = aanMove[ i ][ 1 ] = -1;
        pnSource[ i++ ] = NULL;
    }
    
    /* Order the moves in decreasing order of source point. */
    qsort( aanMove, 4, 4 * sizeof( int ), CompareMoves );

    /* Combine moves of a single chequer. */
    for( i = 0; i < 4; i++ )
        for( j = i; j < 4; j++ )
            if( pnSource[ i ] && pnSource[ j ] &&
                *pnDest[ i ] == *pnSource[ j ] ) {
                if( anBoard[ 0 ][ 24 - *pnDest[ i ] ] )
                    /* Hitting blot; record intermediate point. */
                    *++pnDest[ i ] = *pnDest[ j ];
                else
                    /* Non-hit; elide intermediate point. */
                    *pnDest[ i ] = *pnDest[ j ];

                pnSource[ j ] = NULL;           
            }

    /* Compact array. */
    i = 0;

    for( j = 0; j < 4; j++ )
        if( pnSource[ j ] ) {
            if( j > i ) {
                pnSource[ i ] = pnSource[ j ];
                pnDest[ i ] = pnDest[ j ];
            }

	    i++;
        }

    while( i < 4 )
        pnSource[ i++ ] = NULL;

    for ( i = 0; i < 4; i++)
        anCount[i] = pnSource[i] ? 1 : 0;

    for ( i = 0; i < 3; i++) {
        if (pnSource[i]) {
            nMoves = pnDest[i] - pnSource[i];
            for (j = i + 1; j < 4; j++) {
                if (pnSource[j]) {
                    nDuplicate = 1;
		    
                    if (pnDest[j] - pnSource[j] != nMoves)
                        nDuplicate = 0;
                    else
                        for (k = 0; k <= nMoves && nDuplicate; k++)
			    {
				if (pnSource[i][k] != pnSource[j][k])
				    nDuplicate = 0;
			    }
                    if (nDuplicate) {
                        anCount[i]++;
                        pnSource[j] = NULL;
                    }
                }
            }
        }
    }

    /* Compact array. */
    i = 0;

    for( j = 0; j < 4; j++ )
        if( pnSource[ j ] ) {
            if( j > i ) {
                pnSource[ i ] = pnSource[ j ];
                pnDest[ i ] = pnDest[ j ];
		anCount[ i ] = anCount[ j ];
            }

	    i++;
        }

    if( i < 4 )
        pnSource[ i ] = NULL;

    for( i = 0; i < 4 && pnSource[ i ]; i++ ) {
        if( i )
            *pch++ = ' ';
        
        pch = FormatPoint( pch, *pnSource[ i ] );

        for( j = 1; pnSource[ i ] + j < pnDest[ i ]; j++ ) {
            *pch = '/';
            pch = FormatPoint( pch + 1, pnSource[ i ][ j ] );
            *pch++ = '*';
            fl |= 1 << pnSource[ i ][ j ];
        }

        *pch = '/';
        pch = FormatPoint( pch + 1, *pnDest[ i ] );
        
        if( *pnDest[ i ] && anBoard[ 0 ][ 24 - *pnDest[ i ] ] &&
            !( fl & ( 1 << *pnDest[ i ] ) ) ) {
            *pch++ = '*';
            fl |= 1 << *pnDest[ i ];
        }
	
        if (anCount[i] > 1) {
            *pch++ = '(';
            *pch++ = '0' + anCount[i];
            *pch++ = ')';
        }
    }

    *pch = 0;
    
    return sz;
}

extern int ParseMove( char *pch, int an[ 8 ] ) {

    int i, j, iBegin, iEnd, n, c = 0, anUser[ 8 ];
    unsigned fl = 0;
    
    while( *pch ) {
        if( isspace( *pch ) ) {
            pch++;
            continue;
        } else if( isdigit( *pch ) ) {
            if( c == 8 ) {
                /* Too many points. */
                errno = EINVAL;
                return -1;
            }
            
            if( ( anUser[ c ] = strtol( pch, &pch, 10 ) ) < 0 ||
                anUser[ c ] > 25 ) {
                /* Invalid point number. */
                errno = EINVAL;
                return -1;
            }

            c++;
            continue;
        } else switch( *pch ) {
        case 'o':
        case 'O':
        case '-':
            if( c == 8 ) {
                /* Too many points. */
                errno = EINVAL;
                return -1;
            }
            
            anUser[ c++ ] = 0;

            if( *pch != '-' && ( pch[ 1 ] == 'f' || pch[ 1 ] == 'F' ) ) {
                pch++;
                if( pch[ 1 ] == 'f' || pch[ 1 ] == 'F' )
                    pch++;
            }
            break;
            
        case 'b':
        case 'B':
            if( c == 8 ) {
                /* Too many points. */
                errno = EINVAL;
                return -1;
            }
            
            anUser[ c++ ] = 25;

            if( pch[ 1 ] == 'a' || pch[ 1 ] == 'A' ) {
                pch++;
                if( pch[ 1 ] == 'r' || pch[ 1 ] == 'R' )
                    pch++;
            }
            break;
            
        case '/':
            if( !c || fl & ( 1 << c ) ) {
                /* Leading '/', or duplicate '/'s. */
                errno = EINVAL;
                return -1;
            }

            fl |= 1 << c;
            break;

        case '*':
	case ',':
	case ')':
            /* Currently ignored. */
            break;

        case '(':
	    if( ( n = strtol( pch + 1, &pch, 10 ) - 1 ) < 1 ) {
		/* invalid count */
		errno = EINVAL;
		return -1;
	    }
	    
	    if( c < 2 ) {
		/* incomplete move before ( -- syntax error */
		errno = EINVAL;
		return -1;
	    }
	    
	    if( fl & ( 1 << c ) ) {
		/* / immediately before ( -- syntax error */
		errno = EINVAL;
		return -1;
	    }

	    for( iBegin = c - 1; iBegin >= 0; iBegin-- )
		if( !( fl & ( 1 << iBegin ) ) )
		    break;
	    
	    if( iBegin < 0 ) {
		/* no / anywhere before ( -- syntax error */
		errno = EINVAL;
		return -1;
	    }

	    iEnd = c;

	    if( c + ( iEnd - iBegin ) * n > 8 ) {
		/* Too many moves. */
		errno = EINVAL;
		return -1;
	    }
	    
	    for( i = 0; i < n; i++ )
		for( j = iBegin; j < iEnd; j++ ) {
		    if( fl & ( 1 << j ) )
			fl |= 1 << c;
		    anUser[ c++ ] = anUser[ j ];
		}

	    break;
	    
        default:
            errno = EINVAL;
            return -1;
        }    
        pch++;
    }

    if( fl & ( 1 << c ) ) {
        /* Trailing '/'. */
        errno = EINVAL;
        return -1;
    }
    
    for( i = 0, j = 0; j < c; j++ ) {
        if( i == 8 ) {
            /* Too many moves. */
            errno = EINVAL;
            return -1;
        }

        if( ( ( i & 1 ) && anUser[ j ] == 25 ) ||
            ( !( i & 1 ) && !anUser[ j ] ) ) {
            /* Trying to move from off the board, or to the bar. */
            errno = EINVAL;
            return -1;
        }
        
        an[ i ] = anUser[ j ];

        if( ( i & 1 ) && ( fl & ( 1 << ( j + 1 ) ) ) ) {
            /* Combined move; this destination is also the next source. */
            if( i == 7 ) {
                /* Too many moves. */
                errno = EINVAL;
                return -1;
            }

            if( !an[ i ] || an[ i ] == 25 ) {
                errno = EINVAL;
                return -1;
            }
            
            an[ ++i ] = anUser[ j ];
        }
        
        i++;
    }

    if( i & 1 ) {
        /* Incomplete last move. */
        errno = EINVAL;
        return -1;
    }

    if( i < 8 )
      an[ i ] = -1;

    CanonicalMoveOrder( an );
    
    return i >> 1;
}

/*
 * Generate FIBS board
 *
 * (see <URL:http://www.fibs.com/fibs_interface.html#board_state>)
 *
 * gnubg extends the meaning of the "doubled" field, so the sign of
 * the field indicates who doubled. This is necessaty to correctly
 * detects beavers and raccoons.
 *
 */

extern char *FIBSBoard( char *pch, int anBoard[ 2 ][ 25 ], int fRoll,
			char *szPlayer, char *szOpp, int nMatchTo,
			int nScore, int nOpponent, int nDice0, int nDice1,
			int nCube, int fCubeOwner, int fDoubled, int fTurn,
			int fCrawford, int nChequers ) {
    char *sz = pch;
    int i, anOff[ 2 ];
    
    /* Names and match length/score */
    strcpy( sz, "board:" );
    
    for( sz += 6; *szPlayer; szPlayer++ )
	*sz++ = ( *szPlayer != ':' ? *szPlayer : '_' );
    
    for( *sz++ = ':'; *szOpp; szOpp++ )    
	*sz++ = ( *szOpp != ':' ? *szOpp : '_' );
    
    sprintf( sz, ":%d:%d:%d:", nMatchTo, nScore, nOpponent );

    /* Opponent on bar */
    sprintf( strchr( sz, 0 ), "%d:", -anBoard[ 0 ][ 24 ] );

    /* Board */
    for( i = 0; i < 24; i++ )
	sprintf( strchr( sz, 0 ), "%d:", anBoard[ 0 ][ 23 - i ] ?
		 -anBoard[ 0 ][ 23 - i ] : anBoard[ 1 ][ i ] );

    /* Player on bar */
    sprintf( strchr( sz, 0 ), "%d:", anBoard[ 1 ][ 24 ] );

    /* Whose turn */
    strcat( sz, fRoll ? "1:" : "-1:" );

    anOff[ 0 ] = anOff[ 1 ] = nChequers ? nChequers : 15;
    for( i = 0; i < 25; i++ ) {
	anOff[ 0 ] -= anBoard[ 0 ][ i ];
	anOff[ 1 ] -= anBoard[ 1 ][ i ];
    }

    sprintf( strchr( sz, 0 ), "%d:%d:%d:%d:%d:%d:%d:%d:1:-1:0:25:%d:%d:0:0:0:"
	     "0:%d:0", nDice0, nDice1, nDice0, nDice1, fTurn < 0 ? 1 : nCube,
	     fTurn < 0 || fCubeOwner != 0, fTurn < 0 || fCubeOwner != 1,
	     fDoubled ? ( fTurn ? -1 : 1 ) : 0, anOff[ 1 ], anOff[ 0 ],
	     fCrawford );

    return pch;
}

extern int ParseFIBSBoard( char *pch, int anBoard[ 2 ][ 25 ],
			   char *szPlayer, char *szOpp, int *pnMatchTo,
			   int *pnScore, int *pnScoreOpponent,
			   int anDice[ 2 ], int *pnCube, int *pfCubeOwner,
			   int *pfDoubled, int *pfTurn, int *pfCrawford ) {
    
    int i, c, n, fCanDouble, fOppCanDouble, anOppDice[ 2 ];

    for( i = 0; i < 25; i++ )
	anBoard[ 0 ][ i ] = anBoard[ 1 ][ i ] = 0;
    
    /* Names and match length/score */
    c = -1;
    sscanf( pch, "board:%31[^:]:%31[^:]:%d:%d:%d:%n", szPlayer, szOpp,
	    pnMatchTo, pnScore, pnScoreOpponent, &c );
    if( c < 0 )
	return -1;
    pch += c;

    /* Opponent on bar */
    c = -1;
    sscanf( pch, "%d:%n", &n, &c );
    if( c < 0 )
	return -1;
    pch += c;
    anBoard[ 0 ][ 24 ] = -n;

    /* Board */
    for( i = 0; i < 24; i++ ) {
	c = -1;
	sscanf( pch, "%d:%n", &n, &c );
	if( c < 0 )
	    return -1;
	pch += c;
	if( n < 0 )
	    anBoard[ 0 ][ 23 - i ] = -n;
	else
	    anBoard[ 1 ][ i ] = n;
    }

    /* Player on bar */
    c = -1;
    sscanf( pch, "%d:%n", &n, &c );
    if( c < 0 )
	return -1;
    pch += c;
    anBoard[ 1 ][ 24 ] = n;

    c = -1;
    sscanf( pch, "%d:%d:%d:%d:%d:%d:%d:%d:%d:%*d:%*d:%*d:%*d:%*d:%*d:%*d:%*d:"
	    "%*d:%*d:%d:%n", pfTurn, anDice, anDice + 1, anOppDice,
	    anOppDice + 1, pnCube, &fCanDouble, &fOppCanDouble,
	    pfDoubled, pfCrawford, &c );
    if( c < 0 )
	return -1;

    *pfTurn = *pfTurn > 0;

    if( !anDice[ 0 ] ) {
	anDice[ 0 ] = anOppDice[ 0 ];
	anDice[ 1 ] = anOppDice[ 1 ];
    }

    *pfCubeOwner = fCanDouble != fOppCanDouble ? fCanDouble : -1;

    return 0;
}
