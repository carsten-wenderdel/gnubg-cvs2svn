/*
* widget3d.c
* by Jon Kinsey, 2003
*
* Gtkglext widget for 3d drawing
*
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

#include <stdlib.h>
#include <GL/gl.h>
#include <GL/glu.h>
#include "inc3d.h"
#include <assert.h>

#if HAVE_GTKGLEXT

#include <gtk/gtk.h>

#if USE_GTK2
#include <gtk/gtkgl.h>
#else
#include <gtkgl/gtkglarea.h>
#endif

#include "backgammon.h"
#include "sound.h"
#include "renderprefs.h"

int screenHeight;
guint id = 0;
GtkWidget *widget;
idleFunc *pIdleFun;
#if USE_GTK2
GdkGLConfig *glconfig;
#endif
BoardData *pCurBoard;

#ifdef WIN32

void CheckAccelerated()
{
	const char* vendor = glGetString(GL_VENDOR);
	const char* renderer = glGetString(GL_RENDERER);
//	const char* version = glGetString(GL_VERSION);

	if (strstr(vendor, "Microsoft") && strstr(renderer, "Generic"))
	{
#if USE_GTK2
		GtkWidget *dialog = gtk_message_dialog_new (0, GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_MODAL,
				GTK_MESSAGE_WARNING, GTK_BUTTONS_OK, 
				"No hardware accelerated graphics card found - performance may be slow.\n");
		gtk_dialog_run(GTK_DIALOG(dialog));
		gtk_widget_destroy(dialog);
#endif
	}
}

#else

void CheckAccelerated()
{
}

#endif

void render()
{
	drawBoard(pCurBoard);
}

void Display(void)
{
	if (rdAppearance.showShadows)
		shadowDisplay(render);
	else
		render();
}

void SetupViewingVolume()
{
	GLint viewport[4];
	glGetIntegerv (GL_VIEWPORT, viewport);

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	SetupPerspVolume(pCurBoard, viewport);

	SetupLight(pCurBoard);
	calculateBackgroundSize(pCurBoard, viewport);
}

void SetupViewingVolume3d()
{
	SetupViewingVolume();
}

gboolean idle(GtkWidget *widget)
{
	if (pIdleFun())
		gtk_widget_queue_draw(widget);

	return TRUE;
}

void StopIdle3d()
{
	stopIdleFunc();
}

extern int animation_finished;

void stopIdleFunc()
{
	if (pCurBoard->shakingDice)
	{
		pCurBoard->shakingDice = 0;
		updateDiceOccPos(pCurBoard);
		gtk_main_quit();
	}
	if (pCurBoard->moving)
	{
		pCurBoard->moving = 0;
		updatePieceOccPos(pCurBoard);
		animation_finished = TRUE;
		gtk_main_quit();
	}

	if (id)
	{
		g_idle_remove_by_data(widget);
		id = 0;
	}
}

void setIdleFunc(idleFunc* pFun)
{
	if (id)
	{
		g_idle_remove_by_data(widget);
		id = 0;
	}

	pIdleFun = pFun;
	id = g_idle_add((GtkFunction)idle, widget);
}

void realize(GtkWidget *widget, gpointer data)
{
#if USE_GTK2
	GdkGLContext *glcontext = gtk_widget_get_gl_context(widget);
	GdkGLDrawable *gldrawable = gtk_widget_get_gl_drawable(widget);

	/*** OpenGL BEGIN ***/
	if (!gdk_gl_drawable_gl_begin(gldrawable, glcontext))
		return;
#else
    if (gtk_gl_area_make_current(GTK_GL_AREA(widget)))
	{
#endif

	InitGL();
	SetSkin(pCurBoard, rdAppearance.skin3d);

#if USE_GTK2
	gdk_gl_drawable_gl_end(gldrawable);
	/*** OpenGL END ***/
#else
	}
#endif
}

static gboolean configure_event(GtkWidget *widget, GdkEventConfigure *event, gpointer data)
{
#if USE_GTK2
	GdkGLContext *glcontext = gtk_widget_get_gl_context(widget);
	GdkGLDrawable *gldrawable = gtk_widget_get_gl_drawable(widget);

	/*** OpenGL BEGIN ***/
	if (!gdk_gl_drawable_gl_begin(gldrawable, glcontext))
		return FALSE;
#else
	/* OpenGL functions can be called only if make_current returns true */
	if (gtk_gl_area_make_current(GTK_GL_AREA(widget)))
		return FALSE;
#endif

	screenHeight = widget->allocation.height;
	Reshape(widget->allocation.width, widget->allocation.height);

#if USE_GTK2
	gdk_gl_drawable_gl_end(gldrawable);
	/*** OpenGL END ***/
#endif

	return TRUE;
}

static gboolean expose_event(GtkWidget *widget, GdkEventExpose *event, gpointer data)
{
#if USE_GTK2
	GdkGLContext *glcontext = gtk_widget_get_gl_context(widget);
	GdkGLDrawable *gldrawable = gtk_widget_get_gl_drawable(widget);

	/*** OpenGL BEGIN ***/
	if (!gdk_gl_drawable_gl_begin(gldrawable, glcontext))
		return TRUE;
#else
	/* Draw only last expose. */
	if (event->count > 0)
		return TRUE;

	/* OpenGL functions can be called only if make_current returns true */
	if (gtk_gl_area_make_current(GTK_GL_AREA(widget)))
	{
#endif

	if (rdAppearance.debugTime)
	{
#define NUM_SAMPLES 10
		static int count = NUM_SAMPLES;
		static double total = 0;
		double end, start = get_time();
		Display();
		end = get_time();
		total += end - start;
		count--;
		if (!count)
		{
			g_print("Average draw time:%.1fms.\n", total / NUM_SAMPLES);
			count = NUM_SAMPLES;
			total = 0;
		}
	}
	else
		Display();

#if USE_GTK2
	gdk_gl_drawable_swap_buffers(gldrawable);
#else
	gtk_gl_area_swapbuffers(GTK_GL_AREA(widget));
#endif

#if USE_GTK2
	gdk_gl_drawable_gl_end(gldrawable);
	/*** OpenGL END ***/
#else
	}
#endif

	return TRUE;
}

void updateBoard(BoardData* bd)
{
	int points[2][25];
	read_board(bd, points);
	update_position_id(bd, points);
	update_pipcount(bd, points);

	gtk_widget_queue_draw(widget);
	updatePieceOccPos(bd);
}

/* Snowie style editing: we will try to place i chequers of the
   indicated colour on point n.  The x,y coordinates will be used to
   map to a point n and a checker position i.

   Clicking on a point occupied by the opposite color clears the point
   first.  If not enough chequers are available in the bearoff tray,
   we try to add what we can.  So if there are no chequers in the
   bearoff tray, no chequers will be added.  This may be a point of
   confusion during usage.  Clicking on the outside border of a point
   corresponds to i=0, i.e. remove all chequers from that point. */
void board_quick_edit3d(BoardData* bd, int x, int y, int dragging)
{
	int current, delta, c_chequer;
	int off, opponent_off;
	int bar, opponent_bar;
	int i;

	int colour = bd->drag_button == 1 ? 1 : -1;
	int n = bd->drag_point;

    if (!dragging)
	{
		if (n == 26 || n == 27)
		{	/* click on bearoff tray in edit mode -- bear off all chequers */
			for( i = 0; i < 28; i++ )
				bd->points[i] = 0;

			bd->points[26] = bd->nchequers;
			bd->points[27] = -bd->nchequers;

			updateBoard(bd);
		}
		else if ( n == POINT_UNUSED0 || n == POINT_UNUSED1) 
		{	/* click on unused bearoff tray in edit mode -- reset to starting position */
			int anBoard[ 2 ][ 25 ];
			InitBoard( anBoard, ms.bgv );
			write_board( bd, anBoard );
			
			updateBoard(bd);
		}
    }
    
    /* Only points or bar allowed */
    if (n < 0 || n > 25)
		return;

    /* Make sure that if we drag across a bar, we started on that bar.
       This is to make sure that if you drag a prime across say point
       4 to point 9, you don't inadvertently add chequers to the bar */
    if (dragging && (n == 0 || n == 25) && n != bd->qedit_point)
		return;

	bd->qedit_point = n;

    off          = (colour == 1) ? 26 : 27;
    opponent_off = (colour == 1) ? 27 : 26;

    bar          = (colour == 1) ? 25 : 0;
    opponent_bar = (colour == 1) ? 0 : 25;

    /* Can't add checkers to the wrong bar */
    if (n == opponent_bar)
		return;

    c_chequer = (n == 0 || n == 25) ? 3 : 5;

    current = abs(bd->points[n]);
    
    /* Given n, map (x, y) to the ith checker position on point n*/
	i = board_point(bd, x, y, n);
    if (i < 0)
		return;

    /* We are at the maximum position in the point, so we should not
       respond to dragging.  Each click will try to add one more
       chequer */
    if (!dragging && i >= c_chequer && current >= c_chequer)
		i = current + 1;
    
    /* Clear chequers off the other colour from this point */
    if (current && SGN(bd->points[n]) != colour)
	{
		bd->points[opponent_off] += current * -colour;
		bd->points[n] = 0;
		current = 0;

		gtk_widget_queue_draw(widget);
		updatePieceOccPos(bd);
    }
    
    delta = i - current;
    
    /* No chequers of our colour added or removed */
    if (delta == 0)
		return;

    if (delta < 0)
	{	/* Need to remove some chequers of the same colour */
		bd->points[off] -= delta * colour;
		bd->points[n] += delta * colour;
    }
	else
	{
		int mv, avail = abs(bd->points[off]);
		/* any free chequers? */
		if (!avail)
			return;
		/* move up to delta chequers, from avail chequers to point n */
		mv = (avail > delta) ? delta : avail;

		bd->points[off] -= mv * colour;
		bd->points[n] += mv * colour;
    }

	updateBoard(pCurBoard);
}

gboolean button_press_event(GtkWidget *widget, GdkEventButton *event, gpointer data)
{
	int numOnPoint;
	gint x = (int)event->x;
	gint y = screenHeight - (int)event->y;	/* Reverse screen y coords */
	int editing = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(pCurBoard->edit));

	if (event->type != GDK_BUTTON_PRESS || pCurBoard->drag_point >= 0 || pCurBoard->shakingDice || pCurBoard->moving)
		return TRUE;

	pCurBoard->click_time = gdk_event_get_time( (GdkEvent*)event );
	pCurBoard->drag_button = event->button;

	pCurBoard->drag_point = board_point(pCurBoard, x, y, -1);

    if (editing && (pCurBoard->drag_point == POINT_DICE ||
		pCurBoard->drag_point == POINT_LEFT || pCurBoard->drag_point == POINT_RIGHT))
	{
		if (pCurBoard->drag_point == POINT_LEFT && pCurBoard->turn != 0)
			UserCommand( "set turn 0" );
		else if (pCurBoard->drag_point == POINT_RIGHT && pCurBoard->turn != 1)
			UserCommand( "set turn 1" );

		GTKSetDice( NULL, 0, NULL );
		pCurBoard->drag_point = -1;
		return TRUE;
	}

	switch (pCurBoard->drag_point)
	{
	case -1:
	    /* Click on illegal area. */
	    board_beep(pCurBoard);
	    return TRUE;

	case POINT_CUBE:
	    /* Clicked on cube; double. */
	    pCurBoard->drag_point = -1;
	    
	    if(editing)
			GTKSetCube(NULL, 0, NULL);
	    else if (pCurBoard->doubled)
			UserCommand("take");
		else
			UserCommand("double");
	    
	    return TRUE;

	case POINT_RESIGN:
		stopIdleFunc();
		/* clicked on resignation symbol */
		updateFlagOccPos(pCurBoard);

		pCurBoard->drag_point = -1;
		if (pCurBoard->resigned && !editing)
			UserCommand("accept");

		return TRUE;

	case POINT_DICE:
		if (pCurBoard->dice[0] <= 0)
		{	/* Click on dice (below board) - shake */
			pCurBoard->drag_point = -1;
			UserCommand("roll");
			return TRUE;
		}
	    /* Clicked on dice; end move. */
	    pCurBoard->drag_point = -1;
		if (event->button == 1)
			Confirm(pCurBoard);
	    else 
		{
			/* Other buttons on dice swaps positions. */
			int n = pCurBoard->dice[ 0 ];
			pCurBoard->dice[ 0 ] = pCurBoard->dice[ 1 ];
			pCurBoard->dice[ 1 ] = n;
			
			n = pCurBoard->dice_opponent[ 0 ];
			pCurBoard->dice_opponent[ 0 ] = pCurBoard->dice_opponent[ 1 ];
			pCurBoard->dice_opponent[ 1 ] = n;
			/* Display swapped dice */
			gtk_widget_queue_draw(widget);
	    }
	    
	    return TRUE;

	case POINT_LEFT:
	case POINT_RIGHT:
	/* If playing and dice not rolled yet, this code handles
	   rolling the dice if bottom player clicks the right side of
	   the board, or the top player clicks the left side of the
	   board (his/her right side). */
		if (pCurBoard->playing && pCurBoard->dice[ 0 ] <= 0 &&
			((pCurBoard->drag_point == POINT_RIGHT && pCurBoard->turn == 1) ||
			(pCurBoard->drag_point == POINT_LEFT  && pCurBoard->turn == -1)))
		{
			/* NB: the UserCommand() call may cause reentrancies,
			   so it is vital to reset pCurBoard->drag_point first! */
			pCurBoard->drag_point = -1;
			UserCommand("roll");
			return TRUE;
		}
		pCurBoard->drag_point = -1;
		return TRUE;

	default:	/* Points */
		if (!pCurBoard->playing || (!editing && pCurBoard->dice[0] <= 0 ))
		{
			/* Don't let them move chequers unless the dice have been
			   rolled, or they're editing the board. */
			board_beep(pCurBoard);
			pCurBoard->drag_point = -1;
			
			return TRUE;
		}

		if (editing && !(event->state & GDK_CONTROL_MASK))
		{
			board_quick_edit3d(pCurBoard, x, y, 0);
		    pCurBoard->drag_point = -1;
		    return TRUE;
		}

		if ((pCurBoard->drag_point == POINT_UNUSED0) || (pCurBoard->drag_point == POINT_UNUSED1))
		{	/* Clicked in un-used bear-off tray */
			pCurBoard->drag_point = -1;
			return TRUE;
		}

		/* if nDragPoint is 26 or 27 (i.e. off), bear off as many chequers as possible. */
        if (!editing && pCurBoard->drag_point == (53 - pCurBoard->turn) / 2)
		{
          /* user clicked on bear-off tray: try to bear-off chequers or
             show forced move */
          int anBoard[ 2 ][ 25 ];
          
          memcpy ( anBoard, ms.anBoard, sizeof anBoard );

          pCurBoard->drag_colour = pCurBoard->turn;
          pCurBoard->drag_point = -1;
          
          if (ForcedMove(anBoard, pCurBoard->dice) ||
               GreadyBearoff(anBoard, pCurBoard->dice)) 
		  {
            /* we've found a move: update board  */
            if ( UpdateMove( pCurBoard, anBoard ) ) {
              /* should not happen as ForcedMove and GreadyBearoff
                 always return legal moves */
              assert(FALSE);
            }
          }

          return TRUE;
        }
		/* How many chequers on clicked point */
		numOnPoint = pCurBoard->points[pCurBoard->drag_point];
	
	    /* Click on an empty point or opponent blot; try to make the point. */
		if (!editing && pCurBoard->drag_point <= 24 &&
			(numOnPoint == 0 || numOnPoint == -pCurBoard->turn))
		{
			int n[2], bar, i;
			int old_points[ 28 ], points[ 2 ][ 25 ];
			unsigned char key[ 10 ];
			
			memcpy( old_points, pCurBoard->points, sizeof old_points );
			
			pCurBoard->drag_colour = pCurBoard->turn;
			bar = pCurBoard->drag_colour == pCurBoard->colour ? 25 - pCurBoard->bar : pCurBoard->bar;
			
			if( pCurBoard->dice[ 0 ] == pCurBoard->dice[ 1 ] ) {
			/* Rolled a double; find the two closest chequers to make
			   the point. */
			int c = 0;

			n[ 0 ] = n[ 1 ] = -1;
			
			for( i = 0; i < 4 && c < 2; i++ )
			{
				int j = pCurBoard->drag_point + pCurBoard->dice[ 0 ] * pCurBoard->drag_colour *
				( i + 1 );

				if (j < 0 || j > 25)
					break;

				while( c < 2 && pCurBoard->points[ j ] * pCurBoard->drag_colour > 0 )
				{
					/* temporarily take chequer, so it's not used again */
					pCurBoard->points[ j ] -= pCurBoard->drag_colour;
					n[ c++ ] = j;
				}
			}
			
			/* replace chequers removed above */
			for (i = 0; i < c; i++)
				pCurBoard->points[n[i]] += pCurBoard->drag_colour;
			}
			else
			{
				/* Rolled a non-double; take one chequer from each point
				   indicated by the dice. */
				n[ 0 ] = pCurBoard->drag_point + pCurBoard->dice[ 0 ] * pCurBoard->drag_colour;
				n[ 1 ] = pCurBoard->drag_point + pCurBoard->dice[ 1 ] * pCurBoard->drag_colour;
			}
			
			if (n[ 0 ] >= 0 && n[ 0 ] <= 25 && n[ 1 ] >= 0 && n[ 1 ] <= 25 &&
				pCurBoard->points[ n[ 0 ] ] * pCurBoard->drag_colour > 0 &&
				pCurBoard->points[ n[ 1 ] ] * pCurBoard->drag_colour > 0 )
			{
				/* the point can be made */
				if( pCurBoard->points[ pCurBoard->drag_point ] )
					/* hitting the opponent in the process */
					pCurBoard->points[bar] -= pCurBoard->drag_colour;
			
				pCurBoard->points[ n[ 0 ] ] -= pCurBoard->drag_colour;
				pCurBoard->points[ n[ 1 ] ] -= pCurBoard->drag_colour;
				pCurBoard->points[ pCurBoard->drag_point ] = pCurBoard->drag_colour << 1;
			
				read_board( pCurBoard, points );
				PositionKey( points, key );

				if (!update_move(pCurBoard))
				{
					/* Show Move */
					updatePieceOccPos(pCurBoard);
					gtk_widget_queue_draw(widget);
					
					playSound( SOUND_CHEQUER );
				}
				else
				{
					/* the move to make the point wasn't legal; undo it. */
					memcpy( pCurBoard->points, old_points, sizeof pCurBoard->points );
					update_move( pCurBoard );
					board_beep( pCurBoard );
				}
			}
			else
				board_beep( pCurBoard );
			
			pCurBoard->drag_point = -1;
			return TRUE;
		}

		/* clicked on empty point */
		if (numOnPoint == 0)
		{
			board_beep( pCurBoard );
			pCurBoard->drag_point = -1;
			return TRUE;
		}

		pCurBoard->drag_colour = SGN(numOnPoint);

		/* trying to move opponent's chequer (except off the bar, which is OK) */
		if (!editing && pCurBoard->drag_point != 25 && pCurBoard->drag_colour != pCurBoard->turn)
		{
			board_beep( pCurBoard );
			pCurBoard->drag_point = -1;
			return TRUE;
		}
		/* Remove piece from board */
	    pCurBoard->points[pCurBoard->drag_point] -= pCurBoard->drag_colour;

		/* Start Dragging piece */
		MouseMove(x, y);

		SetMovingPieceRotation(pCurBoard->drag_point);

		/* Show hovering piece */
		updatePieceOccPos(pCurBoard);
		gtk_widget_queue_draw(widget);

		return TRUE;
	}

	return FALSE;
}

gboolean button_release_event(GtkWidget *widget, GdkEventButton *event, gpointer data)
{
	gint x = (int)event->x;
	gint y = screenHeight - (int)event->y;	/* Reverse screen y coords */

	if (pCurBoard->drag_point >= 0)
	{
		int dest = board_point(pCurBoard, x, y, -1);
		pCurBoard->DragTargetHelp = 0;

		if( !gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( pCurBoard->edit ) ) &&
			dest == pCurBoard->drag_point && gdk_event_get_time( (GdkEvent*)event ) -
			pCurBoard->click_time < CLICK_TIME ) 
		{
			/* Automatically place chequer on destination point
			   (as opposed to a full drag). */

			if( pCurBoard->drag_colour != pCurBoard->turn )
			{
				/* can't move the opponent's chequers */
				board_beep( pCurBoard );

				dest = pCurBoard->drag_point;

    			place_chequer_or_revert(pCurBoard, dest );		
			}
			else
			{
				gint left = pCurBoard->dice[0];
				gint right = pCurBoard->dice[1];

				/* Button 1 tries the left roll first.
				   other buttons try the right dice roll first */
				dest = pCurBoard->drag_point - ( pCurBoard->drag_button == 1 ? 
							  left :
							  right ) * pCurBoard->drag_colour;

				if( ( dest <= 0 ) || ( dest >= 25 ) )
					/* bearing off */
					dest = pCurBoard->drag_colour > 0 ? 26 : 27;
				
				if( place_chequer_or_revert(pCurBoard, dest ) )
					playSound( SOUND_CHEQUER );
				else
				{
					/* First roll was illegal.  We are going to 
					   try the second roll next. First, we have 
					   to redo the pickup since we got reverted. */
					pCurBoard->points[ pCurBoard->drag_point ] -= pCurBoard->drag_colour;
					
					/* Now we try the other die roll. */
					dest = pCurBoard->drag_point - ( pCurBoard->drag_button == 1 ?
								  right :
								  left ) * pCurBoard->drag_colour;
					
					if( ( dest <= 0 ) || ( dest >= 25 ) )
					/* bearing off */
					dest = pCurBoard->drag_colour > 0 ? 26 : 27;
					
					if (place_chequer_or_revert(pCurBoard, dest))
						playSound( SOUND_CHEQUER );
					else 
					{
						board_beep( pCurBoard );
					}
				}
			}
		}
		else
		{
			/* This is from a normal drag release */
			if (place_chequer_or_revert(pCurBoard, dest))
			{
				playSound( SOUND_CHEQUER );
			}
			else
				board_beep( pCurBoard );
		}

		/* Update Display */
		updatePieceOccPos(pCurBoard);
		gtk_widget_queue_draw(widget);
		pCurBoard->drag_point = -1;
	}

	return FALSE;
}

gboolean motion_notify_event(GtkWidget *widget, GdkEventMotion *event, gpointer data)
{
	gint x = (int)event->x;
	gint y = screenHeight - (int)event->y;	/* Reverse screen y coords */
	int editing = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(pCurBoard->edit));

	/* In quick editing mode, dragging across points */
	if (editing && !(event->state & GDK_CONTROL_MASK))
	{
		pCurBoard->drag_point = board_point(pCurBoard, x, y, -1);
		board_quick_edit3d(pCurBoard, x, y, 1);
	    pCurBoard->drag_point = -1;
	    return TRUE;
	}

	if (fGUIDragTargetHelp && !editing)
	{
		gint iDestPoints[4];
		gint i, ptx, pty, ptcx, ptcy;
		GdkColor *TargetHelpColor;
		
		if ((ap[ pCurBoard->drag_colour == -1 ? 0 : 1 ].pt == PLAYER_HUMAN)		/* not for computer turn */
			&& (pCurBoard->drag_point != board_point(pCurBoard, x, y, -1)))	/* dragged to different ponit */
		{
			pCurBoard->DragTargetHelp = LegalDestPoints(pCurBoard, pCurBoard->iTargetHelpPoints);
		}
	}

	if (MouseMove(x, y))
		gtk_widget_queue_draw(widget);

	return TRUE;
}

void CreateGLWidget(GtkWidget **drawing_area)
{
#if USE_GTK2
	/* Drawing area for OpenGL */
	*drawing_area = gtk_drawing_area_new();

	/* Set OpenGL-capability to the widget. */
	gtk_widget_set_gl_capability(*drawing_area, glconfig, NULL, TRUE, GDK_GL_RGBA_TYPE);

	gtk_widget_set_events(*drawing_area, GDK_EXPOSURE_MASK | GDK_BUTTON_PRESS_MASK | 
				GDK_BUTTON_RELEASE_MASK | GDK_BUTTON_MOTION_MASK);

	g_signal_connect(G_OBJECT(*drawing_area), "realize", G_CALLBACK(realize), NULL);
	g_signal_connect(G_OBJECT(*drawing_area), "configure_event", G_CALLBACK(configure_event), NULL);
	g_signal_connect(G_OBJECT(*drawing_area), "expose_event", G_CALLBACK(expose_event), NULL);
	g_signal_connect(G_OBJECT(*drawing_area), "button_press_event", G_CALLBACK(button_press_event), NULL);
	g_signal_connect(G_OBJECT(*drawing_area), "button_release_event", G_CALLBACK(button_release_event), NULL);
	g_signal_connect(G_OBJECT(*drawing_area), "motion_notify_event", G_CALLBACK(motion_notify_event), NULL);

#else
	/* create new OpenGL widget */
	*drawing_area = gtk_gl_area_new_vargs(NULL, /* no sharing */
			 GDK_GL_RGBA,
			 GDK_GL_DEPTH_SIZE,1,
			 GDK_GL_DOUBLEBUFFER,
			 GDK_GL_NONE);
	if (*drawing_area == NULL)
	{
		g_print("Can't create GtkGLArea widget\n");
		return FALSE;
	}
	gtk_widget_set_events(*drawing_area, GDK_EXPOSURE_MASK | GDK_BUTTON_PRESS_MASK | 
				GDK_BUTTON_RELEASE_MASK | GDK_BUTTON_MOTION_MASK);

	gtk_signal_connect(GTK_OBJECT(*drawing_area), "realize", GTK_SIGNAL_FUNC(realize), NULL);
	gtk_signal_connect(GTK_OBJECT(*drawing_area), "configure_event", GTK_SIGNAL_FUNC(configure_event), NULL);
	gtk_signal_connect(GTK_OBJECT(*drawing_area), "expose_event", GTK_SIGNAL_FUNC(expose_event), NULL);
	gtk_signal_connect(GTK_OBJECT(*drawing_area), "button_press_event", GTK_SIGNAL_FUNC(button_press_event), NULL);
	gtk_signal_connect(GTK_OBJECT(*drawing_area), "button_release_event", GTK_SIGNAL_FUNC(button_release_event), NULL);
	gtk_signal_connect(GTK_OBJECT(*drawing_area), "motion_notify_event", GTK_SIGNAL_FUNC(motion_notify_event), NULL);
#endif
}

void ShowBoard3d(BoardData *bd)
{
	pCurBoard = bd;
	gtk_widget_queue_draw(widget);
}

void CreateBoard3d(BoardData* bd, GtkWidget** drawing_area)
{
	pCurBoard = bd;

	CreateGLWidget(drawing_area);
	InitBoard3d(pCurBoard);

	widget = *drawing_area;
}

int InitGTK3d(int *argc, char ***argv)
{
#if USE_GTK2
	gtk_gl_init(argc, argv);

	/* Configure OpenGL-capable visual */
	glconfig = gdk_gl_config_new_by_mode(GDK_GL_MODE_RGB | GDK_GL_MODE_DEPTH | GDK_GL_MODE_DOUBLE | GDK_GL_MODE_STENCIL);
	if (glconfig == NULL)
#else
	if (gdk_gl_query() == FALSE)
#endif
	{
		g_print("*** Cannot find OpenGL-capable visual.\n");
		return 1;
	}

	return 0;
}

#endif
