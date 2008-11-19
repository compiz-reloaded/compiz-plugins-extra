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

    if (maximumizeGetIgnoreOverlapping (s->display)) 
    {
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

/* If a window, with decorations, defined by tmp and w is still in free
 * space, evaluate to true. 
 */
#define CHECKREC \
	XRectInRegion (r, tmp->x1 - w->input.left, tmp->y1 - w->input.top,\
		       tmp->x2 - tmp->x1 + w->input.left + w->input.right,\
		       tmp->y2 - tmp->y1 + w->input.top + w->input.bottom)\
	    == RectangleIn


/* Convenience macros to make the code more readable (hopefully) */
#define REDUCE -1
#define INCREASE 1

/* While the rectangle has space, add inc to i. When it CHEKCREC fails (ie:
 * we overstepped our boundaries), reduce i by inc; undo the last change.
 * inc is either 1 or -1, but could easily be looped over for fun and
 * profit. (Ie: start with -100, hit the wall, go with -20, then -1, etc.)
 * 
 * NOTE: We have to pass along tmp, r and w for CHECKREC. 
 * FIXME:  
 */
static void
growGeneric (CompWindow	     *w,
	     BOX	     *tmp,
	     Region	     r,
	     short int       *i,
	     const short int inc)
{
    Bool touch = FALSE;
    while (CHECKREC) 
    {
	*i += inc;
	touch = TRUE;
    }
    if (touch)
	*i -= inc;
}

#undef CHECKREC

/* Grow a box in the left/right directions as much as possible without
 * overlapping other relevant windows.  */
static inline void
growWidth (CompWindow	*w, 
	   BOX		*tmp, 
	   Region       r, 
	   const MaxSet mset)
{
    if (mset.left) 
	growGeneric (w, tmp, r, &tmp->x1, REDUCE);

    if (mset.right) 
	growGeneric (w, tmp, r, &tmp->x2, INCREASE);
}

/* Grow box in the up/down direction as much as possible without
 * overlapping other relevant windows. */
static inline void
growHeight (CompWindow   *w, 
	    BOX		 *tmp, 
	    Region	 r, 
	    const MaxSet mset)
{
    if (mset.down) 
	growGeneric (w, tmp, r, &tmp->y2, INCREASE);
    
    if (mset.up)
	growGeneric (w, tmp, r, &tmp->y1, REDUCE);
}

/* Extends the given box for Window w to fit as much space in region r.
 * If XFirst is true, it will first expand in the X direction,
 * then Y. This is because it gives different results. 
 */
static BOX
maximumizeExtendBox (CompWindow   *w,
		     BOX	  tmp,
		     Region	  r,
		     Bool	  xFirst,
		     const MaxSet mset)
{
    if (xFirst)
    {
	growWidth (w, &tmp, r, mset);
	growHeight (w, &tmp, r, mset);
    }
    else
    {
	growHeight (w, &tmp, r, mset);
	growWidth (w, &tmp, r, mset);
    }

    return tmp;
}

/* These two functions set the width and height respectively, with gravity
 * towards the center of the window. The will set the box-width to width
 * as long as at least one of the sides can be modified.
 */
static void
setBoxWidth (BOX          *box,
	     const int    width,
	     const MaxSet mset)
{
    int original = box->x2 - box->x1;
    int increment;

    if (!mset.left && !mset.right)
	return ;

    if (mset.left != mset.right)
	increment = original - width;
    else
	increment = (original - width) / 2;

    if (mset.left) 
	box->x1 += increment;
    if (mset.right)
	box->x2 -= increment;
}

static void
setBoxHeight (BOX	   *box,
	      const int	   height,
	      const MaxSet mset)
{
    int original = box->y2 - box->y1;
    int increment;

    if (!mset.down && !mset.up)
	return ;

    if (mset.up != mset.down)
	increment = original - height;
    else
	increment = (original - height) / 2;

    if (mset.up) 
	box->y1 += increment;
    if (mset.down)
	box->y2 -= increment;
}

/* Make sure the window isn't maximized */
static void
maximumizeUnmaximizeWindow (CompWindow *w)
{
    int state = w->state;
    if (state & MAXIMIZE_STATE)
    {
	/* Unmaximize */
	state &= (w->state & ~MAXIMIZE_STATE);
	changeWindowState (w, state);
    }
}

/* Reduce box size by setting width and height to 1/4th or the minimum size
 * allowed, whichever is bigger.
 */
static BOX
maximumizeMinimumize (CompWindow *w,
		      BOX	 box,
		      MaxSet	 mset)
{
    const int min_width = w->sizeHints.min_width;
    const int min_height = w->sizeHints.min_height;
    int width, height;

    maximumizeUnmaximizeWindow (w);

    width = box.x2 - box.x1;
    height = box.y2 - box.y1;

    if (width/4 < min_width) 
	setBoxWidth (&box, min_width, mset);
    else 
	setBoxWidth (&box, width/4, mset);

    if (height/4 < min_height) 
	setBoxHeight (&box, min_height, mset);
    else 
	setBoxHeight (&box, height/4, mset);

    return box;
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
    
    if (mset.shrink)
	windowBox = maximumizeMinimumize (w, windowBox, mset);
    
    if (!mset.grow)
	return windowBox;

    // FIXME: split out into a function and make it a proper setting
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
		   int             nOption,
		   Bool            grow)
{
    Window     xid;
    CompWindow *w;
    CompScreen *s;

    xid = getIntOptionNamed (option, nOption, "window", 0);
    w   = findWindowAtDisplay (d, xid);
    if (w)
    {
	int            width, height;
	unsigned int   mask;
	XWindowChanges xwc;
	MaxSet	       mset;

	s = w->screen;
	if (otherScreenGrabExist (s, 0))
	   return FALSE;

	mset.left = maximumizeGetMaximumizeLeft (w->screen->display);
	mset.right = maximumizeGetMaximumizeRight (w->screen->display);
	mset.up = maximumizeGetMaximumizeUp (w->screen->display);
	mset.down = maximumizeGetMaximumizeDown (w->screen->display);
    
	mset.grow = grow;
	mset.shrink = TRUE;

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

    return TRUE;
}

/* 
 * Maximizing on specified direction.
 */
static Bool
maximumizeTriggerDirection (CompDisplay     *d,
			    CompAction      *action,
			    CompActionState state,
			    CompOption      *option,
			    int             nOption,
			    Bool            left,
			    Bool            right,
			    Bool            up,
			    Bool            down,
			    Bool            grow)
{
    Window     xid;
    CompWindow *w;
    CompScreen *s;

    xid = getIntOptionNamed (option, nOption, "window", 0);
    w   = findWindowAtDisplay (d, xid);
    if (w)
    {
	int            width, height;
	unsigned int   mask;
	XWindowChanges xwc;
	MaxSet	       mset;

	s = w->screen;
	if (otherScreenGrabExist (s, 0))
	    return FALSE;


	mset.left = left;
	mset.right = right;
	mset.up = up;
	mset.down = down;
	mset.grow = grow;
	mset.shrink = !grow;


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
    return TRUE;
}

/* 
 * Maximizing triggers 
 */

/* 
 * Maximumize .
 */
static Bool
maximumizeTriggerMax (CompDisplay     *d,
		      CompAction      *action,
		      CompActionState state,
		      CompOption      *option,
		      int             nOption)
{
    return maximumizeTrigger (d,action,state,option,nOption,TRUE);

}

/* 
 * Maximizing to left region limit.
 */
static Bool
maximumizeTriggerMaxLeft (CompDisplay     *d,
			  CompAction      *action,
			  CompActionState state,
			  CompOption      *option,
			  int             nOption)
{
    return maximumizeTriggerDirection (d,action,state,option,nOption,TRUE, FALSE, FALSE, FALSE,TRUE);

}

/* 
 * Maximizing to right region limit.
 *
 */
static Bool
maximumizeTriggerMaxRight (CompDisplay     *d,
			   CompAction      *action,
			   CompActionState state,
			   CompOption      *option,
			   int             nOption)
{
    return maximumizeTriggerDirection (d,action,state,option,nOption,FALSE, TRUE, FALSE, FALSE,TRUE);
}

/* 
 * Maximizing to upper region limit.
 *
 */
static Bool
maximumizeTriggerMaxUp (CompDisplay     *d,
			CompAction      *action,
			CompActionState state,
			CompOption      *option,
			int             nOption)
{
    return maximumizeTriggerDirection (d,action,state,option,nOption,FALSE, FALSE, TRUE, FALSE,TRUE);

}

/* 
 * Maximizing to bottom region limit.
 *
 */
static Bool
maximumizeTriggerMaxDown (CompDisplay     *d,
			  CompAction      *action,
			  CompActionState state,
			  CompOption      *option,
			  int             nOption)
{

    return maximumizeTriggerDirection (d,action,state,option,nOption,FALSE, FALSE, FALSE, TRUE,TRUE);

}

/* 
 * Maximizing horizontally.
 *
 */
static Bool
maximumizeTriggerMaxHorizontally (CompDisplay     *d,
				  CompAction      *action,
				  CompActionState state,
				  CompOption      *option,
				  int             nOption)
{
    return maximumizeTriggerDirection (d,action,state,option,nOption,TRUE, TRUE, FALSE, FALSE,TRUE);

}

/* 
 * Maximizing vertically.
 *
 */
static Bool
maximumizeTriggerMaxVertically (CompDisplay     *d,
				CompAction      *action,
				CompActionState state,
				CompOption      *option,
				int             nOption)
{
    return maximumizeTriggerDirection (d,action,state,option,nOption,FALSE, FALSE, TRUE, TRUE,TRUE);

}

/* 
 * Maximizing to upper left region limit.
 *
 */
static Bool
maximumizeTriggerMaxUpLeft (CompDisplay     *d,
			    CompAction      *action,
			    CompActionState state,
			    CompOption      *option,
			    int             nOption)
{

    return maximumizeTriggerDirection (d,action,state,option,nOption,TRUE, FALSE, TRUE, FALSE,TRUE);

}

/* 
 * Maximizing to upper right region limit.
 *
 */
static Bool
maximumizeTriggerMaxUpRight (CompDisplay     *d,
			     CompAction      *action,
			     CompActionState state,
			     CompOption      *option,
			     int             nOption)
{
    return maximumizeTriggerDirection (d,action,state,option,nOption,FALSE, TRUE, TRUE, FALSE,TRUE);

}

/* 
 * Maximizing to bottom left region limit.
 *
 */
static Bool
maximumizeTriggerMaxDownLeft (CompDisplay     *d,
			      CompAction      *action,
			      CompActionState state,
			      CompOption      *option,
			      int             nOption)
{
    return maximumizeTriggerDirection (d,action,state,option,nOption,TRUE, FALSE, FALSE, TRUE,TRUE);
}

/*
 * Maximizing to bottom right region limit.
 *
 */

static Bool
maximumizeTriggerMaxDownRight (CompDisplay     *d,
			       CompAction      *action,
			       CompActionState state,
			       CompOption      *option,
			       int             nOption)
{
    return maximumizeTriggerDirection (d,action,state,option,nOption,FALSE, TRUE, FALSE, TRUE,TRUE);
}

/* 
 * Minimizing triggers 
 */


/* 
 * Minimumize .
 */
static Bool
maximumizeTriggerMin (CompDisplay     *d,
		      CompAction      *action,
		      CompActionState state,
		      CompOption      *option,
		      int             nOption)
{
    return maximumizeTrigger (d,action,state,option,nOption,FALSE);

}

/* 
 * Minimizing to left region limit.
 */
static Bool
maximumizeTriggerMinLeft (CompDisplay     *d,
			  CompAction      *action,
			  CompActionState state,
			  CompOption      *option,
			  int             nOption)
{
    return maximumizeTriggerDirection (d,action,state,option,nOption,TRUE, FALSE, FALSE, FALSE,FALSE);

}

/* 
 * Minimizing to right region limit.
 *
 */
static Bool
maximumizeTriggerMinRight (CompDisplay     *d,
			   CompAction      *action,
			   CompActionState state,
			   CompOption      *option,
			   int             nOption)
{
    return maximumizeTriggerDirection (d,action,state,option,nOption,FALSE, TRUE, FALSE, FALSE,FALSE);
}

/* 
 * Minimizing to upper region limit.
 *
 */
static Bool
maximumizeTriggerMinUp (CompDisplay     *d,
			CompAction      *action,
			CompActionState state,
			CompOption      *option,
			int             nOption)
{
    return maximumizeTriggerDirection (d,action,state,option,nOption,FALSE, FALSE, TRUE, FALSE,FALSE);

}

/* 
 * Minimizing to bottom region limit.
 *
 */
static Bool
maximumizeTriggerMinDown (CompDisplay     *d,
			  CompAction      *action,
			  CompActionState state,
			  CompOption      *option,
			  int             nOption)
{
    return maximumizeTriggerDirection (d,action,state,option,nOption,FALSE, FALSE, FALSE, TRUE,FALSE);

}

/* 
 * Minimizing horizontally.
 *
 */
static Bool
maximumizeTriggerMinHorizontally (CompDisplay     *d,
				  CompAction      *action,
				  CompActionState state,
				  CompOption      *option,
				  int             nOption)
{
    return maximumizeTriggerDirection (d,action,state,option,nOption,TRUE, TRUE, FALSE, FALSE,FALSE);

}

/* 
 * Minimizing vertically.
 *
 */
static Bool
maximumizeTriggerMinVertically (CompDisplay     *d,
				CompAction      *action,
				CompActionState state,
				CompOption      *option,
				int             nOption)
{
    return maximumizeTriggerDirection (d,action,state,option,nOption,FALSE, FALSE, TRUE, TRUE,FALSE);

}

/* 
 * Minimizing to upper left region limit.
 *
 */
static Bool
maximumizeTriggerMinUpLeft (CompDisplay     *d,
			    CompAction      *action,
			    CompActionState state,
			    CompOption      *option,
			    int             nOption)
{
    return maximumizeTriggerDirection (d,action,state,option,nOption,TRUE, FALSE, TRUE, FALSE,FALSE);

}

/* 
 * Minimizing to upper right region limit.
 *
 */
static Bool
maximumizeTriggerMinUpRight (CompDisplay     *d,
			     CompAction      *action,
			     CompActionState state,
			     CompOption      *option,
			     int             nOption)
{
    return maximumizeTriggerDirection (d,action,state,option,nOption,FALSE, TRUE, TRUE, FALSE,FALSE);

}

/* 
 * Minimizing to bottom left region limit.
 *
 */
static Bool
maximumizeTriggerMinDownLeft (CompDisplay     *d,
			      CompAction      *action,
			      CompActionState state,
			      CompOption      *option,
			      int             nOption)
{
    return maximumizeTriggerDirection (d,action,state,option,nOption,TRUE, FALSE, FALSE, TRUE,FALSE);
}

/*
 * Minimizing to bottom right region limit.
 *
 */

static Bool
maximumizeTriggerMinDownRight (CompDisplay     *d,
			       CompAction      *action,
			       CompActionState state,
			       CompOption      *option,
			       int             nOption)
{
    return maximumizeTriggerDirection (d,action,state,option,nOption,FALSE, TRUE, FALSE, TRUE,FALSE);
}

/* Configuration, initialization, boring stuff. --------------------- */
static Bool
maximumizeInitDisplay (CompPlugin  *p,
		       CompDisplay *d)
{
    if (!checkPluginABI ("core", CORE_ABIVERSION))
	return FALSE;

    /* Maximumize Bindings */
    maximumizeSetTriggerMaxKeyInitiate (d, maximumizeTriggerMax);
    maximumizeSetTriggerMaxLeftInitiate (d, maximumizeTriggerMaxLeft);
    maximumizeSetTriggerMaxRightInitiate (d, maximumizeTriggerMaxRight);
    maximumizeSetTriggerMaxUpInitiate (d, maximumizeTriggerMaxUp);
    maximumizeSetTriggerMaxDownInitiate (d, maximumizeTriggerMaxDown);
    maximumizeSetTriggerMaxHorizontallyInitiate (d, maximumizeTriggerMaxHorizontally);
    maximumizeSetTriggerMaxVerticallyInitiate (d, maximumizeTriggerMaxVertically);
    maximumizeSetTriggerMaxUpLeftInitiate (d, maximumizeTriggerMaxUpLeft);
    maximumizeSetTriggerMaxUpRightInitiate (d, maximumizeTriggerMaxUpRight);
    maximumizeSetTriggerMaxDownLeftInitiate (d, maximumizeTriggerMaxDownLeft);
    maximumizeSetTriggerMaxDownRightInitiate (d, maximumizeTriggerMaxDownRight);

    /* Minimumize Bindings */
    maximumizeSetTriggerMinKeyInitiate (d, maximumizeTriggerMin);
    maximumizeSetTriggerMinLeftInitiate (d, maximumizeTriggerMinLeft);
    maximumizeSetTriggerMinRightInitiate (d, maximumizeTriggerMinRight);
    maximumizeSetTriggerMinUpInitiate (d, maximumizeTriggerMinUp);
    maximumizeSetTriggerMinDownInitiate (d, maximumizeTriggerMinDown);
    maximumizeSetTriggerMinHorizontallyInitiate (d, maximumizeTriggerMinHorizontally);
    maximumizeSetTriggerMinVerticallyInitiate (d, maximumizeTriggerMinVertically);
    maximumizeSetTriggerMinUpLeftInitiate (d, maximumizeTriggerMinUpLeft);
    maximumizeSetTriggerMinUpRightInitiate (d, maximumizeTriggerMinUpRight);
    maximumizeSetTriggerMinDownLeftInitiate (d, maximumizeTriggerMinDownLeft);
    maximumizeSetTriggerMinDownRightInitiate (d, maximumizeTriggerMinDownRight);
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

