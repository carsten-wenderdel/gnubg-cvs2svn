/*
 * rollout.c
 *
 * by Gary Wong <gary@cs.arizona.edu>, 1999, 2000, 2001.
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

#if HAVE_ALLOCA_H
#include <alloca.h>
#endif
#include <errno.h>
#include <isaac.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "backgammon.h"
#include "dice.h"
#include "eval.h"
#if USE_GTK
#include "gtkgame.h"
#endif
#include "matchid.h"
#include "positionid.h"
#include "rollout.h"
#include "i18n.h"


static void
initRolloutstat ( rolloutstat *prs );

/* Quasi-random permutation array: the first index is the "generation" of the
   permutation (0 permutes each set of 36 rolls, 1 permutes those sets of 36
   into 1296, etc.); the second is the roll within the game (limited to 128,
   so we use pseudo-random dice after that); the last is the permutation
   itself.  6 generations are enough for 36^6 > 2^31 trials. */
static unsigned char aaanPermutation[ 6 ][ 128 ][ 36 ];
static int nPermutationSeed = -1;

static void QuasiRandomSeed( int n ) {

    int i, j, k, r, t;
    randctx rc;
    
    if( nPermutationSeed == n )
      return;

    for( i = 0; i < RANDSIZ; i++ )
	rc.randrsl[ i ] = n;

    irandinit( &rc, TRUE );
    
    for( i = 0; i < 6; i++ )
	for( j = i /* no need for permutations below the diagonal */; j < 128;
	     j++ ) {
	    for( k = 0; k < 36; k++ )
		aaanPermutation[ i ][ j ][ k ] = k;
	    for( k = 0; k < 35; k++ ) {
		r = irand( &rc ) % ( 36 - k );
		t = aaanPermutation[ i ][ j ][ k + r ];
		aaanPermutation[ i ][ j ][ k + r ] =
		    aaanPermutation[ i ][ j ][ k ];
		aaanPermutation[ i ][ j ][ k ] = t;
	    }
	}

    nPermutationSeed = n;
}

static int nSkip;

static int RolloutDice( int iTurn, int iGame, int cGames,
                            int fInitial,
                            int anDice[ 2 ],
                            const rng rngx,
                            const int fRotate ) {

  if ( fInitial && !iTurn ) {
      /* rollout of initial position: no doubles allowed */
      if( fRotate ) {
	  int j;
      
	  if( !iGame )
	      nSkip = 0;

	  for( ; ; nSkip++ ) {
	      j = aaanPermutation[ 0 ][ 0 ][ ( iGame + nSkip ) % 36 ];
	      
	      anDice[ 0 ] = j / 6 + 1;
	      anDice[ 1 ] = j % 6 + 1;

	      if( anDice[ 0 ] != anDice[ 1 ] )
		  break;
	  }
	  
	  return 0;
      } else {
	  int n;
     
      reroll:
	  if( ( n = RollDice( anDice, rngx ) ) )
	      return n;
	  
	  if ( fInitial && ! iTurn && anDice[ 0 ] == anDice[ 1 ] )
	      goto reroll;

	  return 0;
      }
  } else if( fRotate && iTurn < 128 ) {
      int i, /* the "generation" of the permutation */
	  j, /* the number we're permuting */
	  k; /* 36**i */
      
      for( i = 0, j = 0, k = 1; i < 6 && i <= iTurn; i++, k *= 36 )
	  j = aaanPermutation[ i ][ iTurn ][ ( iGame / k + j ) % 36 ];
      
      anDice[ 0 ] = j / 6 + 1;
      anDice[ 1 ] = j % 6 + 1;

      return 0;
  } else 
      return RollDice( anDice, rngx );
}

/* Upon return, anBoard contains the board position after making the best
   play considering BOTH players' positions.  The evaluation of the play
   considering ONLY the position of the player in roll is stored in ar.
   The move used for anBoard and ar will usually (but not always) be the
   same.  Returning both values is necessary for performing variance
   reduction. */
#if 0
static int 
FindBestBearoff( int anBoard[ 2 ][ 25 ], int nDice0, int nDice1,
                 float ar[ NUM_OUTPUTS ], const bgvariation bgv ) {

  int i, j, anBoardTemp[ 2 ][ 25 ], iMinRolls = 0;
  unsigned long cBestRolls;
  movelist ml;

  GenerateMoves( &ml, anBoard, nDice0, nDice1, FALSE );
    
  ml.rBestScore = -99999.9;
  cBestRolls = -1;
    
  for( i = 0; i < ml.cMoves; i++ ) {
    PositionFromKey( anBoardTemp, ml.amMoves[ i ].auch );

    SwapSides( anBoardTemp );

    if( ( j = EvalBearoff1Full( anBoardTemp, ar ) ) < cBestRolls ) {
	    iMinRolls = i;
	    cBestRolls = j;
    }
	
    if( ( ml.amMoves[ i ].rScore = -Utility( ar, &ciCubeless ) ) >
        ml.rBestScore ) {
	    ml.iMoveBest = i;
	    ml.rBestScore = ml.amMoves[ i ].rScore;
    }
  }

  PositionFromKey( anBoard, ml.amMoves[ ml.iMoveBest ].auch );

  PositionFromKey( anBoardTemp, ml.amMoves[ iMinRolls ].auch );

  SwapSides( anBoardTemp );
    
  EvalBearoff1( anBoardTemp, ar, bgv );
    
  return 0;
}

/* Rollouts of bearoff positions, with race database variance reduction and no
   lookahead. */
static int 
BearoffRollout( int anBoard[ 2 ][ 25 ], float arOutput[],
                int nTruncate, int iTurn, int iGame, int cGames,
                const bgvariation bgv ) {

  int anDice[ 2 ], i;
  float ar[ NUM_OUTPUTS ], arMean[ NUM_OUTPUTS ], arMin[ NUM_OUTPUTS ];

  for( i = 0; i < NUM_OUTPUTS; i++ )
    arOutput[ i ] = 0.0f;

  while( ( !nTruncate || iTurn < nTruncate ) &&
	 ClassifyPosition( anBoard, bgv ) > CLASS_PERFECT ) {
    if( RolloutDice( iTurn, iGame, cGames, FALSE, 
                         anDice, RNG_MERSENNE, TRUE ) < 0 )
	    return -1;
	
    if( anDice[ 0 ]-- < anDice[ 1 ]-- )
	    swap( anDice, anDice + 1 );

    if( EvaluatePosition( anBoard, arMean, &ciCubeless, NULL ) < 0 )
	    return -1;
	
    if( iTurn & 1 )
	    InvertEvaluation( arMean );
	    
    FindBestBearoff( anBoard, anDice[ 0 ] + 1, anDice[ 1 ] + 1, arMin, bgv );

    if( !( iTurn & 1 ) )
	    InvertEvaluation( arMin );
	
    SwapSides( anBoard );

    for( i = 0; i < NUM_OUTPUTS; i++ )
	    arOutput[ i ] += arMean[ i ] - arMin[ i ];

    if( fAction )
	fnAction();
    
    if( fInterrupt )
	return -1;
	
    iTurn++;
  }

  if( EvaluatePosition( anBoard, ar, &ciCubeless, NULL ) )
    return -1;

  if( iTurn & 1 )
    InvertEvaluation( ar );

  for( i = 0; i < NUM_OUTPUTS; i++ )
    arOutput[ i ] += ar[ i ];

  return 0;
}
#endif

static void
ClosedBoard ( int afClosedBoard[ 2 ], int anBoard[ 2 ][ 25 ] ) {

  int i, j, n;

  for ( i = 0; i < 2; i++ ) {

    n = 0;
    for( j = 0; j < 6; j++ )
      n += anBoard[ i ][ j ] > 1;

    afClosedBoard[ i ] = ( n == 6 );

  }

}

/* called with 
               cube decision                  move rollout
aanBoard       2 copies of same board         1 board
aarOutput      2 arrays for eval              1 array
iTurn          player on roll                 same
iGame          game number                    same
cubeinfo       2 structs for double/nodouble  1 cubeinfo
               or take/pass
CubeDecTop     array of 2 boolean             1 boolean
               (TRUE if a cube decision is valid on turn 0)
cci            2 (number of rollouts to do)   1
prc            1 rollout context              same
aarsStatistics 2 arrays of stats

returns -1 on error/interrupt, fInterrupt TRUE if stopped by user
aarOutput array(s) contain results
*/

static int
BasicCubefulRollout ( int aanBoard[][ 2 ][ 25 ],
                      float aarOutput[][ NUM_ROLLOUT_OUTPUTS ], 
                      int iTurn, int iGame,
                      cubeinfo aci[], int afCubeDecTop[], int cci,
                      rolloutcontext *prc,
                      rolloutstat aarsStatistics[][ 2 ] ) {

  int anDice [ 2 ], cUnfinished = cci;
  cubeinfo *pci;
  cubedecision cd;
  int *pf, ici, i, j, k;
  evalcontext ec;

  positionclass pc, pcBefore;
  int nPipsBefore = 0, nPipsAfter, nPipsDice;
  int anPips [ 2 ];
  int afClosedBoard[ 2 ];

  float arDouble[ NUM_CUBEFUL_OUTPUTS ];
  float aar[ 2 ][ NUM_ROLLOUT_OUTPUTS ];

  int aiBar[ 2 ];

  int afClosedOut[ 2 ] = { FALSE, FALSE };
  int afHit[ 2 ] = { FALSE, FALSE };

  float rDP;
  float r;
  
  int nTruncate = prc->fDoTruncate ? prc->nTruncate: 0x7fffffff;
  int cGames = prc->nTrials;

  int nLateEvals = prc->fLateEvals ? prc->nLate : 0x7fffffff;

  /* Make local copy of cubeinfo struct, since it
     may be modified */
#if __GNUC__
  cubeinfo pciLocal[ cci ];
  int pfFinished[ cci ];
  float aarVarRedn[ cci ][ NUM_ROLLOUT_OUTPUTS ];
#elif HAVE_ALLOCA
  cubeinfo *pciLocal = alloca( cci * sizeof ( cubeinfo ) );
  int *pfFinished = alloca( cci * sizeof( int ) );
  float (*aarVarRedn)[ NUM_ROLLOUT_OUTPUTS ] =
    alloca ( cci * NUM_ROLLOUT_OUTPUTS * sizeof ( float ) );;
#else
  cubeinfo pciLocal[ MAX_ROLLOUT_CUBEINFO ];
  int pfFinished[ MAX_ROLLOUT_CUBEINFO ];
  float aarVarRedn[ MAX_ROLLOUT_CUBEINFO ][ NUM_ROLLOUT_OUTPUTS ];
#endif

  /* variables for variance reduction */

  evalcontext aecVarRedn[ 2 ];
  float arMean[ NUM_ROLLOUT_OUTPUTS ];
  int aaanBoard[ 6 ][ 6 ][ 2 ][ 25 ];
  float aaar[ 6 ][ 6 ][ NUM_ROLLOUT_OUTPUTS ];

  evalcontext ecCubeless0ply = { FALSE, 0, 0, TRUE, 0.0 };
  evalcontext ecCubeful0ply = { TRUE, 0, 0, TRUE, 0.0 };

  /* local pointers to the eval contexts to use */
  evalcontext *pecCube[2], *pecChequer[2];

  if ( prc->fVarRedn ) {

    /*
     * Create evaluation context one ply deep
     */

    for ( ici = 0; ici < cci; ici++ )
      for ( i = 0; i < NUM_ROLLOUT_OUTPUTS; i++ )
        aarVarRedn[ ici ][ i ] = 0.0f;
    
    for ( i = 0; i < 2; i++ ) {
      
      memcpy ( &aecVarRedn[ i ], &prc->aecChequer[ i ],
               sizeof ( evalcontext ) );

      if ( prc->fCubeful )
        /* other no var. redn. on cubeful equities */
        aecVarRedn[ i ].fCubeful = TRUE;

      if ( aecVarRedn[ i ].nPlies ) {
        aecVarRedn[ i ].nPlies--;
      }

    }

  }

  for ( ici = 0; ici < cci; ici++ )
    pfFinished[ ici ] = TRUE;

  memcpy ( pciLocal, aci, cci * sizeof (cubeinfo) );
  
  while ( ( !nTruncate || iTurn < nTruncate ) && cUnfinished ) {
	if (iTurn < nLateEvals) {
	  pecCube[0] = prc->aecCube;
	  pecCube[1] = prc->aecCube + 1;
	  pecChequer[0] = prc->aecChequer;
	  pecChequer[1] = prc->aecChequer + 1;
	} else {
	  pecCube[0] = prc->aecCubeLate;
	  pecCube[1] = prc->aecCubeLate + 1;
	  pecChequer[0] = prc->aecChequerLate;
	  pecChequer[1] = prc->aecChequerLate + 1;
	}

    /* Cube decision */

    for ( ici = 0, pci = pciLocal, pf = pfFinished;
          ici < cci; ici++, pci++, pf++ ) {

      /* check for truncation at bearoff databases */

      pc = ClassifyPosition ( aanBoard[ ici ], pci->bgv );

      if ( prc->fTruncBearoff2 && pc <= CLASS_PERFECT &&
           prc->fCubeful && *pf && ! pci->nMatchTo &&
           ( ( afCubeDecTop[ ici ] && ! prc->fInitial ) || iTurn > 0 ) ) {

        /* truncate at two sided bearoff if money game */

        GeneralEvaluationE( aarOutput[ ici ], aanBoard[ ici ],
                            pci, &ecCubeful0ply );

        if ( iTurn & 1 ) InvertEvaluationR ( aarOutput[ ici ], pci );

        *pf = FALSE;
        cUnfinished--;

      }
      else if ( ( ( prc->fTruncBearoff2 && pc <= CLASS_PERFECT ) ||
                  ( prc->fTruncBearoffOS && pc <= CLASS_BEAROFF_OS ) ) &&
                ! prc->fCubeful && *pf ) {
          
        /* cubeless rollout, requested to truncate at bearoff db */

        GeneralEvaluationE ( aarOutput[ ici ],
                             aanBoard[ ici ],
                             pci, &ecCubeless0ply );

	/* rollout result is for player on play (even iTurn).
	   This point is pre play, so if opponent is on roll, invert */
	
        if ( iTurn & 1 ) InvertEvaluationR ( aarOutput[ ici ], pci );

        *pf = FALSE;
        cUnfinished--;

      }
      
      if ( *pf ) {

        if ( prc->fCubeful && GetDPEq ( NULL, &rDP, pci ) &&
             ( iTurn > 0 || ( afCubeDecTop[ ici ] && ! prc->fInitial ) ) ) {

          if ( GeneralCubeDecisionE ( aar, aanBoard[ ici ],
                                      pci,
                                      pecCube[ pci->fMove ], 0 ) < 0 ) 
            return -1;

          cd = FindCubeDecision ( arDouble, aar, pci );

          switch ( cd ) {

          case DOUBLE_TAKE:
          case DOUBLE_BEAVER:
          case REDOUBLE_TAKE:

            /* update statistics */
	    if( aarsStatistics )
		aarsStatistics[ ici ]
		    [ pci->fMove ].acDoubleTake[ LogCube ( pci->nCube ) ]++; 

            SetCubeInfo ( pci, 2 * pci->nCube, ! pci->fMove, pci->fMove,
			  pci->nMatchTo,
                          pci->anScore, pci->fCrawford, pci->fJacoby,
                          pci->fBeavers, pci->bgv );

            break;
        
          case DOUBLE_PASS:
          case REDOUBLE_PASS:
            *pf = FALSE;
	    cUnfinished--;
	    
            /* assign outputs */

            for ( i = 0; i <= OUTPUT_EQUITY; i++ )
              aarOutput[ ici ][ i ] = aar[ 0 ][ i ];

            /* 
             * assign equity for double, pass:
             * - mwc for match play
             * - normalized equity for money play (i.e, rDP=1)
             */

            aarOutput[ ici ][ OUTPUT_CUBEFUL_EQUITY ] = rDP;

            /* invert evaluations if required */

            if ( iTurn & 1 ) InvertEvaluationR ( aarOutput[ ici ], pci );

            /* update statistics */

	    if( aarsStatistics )
		aarsStatistics[ ici ]
		    [ pci->fMove ].acDoubleDrop[ LogCube ( pci->nCube ) ]++; 

            break;

          case NODOUBLE_TAKE:
          case TOOGOOD_TAKE:
          case TOOGOOD_PASS:
          case NODOUBLE_BEAVER:
          case NO_REDOUBLE_TAKE:
          case TOOGOODRE_TAKE:
          case TOOGOODRE_PASS:
          case NO_REDOUBLE_BEAVER:
          case OPTIONAL_DOUBLE_BEAVER:
          case OPTIONAL_DOUBLE_TAKE:
          case OPTIONAL_REDOUBLE_TAKE:
          case OPTIONAL_DOUBLE_PASS:
          case OPTIONAL_REDOUBLE_PASS:
          default:

            /* no op */
            break;
            
          }
        } /* cube */
      }
    } /* loop over ci */

    /* Chequer play */

    if( RolloutDice( iTurn, iGame, cGames, prc->fInitial, anDice,
                         prc->rngRollout, prc->fRotate ) < 0 )
      return -1;

    if( anDice[ 0 ] < anDice[ 1 ] )
	    swap( anDice, anDice + 1 );


    for ( ici = 0, pci = pciLocal, pf = pfFinished;
          ici < cci; ici++, pci++, pf++ ) {

      if ( *pf ) {

        /* Save number of chequers on bar */

        for ( i = 0; i < 2; i++ )
          aiBar[ i ] = aanBoard[ ici ][ i ][ 24 ];

	/* Save number of pips (for bearoff only) */

	pcBefore = ClassifyPosition ( aanBoard[ ici ], pci->bgv );
	if ( aarsStatistics && pcBefore <= CLASS_BEAROFF1 ) {
	  PipCount ( aanBoard[ ici ], anPips );
	  nPipsBefore = anPips[ 1 ];
	}

        /* Find best move :-) */

        if ( prc->fVarRedn ) {

          /* Variance reduction */

          for ( i = 0; i < NUM_ROLLOUT_OUTPUTS; i++ )
            arMean[ i ] = 0.0f;

          for ( i = 0; i < 6; i++ )
            for ( j = 0; j <= i; j++ ) {

              if ( prc->fInitial && ! iTurn && j == i )
                /* no doubles possible for first roll when rolling
                   out as initial position */
                continue;

              memcpy ( &aaanBoard[ i ][ j ][ 0 ][ 0 ], 
                       &aanBoard[ ici ][ 0 ][ 0 ],
                       2 * 25 * sizeof ( int )  );

              /* Find the best move for each roll on ply 0 only */

              if ( FindBestMove ( NULL, i + 1, j + 1, aaanBoard[ i ][ j ],
                                  pci, NULL, defaultFilters ) < 0 )
                return -1;

              SwapSides ( aaanBoard[ i ][ j ] );

              /* re-evaluate the chosen move at ply n-1 */

              pci->fMove = ! pci->fMove;
              GeneralEvaluationE ( aaar[ i ][ j ],
                                   aaanBoard[ i ][ j ],
                                   pci, &aecVarRedn[ pci->fMove ] );
              pci->fMove = ! pci->fMove;

              if ( ! ( iTurn & 1 ) ) InvertEvaluationR ( aaar[ i ][ j ], pci );

              /* Calculate arMean: the n-ply evaluation of the position */

              for ( k = 0; k < NUM_ROLLOUT_OUTPUTS; k++ )
                arMean[ k ] += ( ( i == j ) ? aaar[ i ][ j ][ k ] :
                                 ( aaar[ i ][ j ][ k ] * 2.0 ) );

            }

          if ( prc->fInitial && ! iTurn && j == i )
            /* no doubles ... */
            for ( i = 0; i < NUM_ROLLOUT_OUTPUTS; i++ )
              arMean[ i ] /= 30.0f;
          else
            for ( i = 0; i < NUM_ROLLOUT_OUTPUTS; i++ )
              arMean[ i ] /= 36.0f;

          /* Find best move */

          if ( pecChequer[ pci->fMove ]->nPlies ||
               prc->fCubeful != pecChequer[ pci->fMove ]->fCubeful )

            /* the user requested n-ply (n>0). Another call to
               FindBestMove is required */

            FindBestMove( NULL, anDice[ 0 ], anDice[ 1 ],
                          aanBoard[ ici ], pci,
                          pecChequer [ pci->fMove ],
                          ( iTurn < nLateEvals ) ? 
                          prc->aaamfChequer[ pci->fMove ] : 
                          prc->aaamfLate[ pci->fMove ] );

          else {

            /* 0-ply play: best move is already recorded */

            memcpy ( &aanBoard[ ici ][ 0 ][ 0 ], 
                     &aaanBoard[ anDice[ 0 ] - 1 ][ anDice[ 1 ] - 1 ][0][0],
                     2 * 25 * sizeof ( int ) );

            SwapSides ( aanBoard[ ici ] );

          }


          /* Accumulate variance reduction terms */

          if ( pci->nMatchTo )
            for ( i = 0; i < NUM_ROLLOUT_OUTPUTS; i++ ) 
              aarVarRedn[ ici ][ i ] += arMean[ i ] -
                aaar[ anDice[ 0 ] - 1 ][ anDice[ 1 ] - 1 ][ i ];
          else {
            for ( i = 0; i <= OUTPUT_EQUITY; i++ ) 
              aarVarRedn[ ici ][ i ] += arMean[ i ] -
                aaar[ anDice[ 0 ] - 1 ][ anDice[ 1 ] - 1 ][ i ];

            r = arMean[ OUTPUT_CUBEFUL_EQUITY ] -
              aaar[ anDice[ 0 ] - 1 ][ anDice[ 1 ] - 1 ]
              [ OUTPUT_CUBEFUL_EQUITY ];
            aarVarRedn[ ici ][ OUTPUT_CUBEFUL_EQUITY ] += 
              r * pci->nCube / aci[ ici ].nCube;
          }

        }
        else {

          /* no variance reduction */
              
          FindBestMove( NULL, anDice[ 0 ], anDice[ 1 ],
                        aanBoard[ ici ], pci,
                        pecChequer [ pci->fMove ],
                        ( iTurn < nLateEvals ) ? 
                        prc->aaamfChequer[ pci->fMove ] : 
                        prc->aaamfLate[ pci->fMove ] );

        }

        /* Save hit statistics */

        /* FIXME: record double hit, triple hits etc. ? */

	if( aarsStatistics && ! afHit [ pci->fMove ] && 
            ( aiBar[ 0 ] < aanBoard[ ici ][ 0 ][ 24 ]  ) ) {

          aarsStatistics[ ici ][ pci->fMove ].nOpponentHit++;
          aarsStatistics[ ici ][ pci->fMove ].rOpponentHitMove += iTurn;
          afHit[ pci->fMove ] = TRUE;

        }

        if( fAction )
          fnAction();
    
        if( fInterrupt )
          return -1;

	/* Calculate number of wasted pips */

	pc = ClassifyPosition ( aanBoard[ ici ], pci->bgv );

	if ( aarsStatistics &&
	     pc <= CLASS_BEAROFF1 && pcBefore <= CLASS_BEAROFF1 ) {

	  PipCount ( aanBoard[ ici ], anPips );
	  nPipsAfter = anPips[ 1 ];
	  nPipsDice = anDice[ 0 ] + anDice[ 1 ];
	  if ( anDice[ 0 ] == anDice[ 1 ] ) nPipsDice *= 2;

	  aarsStatistics[ ici ][ pci->fMove ].nBearoffMoves++;
	  aarsStatistics[ ici ][ pci->fMove ].nBearoffPipsLost +=
	    nPipsDice - ( nPipsBefore - nPipsAfter );

	}

	/* Opponent closed out */

	if ( aarsStatistics && ! afClosedOut[ pci->fMove ] 
	     && aanBoard[ ici ][ 0 ][ 24 ] ) {

	  /* opponent is on bar */

	  ClosedBoard ( afClosedBoard, aanBoard[ ici ] );

	  if ( afClosedBoard[ pci->fMove ] ) {
	    aarsStatistics[ ici ][ pci->fMove ].nOpponentClosedOut++;
	    aarsStatistics[ ici ]
	      [ pci->fMove ].rOpponentClosedOutMove += iTurn;
	    afClosedOut[ pci->fMove ] = TRUE;
	  }

	}


        /* check if game is over */

        if ( pc == CLASS_OVER ) {
          GeneralEvaluationE ( aarOutput[ ici ],
                               aanBoard[ ici ],
                               pci, pecCube[ pci->fMove ] );

          /* Since the game is over: cubeless equity = cubeful equity
             (convert to mwc for match play) */

          aarOutput[ ici ][ OUTPUT_CUBEFUL_EQUITY ] =
            ( pci->nMatchTo ) ?
            eq2mwc ( aarOutput[ ici ][ OUTPUT_EQUITY ], pci ) :
            aarOutput[ ici ][ OUTPUT_EQUITY ];

          if ( iTurn & 1 ) InvertEvaluationR ( aarOutput[ ici ], pci );

          *pf = FALSE;
          cUnfinished--;

          /* update statistics */

	  if( aarsStatistics )
	      switch ( GameStatus ( aanBoard[ ici ], pci->bgv ) ) {
	      case 1:
		  aarsStatistics[ ici ][ pci->fMove ].
		      acWin[ LogCube ( pci->nCube )]++;
		  break;
	      case 2:
		  aarsStatistics[ ici ][ pci->fMove ].
		      acWinGammon[ LogCube ( pci->nCube )]++;
		  break;
	      case 3:
		  aarsStatistics[ ici ][ pci->fMove ].
		      acWinBackgammon[ LogCube ( pci->nCube )]++;
		  break;
	      }

        }

        /* Invert board and more */
	
        SwapSides( aanBoard[ ici ] );

        SetCubeInfo ( pci, pci->nCube, pci->fCubeOwner,
                      ! pci->fMove, pci->nMatchTo,
                      pci->anScore, pci->fCrawford,
                      pci->fJacoby, pci->fBeavers, pci->bgv );

        assert ( cUnfinished >= 0 );


      }
    }

    iTurn++;

  } /* loop truncate */


  /* evaluation at truncation */

  for ( ici = 0, pci = pciLocal, pf = pfFinished;
        ici < cci; ici++, pci++, pf++ ) {

    if ( *pf ) {

      /* ensure cubeful evaluation at truncation */

      memcpy ( &ec, &prc->aecCubeTrunc, sizeof ( ec ) );
      ec.fCubeful = prc->fCubeful;

      /* evaluation at truncation */

      GeneralEvaluationE ( aarOutput[ ici ], aanBoard[ ici ], pci, &ec );

      if ( iTurn & 1 ) InvertEvaluationR ( aarOutput[ ici ], pci );
          
    }

    /* the final output is the sum of the resulting evaluation and
       all variance reduction terms */

    if ( ! pci->nMatchTo ) 
      aarOutput[ ici ][ OUTPUT_CUBEFUL_EQUITY ] *=
        pci->nCube / aci [ ici ].nCube;
    
    if ( prc->fVarRedn )
      for ( i = 0; i < NUM_ROLLOUT_OUTPUTS; i++ )
        aarOutput[ ici ][ i ] += aarVarRedn[ ici ][ i ];

    /* multiply money equities */

    if ( ! pci->nMatchTo )
      aarOutput[ ici ][ OUTPUT_CUBEFUL_EQUITY ] *=
        aci [ ici ].nCube / aci[ 0 ].nCube;

    
    
/*        if ( pci->nMatchTo ) */
/*          aarOutput[ ici ][ OUTPUT_CUBEFUL_EQUITY ] = */
/*            eq2mwc ( aarOutput[ ici ][ OUTPUT_CUBEFUL_EQUITY ], pci ); */
/*        else */
/*          aarOutput[ ici ][ OUTPUT_CUBEFUL_EQUITY ] *= */
/*            pci->nCube / aci [ ici ].nCube; */
      
    }

    return 0;
}


static int
isHyperGammon( const bgvariation bgv ) {

  if ( bgv == VARIATION_HYPERGAMMON_1 )
    return 1;
  if ( bgv == VARIATION_HYPERGAMMON_2 )
    return 1;
  if ( bgv == VARIATION_HYPERGAMMON_3 )
    return 1;

  return 0;

}

/* called with a collection of moves or a cube decision to be rolled out.
   when called with a cube decision, the number of alternatives is always 2
   (nodouble/double or take/drop). Otherwise the number of moves is
   a parameter supplied (alternatives)

anBoard - an array[alternatives] of explicit pointers to Boards - the
    individual boards are not in and of themselves a contiguous set of
    arrays and can't be treated as int x[alternative][2][25]. 2 copies
    of the same board for cube decisions, 1 per move for move rollouts
asz an array of pointers to strings. These will be a contiguous array of
    text labels for displaying results. 2 pointers for cube decisions, 
    1 per move for move rollouts
aarOutput - an array[alternatives] of explicit pointers to arrays for the
    results of the rollout. Again, these may not be contiguous. 2 arrays for 
    cube decisions, 1 per move for move rollouts
aarStdDev - as above for std's of rollout
aarsStatistics - as above for statistics structs
pprc - an array of explicit pointers to rollout contexts. There will be
    2 pointers to the same context for cube decisions, 1 per move for move 
    rollouts 
aci  - an array of explicit pointers cubeinfo's. 2 for cube decisions, one
    per move for move rollouts
alternatives - a count of the number of things to be rolled out. 2 for 
    cube decisions, number of different moves for move rollouts
fInvert - flag if equities should be inverted (used when doing take/drop
    decisions, we evaluate the double/nodouble and invert the equities
    to get take/drop

returns:
-1 on error or if no games were rolled out
no of games rolled out otherwise. aarOutput, aarStdDev aarsStatistic arrays
   will contain results.
pprc rollout contexts will be updated with the number of games rolled out for 
that position.
*/

extern int
RolloutGeneral( int (* apBoard[])[ 2 ][ 25 ], char asz[][ 40 ],
                float (* apOutput[])[ NUM_ROLLOUT_OUTPUTS ],
                float (* apStdDev[])[ NUM_ROLLOUT_OUTPUTS ],
                rolloutstat (* apStatistics[])[2],
                evalsetup (* apes[]),
                cubeinfo (* apci[]), 
                int (* apCubeDecTop[]), int alternatives, 
		int fInvert) {
  
#if HAVE_ALLOCA
  int (* aanBoardEval )[ 2 ][ 25 ] = 
    alloca( alternatives * 50 * sizeof( int ) );
  float (* aar )[ NUM_ROLLOUT_OUTPUTS ] = alloca( alternatives * 
						  NUM_ROLLOUT_OUTPUTS *
						  sizeof ( float ) );
  float (* aarMu )[ NUM_ROLLOUT_OUTPUTS ] = alloca( alternatives * 
						    NUM_ROLLOUT_OUTPUTS *
						    sizeof ( float ) );
  float (* aarSigma )[ NUM_ROLLOUT_OUTPUTS ] = alloca( alternatives *
						       NUM_ROLLOUT_OUTPUTS *
						       sizeof ( float ) );
  double (* aarResult )[ NUM_ROLLOUT_OUTPUTS ] = alloca( alternatives *
							 NUM_ROLLOUT_OUTPUTS *
							 sizeof ( double ) );
  double (* aarVariance )[ NUM_ROLLOUT_OUTPUTS ] = alloca( alternatives * 
						   NUM_ROLLOUT_OUTPUTS * 
							   sizeof ( double ) );
  cubeinfo *aciLocal = alloca ( alternatives * sizeof ( cubeinfo ) );
#else
  int aanBoardEval[ MAX_ROLLOUT_CUBEINFO ][ 2 ][ 25 ];
  float aar[ MAX_ROLLOUT_CUBEINFO ][ NUM_ROLLOUT_OUTPUTS ];
  float aarMu[ MAX_ROLLOUT_CUBEINFO ][ NUM_ROLLOUT_OUTPUTS ];
  float aarSigma[ MAX_ROLLOUT_CUBEINFO ][ NUM_ROLLOUT_OUTPUTS ];
  double aarResult[ MAX_ROLLOUT_CUBEINFO ][ NUM_ROLLOUT_OUTPUTS ];
  double aarVariance[ MAX_ROLLOUT_CUBEINFO ][ NUM_ROLLOUT_OUTPUTS ];
  cubeinfo aciLocal[ MAX_ROLLOUT_CUBEINFO ];
#endif
  
  int i, j, alt;
  int anBoardOrig[ 2 ][ 25 ];
  int		ii;
  int     err_too_big;
  double	v, s;
  unsigned int nFirstTrial;
  evalsetup *pes;
  rolloutcontext *prc;
  rolloutstat *prs;
  int	cGames;

#if !(__GNUC__ || HAVE_ALLOCA)
  if (alternatives > MAX_ROLLOUT_CUBEINFO) {
    errno = EINVAL;
    return -1;
  }
#endif

  if( alternatives < 1 ) {
    errno = EINVAL;
    return -1;
  }

  /* initialise internal variables and figure out what the first 
     trial will be */

  cGames = rcRollout.nTrials;
  nFirstTrial = ~0 ;

  for ( i = 0; i < alternatives; ++i) {
    pes = apes[ i ];
    prc = &pes->rc;

    /* save input cubeinfo */
    memcpy ( &aciLocal[ i ], apci [ i ], sizeof ( cubeinfo ) );

    /* Invert cubeinfo */

    if ( fInvert )
      aciLocal[ i ].fMove = ! aciLocal[ i ].fMove;

    if ((pes->et != EVAL_ROLLOUT) || (prc->nGamesDone == 0)) {

      memcpy (prc, &rcRollout, sizeof (rolloutcontext));
      prc->nGamesDone = 0;
      prc->nSkip = 0;
      nFirstTrial = 0;
      pes->et = EVAL_ROLLOUT;

      if ((prs = &(* apStatistics[ i ])[0] ) != 0)
	initRolloutstat (prs);

      /* initialise internal variables */
      for (j = 0; j < NUM_ROLLOUT_OUTPUTS; ++j) {
	aarResult[ i ][ j ] = aarVariance[ i ][ j ] =
	  aarMu[ i ][ j ] = 0.0f;
      }
    } else {
      int nGames = prc->nGamesDone;
      double r;

      if (prc->nGamesDone < nFirstTrial) 
	nFirstTrial = prc->nGamesDone;
      /* restore internal variables from input values */
      for ( j = 0; j < NUM_ROLLOUT_OUTPUTS; ++j) {
	r = aarMu[ i ][ j ] = (*apOutput[ i ])[ j ];
	aarResult[ i ][ j ] = r * nGames;
	r = aarSigma[ i ][ j ] = (*apStdDev[ i ])[ j ];
	aarVariance[ i ][ j ] = r * r * nGames;
      }
    }

    /* force all moves/cube decisions to be considered 
       and reset the upper bound on trials */
    prc->fNoMore = 0;
    prc->nTrials = cGames;

  }

  /* ============ begin rollout loop ============= */

  for( i = nFirstTrial; i < cGames; i++ ) {
    for (alt = 0; alt < alternatives; ++alt) {
      pes = apes[ alt ];
      prc = &pes->rc;

      /* skip this one if it's already been done this far */
      if ((prc->nGamesDone > i) || prc->fNoMore)
	continue;

      /* save the board we're working on */
      memcpy (anBoardOrig, apBoard[ alt ], sizeof (anBoardOrig));
      if ( fInvert )
	SwapSides( anBoardOrig );

      /* get the dice generator set up... */
      if( prc->fRotate ) 
	QuasiRandomSeed( prc->nSeed );

      nSkip = prc->nSkip;

      /* ... and the RNG */
      if( prc->rngRollout != RNG_MANUAL )
	InitRNGSeed( prc->nSeed + ( i << 8 ), prc->rngRollout );

      memcpy ( &aanBoardEval[alt][0][0], apBoard[ alt ], 
	       sizeof( anBoardOrig ));

      if (fInvert)
	SwapSides( anBoardOrig );

      /* roll something out */
      BasicCubefulRollout( aanBoardEval + alt, aar + alt, 0, i, apci[ alt ], 
			   apCubeDecTop[ alt ], 1, prc, apStatistics[ alt ] );
      if( fInterrupt )
	break;

      pes->rc.nGamesDone = i;

      if( fInvert ) 
	InvertEvaluationR( aar[ alt ], apci[ alt ] );
    
      /* apply the results */
      for( j = 0; j < NUM_ROLLOUT_OUTPUTS; j++ ) {
	float rMuNew, rDelta;
	  
	aarResult[ alt ][ j ] += aar[ alt ][ j ];
	rMuNew = aarResult[ alt ][ j ] / ( i + 1 );
	  
	if ( i ) {
	  /* for i == 0 aarVariance is not defined */

	  rDelta = rMuNew - aarMu[ alt ][ j ];
	  
	  aarVariance[ alt ][ j ] =
	    aarVariance[ alt ][ j ] * ( 1.0 - 1.0 / i ) +
	    ( i + 1 ) * rDelta * rDelta;
	}
	  
	aarMu[ alt ][ j ] = rMuNew;
	  
	if( j < OUTPUT_EQUITY ) {
	  if( aarMu[ alt ][ j ] < 0.0f )
	    aarMu[ alt ][ j ] = 0.0f;
	  else if( aarMu[ alt ][ j ] > 1.0f )
	    aarMu[ alt ][ j ] = 1.0f;
	}
	  
	aarSigma[ alt ][ j ] = sqrt( aarVariance[ alt ][ j ] / ( i + 1 ) );
      }

      if ( ! isHyperGammon( apci[ alt ]->bgv ) )
	SanityCheck( anBoardOrig, aarMu[ alt ] );

    }

    if( fInterrupt )
	break;
      
    if( fShowProgress ) {
#if USE_GTK
      if( fX ) 
	GTKRolloutUpdate( aarMu, aarSigma, i, cGames,
			  prc->fCubeful, alternatives, aciLocal );
      else
#endif
	{
	  outputf( "%28s %5.3f %5.3f %5.3f %5.3f %5.3f (%6.3f) %5.3f "
		   "%5d\r", asz[ 0 ], aarMu[ 0 ][ 0 ],
		   aarMu[ 0 ][ 1 ], aarMu[ 0 ][ 2 ],
		   aarMu[ 0 ][ 3 ], aarMu[ 0 ][ 4 ],
		   aarMu[ 0 ][ 5 ], aarSigma[ 0 ][ 5 ], i + 1 );
	  fflush( stdout );
	}

    }

    /* see if we can quit because the answers are good enough */
    if (prc->fStopOnSTD && (i >= prc->nMinimumGames)) {
      err_too_big = 0;

      for (ii = 0; ii < NUM_ROLLOUT_OUTPUTS; ii++) {
	if ( ii < OUTPUT_EQUITY ) {
	  v = fabs (aarMu[ alt ][ ii ]);
	  s = fabs (aarSigma[ alt ][ ii ]);
	} else if ( ii == OUTPUT_EQUITY ) {
	  if ( ! ms.nMatchTo ) { /* money game */
	    v = fabs (aarMu[ alt ][ ii ] * apci[ alt ]->nCube / 
		      apci[ 0 ]->nCube );
	    s = fabs (aarSigma[ alt ][ ii ] * apci[ alt ]->nCube / 
		      apci[ 0 ]->nCube);
	  } else { /* match play */
	    v = fabs (mwc2eq ( eq2mwc ( aarMu[ alt ][ ii ], apci[ alt ] ), 
			       apci[ 0 ] ));
	    s = fabs (se_mwc2eq ( se_eq2mwc ( aarSigma[ alt ][ ii ], 
					      apci[ alt ] ), apci[ 0 ] ));
	  }
	} else {
	  if (!prc->fCubeful) 
	    continue;

	  if ( ! ms.nMatchTo ) { /* money game */
	    v = fabs (aarMu[ alt ][ ii ]);
	    s = fabs (aarSigma[ alt ][ ii ]); 
	    } else {
	      v = fabs (mwc2eq ( aarMu[ alt ][ ii ], apci[ 0 ] ));
	      s = fabs(se_mwc2eq ( aarSigma[ alt ][ ii ], apci[ 0 ] ) );
	    }
	  } /* else if ( ii == OUTPUT_EQUITY) ... else ) */

	  if ((v >= .0001) && (v * prc->rStdLimit < s)) {
	    err_too_big = 1;
	    break;
	  }
	} /* for (ii = 0; ii < NUM_ROLLOUT_OUTPUTS; ii++) */

      if (!err_too_big) {
	prc->nGamesDone = i + 1;
	prc->fNoMore = 1;
      }
    } /* if (prc->fStopOnSTD && (i >= prc->nMinimumGames)) */
  }

  cGames = i;
  /* return -1 if no games rolled out */
  if( ! cGames)
    return -1;

  /* store results */
  for (alt = 0; alt < alternatives; alt++) {
    if (!apes[alt]->rc.fNoMore && apes[alt]->rc.nGamesDone <= cGames)
      apes[alt]->rc.nGamesDone = cGames;
    
    if( apOutput[alt] )
      for ( i = 0; i < NUM_ROLLOUT_OUTPUTS; i++ )
        (*apOutput[ alt ])[ i ] = aarMu[ alt ][ i ];

    if( apStdDev[alt] )
      for( i = 0; i < NUM_ROLLOUT_OUTPUTS; i++ )
        (*apStdDev[ alt ])[ i ] = aarSigma[ alt ][ i ];	
  }

  if( fShowProgress
#if USE_GTK
      && !fX
#endif
      ) {
    for( i = 0; i < 79; i++ )
      outputc( ' ' );

    outputc( '\r' );
    fflush( stdout );
  }

  /* print statistics */

  return cGames;
}


/*
 * General evaluation functions.
 */

extern int
GeneralEvaluation ( char *sz, 
                    float arOutput[ NUM_ROLLOUT_OUTPUTS ], 
                    float arStdDev[ NUM_ROLLOUT_OUTPUTS ], 
                    rolloutstat arsStatistics[ 2 ],
                    int anBoard[ 2 ][ 25 ],
                    cubeinfo *pci, evalsetup *pes ) {

  int i;

  switch ( pes->et ) {
  case EVAL_EVAL:

    for ( i = 0; i < NUM_ROLLOUT_OUTPUTS; i++ )
      arStdDev[ i ] = 0.0f;

    return GeneralEvaluationE ( arOutput, anBoard, pci, &pes->ec );
    break;

  case EVAL_ROLLOUT:

    return GeneralEvaluationR ( sz, arOutput, arStdDev, arsStatistics,
                                anBoard, pci, &pes->rc );
    break;

  case EVAL_NONE:

    for ( i = 0; i < NUM_ROLLOUT_OUTPUTS; i++ )
      arOutput[ i ] = arStdDev[ i ] = 0.0f;

    break;
  }

  return 0;
}

extern int
GeneralEvaluationR ( char *sz,
                     float arOutput [ NUM_ROLLOUT_OUTPUTS ],
                     float arStdDev [ NUM_ROLLOUT_OUTPUTS ],
                     rolloutstat arsStatistics[ 2 ],
                     int anBoard[ 2 ][ 25 ],
                     cubeinfo *pci, rolloutcontext *prc ) {

  int (* apBoard[1])[2][25];
  float (*apOutput[1])[NUM_ROLLOUT_OUTPUTS];
  float (*apStdDev[1])[NUM_ROLLOUT_OUTPUTS];
  rolloutstat (* apStatistics[1])[2];
  evalsetup  es;
  evalsetup (* apes[1]);
  cubeinfo (* apci[1]);
  int false = 0;
  int (* apCubeDecTop[1]);

  apBoard[0] =  (int (*)[2][25]) anBoard;
  apOutput[0] = (float (*)[NUM_ROLLOUT_OUTPUTS]) arOutput;
  apStdDev[0] = (float (*)[NUM_ROLLOUT_OUTPUTS]) arStdDev;
  apStatistics[0] = (rolloutstat (*)[2]) arsStatistics;
  apes[0] = &es;
  apci[0] = pci;
  apCubeDecTop[0] = &false;

  es.et = EVAL_NONE;
  memcpy (&es.rc, prc, sizeof (rolloutcontext));
  
  if ( RolloutGeneral ( apBoard, ( char (*)[ 40 ] ) sz,
			apOutput, apStdDev, apStatistics, apes, apci,
			apCubeDecTop, 1, FALSE) < 0 )
    return -1;
  
  return 0;
}

extern int
GeneralCubeDecision ( char *sz, 
                      float aarOutput[ 2 ][ NUM_ROLLOUT_OUTPUTS ], 
                      float aarStdDev[ 2 ][ NUM_ROLLOUT_OUTPUTS ], 
                      rolloutstat aarsStatistics[ 2 ][ 2 ],
                      int anBoard[ 2 ][ 25 ],
                      cubeinfo *pci, evalsetup *pes ) {

  int i, j;

  switch ( pes->et ) {
  case EVAL_EVAL:

    for ( j = 0; j < 2; j++ )
      for ( i = 0; i < NUM_ROLLOUT_OUTPUTS; i++ )
        aarStdDev[ j ][ i ] = 0.0f;

    return GeneralCubeDecisionE ( aarOutput, anBoard, pci, &pes->ec, pes );
    break;

  case EVAL_ROLLOUT:

    return GeneralCubeDecisionR ( sz, aarOutput, aarStdDev, aarsStatistics, 
                                  anBoard, pci, &pes->rc, pes );
    break;

  case EVAL_NONE:

    for ( j = 0; j < 2; j++ )
      for ( i = 0; i < NUM_ROLLOUT_OUTPUTS; i++ )
        aarStdDev[ j ][ i ] = 0.0f;

    break;
  }

  return 0;
}


extern int
GeneralCubeDecisionR ( char *sz, 
                       float aarOutput[ 2 ][ NUM_ROLLOUT_OUTPUTS ], 
                       float aarStdDev[ 2 ][ NUM_ROLLOUT_OUTPUTS ], 
                       rolloutstat aarsStatistics[ 2 ][ 2 ],
                       int anBoard[ 2 ][ 25 ],
                       cubeinfo *pci, rolloutcontext *prc, evalsetup *pes ) {

  evalsetup esLocal;
  int (* apBoard[2])[2][25] = { (int (*)[2][25]) anBoard, 
				(int (*)[2][25]) anBoard};
  float (* apOutput[2])[ NUM_ROLLOUT_OUTPUTS ] = 
	 { (float (*)[NUM_ROLLOUT_OUTPUTS]) aarOutput, 
	   (float (*)[NUM_ROLLOUT_OUTPUTS]) aarOutput + 1};
  float (* apStdDev[2])[ NUM_ROLLOUT_OUTPUTS ] = 
	 { (float (*)[NUM_ROLLOUT_OUTPUTS]) aarStdDev[0], 
	   (float (*)[NUM_ROLLOUT_OUTPUTS]) aarStdDev[1]};
  rolloutstat (* apStatistics[2])[2] = 
  { (rolloutstat (*)[2]) aarsStatistics[0], 
    (rolloutstat (*)[2]) aarsStatistics[1] };
  evalsetup (* apes[2]);
  cubeinfo aci[ 2 ];
  cubeinfo (* apci[2]) = { &aci[ 0 ], &aci[ 1 ] };


  char aach[ 2 ][ 40 ];

  int i, cGames;
  int afCubeDecTop[] = { FALSE, FALSE }; /* no cube decision in 
                                            iTurn = 0 */
  int (* apCubeDecTop[2]) = { afCubeDecTop, afCubeDecTop};

  if (pes == 0) {
    /* force rollout from sratch */
    pes = &esLocal;
    memcpy (&pes->rc, &rcRollout, sizeof (rcRollout));
    pes->et = EVAL_NONE;
    pes->rc.nGamesDone=0;
  }

  apes[ 0 ] = apes[ 1 ] = pes;

  SetCubeInfo ( &aci[ 0 ], pci->nCube, pci->fCubeOwner, pci->fMove,
                pci->nMatchTo, pci->anScore, pci->fCrawford, 
                pci->fJacoby, pci->fBeavers, pci->bgv );

  FormatCubePosition ( aach[ 0 ], &aci[ 0 ] );

  SetCubeInfo ( &aci[ 1 ], 2 * pci->nCube, ! pci->fMove, pci->fMove,
                pci->nMatchTo, pci->anScore, pci->fCrawford, 
                pci->fJacoby, pci->fBeavers, pci->bgv );

  FormatCubePosition ( aach[ 1 ], &aci[ 1 ] );

  if ( ! GetDPEq ( NULL, NULL, &aci[ 0 ] ) ) {
    outputl ( _("Cube not available!") );
    return -1;
  }

  if ( ! prc->fCubeful ) {
    outputl ( _("Setting cubeful on") );
    prc->fCubeful = TRUE;
  }


#if USE_GTK
  if( fX )
    GTKRollout( 2, aach, prc->nTrials, aarsStatistics );
  else
#endif
    outputl( _("                               Win  W(g) W(bg)  L(g) L(bg) "
             "Equity                    Trials") );
	
#if USE_GTK
  if( fX )
    GTKRolloutRow( 0 );
#endif

  if( ( cGames = RolloutGeneral( apBoard, aach, apOutput, apStdDev, 
                                 apStatistics, apes, apci,
                                 apCubeDecTop, 2, FALSE)) <= 0 )
    return -1;

  pes->rc.nGamesDone = cGames;
  pes->rc.nSkip = nSkip;

#if USE_GTK
	if( !fX )
#endif
          for ( i = 0; i < 2; i++ )
	    outputf( _("%28s %5.3f %5.3f %5.3f %5.3f %5.3f (%6.3f) "
                     "Cubeful: %6.3f %12d\n"
		     "              Standard error %5.3f %5.3f %5.3f %5.3f"
		     " %5.3f (%6.3f)         %6.3f\n\n"),
		     aach[ i ],
                     aarOutput[ i ][ 0 ], aarOutput[ i ][ 1 ],
                     aarOutput[ i ][ 2 ], aarOutput[ i ][ 3 ],
                     aarOutput[ i ][ 4 ], aarOutput[ i ][ 5 ],
                     aarOutput[ i ][ 6 ],
                     cGames,
                     aarStdDev[ i ][ 0 ], aarStdDev[ i ][ 1 ],
                     aarStdDev[ i ][ 2 ], aarStdDev[ i ][ 3 ],
                     aarStdDev[ i ][ 4 ], aarStdDev[ i ][ 5 ],
                     aarStdDev[ i ][ 6 ] ); 
    
#if USE_GTK
    if( fX )
      GTKRolloutDone();
#endif	
  
    return 0;
}


/*
 * Initialise rollout stat with zeroes.
 *
 * Input: 
 *    - prs: rollout stat to initialize
 *
 * Output:
 *    None.
 *
 * Returns:
 *    void.
 *
 */

static void
initRolloutstat ( rolloutstat *prs ) {

  memset ( prs, 0, sizeof ( rolloutstat ) );

}



/*
 * Construct nicely formatted string with rollout statistics.
 *
 * Input: 
 *    - prs: rollout stat 
 *    - sz : string to fill in
 *
 * Output:
 *    - sz : nicely formatted string with rollout statistics.
 *
 * Returns:
 *    sz
 *
 *
 * FIXME: hopeless outdated!
 */

extern char *
printRolloutstat ( char *sz, const rolloutstat *prs, const int cGames ) {

  static char szTemp[ 32000 ];
  static char *pc;
  int i;
  int nSum;
  int nCube;
  long nSumPoint;
  long nSumPartial;

  pc = szTemp;

  nSumPoint = 0;

  sprintf ( pc, _("Points won:\n\n") );
  pc = strchr ( pc, 0 );
  
  /* single win statistics */

  nSum = 0;
  nCube = 1;
  nSumPartial = 0;

  for ( i = 0; i < 10; i++ ) {

    sprintf ( pc, _("Number of wins %4d-cube          %8d   %7.3f%%     %8d\n"),
              nCube,
              prs->acWin[ i ],
              100.0f * prs->acWin[ i ] / cGames,
              nCube * prs->acWin[ i ] );
    pc = strchr ( pc, 0 );
    
    nSum += prs->acWin[ i ];
    nCube *= 2;
    nSumPartial += nCube * prs->acWin[ i ];

  }


  sprintf ( pc,
            _("------------------------------------------------------------------\n"
            "Total number of wins              %8d   %7.3f%%     %8ld\n"
            "------------------------------------------------------------------\n\n"),
            nSum, 100.0f * nSum / cGames, nSumPartial  );
  pc = strchr ( pc, 0 );

  nSumPoint += nSumPartial;


  /* gammon win statistics */

  nSum = 0;
  nCube = 1;

  for ( i = 0; i < 10; i++ ) {

    sprintf ( pc, _("Number of gammon wins %4d-cube   %8d   %7.3f%%     %8d\n"),
              nCube,
              prs->acWinGammon[ i ],
              100.0f * prs->acWinGammon[ i ] / cGames,
              nCube * 2 * prs->acWinGammon[ i ] );
    pc = strchr ( pc, 0 );
    
    nSum += prs->acWinGammon[ i ];
    nCube *= 2;
    nSumPartial += nCube * 2 * prs->acWinGammon[ i ];

  }

  sprintf ( pc,
            _("------------------------------------------------------------------\n"
            "Total number of gammon wins       %8d   %7.3f%%     %8ld\n"
            "------------------------------------------------------------------\n\n"),
            nSum, 100.0f * nSum / cGames, nSumPartial  );
  pc = strchr ( pc, 0 );

  nSumPoint += nSumPartial;

  /* backgammon win statistics */

  nSum = 0;
  nCube = 1;

  for ( i = 0; i < 10; i++ ) {

    sprintf ( pc, _("Number of bg wins %4d-cube       %8d   %7.3f%%     %8d\n"),
              nCube,
              prs->acWinBackgammon[ i ],
              100.0f * prs->acWinBackgammon[ i ] / cGames,
	      nCube * 3 * prs->acWinBackgammon[ i ] );
    pc = strchr ( pc, 0 );
    
    nSum += prs->acWinBackgammon[ i ];
    nCube *= 2;

  }

  sprintf ( pc,
            _("------------------------------------------------------------------\n"
            "Total number of bg wins           %8d   %7.3f%%     %8ld\n"
            "------------------------------------------------------------------\n\n"),
            nSum, 100.0f * nSum / cGames, nSumPartial  );
  pc = strchr ( pc, 0 );

  nSumPoint += nSumPartial;

  /* double drop statistics */

  nSum = 0;
  nCube = 2;
  nSumPartial = 0;

  for ( i = 0; i < 10; i++ ) {

    sprintf ( pc, _("Number of %4d-cube double, drop  %8d   %7.3f%%    %8d\n"),
              nCube,
              prs->acDoubleDrop[ i ],
              100.0f * prs->acDoubleDrop[ i ] / cGames,
              nCube / 2 * prs->acDoubleDrop[ i ] );
    pc = strchr ( pc, 0 );
    
    nSum += prs->acDoubleDrop[ i ];
    nCube *= 2;
    nSumPartial += nCube / 2 * prs->acDoubleDrop[ i ];

  }

  sprintf ( pc,
            _("------------------------------------------------------------------\n"
            "Total number of double, drop      %8d   %7.3f%%     %8ld\n"
            "------------------------------------------------------------------\n\n"),
            nSum, 100.0f * nSum / cGames, nSumPartial  );
  pc = strchr ( pc, 0 );

  nSumPoint += nSumPartial;

  sprintf ( pc,
            _("==================================================================\n"
            "Total number of wins              %8d   %7.3f%%     %8ld\n"
            "==================================================================\n\n"),
            0, 100.0f * 0 / cGames, nSumPoint  );
  pc = strchr ( pc, 0 );


  /* double take statistics */

  sprintf ( pc, _("\n\nOther statistics:\n\n") );
  pc = strchr ( pc, 0 );
  


  nCube = 2;

  for ( i = 0; i < 10; i++ ) {

    sprintf ( pc, _("Number of %4d-cube double, take  %8d   %7.3f%%\n"),
              nCube,
              prs->acDoubleTake[ i ],
              100.0f * prs->acDoubleTake[ i ] / cGames  );
    pc = strchr ( pc, 0 );
    
    nCube *= 2;

  }

  sprintf ( pc, "\n\n" );
  pc = strchr ( pc, 0 );

#if 0

 FIXME: rewrite

  /* hitting statistics */

  nSum = 0;

  for ( i = 0; i < MAXHIT; i++ ) {

    sprintf ( pc, _("Number of hits move %4d          %8d   %7.3f%%\n"),
              i + 1, 
              prs->acHit[ i ],
              100.0f * prs->acHit[ i ] / cGames  );
    pc = strchr ( pc, 0 );
    
    nSum += prs->acHit[ i ];

  }

  sprintf ( pc, _("Total number of hits              %8d   %7.3f%%\n\n"),
            nSum, 100.0f * nSum / cGames  );
  pc = strchr ( pc, 0 );

#endif


  outputl ( szTemp );

  return sz;

}

/*
 * Calculate whether we should resign or not
 *
 * Input:
 *    anBoard   - current board
 *    pci       - current cube info
 *    pesResign - evaluation parameters
 *
 * Output:
 *    arResign  - evaluation
 *
 * Returns:
 *    -1 on error
 *     0 if we should not resign
 *     1,2, or 3 if we should resign normal, gammon, or backgammon,
 *     respectively.  
 *
 */

extern int
getResignation ( float arResign[ NUM_ROLLOUT_OUTPUTS ],
                 int anBoard[ 2 ][ 25 ],
                 cubeinfo *pci, 
                 evalsetup *pesResign ) {

  float arStdDev[ NUM_ROLLOUT_OUTPUTS ];
  rolloutstat arsStatistics[ 2 ];
  float ar[ NUM_OUTPUTS ] = { 0.0, 0.0, 0.0, 1.0, 1.0 };

  float rPlay;

  /* Evaluate current position */

  if ( GeneralEvaluation ( NULL,
                           arResign, arStdDev,
                           arsStatistics,
                           anBoard,
                           pci, pesResign ) < 0 )
    return -1;

  /* check if we want to resign */

  rPlay = Utility ( arResign, pci );

  if ( arResign [ OUTPUT_LOSEBACKGAMMON ] > 0.0f &&
       Utility ( ar, pci ) == rPlay )
    /* resign backgammon */
    return ( !pci->nMatchTo && pci->fJacoby && pci->fCubeOwner == -1 ) ? 1 : 3;
  else {

    /* worth trying to escape the backgammon */

    ar[ OUTPUT_LOSEBACKGAMMON ] = 0.0f;

    if ( arResign[ OUTPUT_LOSEGAMMON ] > 0.0f &&
         Utility ( ar, pci ) == rPlay )
      /* resign gammon */
      return ( !pci->nMatchTo && pci->fJacoby && pci->fCubeOwner == -1 ) ? 1 : 2; 
    else {

      /* worth trying to escape gammon */

      ar[ OUTPUT_LOSEGAMMON ] = 0.0f;

      return Utility ( ar, pci ) == rPlay;

    }

  }

}


extern void
getResignEquities ( float arResign[ NUM_ROLLOUT_OUTPUTS ],
                    cubeinfo *pci, 
                    int nResigned,
                    float *prBefore, float *prAfter ) {

  float ar[ NUM_OUTPUTS ] = { 0, 0, 0, 0, 0 };

  *prBefore = Utility ( arResign, pci );

  if ( nResigned > 1 )
    ar[ OUTPUT_LOSEGAMMON ] = 1.0f;
  if ( nResigned > 2 )
    ar[ OUTPUT_LOSEBACKGAMMON ] = 1.0f;

  *prAfter = Utility ( ar, pci );

}


extern int
ScoreMoveRollout ( move **ppm, cubeinfo **ppci, int cMoves ) {

  cubeinfo *pci;
  int fCubeDecTop = TRUE;
  int	i;
  int nGamesDone;
  rolloutcontext *prc;

#if HAVE_ALLOCA
  int (* anBoard)[ 2 ][ 25 ] = alloca (cMoves * 2 * 25 * sizeof (int));
  int (** apBoard)[2][25] = alloca (cMoves * sizeof (float));
  float (** apOutput)[ NUM_ROLLOUT_OUTPUTS ] = 
    alloca (cMoves * NUM_ROLLOUT_OUTPUTS * sizeof (float));
  float (** apStdDev)[ NUM_ROLLOUT_OUTPUTS ] =
    alloca (cMoves * NUM_ROLLOUT_OUTPUTS * sizeof (float));
  rolloutstat (** apStatistics)[2] = 
    alloca (cMoves * 2 * sizeof (rolloutstat *));
  evalsetup (** apes) = alloca (cMoves * sizeof (evalsetup *));
  cubeinfo (** apci) = alloca (cMoves * sizeof (cubeinfo *));
  cubeinfo (* aci) = alloca (cMoves * sizeof (cubeinfo));
  int (** apCubeDecTop) = alloca (cMoves * sizeof (int *));
#else
  int 	      anBoard[10][2][25];
  int         (*apBoard[10])[2][25];
  float       (*apOutput[10])[2][25];
  float       (*apStdDev[10])[2][25];
  rolloutstat (*apStatistics[10])[2];
  evalsetup   (*apes[10]);
  cubeinfo    (*apci[10]), aci[ 10 ];
  int         (*apCubeDecTop[10]);

  if (cMoves > 10)
    cMoves = 10;

#endif
  
  /* initialise the arrays we'll need */
  for (i = 0; i < cMoves; ++i) {
    apBoard[ i ] = anBoard + i;
    apOutput[ i ] = &ppm[i]->arEvalMove;
    apStdDev[ i ] = &ppm[i]->arEvalStdDev;
    apStatistics[ i ] = 0;
    apes[ i ] = &ppm[i]->esMove;
    apci[ i ] = aci + i;
    memcpy (aci + i, ppci[ i ], sizeof (cubeinfo));
    apCubeDecTop[ i ] = &fCubeDecTop;

    PositionFromKey( anBoard[ i ], ppm[ i ]->auch );
      
    SwapSides( anBoard[ i ] );

    /* swap fMove in cubeinfo */
    aci[ i ].fMove = ! aci[ i ].fMove;
  
  }

  nGamesDone = RolloutGeneral ( apBoard, NULL, 
				apOutput, apStdDev, apStatistics,
				apes, apci, apCubeDecTop, cMoves, TRUE);
  /* put fMove back again */
  for ( i = 0; i < cMoves; ++i) {
    aci[ i ].fMove = ! aci[ i ].fMove;
  }

  if (nGamesDone < 0)
    return -1;

  for ( i = 0; i < cMoves; ++i) {

    /* Score for move:
       rScore is the primary score (cubeful/cubeless)
       rScore2 is the secondary score (cubeless) */
    prc = &apes[ i ]->rc;
    pci = apci[ i ];

    if ( prc->fCubeful ) {
      if ( pci->nMatchTo )
	ppm[i]->rScore = 
	  mwc2eq ( ppm[ i ]->arEvalMove[ OUTPUT_CUBEFUL_EQUITY ], pci );
      else
	ppm[ i ]->rScore = ppm[ i ]->arEvalMove[ OUTPUT_CUBEFUL_EQUITY ];
    }
    else
      ppm[ i ]->rScore = ppm[ i ]->arEvalMove[ OUTPUT_EQUITY ];

    ppm[ i ]->rScore2 = ppm[ i ]->arEvalMove[ OUTPUT_EQUITY ];
  }

  return 0;

}


extern int
ScoreMoveGeneral ( move *pm, cubeinfo *pci, evalsetup *pes ) {

  switch ( pes->et ) {
  case EVAL_EVAL:
    return ScoreMove ( pm, pci, &pes->ec, pes->ec.nPlies );
    break;
  case EVAL_ROLLOUT:
    return ScoreMoveRollout ( &pm, &pci, 1);
    break;
  default:
    return -1;
    break;
  }

}

