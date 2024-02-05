/*
 * Compiz Grid plugin
 *
 * Copyright (c) 2008 Stephen Kennedy <suasol@gmail.com>
 * Copyright (c) 2010 Scott Moreau <oreaus@gmail.com>
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * Description:
 *
 * Plugin to act like winsplit revolution (http://www.winsplit-revolution.com/)
 * use <Control><Alt>NUMPAD_KEY to move and tile your windows.
 * 
 * Press the tiling keys several times to cycle through some tiling options.
 */

#include <compiz-core.h>
#include <stdlib.h>
#include <string.h>
#include "grid_options.h"

#define GRID_DEBUG 0

#if GRID_DEBUG
#   include <stdio.h>
	static FILE* gridOut;
#   define DEBUG_RECT(VAR) fprintf(gridOut, #VAR " %i %i %i %i\n", VAR.x, VAR.y, VAR.width, VAR.height)
#   define DEBUG_PRINT(ARGS) fprintf ARGS
#else
#   define DEBUG_RECT(VAR)
#   define DEBUG_PRINT(ARGS)
#endif

static int displayPrivateIndex;

typedef enum
{
    GridUnknown = 0,
    GridBottomLeft = 1,
    GridBottom = 2,
    GridBottomRight = 3,
    GridLeft = 4,
    GridCenter = 5,
    GridRight = 6,
    GridTopLeft = 7,
    GridTop = 8,
    GridTopRight = 9,
    GridMaximize = 10,
} GridType;


typedef enum
{
    NoEdge = 0,
    BottomLeft,
    Bottom,
    BottomRight,
    Left,
    Right,
    TopLeft,
    Top,
    TopRight,
} EdgeType;

typedef struct _GridProps
{
    int gravityRight;
    int gravityDown;
    int numCellsX;
    int numCellsY;
} GridProps;

static const GridProps gridProps[] =
{
    {0,1, 1,1},

    {0,1, 2,2},
    {0,1, 1,2},
    {1,1, 2,2},

    {0,0, 2,1},
    {0,0, 1,1},
    {1,0, 2,1},

    {0,0, 2,2},
    {0,0, 1,2},
    {1,0, 2,2},
};

typedef struct _Animation
{
    GLfloat progress;
    XRectangle fromRect;
    XRectangle targetRect;
    XRectangle currentRect;
    GLfloat opacity;
    GLfloat timer;
    int duration;
    Bool complete;
    Bool fadingOut;
} Animation;

typedef struct _GridDisplay {
    int		    screenPrivateIndex;
    HandleEventProc handleEvent;
} GridDisplay;

typedef struct _GridScreen
{
    WindowGrabNotifyProc   windowGrabNotify;
    WindowUngrabNotifyProc windowUngrabNotify;
    PaintOutputProc	   paintOutput;
    PreparePaintScreenProc preparePaintScreen;

    Bool grabIsMove;
    EdgeType edge, lastEdge;
    XRectangle     workarea;
    XRectangle     desiredSlot;
    XRectangle     desiredRect;
    XRectangle     currentRect;
    GridProps      props;
    CompWindow *w;
    int lastOutput;
    Bool drawing;
    Animation anim;
    Bool animating;
} GridScreen;

#define GET_GRID_DISPLAY(d)					       \
    ((GridDisplay *) (d)->base.privates[displayPrivateIndex].ptr)

#define GRID_DISPLAY(d)		           \
    GridDisplay *gd = GET_GRID_DISPLAY (d)

#define GET_GRID_SCREEN(s, gd)					       \
    ((GridScreen *) (s)->base.privates[(gd)->screenPrivateIndex].ptr)

#define GRID_SCREEN(s)						       \
    GridScreen *gs = GET_GRID_SCREEN (s, GET_GRID_DISPLAY (s->display))

static void
slotToRect (CompWindow *w,
	    XRectangle *slot,
	    XRectangle *rect)
{
    rect->x = slot->x + w->input.left;
    rect->y = slot->y + w->input.top;
    rect->width = slot->width - (w->input.left + w->input.right);
    rect->height = slot->height - (w->input.top + w->input.bottom);
}

static void
constrainSize (CompWindow *w,
	       XRectangle *slot,
	       XRectangle *rect)
{
    GRID_SCREEN (w->screen);
    XRectangle r;
    int        cw, ch;

    slotToRect (w, slot, &r);

    if (constrainNewWindowSize (w, r.width, r.height, &cw, &ch))
    {
	/* constrained size may put window offscreen, adjust for that case */
	int dx = r.x + cw - gs->workarea.width - gs->workarea.x + w->input.right;
	int dy = r.y + ch - gs->workarea.height - gs->workarea.y + w->input.bottom;

	if ( dx > 0 )
	    r.x -= dx;
	if ( dy > 0 )
	    r.y -= dy;

	r.width = cw;
	r.height = ch;
    }

    *rect = r;
}


static void
getTargetRect (CompWindow *cw,
	       GridType	  where,
	       Bool setWorkarea)
{
    GRID_SCREEN (cw->screen);

    gs->props = gridProps[where];

    DEBUG_PRINT ((gridOut, "\nPressed KP_%i\n", where));

    /* get current available area */
    if (setWorkarea)
		getWorkareaForOutput (cw->screen, outputDeviceForWindow(cw), &gs->workarea);
    DEBUG_RECT (gs->workarea);

    /* Convention:
     * xxxSlot include decorations (it's the screen area occupied)
     * xxxRect are undecorated (it's the constrained position
				of the contents)
     */

    /* slice and dice to get desired slot - including decorations */
    gs->desiredSlot.y = gs->workarea.y + gs->props.gravityDown *
	                (gs->workarea.height / gs->props.numCellsY);
    gs->desiredSlot.height = gs->workarea.height / gs->props.numCellsY;
    gs->desiredSlot.x = gs->workarea.x + gs->props.gravityRight *
	                (gs->workarea.width / gs->props.numCellsX);
    gs->desiredSlot.width = gs->workarea.width / gs->props.numCellsX;
    DEBUG_RECT (gs->desiredSlot);

    /* Adjust for constraints and decorations */
    constrainSize (cw, &gs->desiredSlot, &gs->desiredRect);
    DEBUG_RECT (gs->desiredRect);
}

/* just keeping this for reference, but can use maximizeWindow instead */
static void sendMaximizationRequest (CompWindow *w)
{
    XEvent xev;
    CompScreen *s = w->screen;
    CompDisplay *d = s->display;

    xev.xclient.type = ClientMessage;
    xev.xclient.display = d->display;
    xev.xclient.format = 32;

    xev.xclient.message_type = d->winStateAtom;
    xev.xclient.window = w->id;

    xev.xclient.data.l[0] = 1;
    xev.xclient.data.l[1] = d->winStateMaximizedHorzAtom;
    xev.xclient.data.l[2] = d->winStateMaximizedVertAtom;
    xev.xclient.data.l[3] = 0;
    xev.xclient.data.l[4] = 0;

    XSendEvent (d->display, s->root, FALSE,
    SubstructureRedirectMask | SubstructureNotifyMask, &xev);
}


static void
gridCommonWindow (CompWindow *cw,
                  GridType   where,
                  Bool setWorkarea)
{
    if ((cw) && (where != GridUnknown))
    {
	GRID_SCREEN (cw->screen);

	/* add maximize option */
	if (where == GridMaximize)
	{
	    sendMaximizationRequest (cw);
	    /* maximizeWindow (cw, MAXIMIZE_STATE); */
	}
	else
	{
	    unsigned int valueMask = 0;
	    int desiredState = 0;

	    getTargetRect (cw, where, setWorkarea);

	    XWindowChanges xwc;

	    /* if keys are pressed again then cycle through 1/3 or 2/3 widths... */

	    /* Get current rect not including decorations */
	    gs->currentRect.x = cw->serverX;
	    gs->currentRect.y = cw->serverY;
	    gs->currentRect.width  = cw->serverWidth;
	    gs->currentRect.height = cw->serverHeight;
	    DEBUG_RECT (gs->currentRect);

	    if ((gs->desiredRect.y == gs->currentRect.y + cw->clientFrame.top &&
			gs->desiredRect.height == gs->currentRect.height - (cw->clientFrame.top + cw->clientFrame.bottom)) &&
			gridGetCycleSizes(cw->screen->display))
	    {
		int slotWidth33 = gs->workarea.width / 3;
		int slotWidth66 = gs->workarea.width - slotWidth33;

		DEBUG_PRINT ((gridOut, "Multi!\n"));

		if (gs->props.numCellsX == 2) /* keys (1, 4, 7, 3, 6, 9) */
		{
		    if (gs->currentRect.width == gs->desiredRect.width +
		        cw->clientFrame.left + cw->clientFrame.right &&
			    gs->currentRect.x == gs->desiredRect.x - cw->clientFrame.left)
		    {
			gs->desiredSlot.width = slotWidth66;
			gs->desiredSlot.x = gs->workarea.x +
					    gs->props.gravityRight * slotWidth33;
		    }
		    else
		    {
			/* tricky, have to allow for window constraints when
			 * computing what the 33% and 66% offsets would be
			 */
			XRectangle rect33, rect66, slot33, slot66;

			slot33 = gs->desiredSlot;
			slot33.x = gs->workarea.x +
				   gs->props.gravityRight * slotWidth66;
			slot33.width = slotWidth33;
			constrainSize (cw, &slot33, &rect33);
			DEBUG_RECT (slot33);
			DEBUG_RECT (rect33);

			slot66 = gs->desiredSlot;
			slot66.x = gs->workarea.x +
				   gs->props.gravityRight * slotWidth33;
			slot66.width = slotWidth66;
			constrainSize (cw, &slot66, &rect66);
			DEBUG_RECT (slot66);
			DEBUG_RECT (rect66);

			if (gs->currentRect.width == (rect66.width +
			    cw->clientFrame.left + cw->clientFrame.right) &&
			    gs->currentRect.x == rect66.x - cw->clientFrame.left)
			{
			    gs->desiredSlot.width = slotWidth33;
			    gs->desiredSlot.x = gs->workarea.x +
						gs->props.gravityRight * slotWidth66;
			}
		    }
		}
		else /* keys (2, 5, 8) */
		{
		    if (gs->currentRect.width == (gs->desiredRect.width +
			    cw->clientFrame.left + cw->clientFrame.right) &&
			    gs->currentRect.x == gs->desiredRect.x - cw->clientFrame.left)
		    {
			gs->desiredSlot.width = slotWidth33;
			gs->desiredSlot.x = gs->workarea.x + slotWidth33;
		    }
		}
		constrainSize (cw, &gs->desiredSlot, &gs->desiredRect);
	        DEBUG_RECT (gs->desiredRect);
	    }

	    /*Note that ClientFrame extents must also be computed in the "if"
	     *blocks for cycling through window sizes or the checks fail
	     */
	    xwc.x = gs->desiredRect.x - cw->clientFrame.left;
	    xwc.y = gs->desiredRect.y - cw->clientFrame.top;
	    xwc.width  = gs->desiredRect.width + cw->clientFrame.left + cw->clientFrame.right;
	    xwc.height = gs->desiredRect.height + cw->clientFrame.top + cw->clientFrame.bottom;

	    if (where == GridRight || where == GridLeft)
	    {
		desiredState = CompWindowStateMaximizedVertMask;
		valueMask = CWX;
	    }
	    else if (where == GridTop || where == GridBottom) 
	    {
		desiredState = CompWindowStateMaximizedHorzMask;
		valueMask = CWY;
	    }
	    else
	    {
		desiredState = 0;
		valueMask = CWX | CWY;
            }

            if (xwc.width != cw->serverWidth)
                valueMask |= CWWidth;
            if (xwc.height != cw->serverHeight)
                valueMask |= CWHeight;

            if (cw->mapNum && (valueMask & (CWWidth | CWHeight))) 
                sendSyncRequest (cw);

	    if (cw->state != desiredState)
		maximizeWindow (cw, desiredState);

	    /* TODO: animate move+resize */
	    configureXWindow (cw, valueMask, &xwc);

	}
    }
}

static Bool
gridCommon (CompDisplay	    *d,
	    CompAction	    *action,
	    CompActionState state,
	    CompOption	    *option,
	    int		    nOption,
	    GridType	    where)
{
    Window     xid;
    CompWindow *cw;

    xid = getIntOptionNamed (option, nOption, "window", 0);
    cw  = findWindowAtDisplay (d, xid);

    gridCommonWindow(cw, where, TRUE);

    return TRUE;
}

static GridType
edgeToGridType (CompDisplay *d,
		EdgeType edge)
{
    GridType ret = GridUnknown;

    switch (edge)
    {
	case Left:
	    ret = (GridType) gridGetLeftEdgeAction (d);
	    break;
	case Right:
	    ret = (GridType) gridGetRightEdgeAction (d);
	    break;
	case Top:
	     ret = (GridType) gridGetTopEdgeAction (d);
	     break;
	case Bottom:
	    ret = (GridType) gridGetBottomEdgeAction (d);
	    break;
	case TopLeft:
	    ret = (GridType) gridGetTopLeftCornerAction (d);
	    break;
	case TopRight:
	    ret = (GridType) gridGetTopRightCornerAction (d);
	    break;
	case BottomLeft:
	    ret = (GridType) gridGetBottomLeftCornerAction (d);
	    break;
	case BottomRight:
	    ret = (GridType) gridGetBottomRightCornerAction (d);
	    break;
	case NoEdge:
	default:
	    ret = -1;
	    break;
    }

    return ret;
}

static void
gridHandleEvent (CompDisplay *d,
		 XEvent      *event)
{
    GridType     where;
    GRID_DISPLAY (d);

    if (event->type != MotionNotify)
		goto out;

	CompScreen *s;
	s = findScreenAtDisplay (d, event->xmotion.root);

	if (!s)
		goto out;

    GRID_SCREEN (s);

    if (!gs->grabIsMove)
		goto out;

	int o = gridGetOutputSelectMousePointer(d) ?
			outputDeviceForPoint (s, pointerX, pointerY) :
			outputDeviceForWindow (gs->w);
	BOX extents = s->outputDev[o].region.extents;
	if (gridGetOutputSelectMousePointer(d))
		getWorkareaForOutput (s, o, &gs->workarea);

	Bool top = (pointerY < extents.y1 + gridGetTopEdgeThreshold(d) &&
				pointerY > extents.y1 - gridGetBottomEdgeThreshold(d));
	Bool bottom = (pointerY > (extents.y2 - gridGetBottomEdgeThreshold(d)) &&
					pointerY < (extents.y2 + gridGetTopEdgeThreshold(d)));
	Bool left = (pointerX < extents.x1 + gridGetLeftEdgeThreshold(d) &&
				pointerX > extents.x1 - gridGetRightEdgeThreshold(d));
	Bool right = (pointerX > (extents.x2 - gridGetRightEdgeThreshold(d)) &&
				 pointerX < (extents.x2 + gridGetLeftEdgeThreshold(d)));

	/* detect corners first */
	if (top)
	{
		if (left)
			gs->edge = TopLeft;
		else if (right)
			gs->edge = TopRight;
		else
			gs->edge = Top;
	} else
	if (bottom)
	{
		if (left)
			gs->edge = BottomLeft;
		else if (right)
			gs->edge = BottomRight;
		else
			gs->edge = Bottom;
	} else
	if (left)
		gs->edge = Left;
	else if (right)
		gs->edge = Right;
	else
		gs->edge = NoEdge;

	/* detect edge region change */
	if (gs->lastEdge != gs->edge || o != gs->lastOutput)
	{
		if (gs->edge != NoEdge)
		{
		where = edgeToGridType(d, gs->edge);

		/* treat Maximize visual indicator same as GridCenter */
		if (where == GridMaximize)
			where=GridCenter;

		/* Do not show animation for GridUnknown (no action) */
		if (where == GridUnknown)
			return;

		getTargetRect (gs->w, where, !gridGetOutputSelectMousePointer(d));

		gs->anim.duration = gridGetAnimationDuration (d);
		gs->anim.timer = gs->anim.duration;
		gs->anim.opacity = 0.0f;
		gs->anim.progress = 0.0f;
		gs->anim.currentRect.x = gs->w->serverX;
		gs->anim.currentRect.y = gs->w->serverY;
		gs->anim.currentRect.width = gs->w->serverWidth;
		gs->anim.currentRect.height = gs->w->serverHeight;
		gs->anim.targetRect = gs->desiredSlot;
		gs->anim.fromRect.x = gs->w->serverX - gs->w->input.left;
		gs->anim.fromRect.y = gs->w->serverY - gs->w->input.top;
		gs->anim.fromRect.width = gs->w->serverWidth +
					  gs->w->input.left +
					  gs->w->input.right +
					  gs->w->serverBorderWidth * 2;
		gs->anim.fromRect.height = gs->w->serverHeight +
					   gs->w->input.top +
					   gs->w->input.bottom +
					   gs->w->serverBorderWidth * 2;
		gs->animating = TRUE;
		gs->anim.fadingOut = FALSE;
		}
		else
		gs->anim.fadingOut = TRUE;

		gs->lastEdge = gs->edge;
	}
	gs->lastOutput = o;

out:
    UNWRAP (gd, d, handleEvent);
    (*d->handleEvent) (d, event);
    WRAP (gd, d, handleEvent, gridHandleEvent);
}

static void
gridWindowGrabNotify (CompWindow   *w,
		      int          x,
		      int          y,
		      unsigned int state,
		      unsigned int mask)
{
    CompScreen *s = w->screen;

    GRID_SCREEN (s);

    if (mask & CompWindowGrabMoveMask)
    {
	gs->grabIsMove = TRUE;
	gs->w = w;
	gs->lastOutput = outputDeviceForWindow (w);
    }

    UNWRAP (gs, s, windowGrabNotify);
    (*s->windowGrabNotify) (w, x, y, state, mask);
    WRAP (gs, s, windowGrabNotify, gridWindowGrabNotify);
}

static void
gridWindowUngrabNotify (CompWindow *w)
{
    CompScreen *s = w->screen;
    CompDisplay *d = s->display;

    GRID_SCREEN (s);

    if (gs->grabIsMove)
    {
	gs->grabIsMove = FALSE;

	if (gs->edge != NoEdge)
	{
	    gridCommonWindow (w, edgeToGridType(d, gs->edge), FALSE);
	    gs->anim.fadingOut = TRUE;
	}
    }

    gs->edge = NoEdge;
    gs->lastEdge = NoEdge;

    UNWRAP (gs, s, windowUngrabNotify);
    (*s->windowUngrabNotify) (w);
    WRAP (gs, s, windowUngrabNotify, gridWindowUngrabNotify);
}

static int
applyProgress (int a, int b, float progress)
{
    return a < b ?
	   b - (abs (a - b) * progress) :
	   b + (abs (a - b) * progress);
}

static void
setCurrentRect (CompScreen *s)
{
    GRID_SCREEN (s);

    gs->anim.currentRect.x = applyProgress (gs->anim.targetRect.x,
					    gs->anim.fromRect.x,
					    gs->anim.progress);
    gs->anim.currentRect.width = applyProgress (gs->anim.targetRect.width,
						gs->anim.fromRect.width,
						gs->anim.progress);
    gs->anim.currentRect.y = applyProgress (gs->anim.targetRect.y,
					    gs->anim.fromRect.y,
					    gs->anim.progress);
    gs->anim.currentRect.height = applyProgress (gs->anim.targetRect.height,
						 gs->anim.fromRect.height,
						 gs->anim.progress);
}

static void
glPaintRectangle (CompScreen		  *s,
		  const ScreenPaintAttrib *sAttrib,
		  const CompTransform	  *transform,
		  CompOutput		  *output)
{
    float alpha = 0;

    GRID_SCREEN (s);

    BoxRec      rect;

    setCurrentRect (s);

    rect.x1=gs->anim.currentRect.x;
    rect.y1=gs->anim.currentRect.y;
    rect.x2=gs->anim.currentRect.x + gs->anim.currentRect.width;
    rect.y2=gs->anim.currentRect.y + gs->anim.currentRect.height;
    CompTransform sTransform = *transform;

    /* rect = desiredSlot;*/

    glPushMatrix ();

    transformToScreenSpace (s, output, -DEFAULT_Z_CAMERA, &sTransform);

    glLoadMatrixf (sTransform.m);

    glDisableClientState (GL_TEXTURE_COORD_ARRAY);
    glEnable (GL_BLEND);

    /* fill rectangle */
    /* TODO: have multiple animations
       for (iter = animations.begin (); iter != animations.end () && animating; iter++)
       { */

    alpha = ((float) gridGetFillColorAlpha (s->display) / 65535.0f) *
	     gs->anim.opacity;

    glColor4f (((float) gridGetFillColorRed (s->display) / 65535.0f) * alpha,
	       ((float) gridGetFillColorGreen (s->display) / 65535.0f) * alpha,
	       ((float) gridGetFillColorBlue (s->display) / 65535.0f) * alpha,
	       alpha);

    glRecti (rect.x1, rect.y2, rect.x2, rect.y1);

    /* draw outline */

    alpha = ((float) gridGetOutlineColorAlpha (s->display) / 65535.0f) *
	     gs->anim.opacity;

    glColor4f (((float) gridGetOutlineColorRed (s->display) / 65535.0f) * alpha,
	       ((float) gridGetOutlineColorGreen (s->display) / 65535.0f) * alpha,
	       ((float) gridGetOutlineColorBlue (s->display) / 65535.0f) * alpha,
	       alpha);

    int thickness = gridGetOutlineThickness (s->display);
    glLineWidth (thickness);
    glBegin (GL_LINE_LOOP);

    /* set outline rect smaller to avoid damage issues */
    /* TODO: maybe get a better way of doing this */
    float half_thickness = thickness * 0.5;
    glVertex2f (rect.x1 + half_thickness, rect.y1 + half_thickness);
    glVertex2f (rect.x2 - half_thickness, rect.y1 + half_thickness);
    glVertex2f (rect.x2 - half_thickness, rect.y2 - half_thickness);
    glVertex2f (rect.x1 + half_thickness, rect.y2 - half_thickness);
    glEnd ();

    /* clean up */
    glColor4usv (defaultColor);
    glDisable (GL_BLEND);
    glEnableClientState (GL_TEXTURE_COORD_ARRAY);
    glPopMatrix ();
}

static void
damagePaintRegion (CompScreen *s)
{
    REGION reg;
    int    x, y;

    GRID_SCREEN (s);

    /* if (!is->fadeTime && !is->drawing)
       return; */

    x = gs->anim.currentRect.x;
    y = gs->anim.currentRect.y;

    reg.rects    = &reg.extents;
    reg.numRects = 1;

    reg.extents.x1 = x - 5;
    reg.extents.y1 = y - 5;
    reg.extents.x2 = x + gs->anim.currentRect.width + 5;
    reg.extents.y2 = y + gs->anim.currentRect.height + 5;

    damageScreenRegion (s, &reg);
}

static Bool
gridPaintOutput (CompScreen		 *s,
		 const ScreenPaintAttrib *sAttrib,
		 const CompTransform	 *transform,
		 Region			 region,
		 CompOutput		 *output,
		 unsigned int		 mask)
{
    Bool status;

    GRID_SCREEN (s);

    UNWRAP (gs, s, paintOutput);
    status = (*s->paintOutput) (s, sAttrib, transform, region, output, mask);
    WRAP (gs, s, paintOutput, gridPaintOutput);

    if (gs->animating && gridGetDrawIndicator (s->display))
    {
	glPaintRectangle (s, sAttrib, transform, output);
	damagePaintRegion (s);
    }

    return status;
}


/* handle the fade in /fade out */
static void
gridPreparePaintScreen (CompScreen *s,
			int        ms)
{
    GRID_SCREEN (s);

    if (gs->animating)
    {
	gs->anim.timer -= ms;

	if (gs->anim.timer < 0)
	    gs->anim.timer = 0;

	if (gs->anim.fadingOut)
	    gs->anim.opacity -= ms * 0.002;
	else
	{
	    if (gs->anim.opacity < 1.0f)
		gs->anim.opacity = gs->anim.progress * gs->anim.progress;
	    else
		gs->anim.opacity = 1.0f;
	}

	if (gs->anim.opacity < 0)
	{
	    gs->anim.opacity = 0.0f;
	    gs->anim.fadingOut = FALSE;
	    gs->anim.complete = TRUE;
	    gs->animating = FALSE;
	}

	gs->anim.progress = (gs->anim.duration - gs->anim.timer) / gs->anim.duration;
    }

    UNWRAP (gs, s, preparePaintScreen);
    (*s->preparePaintScreen) (s, ms);
    WRAP (gs, s, preparePaintScreen, gridPreparePaintScreen);
}

#define HANDLER(WHERE)                                        \
    static Bool                                               \
	grid##WHERE(CompDisplay	    *d,                       \
		    CompAction      *action,                  \
		    CompActionState state,                    \
		    CompOption      *option,                  \
		    int             nOption)                  \
	{                                                     \
	    return gridCommon (d, action, state,              \
			       option, nOption, Grid##WHERE); \
	}

HANDLER (BottomLeft)
HANDLER (Bottom)
HANDLER (BottomRight)
HANDLER (Left)
HANDLER (Center)
HANDLER (Right)
HANDLER (TopLeft)
HANDLER (Top)
HANDLER (TopRight)
HANDLER (Maximize)

#undef HANDLER

/* Configuration, initialization, boring stuff. */

static Bool
gridInitDisplay (CompPlugin  *p,
		 CompDisplay *d)
{
    GridDisplay *gd;

    if (!checkPluginABI ("core", CORE_ABIVERSION))
	return FALSE;

    gridSetPutCenterKeyInitiate (d, gridCenter);
    gridSetPutLeftKeyInitiate (d, gridLeft);
    gridSetPutRightKeyInitiate (d, gridRight);
    gridSetPutTopKeyInitiate (d, gridTop);
    gridSetPutBottomKeyInitiate (d, gridBottom);
    gridSetPutTopleftKeyInitiate (d, gridTopLeft);
    gridSetPutToprightKeyInitiate (d, gridTopRight);
    gridSetPutBottomleftKeyInitiate (d, gridBottomLeft);
    gridSetPutBottomrightKeyInitiate (d, gridBottomRight);
    gridSetPutMaximizeKeyInitiate (d, gridMaximize);

    gd = malloc (sizeof (GridDisplay));
    if (!gd)
	return FALSE;

    gd->screenPrivateIndex = allocateScreenPrivateIndex (d);
    if (gd->screenPrivateIndex < 0)
    {
	free (gd);
	return FALSE;
    }

    WRAP (gd, d, handleEvent, gridHandleEvent);

    d->base.privates[displayPrivateIndex].ptr = gd;

    return TRUE;
}

static void
gridFiniDisplay (CompPlugin  *p,
		 CompDisplay *d)
{
    GRID_DISPLAY (d);

    freeScreenPrivateIndex (d, gd->screenPrivateIndex);

    UNWRAP (gd, d, handleEvent);

    free (gd);
}

static Bool
gridInitScreen (CompPlugin *p,
		CompScreen *s)
{
    GridScreen * gs;

    GRID_DISPLAY (s->display);

    gs = malloc (sizeof (GridScreen));
    if (!gs)
	return FALSE;

    gs->grabIsMove = FALSE;
    gs->edge = NoEdge;
    gs->lastEdge = NoEdge;
    gs->drawing  = FALSE;

    gs->w = 0;

    gs->anim.progress = 0.0f;
    gs->anim.fromRect.x = 0;
    gs->anim.fromRect.y = 0;
    gs->anim.fromRect.width = 0;
    gs->anim.fromRect.height =0;
    gs->anim.targetRect.x = 0;
    gs->anim.targetRect.y = 0;
    gs->anim.targetRect.width = 0;
    gs->anim.targetRect.height = 0;
    gs->anim.currentRect.x = 0;
    gs->anim.currentRect.y = 0;
    gs->anim.currentRect.width = 0;
    gs->anim.currentRect.height = 0;
    gs->anim.opacity = 0.5f;
    gs->anim.timer = 0.0f;
    gs->anim.duration = 0;
    gs->anim.complete = FALSE;
    gs->anim.fadingOut = FALSE;

    gs->animating=FALSE;

    WRAP (gs, s, windowGrabNotify, gridWindowGrabNotify);
    WRAP (gs, s, windowUngrabNotify, gridWindowUngrabNotify);
    WRAP (gs, s, paintOutput, gridPaintOutput);
    WRAP (gs, s, preparePaintScreen, gridPreparePaintScreen);

    s->base.privates[gd->screenPrivateIndex].ptr = gs;

    return TRUE;
}

static void
gridFiniScreen (CompPlugin *p,
		CompScreen *s)
{
    GRID_SCREEN (s);

    UNWRAP (gs, s, windowGrabNotify);
    UNWRAP (gs, s, windowUngrabNotify);
    UNWRAP (gs, s, paintOutput);
    UNWRAP (gs, s, preparePaintScreen);

    free (gs);
}

static CompBool
gridInitObject (CompPlugin *p,
		CompObject *o)
{
    static InitPluginObjectProc dispTab[] = {
	(InitPluginObjectProc) 0, /* InitCore */
	(InitPluginObjectProc) gridInitDisplay,
	(InitPluginObjectProc) gridInitScreen
    };

    RETURN_DISPATCH (o, dispTab, ARRAY_SIZE (dispTab), TRUE, (p, o));
}

static void
gridFiniObject (CompPlugin *p,
		CompObject *o)
{
    static FiniPluginObjectProc dispTab[] = {
	   (FiniPluginObjectProc) 0, /* FiniCore */
	   (FiniPluginObjectProc) gridFiniDisplay,
	   (FiniPluginObjectProc) gridFiniScreen
    };

    DISPATCH (o, dispTab, ARRAY_SIZE (dispTab), (p, o));
}

static Bool
gridInitPlugin (CompPlugin *p)
{
#if GRID_DEBUG
    gridOut = fopen("/tmp/grid.log", "w");
    setlinebuf(gridOut);
#endif

    displayPrivateIndex = allocateDisplayPrivateIndex ();
    if (displayPrivateIndex < 0)
	return FALSE;

    return TRUE;
}

static void
gridFiniPlugin (CompPlugin *p)
{
#if GRID_DEBUG
    fclose(gridOut);
    gridOut = NULL;
#endif

    freeDisplayPrivateIndex(displayPrivateIndex);
}

CompPluginVTable gridVTable =
{
    "grid",
    0,
    gridInitPlugin,
    gridFiniPlugin,
    gridInitObject,
    gridFiniObject,
    0,
    0
};

CompPluginVTable *
getCompPluginInfo ()
{
    return &gridVTable;
}
