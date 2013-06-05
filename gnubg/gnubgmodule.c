/*
 * gnubgmodule.c
 *
 * by Joern Thyssen <jth@gnubg.org>, 2003.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 3 or later of the GNU General Public License as
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

#include "gnubgmodule.h"
#include "backgammon.h"
#include "positionid.h"
#include "drawboard.h"

#include <signal.h>
#include <glib.h>
#include <glib/gstdio.h>

#include "eval.h"
#include "matchequity.h"
#include "positionid.h"
#include "matchid.h"
#include "util.h"

#if USE_GTK
#include "gtkgame.h"
#include <gtk/gtk.h>
#endif

#if USE_PYTHON

static PyObject *
BoardToPy( const TanBoard anBoard )
{
  PyObject* b = PyTuple_New(2);
  PyObject* b0 = PyTuple_New(25);
  PyObject* b1 = PyTuple_New(25);
  unsigned int k;
  
  for(k = 0; k < 25; ++k) {
    PyTuple_SET_ITEM(b0, k, PyInt_FromLong(anBoard[ 0 ][k]));
    PyTuple_SET_ITEM(b1, k, PyInt_FromLong(anBoard[ 1 ][k]));
  }

  PyTuple_SET_ITEM(b, 0, b0);
  PyTuple_SET_ITEM(b, 1, b1);

  return b;
}

static PyObject *
Board1ToPy( unsigned int anBoard [ 25 ] ) {
  unsigned int k;
  PyObject* b = PyTuple_New(25);
  
  for(k = 0; k < 25; ++k) {
    PyTuple_SET_ITEM(b, k, PyInt_FromLong(anBoard[k]));
  }
  
  return b;
}

/* Convert Move tuple to an anMove structure. 
   A tuple can be expressed in two forms.
   form returned by findbestmove ie. (21,18,11,5)
   form returned by parsemove ie. ((21,18),(11,5))

   on return populates the passed anMove structure
*/
static int
PyToMove( PyObject* p, unsigned int anMove[ 8 ] )
{
  int tuplelen;

  if( !PySequence_Check(p))
    return 0;

  tuplelen = PySequence_Size(p);
  if( tuplelen > 0 ) {
    int j;
    int anIndex = 0;

    /* Process outter tuple */
    for ( j = 0; j < tuplelen && anIndex < 8; ++j ) {
      PyObject* pi = PySequence_Fast_GET_ITEM(p, j);
      if( !PySequence_Check(pi))
        if (PyInt_Check(pi))
          /* Found value like findbestmove returns */
          anMove[ anIndex++ ] = (int) PyInt_AsLong( pi ) - 1;
       else
          /* Value not an integer */
          return 0;
      else {
        /* Found inner tuple like parsemove returns */
        if ( PySequence_Check(pi) && PySequence_Size(pi) == 2 ) {
          int k;
          /* Process inner tuple */
          for ( k = 0; k < 2 && anIndex < 8; ++k, anIndex++ ) {
            PyObject* pii = PySequence_Fast_GET_ITEM(pi, k);
            if (PyInt_Check(pii))
   	          anMove[ anIndex ] = (int) PyInt_AsLong( pii ) - 1;
            else
              /* Value not an integer */
              return 0;
          }
        }
        /*if an inner tuple doesn't have exactly 2 elements there is an error */
        else
          return 0;
      }
    }
    /* If we have found more items than the maximum */
    if ( anIndex >= 8 && j < tuplelen)
      return 0;

    return 1;
  }
  else {
    /* No tuples equivalent to no legal moves */
    return 1;
  }

  return 0;
}

static int
PyToBoard1( PyObject* p, unsigned int anBoard[ 25 ] )
{
  if( PySequence_Check(p) && PySequence_Size(p) == 25 ) {
    int j;

    for ( j = 0; j < 25; ++j ) {
      PyObject* pi = PySequence_Fast_GET_ITEM(p, j);
    
      anBoard[ j ] = (int) PyInt_AsLong( pi );
    }
    return 1;
  }

  return 0;
}

static int
PyToBoard(PyObject* p, TanBoard anBoard)
{
  if( PySequence_Check(p) && PySequence_Size(p) == 2 ) {
    int i;
    for(i = 0; i < 2; ++i) {
      PyObject* py = PySequence_Fast_GET_ITEM(p, i);

      if ( ! PyToBoard1( py, anBoard[ i ] ) ) {
	return 0;
      }
    }
    return 1;
  }

  return 0;
}

static int
PyToDice( PyObject* p, int anDice[ 2 ] )
{
  if( PySequence_Check(p) && PySequence_Size(p) == 2 ) {
    int j;

    for ( j = 0; j < 2; ++j ) {
      PyObject* pi = PySequence_Fast_GET_ITEM(p, j);
    
      anDice[ j ] = (int) PyInt_AsLong( pi );
    }
    return 1;
  }

  return 0;
}

static PyObject *
CubeInfoToPy( const cubeinfo *pci )
{
  return Py_BuildValue( "{s:i,s:i,s:i,s:i,s:(i,i),"
                        "s:i,s:i,s:i,s:((f,f),(f,f)),s:i}",
                        "cube", pci->nCube,
                        "cubeowner", pci->fCubeOwner,
                        "move", pci->fMove,
                        "matchto", pci->nMatchTo,
                        "score", pci->anScore[ 0 ], pci->anScore[ 1 ],
                        "crawford", pci->fCrawford,
                        "jacoby", pci->fJacoby,
                        "beavers", pci->fBeavers,
                        "gammonprice", 
                        pci->arGammonPrice[ 0 ],
                        pci->arGammonPrice[ 2 ],
                        pci->arGammonPrice[ 1 ],
                        pci->arGammonPrice[ 3 ],
                        "bgv", pci->bgv );
}


static int
PyToCubeInfo( PyObject *p, cubeinfo *pci ) {

  PyObject *pyKey, *pyValue;
  Py_ssize_t iPos = 0;
  char *pchKey;
  static const char *aszKeys[] = {
    "jacoby", "crawford", "move", "beavers", "cube", "matchto",
    "bgv", "cubeowner", "score", "gammonprice", NULL };
  int iKey;
  void *ap[10];
  int *pi;
  float *pf;
  int i = 0;
  ap[i++] = &pci->fJacoby;
  ap[i++] = &pci->fCrawford;
  ap[i++] = &pci->fMove;
  ap[i++] = &pci->fBeavers;
  ap[i++] = &pci->nCube;
  ap[i++] = &pci->nMatchTo;
  ap[i++] = &pci->bgv;
  ap[i++] = &pci->fCubeOwner;
  ap[i++] = pci->anScore;
  ap[i++] = pci->arGammonPrice;

  while( PyDict_Next( p, &iPos, &pyKey, &pyValue ) ) {

    if ( ! ( pchKey = PyString_AsString( pyKey ) ) )
      return -1;

    iKey = -1;

    for ( i = 0; aszKeys[ i ] && iKey < 0; ++i )
      if ( ! strcmp( aszKeys[ i ], pchKey ) )
        iKey = i;

    if ( iKey < 0 ) {
      /* unknown dict value */
      PyErr_SetString( PyExc_ValueError, 
                       _("invalid dict value in cubeinfo "
                         "(see gnubg.cubeinfo() for an example)") );
      return -1;
    }

    switch( iKey ) {
    case 0:
    case 1:
    case 2:
    case 3:
    case 4:
    case 5:
    case 6:
    case 7:
      /* simple integer */
      if ( ! PyInt_Check( pyValue ) ) {
        /* unknown dict value */
        PyErr_SetString( PyExc_ValueError, 
                         _("invalid value cubeinfo "
                           "(see gnubg.setcubeinfo() for an example)") );
        return -1;
      }
      
      *((int *) ap[ iKey ]) = (int) PyInt_AsLong( pyValue );

      break;

    case 8:
      /* score */
      pi = (int *) ap[ iKey ];
      if ( ! PyArg_ParseTuple( pyValue, "ii", pi, pi + 1 ) )
        return -1;
      break;

    case 9:
      /* gammon price */
      pf = (float *) ap[ iKey ];
      if ( ! PyArg_ParseTuple( pyValue, "(ff)(ff)", 
                               pf, pf + 2, pf + 1, pf + 3 ) )
        return -1;
      break;
      
    default:
      g_assert_not_reached();

    }

  }

  return 0;

}

static int 
SetPosInfo( posinfo *ppi, const int fTurn, const int fResigned,
                  const int fDoubled, const gamestate gs, const int anDice[ 2 ] ) {
    
    if( fTurn < 0 || fTurn > 1 || fResigned < 0 || fResigned > 1 || 
    	  fDoubled < 0 || fDoubled > 1 || anDice[ 0 ] > 6 || anDice[ 0 ] < 0 ||
    	  anDice[ 1 ] > 6 || anDice[ 1 ] < 0 ||  gs > 7) {
		memset(ppi, 0, sizeof(posinfo));
		return -1;
    }
    
    ppi->fTurn = fTurn;
    ppi->fResigned = fResigned;
    ppi->fDoubled = fDoubled;
    ppi->gs = gs;
    ppi->anDice[ 0 ] = anDice[ 0 ];
    ppi->anDice[ 1 ] = anDice[ 1 ];
                
    return 0;
}


static PyObject *
PosInfoToPy( const posinfo *ppi )
{
  return Py_BuildValue( "{s:(i,i),s:i,s:i,s:i,s:i}",
                        "dice", ppi->anDice[0], ppi->anDice[1],
                        "turn", ppi->fTurn,
                        "resigned", ppi->fResigned,
                        "doubled", ppi->fDoubled,
                        "gamestate", ppi->gs );
}


static int
PyToPosInfo( PyObject *p, posinfo *ppi ) {

  PyObject *pyKey, *pyValue;
  Py_ssize_t iPos = 0;
  char *pchKey;
  static const char *aszKeys[] = {
    "turn", "resigned", "doubled", "gamestate", "dice", NULL };
  int iKey;
  void *ap[5];
  int *pi;
  int i = 0;
  ap[i++] = &ppi->fTurn;
  ap[i++] = &ppi->fResigned;
  ap[i++] = &ppi->fDoubled;
  ap[i++] = &ppi->gs;
  ap[i++] = &ppi->anDice;

  while( PyDict_Next( p, &iPos, &pyKey, &pyValue ) ) {

    if ( ! ( pchKey = PyString_AsString( pyKey ) ) )
      return -1;

    iKey = -1;

    for ( i = 0; aszKeys[ i ] && iKey < 0; ++i )
      if ( ! strcmp( aszKeys[ i ], pchKey ) )
        iKey = i;

    if ( iKey < 0 ) {
      /* unknown dict value */
      PyErr_SetString( PyExc_ValueError, 
                       _("invalid dict value in posinfo "
                         "(see gnubg.posinfo() for an example)") );
      return -1;
    }

    switch( iKey ) {
    case 0:
    case 1:
    case 2:
      /* simple unsigned integer (gamestate) */
      if ( ! PyInt_Check( pyValue ) ) {
        /* unknown dict value */
        PyErr_SetString( PyExc_ValueError, 
                         _("invalid value posinfo "
                           "(see gnubg.posinfo() for an example)") );
        return -1;
      }
      
      *((gamestate *) ap[ iKey ]) = (gamestate) PyInt_AsLong( pyValue );

      break;

    case 3:
      /* simple integer */
      if ( ! PyInt_Check( pyValue ) ) {
        /* unknown dict value */
        PyErr_SetString( PyExc_ValueError, 
                         _("invalid value posinfo "
                           "(see gnubg.posinfo() for an example)") );
        return -1;
      }
      
      *((int *) ap[ iKey ]) = (int) PyInt_AsLong( pyValue );
		break;
		
    case 4:
      /* Dice */
      pi = (int *) ap[ iKey ];
      if ( ! PyArg_ParseTuple( pyValue, "ii", pi, pi + 1 ) )
        return -1;
      break;
      
    default:
      g_assert_not_reached();

    }

  }

  return 0;

}

static PyObject*
EvalContextToPy( const evalcontext* pec)
{
  return Py_BuildValue( "{s:i,s:i,s:i,s:f}",
                        "cubeful", pec->fCubeful,
                        "plies", pec->nPlies,
                        "deterministic", pec->fDeterministic,
                        "noise", pec->rNoise );
}


static int
PyToEvalContext( PyObject *p, evalcontext *pec ) {

  PyObject *pyKey, *pyValue;
  Py_ssize_t iPos = 0;
  char *pchKey;
  static const char *aszKeys[] = {
    "cubeful", "plies", "deterministic", "noise", NULL };
  int iKey;
  int i;
  
  while( PyDict_Next( p, &iPos, &pyKey, &pyValue ) ) {

    if ( ! ( pchKey = PyString_AsString( pyKey ) ) )
      return -1;

    iKey = -1;

    for ( i = 0; aszKeys[ i ] && iKey < 0; ++i )
      if ( ! strcmp( aszKeys[ i ], pchKey ) )
        iKey = i;

    if ( iKey < 0 ) {
      /* unknown dict value */
      PyErr_SetString( PyExc_ValueError, 
                       _("invalid dict value in evalcontext "
                         "(see gnubg.evalcontext() for an example)") );
      return -1;
    }

    switch( iKey ) {
    case 0:
    case 1:
    case 2:
      /* simple integer */
      if ( ! PyInt_Check( pyValue ) ) {
        /* not an integer */
        PyErr_SetString( PyExc_ValueError, 
                         _("invalid value evalcontext "
                           "(see gnubg.evalcontext() for an example)") );
        return -1;
      }

      i = (int) PyInt_AsLong( pyValue );

      if ( !iKey )
        pec->fCubeful = i ? 1 : 0;
      else if ( iKey == 1 )
        pec->nPlies = ( i < 8 ) ? i : 7;
      else if ( iKey == 2 ) {
      } else 
        pec->fDeterministic = i ? 1 : 0;

      break;

    case 3:
      /* float */
      if( ! PyFloat_Check( pyValue ) ) {
        /* not a float */
        PyErr_SetString( PyExc_ValueError,
                         _("invalid value in evalcontext "
                           "(see gnubg.evalcontext() for an example)" ) );
        return -1;
      }

      pec->rNoise = (float) PyFloat_AsDouble( pyValue );

      break;

    default:
      g_assert_not_reached();

    }

  }

  return 0;

}

static PyObject *
PythonCubeInfo(PyObject* UNUSED(self), PyObject* args) {

  cubeinfo ci;
  int nCube = ms.nCube;
  int fCubeOwner = ms.fCubeOwner;
  int fMove = ms.fMove;
  int nMatchTo = ms.nMatchTo;
  int anScore[ 2 ];
  int fCrawford = ms.fCrawford;
  int fJacoby = ms.fJacoby;
  int fBeavers = ms.cBeavers;
  bgvariation bgv = ms.bgv;
  anScore[ 0 ] = ms.anScore[ 0 ];
  anScore[ 1 ] = ms.anScore[ 1 ];


  if ( ! PyArg_ParseTuple( args, "|iiii(ii)iiii:cubeinfo", 
                           &nCube, &fCubeOwner, &fMove, &nMatchTo, 
                           &anScore[ 0 ], &anScore[ 1 ], &fCrawford, &bgv ) )
    return NULL;

  if ( SetCubeInfo( &ci, nCube, fCubeOwner, fMove, nMatchTo, anScore,
                    fCrawford, fJacoby, fBeavers, bgv ) ) {
    printf( _("error in SetCubeInfo\n") );
    return NULL;
  }

  return CubeInfoToPy( &ci );

}

static PyObject *
PythonPosInfo(PyObject* UNUSED(self), PyObject* args) {

  posinfo pi;
  int fTurn = ms.fTurn;
  int fResigned = ms.fResigned;
  int fDoubled = ms.fDoubled;
  int gs = ms.gs;
  int anDice[ 2 ];
  anDice [ 0 ] = ms.anDice [ 0 ];
  anDice [ 1 ] = ms.anDice [ 1 ];

  if ( ! PyArg_ParseTuple( args, "|iiii(ii):posinfo", 
                           &fTurn, &fResigned, &fDoubled, &gs, 
                           &anDice[ 0 ], &anDice[ 1 ] ) )
    return NULL;

  if ( SetPosInfo( &pi, fTurn, fResigned, fDoubled, gs, anDice ) ) {
    printf( _("error in SetPosInfo\n") );
    return NULL;
  }

  return PosInfoToPy( &pi );

}

static PyObject *
PythonNextTurn( PyObject *UNUSED(self), PyObject *UNUSED(args) ) {

  fNextTurn = TRUE;
  while( fNextTurn ) {
    if (NextTurn( TRUE ) == -1)
      fNextTurn = FALSE;
  }

  return Py_None;

}

static PyObject *
PythonUpdateUI( PyObject *UNUSED(self), PyObject *UNUSED(args) ) {

#if USE_GTK
  if (fX){
    while (gtk_events_pending ())
      gtk_main_iteration ();
  }
#endif

  return Py_None;
}


static PyObject *
PythonEvalContext( PyObject* UNUSED(self), PyObject *args ) {

  evalcontext ec;
  int fCubeful = 0, nPlies = 0, fDeterministic = 1;
  float rNoise = 0.0f;

  if ( ! PyArg_ParseTuple( args, "|iiif",
                           &fCubeful, &nPlies, &fDeterministic,
                           &rNoise ) )
    return NULL;

  ec.fCubeful = fCubeful ? 1 : 0;
  ec.nPlies = ( nPlies < 8 ) ? nPlies : 7;
  ec.fDeterministic = fDeterministic ? 1 : 0;
  ec.rNoise = rNoise;
                           
  return EvalContextToPy( &ec );
}

static PyObject *PythonCommand(PyObject * UNUSED(self), PyObject * args)
{

	char *pch;
	char *sz;
	psighandler sh;

	if (!PyArg_ParseTuple(args, "s:command", &pch))
		return NULL;

	sz = g_strdup(pch);

	PortableSignal(SIGINT, HandleInterrupt, &sh, FALSE);
	HandleCommand(sz, acTop);

	/* this is what the cl interface does. Let's try that. */
	while (fNextTurn)
		NextTurn(TRUE);
	/* before we had 
	 * if( ms.gs != GAME_NONE )
	 * PythonNextTurn(0, 0);
	 * which always run NextTurn(TRUE), and that isn't right */

	outputx();
	g_free(sz);
	PortableSignalRestore(SIGINT, &sh);
	if (fInterrupt) {
		raise(SIGINT);
		fInterrupt = FALSE;
	}

	PythonUpdateUI( Py_None, Py_None );
	Py_INCREF(Py_None);
	return Py_None;
}

static PyObject *PythonParseMove(PyObject * UNUSED(self), PyObject * args)
{
	PyObject *pyMoves;
	PyObject *pyMove;
	PyObject *pyMoveL;
	PyObject *pyMoveR;
	char *pch;
	char *sz;
	int an [8];
	int nummoves;
	int movesidx, moveidx;

	if (!PyArg_ParseTuple(args, "s:moves", &pch))
		return NULL;

	sz = g_strdup(pch);
	for( pch = sz; *pch; pch++ )
		if( *pch == '-' )
			*pch = '/';

	nummoves = ParseMove( sz, an );
	g_free(sz);

	if (nummoves < 0)
		return NULL;

	if ( ! ( pyMoves = PyTuple_New( nummoves ) ) )
		return NULL;

	for ( movesidx = 0; movesidx < nummoves; movesidx++  ) {
		pyMoveL = PyInt_FromLong(an [ movesidx * 2 ]);
		pyMoveR = PyInt_FromLong(an [ movesidx * 2 + 1 ]);

		if ( ! ( pyMove = PyTuple_New( 2 ) ) )
			return NULL;

		moveidx = 0;
		if ( PyTuple_SetItem( pyMove, moveidx, pyMoveL ) < 0 )
			return NULL;
		if ( PyTuple_SetItem( pyMove, moveidx+1, pyMoveR ) < 0 )
			return NULL;

	    if ( PyTuple_SetItem( pyMoves, movesidx, pyMove ) < 0 )
			return NULL;
	}

	return pyMoves;
}


static PyObject *
PythonBoard( PyObject* UNUSED(self), PyObject *args ) {

  if ( ! PyArg_ParseTuple( args, ":board" ) )
    return NULL;

  if ( ms.gs == GAME_NONE ) {
    /* no board available */
    Py_INCREF(Py_None);
    return Py_None;
  }

  return BoardToPy( msBoard() );
}

static PyObject *
PythonLuckRating( PyObject* UNUSED(self), PyObject *args ) {

  float r;
  if ( ! PyArg_ParseTuple( args, "f", &r ) )
    return NULL;

  return PyInt_FromLong( getLuckRating( r ) );

}

static PyObject *
PythonErrorRating( PyObject* UNUSED(self), PyObject *args ) {

  float r;
  if ( ! PyArg_ParseTuple( args, "f", &r ) )
    return NULL;

  return PyInt_FromLong( GetRating( r ) );

}

/* Convert Move tuple to a string 
   A tuple can be expressed in two forms.
   form returned by findbestmove ie. (21,18,11,5)
   form returned by parsemove ie. ((21,18),(11,5))

   returns a string representing the move tuple.
*/

static PyObject *
PythonMoveTuple2String( PyObject* UNUSED(self), PyObject *args ) {

  PyObject *pyBoard = NULL;
  PyObject *pyMove = NULL;

  char szMove[32];
  unsigned int anMove [ 8 ];
  TanBoard anBoard;

  memset( anBoard, 0, sizeof(TanBoard) );
  memset( anMove, -1, sizeof(anMove) );

  if ( ! PyArg_ParseTuple( args, "|OO", 
                           &pyMove, &pyBoard ) ) 
    return NULL;

  if ( !pyMove || !pyBoard ) {
    PyErr_SetString( PyExc_TypeError, 
                     _("requires 2 arguments (MoveTuple, Board). "
                       "(see gnubg.findbestmove() and gnubg.board() for examples)") );
	 return NULL;
  }

  if ( !PyToMove( pyMove, anMove ) ) {
    PyErr_SetString( PyExc_StandardError, 
                     _("Invalid move tuple as argument") );
    return NULL;
  }

  if ( pyBoard && !PyToBoard( pyBoard, anBoard ) ) {
    PyErr_SetString( PyExc_StandardError, 
                     _("Invalid board as argument ") );
    return NULL;
  }

  szMove[0] = '\0';
  FormatMove( szMove, (ConstTanBoard)anBoard, anMove );

  return PyString_FromString(szMove);
}

STACKALIGN static PyObject *
PythonEvaluate( PyObject* UNUSED(self), PyObject *args ) {

  PyObject *pyBoard = NULL;
  PyObject *pyCubeInfo = NULL;
  PyObject *pyEvalContext = NULL;

  TanBoard anBoard;
  cubeinfo ci;
  evalcontext ec = { 0, 0, 0, 1, 0.0f };
  float arOutput[ 7 ];

  memcpy( anBoard, msBoard(), sizeof(TanBoard) );
  GetMatchStateCubeInfo( &ci, &ms );

  if ( ! PyArg_ParseTuple( args, "|OOO", 
                           &pyBoard, &pyCubeInfo, &pyEvalContext ) )
    return NULL;

  if ( pyBoard && !PyToBoard( pyBoard, anBoard ) )
    return NULL;

  if ( pyCubeInfo && PyToCubeInfo( pyCubeInfo, &ci ) )
    return NULL;

  if ( pyEvalContext && PyToEvalContext( pyEvalContext, &ec ) )
    return NULL;

  if ( GeneralEvaluationE( arOutput, (ConstTanBoard)anBoard, &ci, &ec ) ) {
    PyErr_SetString( PyExc_StandardError, 
                     _("interupted/errno in GeneralEvaluateE") );
    return NULL;
  }

  {
    PyObject* p = PyTuple_New(6);
    int k;
    for(k = 0; k < 6; ++k) {
      PyTuple_SET_ITEM(p, k, PyFloat_FromDouble(arOutput[k]));
    }
    return p;
  }
}

STACKALIGN static PyObject *
PythonEvaluateCubeful( PyObject* UNUSED(self), PyObject *args ) {

  PyObject *pyBoard = NULL;
  PyObject *pyCubeInfo = NULL;
  PyObject *pyEvalContext = NULL;

  TanBoard anBoard;
  float aarOutput[ 2 ][ NUM_ROLLOUT_OUTPUTS ], arCube[ NUM_CUBEFUL_OUTPUTS ];
  cubeinfo ci;
  evalcontext ec = { 0, 0, 0, 1, 0.0f };
  int cp;

  memcpy( anBoard, msBoard(), sizeof(TanBoard) );
  
  GetMatchStateCubeInfo( &ci, &ms );

  if ( ! PyArg_ParseTuple( args, "|OOO", 
                           &pyBoard, &pyCubeInfo, &pyEvalContext ) )
    return NULL;

  if ( pyBoard && !PyToBoard( pyBoard, anBoard ) )
    return NULL;

  if ( pyCubeInfo && PyToCubeInfo( pyCubeInfo, &ci ) )
    return NULL;

  if ( pyEvalContext && PyToEvalContext( pyEvalContext, &ec ) )
    return NULL;

  if ( GeneralCubeDecisionE ( aarOutput, (ConstTanBoard)anBoard, &ci, &ec, 0 ) < 0 ){
    PyErr_SetString( PyExc_StandardError, 
                     _("interupted/errno in GeneralCubeDecisionE") );
    return NULL;
  }

  cp = FindCubeDecision ( arCube,  aarOutput, &ci );

  {
    PyObject* p = PyTuple_New( NUM_CUBEFUL_OUTPUTS + 2 );
    int k;
    for(k = 0; k < NUM_CUBEFUL_OUTPUTS; ++k) {
      PyTuple_SET_ITEM(p, k, PyFloat_FromDouble(arCube[k]));
    }
	PyTuple_SET_ITEM(p, NUM_CUBEFUL_OUTPUTS, PyInt_FromLong(cp));
	PyTuple_SET_ITEM(p, NUM_CUBEFUL_OUTPUTS+1, PyString_FromString(GetCubeRecommendation(cp)));

    return p;
  }
}

STACKALIGN static PyObject *
PythonFindBestMove( PyObject* UNUSED(self), PyObject *args ) {

  PyObject *pyDice = NULL;
  PyObject *pyBoard = NULL;
  PyObject *pyCubeInfo = NULL;
  PyObject *pyEvalContext = NULL;

  TanBoard anBoard;
  int anDice[ 2 ];
  int anMove[ 8 ];
  cubeinfo ci;
  evalcontext ec = { 0, 0, 0, 1, 0.0f };

  memcpy( anBoard, msBoard(), sizeof(TanBoard) );
  memcpy( anDice, ms.anDice, sizeof anDice );
  GetMatchStateCubeInfo( &ci, &ms );
  if ( ! PyArg_ParseTuple( args, "|OOOO", 
                           &pyBoard, &pyCubeInfo, &pyEvalContext, &pyDice ) )
    return NULL;

  if ( pyDice && !PyToDice( pyDice, anDice ) )
    return NULL;

  if (anDice[0] == 0)
  {
	  printf(_("What? No dice?\n"));
	  return NULL;
  }

  if ( pyBoard && !PyToBoard( pyBoard, anBoard ) )
    return NULL;

  if ( pyCubeInfo && PyToCubeInfo( pyCubeInfo, &ci ) )
    return NULL;

  if ( pyEvalContext && PyToEvalContext( pyEvalContext, &ec ) )
    return NULL;

  /* 
   * FIXME: The function will use the eval movefilter. The function
   * should take a movefilter as an argument.
   */
  
  if( FindBestMove( anMove, anDice[ 0 ], anDice[ 1 ],
                        anBoard, &ci, &ec, aamfEval ) < 0 )
        return NULL;
  {
    PyObject* p = PyTuple_New( 8 );
    int k;
    for(k = 0; k < 8; ++k) {
      PyTuple_SET_ITEM(p, k, Py_BuildValue( "i", anMove[k]+1));
    }
    return p;
  }

}

static PyObject *
METRow( float ar[ MAXSCORE ], const int n ) {

  int i;
  PyObject *pyList;
  PyObject *pyf;

  if ( ! ( pyList = PyList_New( n ) ) )
    return NULL;

  for ( i = 0; i < n; ++i ) {

    if ( ! ( pyf = PyFloat_FromDouble( ar[ i ] ) ) )
      return NULL;

    if ( PyList_SetItem( pyList, i, pyf ) < 0 )
      return NULL;

  }

  return pyList;

}

static PyObject *
METPre( float aar[ MAXSCORE ][ MAXSCORE ], const int n ) {

  int i;
  PyObject *pyList;
  PyObject *pyRow;

  if ( ! ( pyList = PyList_New( n ) ) )
    return NULL;

  for ( i = 0; i < n; ++i ) {
    if ( ! ( pyRow = METRow( aar[ i ], n ) ) )
      return NULL;

    if ( PyList_SetItem( pyList, i, pyRow ) < 0 )
      return NULL;

  }

  return pyList;
}


static PyObject *
PythonDiceRolls( PyObject* UNUSED(self), PyObject *args ) {
  PyObject *pyDiceRolls;
  PyObject *pyDiceRoll;
  PyObject *pyDie1;
  PyObject *pyDie2;
  long n;
  unsigned int anDice[2];
  int dieidx, rollsidx;

  if ( ! PyArg_ParseTuple( args, "l:dicerolls", &n ) )
    return NULL;

  if ( n <= 0 ) {
    PyErr_SetString( PyExc_ValueError, _("number of rolls must be greater than 0" ) );
    return NULL;
  }

  if ( ! ( pyDiceRolls = PyTuple_New( n ) ) )
    return NULL;

  rollsidx = 0;
  while (n-- > 0) {
    RollDice( anDice, &rngCurrent, rngctxCurrent );

    pyDie1 = PyInt_FromLong(anDice[0]);
    pyDie2 = PyInt_FromLong(anDice[1]);

    if ( ! ( pyDiceRoll = PyTuple_New( 2 ) ) )
      return NULL;

    dieidx = 0;
    if ( PyTuple_SetItem( pyDiceRoll, dieidx, pyDie1 ) < 0 )
      return NULL;
    if ( PyTuple_SetItem( pyDiceRoll, dieidx+1, pyDie2 ) < 0 )
      return NULL;

    if ( PyTuple_SetItem( pyDiceRolls, rollsidx++, pyDiceRoll ) < 0 )
      return NULL;
  }
  return pyDiceRolls;
}

static PyObject *
PythonMET( PyObject* UNUSED(self), PyObject *args ) {

  int n = ms.nMatchTo ? ms.nMatchTo : MAXSCORE;
  int i;
  PyObject *pyMET;
  PyObject *pyList;

  if ( ! PyArg_ParseTuple( args, "|i:matchequiytable", &n ) )
    return NULL;

  if ( n < 0 || n > MAXSCORE ) {
    PyErr_SetString( PyExc_ValueError, _("invalid matchlength" ) );
    return NULL;
  }

  if ( ! ( pyMET = PyList_New( 3 ) ) )
    return NULL;

  /* pre-Crawford scores */

  if ( ! ( pyList = METPre( aafMET, n ) ) )
    return NULL;

  if ( PyList_SetItem( pyMET, 0, pyList ) < 0 )
    return NULL;

  /* post-Crawford scores */

  for ( i = 0; i < 2; ++i ) {

    if ( ! ( pyList = METRow( aafMETPostCrawford[ i ], n ) ) )
      return NULL;

    if ( PyList_SetItem( pyMET, i +1, pyList ) < 0 )
      return NULL;

  }

  return pyMET;

}


STACKALIGN static PyObject *
PythonEq2mwc( PyObject* UNUSED(self), PyObject *args ) {

  PyObject *pyCubeInfo = NULL;
  float r = 0.0f;
  cubeinfo ci;

  if ( ! PyArg_ParseTuple( args, "|fO:eq2mwc", &r, &pyCubeInfo ) )
    return NULL;

  GetMatchStateCubeInfo( &ci, &ms );

  if ( pyCubeInfo && PyToCubeInfo( pyCubeInfo, &ci ) )
    return NULL;

  return PyFloat_FromDouble( eq2mwc( r, &ci ) );

}

static PyObject *PythonMatchChecksum( PyObject* UNUSED(self), PyObject *UNUSED(args) )
{
	return PyString_FromString(GetMatchCheckSum());
}

STACKALIGN static PyObject *
PythonMwc2eq( PyObject* UNUSED(self), PyObject *args ) {

  PyObject *pyCubeInfo = NULL;
  float r = 0.0f;
  cubeinfo ci;

  if ( ! PyArg_ParseTuple( args, "|fO:mwc2eq", &r, &pyCubeInfo ) )
    return NULL;

  GetMatchStateCubeInfo( &ci, &ms );

  if ( pyCubeInfo && PyToCubeInfo( pyCubeInfo, &ci ) )
    return NULL;

  return PyFloat_FromDouble( mwc2eq( r, &ci ) );
}

static PyObject *
PythonPositionID( PyObject* UNUSED(self), PyObject *args ) {

  PyObject *pyBoard = NULL;
  TanBoard anBoard;

  memcpy( anBoard, msBoard(), sizeof(TanBoard) );

  if ( ! PyArg_ParseTuple( args, "|O:positionid", &pyBoard ) )
    return NULL;

  if ( pyBoard && !PyToBoard( pyBoard, anBoard ) )
    return NULL;

  return PyString_FromString( PositionID( (ConstTanBoard)anBoard ) );

}

static PyObject *
PythonGnubgID( PyObject* UNUSED(self), PyObject *args ) {
  char *szGnubgID = NULL;
  char *szPosID = NULL;
  char *szMatchID = NULL;
  PyObject *pyRetVal = NULL;
  PyObject *pyCubeInfo = NULL;
  PyObject *pyPosInfo = NULL;
  PyObject *pyBoard = NULL;
  cubeinfo ci;
  posinfo pi;
  TanBoard anBoard;
  
  memcpy( anBoard, msBoard(), sizeof(TanBoard) );
  pi.anDice[0] = ms.anDice[0];
  pi.anDice[1] = ms.anDice[1];
  pi.fTurn = ms.fTurn;
  pi.fResigned = ms.fResigned;
  pi.fDoubled = ms.fDoubled; 
  pi.gs = ms.gs;
  ci.fMove = ms.fMove;
  ci.fCubeOwner = ms.fCubeOwner;
  ci.fCrawford = ms.fCrawford;
  ci.nMatchTo = ms.nMatchTo;
  ci.anScore[0] = ms.anScore[0];
  ci.anScore[1] = ms.anScore[1];
  ci.nCube = ms.nCube;
#if USE_EXTENDEDMATCHID 
  ci.fJacoby = ms.fJacoby;
#endif

  if ( ! PyArg_ParseTuple( args, "|OOO:gnubgid", &pyBoard, &pyCubeInfo, &pyPosInfo ) )
    return NULL;

  if ( pyBoard && (!pyPosInfo || !pyCubeInfo)) {
    PyErr_SetString( PyExc_TypeError, 
                     _("requires 0 or exactly 3 arguments (Board, Cube-Info dict, Pos-Info dict). "
                       "(see gnubg.board(), gnubg.cubeinfo(), gnubg.posinfo() for examples)") );
	 return NULL;
  }

  if ( pyBoard && !PyToBoard( pyBoard, anBoard ) )
    return NULL;

  if( !pyBoard && ms.gs == GAME_NONE ) {
    PyErr_SetString( PyExc_ValueError,
                     _("no current position available") );
    return NULL;
  }
  
  if ( pyCubeInfo && PyToCubeInfo( pyCubeInfo, &ci ) )
    return NULL;

  if ( pyPosInfo && PyToPosInfo( pyPosInfo, &pi ) )
    return NULL;

  szPosID = g_strdup (PositionID( (ConstTanBoard)anBoard ) );
  szMatchID = g_strdup (MatchID ( (unsigned int *)pi.anDice, pi.fTurn, pi.fResigned, 
			pi.fDoubled, ci.fMove, ci.fCubeOwner, ci.fCrawford, ci.nMatchTo, 
#if USE_EXTENDEDMATCHID 
                        ci.anScore, ci.nCube, ci.fJacoby, pi.gs ) );
#else                        
                        ci.anScore, ci.nCube, pi.gs ) );
#endif
                     
  szGnubgID = g_strjoin (":", szPosID, szMatchID, NULL);
  pyRetVal = PyString_FromString( szGnubgID );

  g_free (szPosID);
  g_free (szMatchID);
  g_free (szGnubgID);

  return (pyRetVal);  
}

static PyObject *
PythonMatchID( PyObject* UNUSED(self), PyObject *args ) {

  PyObject *pyCubeInfo = NULL;
  PyObject *pyPosInfo = NULL;
  cubeinfo ci;
  posinfo pi;

  if ( ! PyArg_ParseTuple( args, "|OO:matchid", &pyCubeInfo , &pyPosInfo) )
    return NULL;

  if( !pyCubeInfo && ms.gs == GAME_NONE ) {
    PyErr_SetString( PyExc_ValueError,
                     _("no current position available") );
    return NULL;
  }

  if ( pyCubeInfo && PyToCubeInfo( pyCubeInfo, &ci ) )
    return NULL;

  if ( pyPosInfo && PyToPosInfo( pyPosInfo, &pi ) )
    return NULL;

  if ( pyCubeInfo && !pyPosInfo) {
    PyErr_SetString( PyExc_TypeError, 
                     _("a cube-info argument requires a pos-info dictionary as an argument "
                       "(see gnubg.matchid() for an example)") );
	 return NULL;
  }
  
  if (!pyCubeInfo)
    return PyString_FromString( MatchIDFromMatchState ( &ms ) );
  else
    return PyString_FromString( MatchID ( (unsigned int *)pi.anDice, pi.fTurn, pi.fResigned, 
				pi.fDoubled, ci.fMove, ci.fCubeOwner, ci.fCrawford, ci.nMatchTo,
#if USE_EXTENDEDMATCHID 
				ci.anScore, ci.nCube, ci.fJacoby, pi.gs ) );
#else
				ci.anScore, ci.nCube, pi.gs ) );
#endif
}

static PyObject *
PythonPositionFromID( PyObject* UNUSED(self), PyObject *args )
{
  char* sz = NULL;
  TanBoard anBoard;

  if( ! PyArg_ParseTuple( args, "|s:positionfromid", &sz ) ) {
    return NULL;
  }

  if( sz ) {
    if( ! PositionFromID(anBoard, sz) ) {
      PyErr_SetString( PyExc_ValueError, 
		       _("invalid positionid") );
      return NULL;
    }
  } else {
    memcpy( anBoard, msBoard(), sizeof(TanBoard) );
  }

  return BoardToPy( (ConstTanBoard)anBoard );
}


static PyObject *
PythonPositionKey( PyObject* UNUSED(self), PyObject *args ) {

  PyObject *pyBoard = NULL;
  TanBoard anBoard;
  oldpositionkey key;

  memcpy( anBoard, msBoard(), sizeof(TanBoard) );

  if ( ! PyArg_ParseTuple( args, "|O!:positionkey", &PyList_Type, &pyBoard ) )
    return NULL;

  if ( pyBoard && !PyToBoard( pyBoard, anBoard ) )
    return NULL;

  oldPositionKey( (ConstTanBoard)anBoard, &key );

  {
    PyObject* a = PyTuple_New(10);
    int i;
    for(i = 0; i < 10; ++i) {
      PyTuple_SET_ITEM(a, i, PyInt_FromLong(key.auch[i]));
    }
    return a;
  }
}

static PyObject *
PythonPositionFromKey( PyObject* UNUSED(self), PyObject *args ) {

  TanBoard anBoard;
  int i;
  PyObject *pyKey = NULL;
  PyObject *py;
  oldpositionkey key;

  if( ! PyArg_ParseTuple( args, "|O!:positionfromkey", &PyList_Type, &pyKey ) )
    return NULL;

  if ( pyKey ) {

    for ( i = 0; i < 10; ++i ) {
      if ( ! ( py = PyList_GetItem( pyKey, i ) ) )
        return NULL;
      
      key.auch[ i ] = (unsigned char) PyInt_AsLong( py );
    }

  }
  else {

    for ( i = 0; i < 10; ++i )
      key.auch[ i ] = 0;

  }

  oldPositionFromKey( anBoard, &key );

  return BoardToPy( (ConstTanBoard)anBoard );
}


static PyObject *
PythonPositionBearoff( PyObject* UNUSED(self), PyObject *args )
{
  PyObject *pyBoard = NULL;
  int nChequers = 15;
  int nPoints = 6;
  TanBoard anBoard;

  memcpy( anBoard, msBoard(), sizeof(anBoard) );

  if ( ! PyArg_ParseTuple( args, "|Oii:positionbearoff", 
                           &pyBoard, &nPoints, &nChequers ) )
    return NULL;

  if ( pyBoard && !PyToBoard1( pyBoard, anBoard[0] ) )
    return NULL;

  return PyInt_FromLong( PositionBearoff( anBoard[0], nPoints, nChequers ) );
}

static PyObject *PythonPositionFromBearoff( PyObject* UNUSED(self), PyObject *args )
{
  TanBoard anBoard;
  int iPos = 0;
  int nChequers = 15;
  int nPoints = 6;
  int n;

  if ( ! PyArg_ParseTuple( args, "|iii:positionfrombearoff", 
                           &iPos, nChequers, &nPoints  ) )
    return NULL;

  if ( nChequers < 1 || nChequers > 15 || nPoints < 1 || nPoints > 25 ) {
    PyErr_SetString( PyExc_ValueError,
                     _("invalid number of chequers or points") );
    return NULL;
  }

  n = Combination( nChequers + nPoints, nPoints );

  if ( iPos < 0 || iPos >= n ) {
    PyErr_SetString( PyExc_ValueError,
                     _("invalid position number") );
    return NULL;
  }

  memset( anBoard, 0, sizeof(anBoard) );
  PositionFromBearoff( anBoard[0], iPos, nPoints, nChequers );

  return Board1ToPy( anBoard[0] );
}

#if PY_MAJOR_VERSION < 2 || (PY_MAJOR_VERSION == 2 && PY_MINOR_VERSION < 3)

/* Bool introduced in 2.3 */
#define PyBool_FromLong PyInt_FromLong

/* Fix incorrect prototype in early python */
#define CHARP_HACK (char*)
#else
#define CHARP_HACK
#endif

static void DictSetItemSteal(PyObject * dict, const char *key, PyObject * val)
{
	int const s = PyDict_SetItemString(dict, CHARP_HACK key, val);
	{
		g_assert(s == 0);
	}
	Py_DECREF(val);
}

typedef struct {
  const evalcontext*    ec;
  const rolloutcontext* rc;
} PyMatchState;

static PyObject*
diffContext(const evalcontext* c, PyMatchState* ms)
{
  const evalcontext* s = ms->ec;
  
  if( !s ) {
    ms->ec = c;
    return 0;
  }

  if( cmp_evalcontext(s, c) == 0 ) {
    return 0;
  }

  {
    PyObject* context = PyDict_New();

    if( c->fCubeful != s->fCubeful ) {
      DictSetItemSteal(context, "cubeful", PyInt_FromLong(c->fCubeful));
    }

    if( c->nPlies != s->nPlies ) {
      DictSetItemSteal(context, "plies", PyInt_FromLong(c->nPlies));
    }

    if( c->fDeterministic != s->fDeterministic ) {
      DictSetItemSteal(context, "deterministic",
		       PyInt_FromLong(c->fDeterministic));
    }
    
    if( c->rNoise != s->rNoise ) {
      DictSetItemSteal(context, "noise", PyFloat_FromDouble(c->rNoise));
    }
    
    return context;
  }
}

static PyObject*
diffRolloutContext(const rolloutcontext* c, PyMatchState* ms)
{
  const rolloutcontext* s = ms->rc;
  
  if( !s ) {
    ms->rc = c;
    return 0;
  }

  {
    PyObject* context = PyDict_New();

    if( c->fCubeful != s->fCubeful ) {
      DictSetItemSteal(context, "cubeful", PyInt_FromLong(c->fCubeful));
    }

    if( c->fVarRedn != s->fVarRedn ) {
      DictSetItemSteal(context, "variance-reduction",
		       PyInt_FromLong(c->fVarRedn));
    }

    if( c->fInitial != s->fInitial ) {
      DictSetItemSteal(context, "initial-position",
		       PyInt_FromLong(c->fInitial));
    }

    if( c->fRotate != s->fRotate ) {
      DictSetItemSteal(context, "quasi-random-dice",
		       PyInt_FromLong(c->fRotate));
    }

    if( c->fLateEvals != s->fLateEvals ) {
      DictSetItemSteal(context, "late-eval",
		       PyInt_FromLong(c->fLateEvals));
    }
    
    if( c->fDoTruncate != s->fDoTruncate ) {
      DictSetItemSteal(context, "truncated-rollouts",
		       PyInt_FromLong(c->fDoTruncate));
    }
    
    if( c->nTruncate != s->nTruncate ) {
      DictSetItemSteal(context, "n-truncation", PyInt_FromLong(c->nTruncate));
    }

    if( c->fTruncBearoff2 != s->fTruncBearoff2 ) {
      DictSetItemSteal(context, "truncate-bearoff2",
		       PyInt_FromLong(c->fTruncBearoff2));
    }
    
    if( c->fTruncBearoffOS != s->fTruncBearoffOS ) {
      DictSetItemSteal(context, "truncate-bearoffOS",
		       PyInt_FromLong(c->fTruncBearoffOS));
    }
    
    if( c->fStopOnSTD != s->fStopOnSTD ) {
      DictSetItemSteal(context, "stop-on-std",
		       PyInt_FromLong(c->fStopOnSTD));
    }
    
    if( c->nTrials != s->nTrials ) {
      DictSetItemSteal(context, "trials", PyInt_FromLong(c->nTrials));
    }

    if( c->nSeed != s->nSeed ) {
      DictSetItemSteal(context, "seed", PyInt_FromLong(c->nSeed));
    }
    
    if( c->nMinimumGames != s->nMinimumGames ) {
      DictSetItemSteal(context, "minimum-games",
		       PyInt_FromLong(c->nMinimumGames));
    }
    
    if( PyDict_Size(context) == 0 ) {
      Py_DECREF(context);
      context = 0;
    }
    
    return context;
  }
}

static PyObject*
RolloutContextToPy(const rolloutcontext* rc)
{
  PyObject* dict =
    Py_BuildValue("{s:i,s:i,s:i,s:i,s:i,s:i,s:i,s:i,s:i,s:i,"
		  "s:i,s:i,s:i}",
		  "cubeful", rc->fCubeful,
		  "variance-reduction", rc->fVarRedn,
		  "initial-position", rc->fInitial,
		  "quasi-random-dice", rc->fRotate,
		  "late-eval", rc->fLateEvals,
		  "truncated-rollouts", rc->fDoTruncate,
		  "n-truncation", rc->nTruncate,
		  "truncate-bearoff2", rc->fTruncBearoff2,
		  "truncate-bearoffOS", rc->fTruncBearoffOS,
		  "stop-on-std", rc->fStopOnSTD,
		  "trials", rc->nTrials,
		  "seed", rc->nSeed,
		  "minimum-games", rc->nMinimumGames);
  return dict;
}

  
static PyObject*
PyMove(const int move[8])
{
  /* Lazy. Allocate full move, resize later */
	    
  PyObject* moveTuple = PyTuple_New(4);
  int i;
	    
  for(i = 0; i < 4; ++i) {
    if( move[2*i] < 0 ) {
      break;
    }
    {
      PyObject* c = Py_BuildValue("(ii)", move[2*i]+1, move[2*i+1]+1);
      
      PyTuple_SET_ITEM(moveTuple, i, c);
    }
  }

  if( i < 4 ) {
    int s = _PyTuple_Resize(&moveTuple, i);                  g_assert(s != -1);
  }
  
  return moveTuple;
}

static const char*
luckString(lucktype const lt, int const ignoreNone)
{
  switch( lt ) {
    case LUCK_VERYBAD: return "verybad";
    case LUCK_BAD: return "bad";
    case LUCK_NONE: return ignoreNone ? 0 : "unmarked";
    case LUCK_GOOD: return "good";
    case LUCK_VERYGOOD: return "verygood";
  }
  g_assert_not_reached();
  return 0;
}
    
static const char*
skillString(skilltype const st, int const ignoreNone)
{
  switch( st ) {
    case SKILL_VERYBAD:     return "very bad";
    case SKILL_BAD:         return "bad";
    case SKILL_DOUBTFUL:    return "doubtful";
    case SKILL_NONE:        return ignoreNone ? 0 : "unmarked";
  }
  g_assert_not_reached();
  return 0;
}

static void
addSkill(PyObject* dict, skilltype const st, const char* name)
{
  if( dict ) {
    const char* s = skillString(st, 1);
    if( s ) {
      if( ! name ) name = "skill";
      
      DictSetItemSteal(dict, name, PyString_FromString(s));
    }
  }
}

static void
addLuck(PyObject* dict, float const rLuck, lucktype const lt)
{
  if( dict ) {
    const char* l = luckString(lt, 1);

    if( rLuck != ERR_VAL ) {
      DictSetItemSteal(dict, "luck-value", PyFloat_FromDouble(rLuck));
    }

    if( l ) {
      DictSetItemSteal(dict, "luck", PyString_FromString(l));
    }
  }
}

STACKALIGN static PyObject*
PyMoveAnalysis(const movelist* pml, PyMatchState* ms)
{
  unsigned int i;
  unsigned int n = 0;
  
  for(i = 0; i < pml->cMoves; i++) {
    const move* mi = &pml->amMoves[i] ;
	
    switch( mi->esMove.et ) {
      case EVAL_EVAL:
      case EVAL_ROLLOUT:
      {
	++n;
	break;
      }
      
      default: break;
    }
  }

  if( !n ) {
    return 0;
  }

  {
    PyObject* l = PyTuple_New(n);
    n = 0;
    
    for(i = 0; i < pml->cMoves; i++) {
      const move* mi = &pml->amMoves[i] ;
      PyObject* v = 0;;
      
      switch( mi->esMove.et ) {
        case EVAL_EVAL:
	{
	  PyObject* m = PyMove(mi->anMove);
	  v = Py_BuildValue("{s:s,s:O,s:(fffff),s:f}",
			    "type", "eval",
			    "move", m,
			    "probs", mi->arEvalMove[0], mi->arEvalMove[1],
			    mi->arEvalMove[2], mi->arEvalMove[3],
			    mi->arEvalMove[4],
			    "score", mi->rScore);
	  Py_DECREF(m);

	  {
	    PyObject* c = diffContext(&mi->esMove.ec, ms);
	    if( c ) {
	      DictSetItemSteal(v, "evalcontext", c);
	    }
	  }

	  break;
	}    
        case EVAL_ROLLOUT:
	{
	  PyObject* m = PyMove(mi->anMove);
	  const evalsetup* pes = &mi->esMove;
	  const float* p =  mi->arEvalMove;
	  const float* s =  mi->arEvalStdDev;
	
	  v = Py_BuildValue("{s:s,s:O,s:i,s:(fffff),s:f,s:f"
			    ",s:(fffff),s:f,s:f}",
			    "type", "rollout",
			    "move", m,
			    "trials", pes->rc.nGamesDone,

			    "probs", p[0], p[1], p[2], p[3], p[4],
			    "match-eq",   p[OUTPUT_EQUITY],
			    "cubeful-eq", p[OUTPUT_CUBEFUL_EQUITY],
			    "probs-std", s[0], s[1], s[2], s[3], s[4],
			    "match-eq-std",   s[OUTPUT_EQUITY],
			    "cubeful-eq-std", s[OUTPUT_CUBEFUL_EQUITY]);
	  Py_DECREF(m);

	  {
	    PyObject* c = diffRolloutContext(&pes->rc, ms);
	    if( c ) {
	      DictSetItemSteal(v, "rollout-context", c);
	    }
	  }
	  
	  break;
	}

        case EVAL_NONE: break;

        default: g_assert( 0 );
      }

      if( v ) {
	PyTuple_SET_ITEM(l, n++, v);
      }
    }

    return l;
  }
}

STACKALIGN static PyObject*
PyDoubleAnalysis(const evalsetup* pes,
		 float aarOutput[][ NUM_ROLLOUT_OUTPUTS ],
		 float aarStdDev[][ NUM_ROLLOUT_OUTPUTS ],
		 PyMatchState* ms,
		 int const verbose)
{
  PyObject* dict = 0;
  
  switch( pes->et ) {
    case EVAL_EVAL:
    {
      const float* p = aarOutput[0];
      
      dict =
	Py_BuildValue("{s:s,s:(fffff),s:f,s:f}",
		      "type", "eval",
		      "probs", p[0], p[1], p[2], p[3], p[4],
		      "nd-cubeful-eq", p[OUTPUT_CUBEFUL_EQUITY],
		      "dt-cubeful-eq", aarOutput[1][OUTPUT_CUBEFUL_EQUITY]);
      
      if( verbose ) {
	DictSetItemSteal(dict, "nd-match-eq",
			 PyFloat_FromDouble(p[OUTPUT_EQUITY]));
	DictSetItemSteal(dict,"dt-match-eq",
			 PyFloat_FromDouble(aarOutput[1][OUTPUT_EQUITY]));
      }

      {
	PyObject* c = diffContext(&pes->ec, ms);
	if( c ) {
	  DictSetItemSteal(dict, "eval-context", c);
	}
      }
      break;
    }
    case EVAL_ROLLOUT:
    {
      const float* nd  = aarOutput[0];
      const float* nds = aarStdDev[0];
      const float* dt  = aarOutput[1];
      const float* dts = aarStdDev[1];
      
      dict =
	Py_BuildValue("{s:s,s:i,"
		      "s:(fffff),s:f,s:f,"
		      "s:(fffff),s:f,s:f,"
		      "s:(fffff),s:f,s:f,"
		      "s:(fffff),s:f,s:f}",

		      "type", "rollout",
		      "trials", pes->rc.nGamesDone,

		      "nd-probs", nd[0], nd[1], nd[2], nd[3], nd[4],
		      "nd-match-eq",   nd[OUTPUT_EQUITY],
		      "nd-cubeful-eq", nd[OUTPUT_CUBEFUL_EQUITY],
				  
		      "nd-probs-std", nds[0], nds[1], nds[2], nds[3], nds[4],
		      "nd-match-eq-std",   nds[OUTPUT_EQUITY],
		      "nd-cubeful-eq-std", nds[OUTPUT_CUBEFUL_EQUITY],

		      "dt-probs", dt[0], dt[1], dt[2], dt[3], dt[4],
		      "dt-match-eq",   dt[OUTPUT_EQUITY],
		      "dt-cubeful-eq", dt[OUTPUT_CUBEFUL_EQUITY],
		      
		      "dt-probs-std", dts[0], dts[1], dts[2], dts[3], dts[4],
		      "dt-match-eq-std",   dts[OUTPUT_EQUITY],
		      "dt-cubeful-eq-std", dts[OUTPUT_CUBEFUL_EQUITY]);
      
      {
	PyObject* c = diffRolloutContext(&pes->rc, ms);
	if( c ) {
	  DictSetItemSteal(dict, "rollout-context", c);
	}
      }
      break;
    }
    default:
      g_assert( 0 );
  }

  return dict;
}

static PyObject*
PyGameStats(const statcontext* sc, const int fIsMatch, const int nMatchTo)
{
  PyObject* p[2];
  float aaaar[ 3 ][ 2 ][ 2 ][ 2 ];

  if( ! (sc->fMoves || sc->fCube || sc->fDice) ) {
    return 0;
  }

  getMWCFromError( sc, aaaar );

  p[0] = PyDict_New();
  p[1] = PyDict_New();
  
  if( sc->fMoves ) {
    int side;
    for(side = 0; side < 2; ++side) {
      
      PyObject* d =
	Py_BuildValue("{s:i,s:i,s:f,s:f}",
		      "unforced-moves", sc->anUnforcedMoves[side],
		      "total-moves", sc->anTotalMoves[side],
		      "error-skill", sc->arErrorCheckerplay[side][0],
		      "error-cost", sc->arErrorCheckerplay[side][1]);
      PyObject* m = PyDict_New();

      {
	skilltype st;
	for( st = SKILL_VERYBAD; st <= SKILL_NONE; st++ ) {
	  DictSetItemSteal(m, skillString(st, 0),
			   PyInt_FromLong(sc->anMoves[side][st]));
	}
      }

      DictSetItemSteal(d, "marked", m);

      DictSetItemSteal(p[side], "moves", d);
    }
  }
  
  if( sc->fCube ) {
    int side;
    for(side = 0; side < 2; ++side) {
      float errorskill = 
         aaaar[ CUBEDECISION ][ PERMOVE ][ side ][ NORMALISED ];
      float errorcost = 
         aaaar[ CUBEDECISION ][ PERMOVE ][ side ][ UNNORMALISED ];

      PyObject* d =
	Py_BuildValue("{s:i,s:i,s:i,s:i,s:i,s:i,s:i,s:i,s:i,s:i,s:i"
		      "s:f,s:f,s:f,s:f,s:f,s:f,s:f,s:f,s:f,s:f,s:f,s:f,"
                      "s:f,s:f}",
		      "total-cube",sc->anTotalCube[side],
                      "close-cube", sc->anCloseCube[side],
		      "n-doubles",sc->anDouble[side],
		      "n-takes", sc->anTake[side],
		      "n-drops", sc->anPass[side],

		      "missed-double-below-cp",sc->anCubeMissedDoubleDP[side],
		      "missed-double-above-cp", sc->anCubeMissedDoubleTG[side],
		      "wrong-double-below-dp", sc->anCubeWrongDoubleDP[side],
		      "wrong-double-above-tg",sc->anCubeWrongDoubleTG[side],
		      "wrong-take", sc->anCubeWrongTake[side],
		      "wrong-drop", sc->anCubeWrongPass[side],

		      "err-missed-double-below-cp-skill",
		      sc->arErrorMissedDoubleDP[side][0],
		      "err-missed-double-below-cp-cost",
		      sc->arErrorMissedDoubleDP[side][1],

		      "err-missed-double-above-cp-skill",
		      sc->arErrorMissedDoubleTG[side][ 0 ],
		      "err-missed-double-above-cp-cost",
		      sc->arErrorMissedDoubleTG[side][ 1 ],

		      "err-wrong-double-below-dp-skill",
		      sc->arErrorWrongDoubleDP[side][ 0 ],
		      "err-wrong-double-below-dp-cost",
		      sc->arErrorWrongDoubleDP[side][ 1 ],

		      "err-wrong-double-above-tg-skill",
		      sc->arErrorWrongDoubleTG[side][ 0 ],
		      "err-wrong-double-above-tg-cost",
		      sc->arErrorWrongDoubleTG[side][ 1 ],

		      "err-wrong-take-skill",
		      sc->arErrorWrongTake[side][ 0 ],
		      "err-wrong-take-cost",
		      sc->arErrorWrongTake[side][ 1 ],
			
		      "err-wrong-drop-skill",
		      sc->arErrorWrongPass[side][ 0 ],
		      "err-wrong-drop-cost",
		      sc->arErrorWrongPass[side][ 1 ],

                      "error-skill",
                      errorskill,
                      "error-cost",
                      errorcost );

      DictSetItemSteal(p[side], "cube", d);
    }
  }
    
  if( sc->fDice ) {
    int side;
    for(side = 0; side < 2; ++side) {
      PyObject* d =
	Py_BuildValue("{s:f,s:f,s:f,s:f}",
		      "luck", sc->arLuck[side][0],
		      "luck-cost", sc->arLuck[side][1], 
                      "actual-result", sc->arActualResult[side],
                      "luck-adjusted-result", sc->arLuckAdj[side]);
    
      PyObject* m = PyDict_New();
  
      {
	lucktype lt;
	for( lt = LUCK_VERYBAD; lt <= LUCK_VERYGOOD; lt++ ) {
	  DictSetItemSteal(m, luckString(lt, 0),
			   PyInt_FromLong(sc->anLuck[0][lt]));
	}
      }

      /* luck based fibs rating difference */
  
      if ( nMatchTo && fIsMatch ) {
         float r = 0.5f + sc->arActualResult[ side ] -
              sc->arLuck[ side ][ 1 ] + sc->arLuck[ !side ][ 1 ];
         if ( r > 0.0f && r < 1.0f )
            DictSetItemSteal( d, "fibs-rating-difference",
                              PyFloat_FromDouble( relativeFibsRating( r,
                                                  nMatchTo ) ) );
      }
      
      DictSetItemSteal(d, "marked-rolls", m);

      DictSetItemSteal(p[side], "dice", d);
    }
  }

  /* advantage */

  if ( sc->fDice && fIsMatch && !nMatchTo && sc->nGames > 1 ) {

     int side;
     for ( side = 0; side < 2; ++side ) {
        PyObject *d =
            Py_BuildValue( "{s:f,s:f,s:f,s:f}",
                           "actual", sc->arActualResult[ side ] / sc->nGames,
                           "actual-ci", 1.95996f * 
                           sqrt( sc->arVarianceActual[ side ] / sc->nGames ),
                           "luck-adjusted", 
                           sc->arLuckAdj[ side ] / sc->nGames,
                           "luck-adjusted-ci", 1.95996f * 
                           sqrt( sc->arVarianceLuckAdj[ side ] / sc->nGames ) );

         DictSetItemSteal( p[ side ], "ppg-advantage", d );
      }

  }

  /* error based fibs ratings */

  if ( ( sc->fCube || sc->fMoves ) && fIsMatch && nMatchTo ) {

     int side;
     for ( side = 0; side < 2; ++side ) {

        PyObject* d = PyDict_New();

        DictSetItemSteal( d, "total", 
                          PyFloat_FromDouble( absoluteFibsRating( aaaar[ CHEQUERPLAY ][ PERMOVE ][ side ][ NORMALISED ],
                                                                  aaaar[ CUBEDECISION ][ PERMOVE ][ side ][ NORMALISED ],
                                                                  nMatchTo, rRatingOffset ) ) );
 
        if ( sc->anUnforcedMoves[ side ] ) {
           DictSetItemSteal( d, "chequer", 
                             PyFloat_FromDouble( absoluteFibsRatingChequer( aaaar[ CHEQUERPLAY ][ PERMOVE ][ side ][ NORMALISED ],
                             nMatchTo ) ) );
 
        }

        if ( sc->anCloseCube[ side ] ) {
           DictSetItemSteal( d, "cube", 
                             PyFloat_FromDouble( absoluteFibsRatingCube( aaaar[ CUBEDECISION ][ PERMOVE ][ side ][ NORMALISED ],
                             nMatchTo ) ) );
 
        }

        DictSetItemSteal( p[ side ], "error-based-fibs-rating", d );

     }

  }
  
  /* */

  {
    PyObject* d = PyDict_New();

    DictSetItemSteal(d, "X", p[0]);
    DictSetItemSteal(d, "O", p[1]);

    return d;
  }
}

/* plGame: Game as a list of records.
   doAnalysis: if true, add analysis info.
   verbose: if true, add derived analysis data.
   scMatch: if non-0, add game & match statistics.
 */

static PyObject*
PythonGame(const listOLD*    plGame,
	   int const      doAnalysis,
	   int const      verbose,
	   statcontext*   scMatch,
	   int const      includeBoards,
	   PyMatchState*  ms)
{
  const listOLD* pl = plGame->plNext;
  const moverecord* pmr = pl->p;            
  const xmovegameinfo* g = &pmr->g;

  PyObject* gameDict = PyDict_New();
  PyObject* gameInfoDict = PyDict_New();

  g_assert( pmr->mt == MOVE_GAMEINFO );

  if( ! (gameDict && gameInfoDict) ) {
    PyErr_SetString(PyExc_MemoryError, "");
    return 0;
  }
  
  DictSetItemSteal(gameInfoDict, "score-X", PyInt_FromLong(g->anScore[0]));
  DictSetItemSteal(gameInfoDict, "score-O", PyInt_FromLong(g->anScore[1]));

  /* Set Crawford info only if it is relevant, i.e. if any side is 1 point away
     from winning the match */
  
  if( g->anScore[0] + 1 == g->nMatch || g->anScore[1] + 1 == g->nMatch ) {
    DictSetItemSteal(gameInfoDict, "crawford",
			 PyBool_FromLong(g->fCrawfordGame));
  }
    
  if( g->fWinner >= 0 ) {
    DictSetItemSteal(gameInfoDict, "winner",
			 PyString_FromString(g->fWinner ? "O" : "X"));
    
    DictSetItemSteal(gameInfoDict, "points-won",
			 PyInt_FromLong(g->nPoints));
    
    DictSetItemSteal(gameInfoDict, "resigned",
			 PyBool_FromLong(g->fResigned));
  } else {
    Py_INCREF(Py_None);
    DictSetItemSteal(gameInfoDict, "winner", Py_None);
  }

  if( g->nAutoDoubles ) {
    DictSetItemSteal(gameInfoDict, "initial-cube",
		     PyInt_FromLong(1 << g->nAutoDoubles) );
  }

  DictSetItemSteal(gameDict, "info", gameInfoDict);
    
  if( scMatch ) {
    updateStatisticsGame( plGame );
    
    AddStatcontext(&g->sc, scMatch);
    
    {
      PyObject* s = PyGameStats(&g->sc,FALSE,g->nMatch);

      if( s ) {
	DictSetItemSteal(gameDict, "stats", s);
      }
    }
  }
  
  {
    int nRecords = 0;
    TanBoard anBoard;
    PyObject* gameTuple;

    {
      listOLD* t;
      for( t = pl->plNext; t != plGame; t = t->plNext ) {
	++nRecords;
      }
    }

    gameTuple = PyTuple_New(nRecords);

    nRecords = 0;

    if( includeBoards ) {
      InitBoard(anBoard, g->bgv);
    }

    for( pl = pl->plNext; pl != plGame; pl = pl->plNext ) {
      const char* action = 0;
      int player = -1;
	  long points = -1;
      PyObject* recordDict = PyDict_New();
      PyObject* analysis = doAnalysis ? PyDict_New() : 0;
      
      pmr = pl->p;
      
      switch( pmr->mt ) {
	case MOVE_NORMAL:
	{
	  action = "move";
	  player = pmr->fPlayer;

	  {
	    PyObject* dice = Py_BuildValue("(ii)", 
                                           pmr->anDice[0], pmr->anDice[1]);

	    
	    DictSetItemSteal(recordDict, "dice", dice);
	  }

	  DictSetItemSteal(recordDict, "move", PyMove(pmr->n.anMove));

	  if( includeBoards ) {
	    DictSetItemSteal(recordDict, "board",
			     PyString_FromString(PositionID((ConstTanBoard)anBoard)));
	    
	    ApplyMove(anBoard, pmr->n.anMove, 0);
	    SwapSides(anBoard);
	  }
	  
	  if( analysis ) {
	    if( pmr->CubeDecPtr->esDouble.et != EVAL_NONE ) {
	      PyObject* d =
		PyDoubleAnalysis(&pmr->CubeDecPtr->esDouble,   
                                  (float (*)[ NUM_ROLLOUT_OUTPUTS ])pmr->CubeDecPtr->aarOutput, 
                                  (float (*)[ NUM_ROLLOUT_OUTPUTS ])pmr->CubeDecPtr->aarStdDev,
				 ms, verbose);
	      {
		int s = PyDict_Merge(analysis, d, 1);     g_assert( s != -1 );
	      }
	      Py_DECREF(d);
	    }
	  
	  
	    if( pmr->ml.cMoves ) {
	      PyObject* a = PyMoveAnalysis(&pmr->ml, ms);

	      if( a ) {
		DictSetItemSteal(analysis, "moves", a);

		DictSetItemSteal(analysis, "imove", PyInt_FromLong(pmr->n.iMove));
	      }
	    }

	    addLuck(analysis, pmr->rLuck, pmr->lt);
	    addSkill(analysis, pmr->n.stMove, 0);
	    addSkill(analysis, pmr->stCube, "cube-skill");
	  }
	    
	  break;
	}
	case MOVE_DOUBLE:
	{
	  action = "double";
	  player = pmr->fPlayer;

	  if( includeBoards ) {
	    DictSetItemSteal(recordDict, "board",
			     PyString_FromString(PositionID((ConstTanBoard)anBoard)));
	  }
	    
	  if( analysis ) {
	    const cubedecisiondata* c = pmr->CubeDecPtr;
	    if( c->esDouble.et != EVAL_NONE ) {
	      PyObject* d = PyDoubleAnalysis(&c->esDouble, (float (*)[ NUM_ROLLOUT_OUTPUTS ])c->aarOutput,
					     (float (*)[ NUM_ROLLOUT_OUTPUTS ])c->aarStdDev, ms, verbose);
	      {
		int s = PyDict_Merge(analysis, d, 1);     g_assert( s != -1 );
	      }
	      Py_DECREF(d);
	    }
	  
	    addSkill(analysis, pmr->stCube, 0);
	  }
	    
	  break;
	}
	case MOVE_TAKE:
	{
	  action = "take";
	  player = pmr->fPlayer;

	  /* use nAnimals to point to double analysis ? */
	  
	  addSkill(analysis, pmr->stCube, 0);
	    
	  break;
	}
	case MOVE_DROP:
	{
	  action = "drop";
	  player = pmr->fPlayer;
	    
	  addSkill(analysis, pmr->stCube, 0);
	    
	  break;
	}
	case MOVE_RESIGN:
	{
	  action = "resign";
	  player = pmr->fPlayer;
	  points = pmr->r.nResigned;
	  if (points < 1) points = 1;
	  else if (points > 3) points = 3;
	  break;
	}
	
	case MOVE_SETBOARD:
	{
	  PyObject* id = 
            PyString_FromString(PositionIDFromKey(&pmr->sb.key));

	  action = "set";

	  DictSetItemSteal(recordDict, "board", id);

	  if( includeBoards ) {
	    /* (FIXME) what about side? */
            /* JTH: the board is always stored as if player 0 was on roll */
	    PositionFromKey(anBoard, &pmr->sb.key);
	  }
	  
	  break;
	}
	    
	case MOVE_SETDICE:
	{
          PyObject *dice;

	  player = pmr->fPlayer;
	  action = "set";

	  dice = Py_BuildValue("(ii)", pmr->anDice[0], pmr->anDice[1]);

	  DictSetItemSteal(recordDict, "dice", dice);
	    
	  addLuck(analysis, pmr->rLuck, pmr->lt);
	    
	  break;
	}
	    
	case MOVE_SETCUBEVAL:
	{
	  action = "set";
	  DictSetItemSteal(recordDict, "cube", PyInt_FromLong(pmr->scv.nCube));
	  break;
	}
	    
	case MOVE_SETCUBEPOS:
	{
	  const char* s[] = {"centered", "X", "O"};
	  const char* o = s[pmr->scp.fCubeOwner + 1];
	  
	  action = "set";
	  DictSetItemSteal(recordDict, "cube-owner",
			       PyString_FromString(o));
	  break;
	}

	default:
	{
	  g_assert_not_reached();
	}
      }

      if( action ) {
	DictSetItemSteal(recordDict, "action",
			     PyString_FromString(action));
      }
      
      if( player != -1 ) {
	DictSetItemSteal(recordDict, "player",
			     PyString_FromString(player ? "O" : "X"));
      }

      if( points != -1 ) {
	DictSetItemSteal(recordDict, "points",
			     PyInt_FromLong(points));
      }

      if( analysis ) {
	if( PyDict_Size(analysis) > 0 ) {
	  DictSetItemSteal(recordDict, "analysis", analysis);
	} else {
	  Py_DECREF(analysis); analysis = 0;
	}
      }
      
      if( pmr->sz ) {
	DictSetItemSteal(recordDict, "comment",
			     PyString_FromString(pmr->sz));
      }

      PyTuple_SET_ITEM(gameTuple, nRecords, recordDict);
      ++nRecords;
    }

    DictSetItemSteal(gameDict, "game", gameTuple);
  }

  return gameDict;
}

static void
addProperty(PyObject* dict, const char* name, const char* val)
{
  if( ! val ) {
    return;
  }

  DictSetItemSteal(dict, name, PyString_FromString(val));
}
  
static PyObject*
PythonMatch(PyObject* UNUSED(self), PyObject* args, PyObject* keywds)
{
  /* take match info from first game */
  const listOLD* firstGame = lMatch.plNext->p;
  const moverecord* pmr;
  const xmovegameinfo* g;
  int includeAnalysis = 1;
  int verboseAnalysis = 0;
  int statistics = 0;
  int boards = 1;
  PyObject* matchDict;
  PyObject* matchInfoDict;
  PyMatchState s;
  
  static char* kwlist[] = {"analysis", "boards", "statistics", "verbose", 0};
  
  if( ! firstGame ) {
    Py_INCREF(Py_None);
    return Py_None;
  }
  
  pmr = firstGame->plNext->p;
  g_assert( pmr->mt == MOVE_GAMEINFO );
  g = &pmr->g;

  if( !PyArg_ParseTupleAndKeywords(args, keywds, "|iiii", kwlist,
				   &includeAnalysis, &boards, &statistics,
				   &verboseAnalysis) )
    return 0;

  
  matchDict = PyDict_New();
  matchInfoDict = PyDict_New();

  if( !matchDict && !matchInfoDict ) {
    PyErr_SetString(PyExc_MemoryError, "");
    return 0;
  }

  if (g->i != 0)
  {
      PyErr_SetString(PyExc_StandardError, "First game missing from match");
	  return 0;
  }
  g_assert( g->i == 0 );

  /* W,X,0 
     B,O,1 */
  {
    int side;
    for(side = 0; side < 2; ++side) {
      PyObject* d = PyDict_New();
      addProperty(d, "rating", mi.pchRating[side]);
      addProperty(d, "name", ap[side].szName);

      DictSetItemSteal(matchInfoDict, side == 0 ? "X" : "O", d);
    }
  }

  DictSetItemSteal(matchInfoDict, "match-length", PyInt_FromLong(g->nMatch));
  
  if( mi.nYear ) {
    PyObject* date =  Py_BuildValue("(iii)", mi.nDay, mi.nMonth, mi.nYear);

    DictSetItemSteal(matchInfoDict, "date", date);
  }

  addProperty(matchInfoDict, "event", mi.pchEvent);
  addProperty(matchInfoDict, "round", mi.pchRound);
  addProperty(matchInfoDict, "place", mi.pchPlace);
  addProperty(matchInfoDict, "annotator", mi.pchAnnotator);
  addProperty(matchInfoDict, "comment", mi.pchComment);

  {	/* Work out the result (-1,0,1) - (p0 win, unfinished, p1 win) */
	int result = 0;
  	int anFinalScore[2];
	if (getFinalScore(anFinalScore))
	{
		if (anFinalScore[0] > g->nMatch)
			result = -1;
		else if (anFinalScore[1] > g->nMatch)
			result = 1;
	}
	DictSetItemSteal(matchInfoDict, "result", PyInt_FromLong(result));
  }

  {
    const char* v[] = { "Standard", "Nackgammon", "Hypergammon1", "Hypergammon2",
		  "Hypergammon3" };

    addProperty(matchInfoDict, "variation", v[g->bgv]);
  }
  
  {
    unsigned int n = !g->fCubeUse + !!g->fCrawford + !!g->fJacoby;
    if( n ) {
      PyObject* rules = PyTuple_New(n);

      n = 0;
      if( !g->fCubeUse ) {
	PyTuple_SET_ITEM(rules, n++, PyString_FromString("NoCube"));
      }
      if( g->fCrawford ) {
	PyTuple_SET_ITEM(rules, n++, PyString_FromString("Crawford"));
      }
      if( g->fJacoby ) {
	PyTuple_SET_ITEM(rules, n++, PyString_FromString("Jacoby"));
      }

      DictSetItemSteal(matchInfoDict, "rules", rules);
    }
  }

  DictSetItemSteal(matchDict, "match-info", matchInfoDict);

  s.ec = 0;
  s.rc = 0;
  
  {
    int nGames = 0;
    const listOLD* pl;
    PyObject *matchTuple;
    statcontext scMatch;

    for( pl = lMatch.plNext; pl != &lMatch; pl = pl->plNext ) {
      ++nGames;
    }

    if( statistics ) {
      IniStatcontext(&scMatch);
    }
    
    matchTuple = PyTuple_New(nGames);

    nGames = 0;
    for(pl = lMatch.plNext; pl != &lMatch; pl = pl->plNext) {
      PyObject* g = PythonGame(pl->p, includeAnalysis, verboseAnalysis,
			       statistics ? &scMatch : 0 , boards, &s);

      if( ! g ) {
	/* Memory leaked. out of memory anyway */
	return 0;
      }
      
      PyTuple_SET_ITEM(matchTuple, nGames, g);
      ++nGames;
    }

    DictSetItemSteal(matchDict, "games", matchTuple);

    if( statistics ) {
      PyObject* s = PyGameStats(&scMatch,TRUE,g->nMatch);

      if( s ) {
	DictSetItemSteal(matchDict, "stats", s);
      }
    }
  }

  if( s.ec ) {
    PyObject* e = EvalContextToPy(s.ec);
    DictSetItemSteal(matchInfoDict, "default-eval-context", e);
  }
  
  if( s.rc ) {
    PyObject* e = RolloutContextToPy(s.rc);
    DictSetItemSteal(matchInfoDict, "default-rollout-context", e);

    /* No need for that, I think */
/*     DictSetItemSteal(matchInfoDict, "sgf-rollout-version", */
/* 		     PyInt_FromLong(SGF_ROLLOUT_VER)); */
  }

  return matchDict;
}


static PyObject*
PythonNavigate(PyObject* UNUSED(self), PyObject* args, PyObject* keywds)
{
  int nextRecord = INT_MIN;
  int nextGame = INT_MIN;
  int gamesDif = 0;
  int recordsDiff = 0;
  PyObject *r;
  
  static char* kwlist[] = {"next", "game", 0};

  if( ! lMatch.plNext ) {
    PyErr_SetString(PyExc_RuntimeError, "no active match");
    return 0;
  }
    
  if( !PyArg_ParseTupleAndKeywords(args, keywds, "|ii", kwlist,
				   &nextRecord, &nextGame) )
    return 0;


  r = 0;
  
  if( nextRecord == INT_MIN && nextGame == INT_MIN ) {
    /* no args, go to start */
    ChangeGame( lMatch.plNext->p );
  } else {

    if( nextRecord != INT_MIN && nextRecord < 0 ) {
      PyErr_SetString(PyExc_ValueError, "negative next record");
      return 0;
    }
  
    if( nextGame != INT_MIN && nextGame != 0 ) {
      listOLD* pl= lMatch.plNext;

      for( ; pl->p != plGame && pl != &lMatch; pl = pl->plNext)
	;	

      {                                            g_assert( pl->p == plGame ); }
      {
	int n = nextGame;
	if( n > 0 ) {
	  while( n > 0 && pl->plNext->p ) {
	    pl = pl->plNext;
	    --n;
	  }
	} else if( n < 0 ) {
	  while( n < 0 && pl->plPrev->p ) {
	    pl = pl->plPrev;
	    ++n;
	  }
	}
	ChangeGame(pl->p);

	gamesDif = abs(nextGame) - n;
      }
    }
  
    if( nextRecord != INT_MIN ) {
      recordsDiff = nextRecord - InternalCommandNext(0, 0, nextRecord);
    }

    /* (HACK)
       If normal move, set global dice for export and such.
     */
    if ( plLastMove->plNext && plLastMove->plNext->p ) {
      const moverecord* r = (const moverecord*)(plLastMove->plNext->p);
      
      if( r->mt == MOVE_NORMAL ) {
	memcpy(ms.anDice, r->anDice, sizeof(ms.anDice));
      }
    }
    
    if( recordsDiff || gamesDif ) {
      r = Py_BuildValue("(ii)", recordsDiff, gamesDif);
    }
  }
  
  /* (HACK) */
  if( ms.gs ==  GAME_NONE) {
    ms.gs = GAME_PLAYING;
  }

  if( ! r ) {
    Py_INCREF(Py_None);
    r = Py_None;
  }

  return r;
}


PyMethodDef gnubgMethods[] = {

  { "board", PythonBoard, METH_VARARGS,
    "Get the current board\n"
    "    arguments: none\n"
    "    returns: tuple of two lists of 25 ints:\n"
    "        pieces on points 1..24 and the bar" },
  { "command", PythonCommand, METH_VARARGS,
    "Execute a command\n"
    "    arguments: string containing command\n"
    "    returns: nothing" },
  { "cfevaluate", PythonEvaluateCubeful, METH_VARARGS,
    "Cubeful evaluation\n"
    "    arguments: [board] [cube-info] [eval-context]\n"
    "       board = tuple ( see \"board\" )\n"
    "       cube-info = dictionary: 'jacoby'=>0/1, 'crawford'=>0/1\n"
    "           'move'=>0/1, 'beavers'=>0/1, 'cube'=>cube val (int)\n"
    "           'matchto'=>length (0 for money), 'bgv'=>0..4\n"
    "           'score'=>(int, int), 'gammonprice'=(float[4])\n"
    "       eval-context = dictionary: 'cubeful'=>0/1, 'plies'=>int,\n"
    "           'deterministic'=> 0/1, 'noise'->float\n"
    "    returns: evaluation = tuple (floats optimal, nodouble, take, drop, int recommendation, String recommendationtext)" },
  { "dicerolls", PythonDiceRolls, METH_VARARGS,
    "return a list of dice rolls from current RNG\n"
    "   arguments: number of rolls\n"
    "    returns: list of tuples (2 elements each, one for each die)\n"},
  { "evaluate", PythonEvaluate, METH_VARARGS,
    "Cubeless evaluation\n"
    "    arguments: [board] [cube-info] [eval context]\n"
    "         see 'cfevaluate'\n"
    "    returns tuple(floats P(win), P(win gammon), P(win backgammnon)\n"
    "         P(lose gammon), P(lose backgammon), cubeless equity)" },
  { "evalcontext", PythonEvalContext, METH_VARARGS,
    "make an evalcontext\n"
    "    argument: tuple ( 4 int, float )\n"
    "    returns:  eval-context ( see 'cfevaluate' )"   },
  { "eq2mwc", PythonEq2mwc, METH_VARARGS,
    "convert equity to MWC\n"
    "    argument: [float equity], [cube-info]\n"
    "         defaults equity = 0.0, cube-info see 'cfevaluate'\n"
    "    return float mwc" },
  { "findbestmove", PythonFindBestMove, METH_VARARGS,
    "Find the best move\n"
    "    arguments: [board] [cube-info] [eval-context]\n"
    "        see 'cfevaluate'\n"
    "    returns: tuple( ints point from, point to, \n"
    "        unused moves are set to zero" },
  { "mwc2eq", PythonMwc2eq, METH_VARARGS,
    "convert MWC to equity\n"
    "    argument: [float match-winning-chance], [cube-info]\n"
    "         defaults mwc = 0.0, cube-info see 'cfevaluate'\n"
    "    returns: float equity" },
  { "matchchecksum", PythonMatchChecksum, METH_VARARGS,
    "Calculate checksum for current match\n"
    "    arguments: none\n"
    "    returns: MD5 digest as 32 char hex string" },
  { "cubeinfo", PythonCubeInfo, METH_VARARGS,
    "Make a cubeinfo\n"
    "    arguments: [cube value, cube owner = 0/1, player on move = 0/1, \n"
    "        match length (0 = money), score (tuple int, int), \n"
    "        is crawford = 0/1, bg variant = 0/5]\n"
    "    returns pos-info dictionary ( see 'cfevaluate' )" },
  { "posinfo", PythonPosInfo, METH_VARARGS,
    "Make a posinfo dictionary\n"
    "    arguments: [player on roll = 0/1, player resigned = 0/1, \n"
    "        player doubled = 0/1, gamestate = 0..7, dice = tuple(0..6, 0..6)] \n"
    "    returns pos-info dictionary\n"
    "       pos-info = dictionary: 'dice'=>tuple (int,int), 'turn'=>0/1\n"
    "           'resigned'=>0/1, 'doubled'=>0/1, 'gamestate'=>int (0..7)\n" },
  { "met", PythonMET, METH_VARARGS,
    "return the current match equity table\n"
    "   arguments: [max score]\n"
    "    returns: list of list n of list n (rows of pre-crawford table\n"
    "        list 2 of list n of post-crawford for player 0/player 1" },
  { "positionid", PythonPositionID, METH_VARARGS,
    "return position ID from board\n"
    "    arguments: [board] ( see 'cfevaluate' )\n"
    "    returns: position ID as string" },
  { "matchid", PythonMatchID, METH_VARARGS,
    "return MatchID from current position, or from cube-info, pos-info\n"
    "    arguments: [cube-info dictionary], [pos-info dictionary] \n"
    "        cube-info: see 'cfevaluate'\n"
    "        pos-info: see 'posinfo'\n"
    "    returns: Match ID as string" },
  { "gnubgid", PythonGnubgID, METH_VARARGS,
    "return GNUBGID from current position, or from board, cube-info, pos-info\n"
    "    arguments: [board, cube-info dictionary, pos-info dictionary]\n"
    "        board, cube-info: see 'cfevaluate'\n"
    "        pos-info: see 'posinfo'\n"
    "    returns: GNUBGID as string" },
  { "movetupletostring", PythonMoveTuple2String, METH_VARARGS,
    "Convert a move tuple to a move string\n"
    "    arguments: tuple of 8 ints\n"
    "    returns: String representation of move" },
  { "parsemove", PythonParseMove, METH_VARARGS,
    "Parse move\n"
    "    arguments: string containing move to parse\n"
    "    returns: tuple of (tuple (int, int)) representing each move " },
  { "positionfromid", PythonPositionFromID, METH_VARARGS,
    "return board from position ID\n"
    "    arguments: [position ID as string]\n"
    "    returns: board ( see 'cfevaluate' )" },
  { "positionbearoff", PythonPositionBearoff, METH_VARARGS,
    "return the bearoff id for the given position\n"},
  { "positionfrombearoff", PythonPositionFromBearoff, METH_VARARGS,
    "return the board from the given bearoff id\n"
    "    arguments: [bearoff id] [no. chequers] [no. points]\n"
    "    returns: board ( see 'cfevaluate' )" },
  { "positionkey", PythonPositionKey, METH_VARARGS,
    "return key for position\n"
    "    arguments: [ board ] ( see 'cfevaluate' )\n"
    "    returns: tuple (10 ints)" },
  { "positionfromkey", PythonPositionFromKey, METH_VARARGS,
    "return position from key\n"
    "    arguments: [ list of 10 ints] \n"
    "    returns: board ( see 'cfevaluate' )" },
  { "match", (PyCFunction)PythonMatch, METH_VARARGS|METH_KEYWORDS,
    "Get the current match\n"
    "    arguments: [ include-analysis = 0/1, include-boards = 0/1,\n"
    "       include-statistics = 0/1, verbose = 0/1 ]\n"
    "    returns: dictionary of match info:\n"
    "       'games' => list of dictionaries, one per game\n"
    "         'info' => dictionary\n"
    "           'points-won'=>int, 'score-X'=> int, 'winner'=>'X'/'O'\n"
    "           'resigned'=> 0/1, 'score-O'=> int\n"
    "         'stats' => dictionary\n"
    "           'X' => player 0 dictionary of stats\n"
    "             'cube'=>dictionary\n"
    "               'close-cube'=>int, 'err-missed-double-above-cp-cost'=>float\n"
    "               'err-missed-double-above-cp-skill'=>float\n"
    "               'err-missed-double-below-cp-cost'=>float\n"
    "               'err-missed-double-below-cp-skill'=>float\n"
    "               'err-wrong-double-above-tg-cost'=>float\n"
    "               'err-wrong-double-above-tg-skill'=>float\n"
    "               'err-wrong-double-below-dp-cost'=>float\n"
    "               'err-wrong-double-below-dp-skill'=>float\n"
    "               'err-wrong-drop-cost'=>float, 'err-wrong-drop-skill'=>float\n"
    "               'err-wrong-take-cost'=>float, 'err-wrong-take-skill'=>float\n"
    "               'error-cost'=>float, 'error-skill'=>float\n"
    "               'missed-double-above-cp'=>int, 'missed-double-below-cp'=>int\n"
    "               'n-doubles'=>int, 'n-drops'=>int, 'n-takes': =>int,\n"
    "               'total-cube'=>int, 'wrong-double-above-tg'=>int\n"
    "               'wrong-double-below-dp'=>int, 'wrong-drop'=>int,\n"
    "               'wrong-take'=>int\n"
    "             'moves'=>dictionary\n"
    "               'marked'=>dictionarly\n"
    "                  'good'=> int, 'unmarked'->int, 'doubtful'=>int,\n"
    "                  'bad'=>int, 'very bad'=>int\n"
    "               'total-moves'=>int, 'unforced-moves'=>int,\n"
    "               'error-cost'=>float, 'error-skill'=>float\n"
    "             'dice'=>dictionary\n"
    "               'actual-result;=>float, 'cube'=>float, 'luck'=>float,\n"
    "               'luck-cost'=>float, 'luck-adjusted-result'=>float\n"
    "               'marked-rolls'=>dictionary\n"
    "                 'verygood'=>int, 'good'=>int, 'unmarked'=>int,\n"
    "                 'bad'=>int, 'verybad'=>int\n"
    "              'time'=>dictionary\n"
    "                'time-penalty'=>int, 'time-penalty-cost'=>float,\n"
    "                'time-penalty-skill'=>float\n"
    "            'O'=>  player 1 dicrtionary of stats - see 'X; above\n"
    "         'game'=>list of dictionaries, one per move\n"
    "            'dice'=>(int, int), move=>((int, int),[(int, int),...])\n"
    "            'player'=>'X'/'O', 'board'=>board-id-string,\n"
    "            'action'=>'move', 'double', 'take', 'drop', 'resign'\n"
    "            'analysis'=>dictionary\n"
    "              'imove'=>int index of move in list of analysed moves\n"
    "              'moves'=>list of dictionaries, one per analysed move\n"
    "                 'score'=>equity for move, 'type'=>'eval''rollout'\n"
    "                 'move'=>((int, int),[(int, int),...])\n"
    "                 'probs'=> tuple (5 floats - P(win),\n"
    "                 P(win gammon)..P(lose bkgammon)\n"
    "                 [ 'evalcontext' = dictionary describing eval context\n"
    "                        if not default\n"
    "        'match-info' = dictionary \n"
    "          'X' => player-0 dictionary\n"
    "            'rating' = rating if known\n"
    "            'name'   = player name\n"
    "          'O' => player-1 dictionary, as 'X', above\n" 
    "          'date'=>(tuple dd, mm, yyyy)\n"
    "          'default-eval-context' = dictionary\n"
    "            'plies'=> int, 'deterministic'=>0/1, 'noise'=>float,\n"
    "            'cubeful'=>0/1\n"
    "          'match_length' = int\n"
    "          'result' =>0/1\n"
    "          'rules' = 'Crawford'/whatever\n"
    "          'variation' => 'Standard' or whatever\n" },
  { "navigate", (PyCFunction)PythonNavigate, METH_VARARGS|METH_KEYWORDS,
    "go to a position in a match or session'n"
    "    arguments: no args = go to start of match/session\n"
    "         [ game=offset] go forward/backward n games\n"
    "         [record=offset] go gorward/backward n moves'n"
    "    returns: None if no change, tuple( games moved, records moved)" },
  { "nextturn", (PyCFunction) PythonNextTurn, METH_VARARGS, 
    "play one turn\n"
    "    arguments: none\n"
    "    returns: None" },
  { "luckrating", (PyCFunction) PythonLuckRating, METH_VARARGS, 
    "convert a luck per move amount into a rating 0..5 for very unlucky to very lucky\n"
    "    arguments: float luck per move\n"
    "    returns: int 0..5" },
  { "errorrating", (PyCFunction) PythonErrorRating, METH_VARARGS, 
    "convert an error per move amount to a rating 0 = awful..7=supernatural\n"
    "    arguments: float error per move\n"
    "    returns: int\n" },
  { "updateui", (PyCFunction) PythonUpdateUI, METH_VARARGS, 
    "Allows the UI to update itself\n"
    "    arguments: none\n"
    "    returns: None" },

  { NULL, NULL, 0, NULL }

};

static char *python_dir = NULL;
 
extern void PythonInitialise(char *argv0)
{

#ifdef WIN32
  /* Setup python to look in the pythonlib directory if present */
  char *working_dir = g_get_current_dir();
  python_dir = g_build_filename(working_dir, "PythonLib", NULL);
  if (access(python_dir, F_OK) == 0) {	
    /* Set Python to use this directory */
    Py_SetPythonHome(python_dir);
  }
  g_free(working_dir);
#endif

  Py_SetProgramName(argv0);
  Py_Initialize();

  /* ensure that python know about our gnubg module */
  Py_InitModule( "gnubg", gnubgMethods );
  PyRun_SimpleString( "import gnubg\n" );

  /* run gnubg.py start up script */
  LoadPythonFile("gnubg.py");
}

extern void PythonShutdown( void )
{
  Py_Finalize();

#ifdef WIN32
  if (python_dir)
    g_free(python_dir);
#endif
}

extern void PythonRun(const char *sz)
{
	if (*sz) {
		PyRun_SimpleString(sz);
	} else {
		/* Run python interactively with history and auto completion
		   if available
		*/
#if USE_GTK
	        if (fX) {
			if (!PyRun_SimpleString("import sys\n"
					   "sys.argv=['','-n']\n"
					   "import idlelib.PyShell\n"
					   "idlelib.PyShell.main()\n")){

				return;
			}
		}
#endif
		PyRun_SimpleString("try:\n"
				   "    import readline\n"
				   "except ImportError:\n" 
				   "    try:\n"
				   "        import pyreadline as readline\n"
				   "    except: pass\n"
				   "try:\n"
				   "    import sys, rlcompleter\n"
				   "    readline.parse_and_bind('tab: complete')\n"
				   "except: pass\n"
				   "print 'Python', sys.version\n");

		PyRun_InteractiveLoop(stdin, "<stdin>");
	}
}

extern int LoadPythonFile(const char *sz)
{
	char *path = NULL;
	char *cmd = NULL;
	char *escpath = NULL;
	int ret = FALSE;

	if (g_file_test(sz, G_FILE_TEST_EXISTS))
		path = g_strdup(sz);
	else {
		path = BuildFilename2("/scripts", sz);
		if (!g_file_test(path, G_FILE_TEST_EXISTS)) {
			g_free(path);
			path = g_build_filename("scripts", sz, NULL);
		}
	}
	if (!g_file_test(path, G_FILE_TEST_IS_REGULAR)) {
		g_free(path);
		outputerrf("Python file (%s) not found\n", sz);
		return FALSE;
	}
	escpath = g_strescape (path, NULL);
	cmd = g_strdup_printf("execfile('%s')", escpath);
	PyRun_SimpleString(cmd);
	g_free(escpath);
	g_free(path);
	g_free(cmd);

	return ret;
}
#endif
