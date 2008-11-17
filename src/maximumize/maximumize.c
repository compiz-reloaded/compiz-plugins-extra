/*
 * Compiz Fusion Maximumize plugin
 *
 * Copyright 2007-2008 Kristian Lyngstøl <kristian@bohemians.org>
 * Copyright 2008 Eduardo Gurgel Pinho <edgurgel@gmail.com>
 * Copyright 2008 Marco Diego Aurelio Mesquita <marcodiegomesquita@gmail.com>
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
 * Author(s):
 * Kristian Lyngstøl <kristian@bohemians.org>
 * Eduardo Gurgel <edgurgel@gmail.com>
 * Marco Diego Aurélio Mesquita <marcodiegomesquita@gmail.com>
 *
 * Description:
 *
 * Maximumize resizes a window so it fills as much of the free space in any
 * direction as possible without overlapping with other windows.
 *
 * Todo:
 *  - The algorithm moves one pixel at a time, it should be smarter.
 *  - Handle slight overlaps. 
 *  - Transform left,right,up,down booleans into:  int Direction
 */

#include <compiz-core.h>
#include "maximumize_options.h"

typedef struct _maximumizeSettings
{
    Bool    left;
    Bool    right;
    Bool    up;
    Bool    down;
    Bool    shrink; // Shrink and grow can both be executed.
    Bool    grow;   // Shrink will run first.
} MaxSet;

/* Returns true if rectangles a and b intersect by at least 40 in both
 * directions
 */
static Bool 
maximumizeSubstantialOverlap (XRectangle a,
			      XRectangle b)
{
    if (a.x + a.width <= b.x + 40) return False;
    if (b.x + b.width <= a.x + 40) return False;
    if (a.y + a.height <= b.y + 40) return False;
    if (b.y + b.height <= a.y + 40) return False;
    return True;
}

/* Generates a region containing free space (here the
 * active window counts as free space). The region argument
 * is the start-region (ie: the output dev).
 * Logic borrowed from opacify (courtesy of myself).
 */
static Region
maximumizeEmptyRegion (CompWindow *window,
		       Region     region)
{
    CompScreen *s = window->screen;
    CompWindow *w;
    Region     newRegion, tmpRegion;
    XRectangle tmpRect, windowRect;

    newRegion = XCreateRegion ();
    if (!newRegion)
	return NULL;

    tmpRegion = XCreateRegion ();
    if (!tmpRegion)
    {
	XDestroyRegion (newRegion);
	return NULL;
    }

    XUnionRegion (region, newRegion, newRegion);

    if (maximumizeGetIgnoreOverlapping (s->display)) {
	windowRect.x = window->serverX - window->input.left;
	windowRect.y = window->serverY - window->input.top;
	windowRect.width  = window->serverWidth + window->input.right + 
	    window->input.left;
	windowRect.height = window->serverHeight + window->input.top +
	    window->input.bottom;
    }

    for (w = s->windows; w; w = w->next)
    {
	EMPTY_REGION (tmpRegion);
        if (w->id == window->id)
            continue;

        if (w->invisible || w->hidden || w->minimized)
            continue;

	if (w->wmType & CompWindowTypeDesktopMask)
	    continue;

	if (w->wmType & CompWindowTypeDockMask)
	{
	    if (w->struts) 
	    {
		XUnionRectWithRegion (&w->struts->left, tmpRegion, tmpRegion);
		XUnionRectWithRegion (&w->struts->right, tmpRegion, tmpRegion);
		XUnionRectWithRegion (&w->struts->top, tmpRegion, tmpRegion);
		XUnionRectWithRegion (&w->struts->bottom, tmpRegion, tmpRegion);
		XSubtractRegion (newRegion, tmpRegion, newRegion);
	    }
	    continue;
	}

	if (maximumizeGetIgnoreSticky (s->display) && 
	    (w->state & CompWindowStateStickyMask) &&
	    !(w->wmType & CompWindowTypeDockMask))
	    continue;

	tmpRect.x = w->serverX - w->input.left;
	tmpRect.y = w->serverY - w->input.top;
	tmpRect.width  = w->serverWidth + w->input.right + w->input.left;
	tmpRect.height = w->serverHeight + w->input.top +
	                 w->input.bottom;

	if (maximumizeGetIgnoreOverlapping (s->display) &&
		maximumizeSubstantialOverlap (tmpRect, windowRect))
	    continue;
	XUnionRectWithRegion (&tmpRect, tmpRegion, tmpRegion);
	XSubtractRegion (newRegion, tmpRegion, newRegion);
    }

    XDestroyRegion (tmpRegion);
    
    return newRegion;
}

/* Returns true if box a has a larger area than box b.
 */
static Bool
maximumizeBoxCompare (BOX a,
		      BOX b)
{
    int areaA, areaB;

    areaA = (a.x2 - a.x1) * (a.y2 - a.y1);
    areaB = (b.x2 - b.x1) * (b.y2 - b.y1);

    return (areaA > areaB);
}

/* Extends the given box for Window w to fit as much space in region r.
 * If XFirst is true, it will first expand in the X direction,
 * then Y. This is because it gives different results. 
 * PS: Decorations are icky.
 */
static BOX
maximumizeExtendBox (CompWindow *w,
		     BOX        tmp,
		     Region     r,
		     Bool	xFirst,
		     MaxSet     mset)
{
    short int counter = 0;
    Bool      touch = FALSE;
#define CHECKREC \
	XRectInRegion (r, tmp.x1 - w->input.left, tmp.y1 - w->input.top,\
		       tmp.x2 - tmp.x1 + w->input.left + w->input.right,\
		       tmp.y2 - tmp.y1 + w->input.top + w->input.bottom)\
	    == RectangleIn

    while (counter < 1)
    {
	if ((xFirst && counter == 0) || (!xFirst && counter == 1))
	{
	    if (mset.left) 
	    {
		while (CHECKREC) {
		    tmp.x1--;
		    touch = TRUE;
		}
		if (touch) tmp.x1++;
	    }
	    touch = FALSE;
	    if (mset.right) {
		while (CHECKREC) {
		    tmp.x2++;
		    touch = TRUE;
		}
		if (touch) tmp.x2--;
	    }
	    touch = FALSE;
	    counter++;
	}

	if ((xFirst && counter == 1) || (!xFirst && counter == 0))
	{
	    if (mset.down) {
		while (CHECKREC) {
		    tmp.y2++;
		    touch = TRUE;
		}
		if (touch) tmp.y2--;
	    }
	    touch = FALSE;

	    if (mset.up) {
		while (CHECKREC) {
		    tmp.y1--;
		    touch = TRUE;
		}
		if (touch) tmp.y1++;
	    }
	    touch = FALSE;
	    counter++;
	}
    }		
#undef CHECKREC
    return tmp;
}

/* Create a box for resizing in the given region
 * Also shrinks the window box in case of minor overlaps.
 * FIXME: should be somewhat cleaner.
 */
static BOX
maximumizeFindRect (CompWindow *w,
		    Region     r,
		    MaxSet     mset)
{
    BOX windowBox, ansA, ansB, orig;

    windowBox.x1 = w->serverX;
    windowBox.x2 = w->serverX + w->serverWidth;
    windowBox.y1 = w->serverY;
    windowBox.y2 = w->serverY + w->serverHeight;

    orig = windowBox;

    if (windowBox.x2 - windowBox.x1 > 80)
    {
	if (mset.left)
	    windowBox.x1 += 40;
	if (mset.right)	
	    windowBox.x2 -= 40;
    }
    if (windowBox.y2 - windowBox.y1 > 80)
    {
	if (mset.up) 
	    windowBox.y1 += 40;
	if (mset.down)
	    windowBox.y2 -= 40;
    }

    ansA = maximumizeExtendBox (w, windowBox, r, TRUE, mset);
    ansB = maximumizeExtendBox (w, windowBox, r, FALSE, mset);

    if (maximumizeBoxCompare (orig, ansA) &&
	maximumizeBoxCompare (orig, ansB))
	return orig;
    if (maximumizeBoxCompare (ansA, ansB))
	return ansA;
    else
	return ansB;

}

/* Calls out to compute the resize */
static unsigned int
maximumizeComputeResize (CompWindow     *w,
			 XWindowChanges *xwc,
			 MaxSet	   mset)
{
    CompOutput   *output;
    Region       region;
    unsigned int mask = 0;
    BOX          box;

    output = &w->screen->outputDev[outputDeviceForWindow (w)];
    region = maximumizeEmptyRegion (w, &output->region);
    if (!region)
	return mask;
    
    box = maximumizeFindRect (w, region, mset);
    XDestroyRegion (region);

    if (box.x1 != w->serverX)
	mask |= CWX;

    if (box.y1 != w->serverY)
	mask |= CWY;

    if ((box.x2 - box.x1) != w->serverWidth)
	mask |= CWWidth;

    if ((box.y2 - box.y1) != w->serverHeight)
	mask |= CWHeight;

    xwc->x = box.x1;
    xwc->y = box.y1;
    xwc->width = box.x2 - box.x1; 
    xwc->height = box.y2 - box.y1;

    return mask;
}

/* Initially triggered key binding.
 * Fetch the window, fetch the resize, constrain it.
 */
static Bool
maximumizeTrigger (CompDisplay     *d,
		   CompAction      *action,
		   CompActionState state,
		   CompOption      *option,
		   int             nOption)
{
    Window     xid;
    CompWindow *w;
    CompScreen *s;
    int grabIndex = 0;   

    xid = getIntOptionNamed (option, nOption, "window", 0);
    w   = findWindowAtDisplay (d, xid);
    if (w)
    {
	int            width, height;
	unsigned int   mask;
	XWindowChanges xwc;
	MaxSet	       mset;

	s = w->screen;
	if (otherScreenGrabExist (s, "maximumize", 0))
	   return FALSE;
	/*
	 * Grab the screen.
	 */
	grabIndex = pushScreenGrab (s, s->invisibleCursor, "maximumize");
	if (!grabIndex)
	    return FALSE;

	mset.left = maximumizeGetMaximumizeLeft (w->screen->display);
	mset.right = maximumizeGetMaximumizeRight (w->screen->display);
	mset.up = maximumizeGetMaximumizeUp (w->screen->display);
	mset.down = maximumizeGetMaximumizeDown (w->screen->display);
    
	mask = maximumizeComputeResize (w, &xwc, mset); 
	if (mask)
	{
	    if (constrainNewWindowSize (w, xwc.width, xwc.height,
					&width, &height))
	    {
		mask |= CWWidth | CWHeight;
		xwc.width  = width;
		xwc.height = height;
	    }

	    if (w->mapNum && (mask & (CWWidth | CWHeight)))
		sendSyncRequest (w);

	    configureXWindow (w, mask, &xwc);
	}
    }
    /* Release the screen grab */
    if (grabIndex)
	removeScreenGrab (s,grabIndex, NULL);

    return TRUE;
}

/* 
 * Maximumazing on specified direction.
 */
static Bool
maximumizeTriggerDirection (CompDisplay     *d,
			    CompAction      *action,
			    CompActionState state,
			    CompOption      *option,
			    Bool            left,
			    Bool            right,
			    Bool            up,
			    Bool            down,
			    int             nOption)
{
    Window     xid;
    CompWindow *w;
    CompScreen *s;
    int grabIndex = 0;

    xid = getIntOptionNamed (option, nOption, "window", 0);
    w   = findWindowAtDisplay (d, xid);
    if (w)
    {
	int            width, height;
	unsigned int   mask;
	XWindowChanges xwc;
	MaxSet	       mset;

	s = w->screen;
	if (otherScreenGrabExist (s, "maximumize", 0))
	    return FALSE;

	/*
	 * Grab the screen.
	 */
	grabIndex = pushScreenGrab (s, s->invisibleCursor, "maximumize");
	if (!grabIndex)
	    return FALSE;

	mset.left = left;
	mset.right = right;
	mset.up = up;
	mset.down = down;

	mask = maximumizeComputeResize (w, &xwc, mset); 
	if (mask)
	{
	    if (constrainNewWindowSize (w, xwc.width, xwc.height,
					&width, &height))
	    {
		mask |= CWWidth | CWHeight;
		xwc.width  = width;
		xwc.height = height;
	    }

	    if (w->mapNum && (mask & (CWWidth | CWHeight)))
		sendSyncRequest (w);

	    configureXWindow (w, mask, &xwc);
	}
    }
    /* Release the screen grab */
    if (grabIndex)
	removeScreenGrab (s,grabIndex, NULL);
    return TRUE;
}

/* 
 * Maximumazing to left region limit.
 */
static Bool
maximumizeTriggerLeft (CompDisplay     *d,
		       CompAction      *action,
		       CompActionState state,
		       CompOption      *option,
		       int             nOption)
{
    return maximumizeTriggerDirection (d,action,state,option,TRUE, FALSE, FALSE, FALSE,nOption);

}

/* 
 * Maximumazing to right region limit.
 *
 */
static Bool
maximumizeTriggerRight (CompDisplay     *d,
			CompAction      *action,
			CompActionState state,
			CompOption      *option,
			int             nOption)
{
    return maximumizeTriggerDirection (d,action,state,option,FALSE, TRUE, FALSE, FALSE,nOption);
}

/* 
 * Maximumazing to upper region limit.
 *
 */
static Bool
maximumizeTriggerUp (CompDisplay     *d,
		     CompAction      *action,
		     CompActionState state,
		     CompOption      *option,
		     int             nOption)
{
    return maximumizeTriggerDirection (d,action,state,option,FALSE, FALSE, TRUE, FALSE,nOption);

}

/* 
 * Maximumazing to bottom region limit.
 *
 */
static Bool
maximumizeTriggerDown (CompDisplay     *d,
		       CompAction      *action,
		       CompActionState state,
		       CompOption      *option,
		       int             nOption)
{

    return maximumizeTriggerDirection (d,action,state,option,FALSE, FALSE, FALSE, TRUE,nOption);

}

/* 
 * Maximumazing horizontally.
 *
 */
static Bool
maximumizeTriggerHorizontally (CompDisplay     *d,
			       CompAction      *action,
			       CompActionState state,
			       CompOption      *option,
			       int             nOption)
{
    return maximumizeTriggerDirection (d,action,state,option,TRUE, TRUE, FALSE, FALSE,nOption);

}

/* 
 * Maximumazing vertically.
 *
 */
static Bool
maximumizeTriggerVertically (CompDisplay     *d,
			     CompAction      *action,
			     CompActionState state,
			     CompOption      *option,
			     int             nOption)
{
    return maximumizeTriggerDirection (d,action,state,option,FALSE, FALSE, TRUE, TRUE,nOption);

}

/* 
 * Maximumazing to upper left region limit.
 *
 */
static Bool
maximumizeTriggerUpLeft (CompDisplay     *d,
			 CompAction      *action,
			 CompActionState state,
			 CompOption      *option,
			 int             nOption)
{

    return maximumizeTriggerDirection (d,action,state,option,TRUE, FALSE, TRUE, FALSE,nOption);

}

/* 
 * Maximumazing to upper right region limit.
 *
 */
static Bool
maximumizeTriggerUpRight (CompDisplay     *d,
			  CompAction      *action,
			  CompActionState state,
			  CompOption      *option,
			  int             nOption)
{
    return maximumizeTriggerDirection (d,action,state,option,FALSE, TRUE, TRUE, FALSE,nOption);

}

/* 
 * Maximumazing to bottom left region limit.
 *
 */
static Bool
maximumizeTriggerDownLeft (CompDisplay     *d,
			   CompAction      *action,
			   CompActionState state,
			   CompOption      *option,
			   int             nOption)
{
    return maximumizeTriggerDirection (d,action,state,option,TRUE, FALSE, FALSE, TRUE,nOption);
}

/*
 * Maximumazing to bottom right region limit.
 *
 */

static Bool
maximumizeTriggerDownRight (CompDisplay     *d,
			    CompAction      *action,
			    CompActionState state,
			    CompOption      *option,
			    int             nOption)
{
    return maximumizeTriggerDirection (d,action,state,option,FALSE, TRUE, FALSE, TRUE,nOption);
}


/* Configuration, initialization, boring stuff. --------------------- */
static Bool
maximumizeInitDisplay (CompPlugin  *p,
		       CompDisplay *d)
{
    if (!checkPluginABI ("core", CORE_ABIVERSION))
	return FALSE;

    maximumizeSetTriggerKeyInitiate (d, maximumizeTrigger);
    maximumizeSetTriggerLeftInitiate (d, maximumizeTriggerLeft);
    maximumizeSetTriggerRightInitiate (d, maximumizeTriggerRight);
    maximumizeSetTriggerUpInitiate (d, maximumizeTriggerUp);
    maximumizeSetTriggerDownInitiate (d, maximumizeTriggerDown);
    maximumizeSetTriggerHorizontallyInitiate (d, maximumizeTriggerHorizontally);
    maximumizeSetTriggerVerticallyInitiate (d, maximumizeTriggerVertically);
    maximumizeSetTriggerUpLeftInitiate (d, maximumizeTriggerUpLeft);
    maximumizeSetTriggerUpRightInitiate (d, maximumizeTriggerUpRight);
    maximumizeSetTriggerDownLeftInitiate (d, maximumizeTriggerDownLeft);
    maximumizeSetTriggerDownRightInitiate (d, maximumizeTriggerDownRight);

    return TRUE;
}

static CompBool
maximumizeInitObject (CompPlugin *p,
		      CompObject *o)
{
    static InitPluginObjectProc dispTab[] = {
	(InitPluginObjectProc) 0, /* InitCore */
	(InitPluginObjectProc) maximumizeInitDisplay,
	0, 
	0 
    };

    RETURN_DISPATCH (o, dispTab, ARRAY_SIZE (dispTab), TRUE, (p, o));
}

static void
maximumizeFiniObject (CompPlugin *p,
		      CompObject *o)
{
    static FiniPluginObjectProc dispTab[] = {
	(FiniPluginObjectProc) 0, /* FiniCore */
	0, 
	0, 
	0 
    };

    DISPATCH (o, dispTab, ARRAY_SIZE (dispTab), (p, o));
}

CompPluginVTable maximumizeVTable = {
    "maximumize",
    0,
    0,
    0,
    maximumizeInitObject,
    maximumizeFiniObject,
    0,
    0
};

CompPluginVTable*
getCompPluginInfo (void)
{
    return &maximumizeVTable;
}

