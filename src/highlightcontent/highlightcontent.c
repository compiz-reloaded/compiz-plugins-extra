/*
 *
 * Compiz highlight content plugin
 *
 * highlightcontent.c
 * 
 * Copyright (C) 2019 by Hypra
 * E-mail    contact@hypra.fr
 *
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
 */

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include <compiz-core.h>
#include <compiz-focuspoll.h>

#include <X11/Xatom.h>
#include <X11/extensions/Xfixes.h>
#include <X11/extensions/Xrender.h>
#include <X11/extensions/shape.h>

#include <cairo/cairo-xlib.h>

#include "highlightcontent_options.h"

#define GET_HIGHLIGHTCONTENT_DISPLAY(d)                                  \
    ((HighlightcontentDisplay *) (d)->base.privates[displayPrivateIndex].ptr)

#define HIGHLIGHTCONTENT_DISPLAY(d)                      \
    HighlightcontentDisplay *sd = GET_HIGHLIGHTCONTENT_DISPLAY (d)

#define GET_HIGHLIGHTCONTENT_SCREEN(s, sd)                                   \
    ((HighlightcontentScreen *) (s)->base.privates[(sd)->screenPrivateIndex].ptr)

#define HIGHLIGHTCONTENT_SCREEN(s)                                                      \
    HighlightcontentScreen *ss = GET_HIGHLIGHTCONTENT_SCREEN (s, GET_HIGHLIGHTCONTENT_DISPLAY (s->display))


static int displayPrivateIndex = 0;

typedef struct _HighlightcontentDisplay
{
    int  screenPrivateIndex;

    FocusPollFunc *fpFunc;

    HandleEventProc handleEvent;
}
HighlightcontentDisplay;

typedef struct _HighlightcontentScreen
{
    int poll_x;
    int poll_y;
    int poll_w;
    int poll_h;

    Bool active;

    struct
    {
	/* offset of our window relative to the root, in order to avoid
	 * offset between crosshairs and actual mouse in case our
	 * window didn't get placed at 0,0 */
	int xOffset;
	int yOffset;

	Window window;
	cairo_surface_t *surface;
    } overlay;

    FocusPollingHandle pollHandle;

    CompTimeoutHandle      speechTimeoutHandle;
	
    PreparePaintScreenProc preparePaintScreen;
    DonePaintScreenProc    donePaintScreen;
    PaintOutputProc        paintOutput;
}
HighlightcontentScreen;


/* copied from Wallpaper plugin */
static Visual *
findArgbVisual (Display *dpy,
		int     screen)
{
    XVisualInfo         temp;
    int                 nvi;

    temp.screen  = screen;
    temp.depth   = 32;
    temp.class   = TrueColor;

    XVisualInfo *xvi = XGetVisualInfo (dpy,
				       VisualScreenMask |
				       VisualDepthMask  |
				       VisualClassMask,
				       &temp,
				       &nvi);
    if (!xvi)
	return 0;

    Visual            *visual = 0;
    XRenderPictFormat *format;

    for (int i = 0; i < nvi; ++i)
    {
	format = XRenderFindVisualFormat (dpy, xvi[i].visual);

	if (format->type == PictTypeDirect && format->direct.alphaMask)
	{
	    visual = xvi[i].visual;
	    break;
	}
    }

    XFree (xvi);

    return visual;
}

/*
 * Shapes the rectangle on the cairo context.  This can be used for
 * painting, clipping or anything else that requires a Cairo shape.
 */
static void
shapeRectangle (CompScreen *s,
		cairo_t *cr,
		const int x,
		const int y,
		const int w,
		const int h,
		const int inside)
{
    HIGHLIGHTCONTENT_SCREEN (s);

    int thickness = highlightcontentGetSpeechThickness (s);
    /* when in stroke mode, switch to float to avoid blurry edges
     * See: https://cairographics.org/FAQ/#sharp_lines */
    // float delta = thickness/2.;
    int delta = thickness;
    if (inside)
	delta = 0;

    cairo_rectangle (cr,
		     x - ss->overlay.xOffset - delta,
		     y - ss->overlay.yOffset - delta,
		     w - ss->overlay.xOffset + delta*2,
		     h - ss->overlay.yOffset + delta*2);
}

static void
paintSpeech (CompScreen *s,
	     cairo_t *cr,
	     const int newX,
	     const int newY,
	     const int newWidth,
	     const int newHeight)
{
    unsigned short *color = highlightcontentGetSpeechColor (s);
    int thickness         = highlightcontentGetSpeechThickness (s);

    shapeRectangle (s, cr, newX, newY, newWidth, newHeight, 0);
    cairo_set_source_rgba (cr,
			   color[0] / 65535.0,
			   color[1] / 65535.0,
			   color[2] / 65535.0,
			   color[3] / 65535.0);
    cairo_set_line_width (cr, thickness);
    cairo_fill (cr);
    if (highlightcontentGetSpeechHollow(s)) {
	shapeRectangle (s, cr, newX, newY, newWidth, newHeight, 1);
	cairo_set_operator (cr, CAIRO_OPERATOR_CLEAR);
	cairo_fill (cr);
    }
}

static void
eraseSpeechHL (CompScreen *s)
{
    HIGHLIGHTCONTENT_SCREEN (s);
    cairo_t *cr = cairo_create (ss->overlay.surface);

    cairo_set_operator (cr, CAIRO_OPERATOR_CLEAR);
    cairo_paint (cr);
    paintSpeech (s, cr, ss->poll_x, ss->poll_y, ss->poll_w, ss->poll_h);
    cairo_destroy (cr);
}

static CompBool
eraseSpeechHLCB (void *data)
{
    CompScreen *s = data;

    eraseSpeechHL (s);

    return FALSE;
}

static void
clearOverlayWindow (CompScreen *s)
{
    eraseSpeechHL (s);
}

static void
showOverlayWindow (CompScreen *s)
{
    HIGHLIGHTCONTENT_SCREEN (s);

    if (!ss->overlay.window)
    {
	Display		    *dpy = s->display->display;
	XSizeHints	     xsh;
	XWMHints	     xwmh;
	XClassHint           xch;
	Atom		     state[] = {
	    s->display->winStateAboveAtom,
	    s->display->winStateStickyAtom,
	    s->display->winStateSkipTaskbarAtom,
	    s->display->winStateSkipPagerAtom,
	};
	XSetWindowAttributes attr;
	Visual		    *visual;
	XserverRegion	     region;

	visual = findArgbVisual (dpy, s->screenNum);
	if (!visual)
	    return;

	xsh.flags       = PSize | PPosition | PWinGravity;
	xsh.width       = s->width;
	xsh.height      = s->height;
	xsh.win_gravity = StaticGravity;

	xwmh.flags = InputHint;
	xwmh.input = 0;

	xch.res_name  = (char *)"compiz";
	xch.res_class = (char *)"highlightcontent-window";

	attr.background_pixel = 0;
	attr.border_pixel     = 0;
	attr.colormap	      = XCreateColormap (dpy, s->root,
						 visual, AllocNone);
	attr.override_redirect = TRUE;

	ss->overlay.window =
	    XCreateWindow (dpy, s->root,
			   0, 0,
			   (unsigned) xsh.width, (unsigned) xsh.height, 0,
			   32, InputOutput, visual,
			   CWBackPixel | CWBorderPixel | CWColormap | CWOverrideRedirect, &attr);

	XSelectInput (dpy, ss->overlay.window, ExposureMask | StructureNotifyMask);

	XSetWMProperties (dpy, ss->overlay.window, NULL, NULL,
			  programArgv, programArgc,
			  &xsh, &xwmh, &xch);

	XChangeProperty (dpy, ss->overlay.window,
			 s->display->winStateAtom,
			 XA_ATOM, 32, PropModeReplace,
			 (unsigned char *) state,
			 sizeof state / sizeof *state);

	XChangeProperty (dpy, ss->overlay.window,
			 s->display->winTypeAtom,
			 XA_ATOM, 32, PropModeReplace,
			 (unsigned char *) &s->display->winTypeUtilAtom, 1);

	setWindowProp (s->display, ss->overlay.window,
		       s->display->winDesktopAtom, 0xffffffff);

	/* make the window an output-only window by having no shape for input */
	region = XFixesCreateRegion (dpy, NULL, 0);
	XFixesSetWindowShapeRegion (dpy, ss->overlay.window, ShapeBounding, 0, 0, None);
	XFixesSetWindowShapeRegion (dpy, ss->overlay.window, ShapeInput, 0, 0, region);
	XFixesDestroyRegion (dpy, region);

	ss->overlay.surface = cairo_xlib_surface_create (dpy,
							 ss->overlay.window, visual,
							 xsh.width, xsh.height);
    }

    XMapWindow (s->display->display, ss->overlay.window);

    /* in case we re-show, we don't always get an EXPOSE event, so
     * force an initial redraw */
    clearOverlayWindow (s);
}

static void
hideOverlayWindow (CompScreen *s)
{
    HIGHLIGHTCONTENT_SCREEN (s);

    if (ss->overlay.window)
	XUnmapWindow (s->display->display, ss->overlay.window);
}

static CompScreen *
findScreenForXWindow (CompDisplay *d,
		      Window xid)
{
    XWindowAttributes attr;

    /* FIXME: isn't there a cheaper way to find the CompScreen? */
    if (XGetWindowAttributes (d->display, xid, &attr))
	return findScreenAtDisplay (d, attr.root);

    return NULL;
}

static void
moveSpeechHL (CompScreen *s, int x, int y, int w, int h)
{
    HIGHLIGHTCONTENT_SCREEN (s);

    clearOverlayWindow (s);

    ss->poll_x = x;
    ss->poll_y = y;
    ss->poll_w = w;
    ss->poll_h = h;

    if (ss->poll_w != 0) {
	cairo_t *cr = cairo_create (ss->overlay.surface);
	paintSpeech (s, cr, ss->poll_x, ss->poll_y, ss->poll_w, ss->poll_h);
	cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
	cairo_destroy (cr);
    }

    if (ss->speechTimeoutHandle)
	compRemoveTimeout(ss->speechTimeoutHandle);

    ss->speechTimeoutHandle = compAddTimeout (5000,
						5000,
						eraseSpeechHLCB,
						s);
}



/* Focus Poll handler */
static void
focusUpdated (CompScreen *s, FocusEventNode *list)
{
    for (FocusEventNode *cur = list; cur; cur = cur->next)
    {
	if (strcmp (cur->type, "region-changed") == 0)
	{
	    if (highlightcontentGetSpeech(s))
		moveSpeechHL (s, cur->x, cur->y, cur->width, cur->height);
	}
    }
}

/* Enables polling of focus position */
static void
enableFocusPolling (CompScreen *s)
{
    HIGHLIGHTCONTENT_SCREEN (s);
    HIGHLIGHTCONTENT_DISPLAY (s->display);
    if (!sd->fpFunc)
	return;
    ss->pollHandle =
	(*sd->fpFunc->addFocusPolling) (s, focusUpdated);
}

/* Disables polling of focus position */
static void
disableFocusPolling (CompScreen *s)
{
    HIGHLIGHTCONTENT_SCREEN (s);
    HIGHLIGHTCONTENT_DISPLAY (s->display);
    if (!sd->fpFunc)
	return;

    (*sd->fpFunc->removeFocusPolling) (s, ss->pollHandle);
    ss->pollHandle = 0;
}


static void
highlightcontentHandleEvent (CompDisplay *d,
		      XEvent *event)
{
    CompScreen *s;
    HIGHLIGHTCONTENT_DISPLAY (d);

    UNWRAP (sd, d, handleEvent);
    (*d->handleEvent) (d, event);
    WRAP (sd, d, handleEvent, highlightcontentHandleEvent);

    switch (event->type)
    {
    case ConfigureNotify:
	s = findScreenForXWindow (d, event->xconfigure.window);
	if (s)
	{
	    HIGHLIGHTCONTENT_SCREEN (s);

	    if (event->xconfigure.window == ss->overlay.window)
	    {
		ss->overlay.xOffset = event->xconfigure.x;
		ss->overlay.yOffset = event->xconfigure.y;
	    }
	}
	break;

    case MapNotify:
	s = findScreenForXWindow (d, event->xmap.window);
	if (s)
	{
	    HIGHLIGHTCONTENT_SCREEN (s);

	    if (event->xmap.window == ss->overlay.window)
	    {
		XWindowAttributes attr;

		XGetWindowAttributes (event->xmap.display, event->xmap.window, &attr);
		ss->overlay.xOffset = attr.x;
		ss->overlay.yOffset = attr.y;
	    }
	    else if (ss->overlay.window)
		XRaiseWindow (s->display->display, ss->overlay.window);
	}
	break;
    }
}

static void
stop_us(CompScreen *s)
{
    HIGHLIGHTCONTENT_SCREEN (s);

    ss->active = FALSE;

    hideOverlayWindow (s);
}

static Bool
highlightcontentTerminate (CompDisplay     *d,
		    CompAction      *action,
		    CompActionState state,
		    CompOption      *option,
		    int             nOption)
{
    CompScreen *s;
    Window     xid;

    xid = getIntOptionNamed (option, nOption, "root", 0);

    s = findScreenAtDisplay (d, xid);
    if (s)
    {
	stop_us (s);

	return TRUE;
    }
    return FALSE;
}

static void
start_us (CompScreen *s)
{
    HIGHLIGHTCONTENT_SCREEN (s);

    ss->active = TRUE;

    if (highlightcontentGetSpeech (s))
    {
	showOverlayWindow (s);
	enableFocusPolling (s);
    }
}

static Bool
highlightcontentInitiate (CompDisplay     *d,
		   CompAction      *action,
		   CompActionState state,
		   CompOption      *option,
		   int             nOption)
{
    CompScreen *s;
    Window     xid;

    xid    = getIntOptionNamed (option, nOption, "root", 0);

    s = findScreenAtDisplay (d, xid);
    if (s)
    {
	HIGHLIGHTCONTENT_SCREEN (s);

	if (ss->active)
	    return highlightcontentTerminate (d, action, state, option, nOption);

	start_us (s);

	return TRUE;
    }
    return FALSE;
}

/* Change notify for bcop */
static void
ononinitOptionChanged (CompDisplay             *d,
                       CompOption              *opt,
                       HighlightcontentDisplayOptions num)
{
    CompScreen *s;

    if (highlightcontentGetOnoninit(d)) {
        for (s = d->screens; s; s = s->next) {
	    HIGHLIGHTCONTENT_SCREEN (s);
	    if (!ss->active)
		start_us (s);
	}
    }
}

static void
speechOptionNotify (CompScreen            *s,
		   CompOption            *option,
		   HighlightcontentScreenOptions num)
{
    switch (num)
    {
    case HighlightcontentScreenOptionSpeech:
    {
	HIGHLIGHTCONTENT_SCREEN (s);

	if (ss->active && highlightcontentGetSpeech (s))
	{
	    showOverlayWindow (s);
	    enableFocusPolling (s);
	}
	else
	{
	    disableFocusPolling (s);
	    hideOverlayWindow (s);
	}
	break;
    }

    default:
	if (highlightcontentGetSpeech (s))
	    clearOverlayWindow (s);
	break;
    }
}


static Bool
highlightcontentInitScreen (CompPlugin *p,
		     CompScreen *s)
{
    HIGHLIGHTCONTENT_DISPLAY (s->display);

    HighlightcontentScreen *ss = (HighlightcontentScreen *) calloc (1, sizeof (HighlightcontentScreen) );

    if (!ss)
	return FALSE;

    s->base.privates[sd->screenPrivateIndex].ptr = ss;

    ss->active = FALSE;

    ss->pollHandle = 0;
    ss->speechTimeoutHandle = 0;

    highlightcontentSetSpeechNotify (s, speechOptionNotify);

    return TRUE;
}


static void
highlightcontentFiniScreen (CompPlugin *p,
		     CompScreen *s)
{
    HIGHLIGHTCONTENT_SCREEN (s);
    HIGHLIGHTCONTENT_DISPLAY (s->display);

    if (ss->pollHandle)
	(*sd->fpFunc->removeFocusPolling) (s, ss->pollHandle);

    if (ss->overlay.window)
	XDestroyWindow (s->display->display, ss->overlay.window);
    if (ss->overlay.surface)
	cairo_surface_destroy (ss->overlay.surface);

    //Free the pointer
    free (ss);
}

static Bool
highlightcontentInitDisplay (CompPlugin  *p,
		      CompDisplay *d)
{
    //Generate a highlightcontent display
    HighlightcontentDisplay *sd;
    int              index;

    if (!checkPluginABI ("core", CORE_ABIVERSION) ||
        !checkPluginABI ("focuspoll", FOCUSPOLL_ABIVERSION))
	return FALSE;

    if (!getPluginDisplayIndex (d, "focuspoll", &index))
	return FALSE;

    sd = (HighlightcontentDisplay *) malloc (sizeof (HighlightcontentDisplay));

    if (!sd)
	return FALSE;
 
    //Allocate a private index
    sd->screenPrivateIndex = allocateScreenPrivateIndex (d);

    //Check if its valid
    if (sd->screenPrivateIndex < 0)
    {
	//Its invalid so free memory and return
	free (sd);
	return FALSE;
    }

    sd->fpFunc = d->base.privates[index].ptr;

    highlightcontentSetInitiateInitiate (d, highlightcontentInitiate);
    highlightcontentSetInitiateTerminate (d, highlightcontentTerminate);
    highlightcontentSetInitiateButtonInitiate (d, highlightcontentInitiate);
    highlightcontentSetInitiateButtonTerminate (d, highlightcontentTerminate);
    highlightcontentSetInitiateEdgeInitiate (d, highlightcontentInitiate);
    highlightcontentSetInitiateEdgeTerminate (d, highlightcontentTerminate);

    highlightcontentSetOnoninitNotify (d, ononinitOptionChanged);

    //Record the display
    d->base.privates[displayPrivateIndex].ptr = sd;

    WRAP (sd, d, handleEvent, highlightcontentHandleEvent);

    return TRUE;
}

static void
highlightcontentFiniDisplay (CompPlugin  *p,
		      CompDisplay *d)
{
    HIGHLIGHTCONTENT_DISPLAY (d);

    UNWRAP (sd, d, handleEvent);

    //Free the private index
    freeScreenPrivateIndex (d, sd->screenPrivateIndex);
    //Free the pointer
    free (sd);
}



static Bool
highlightcontentInit (CompPlugin * p)
{
    displayPrivateIndex = allocateDisplayPrivateIndex();

    if (displayPrivateIndex < 0)
	return FALSE;

    return TRUE;
}

static void
highlightcontentFini (CompPlugin * p)
{
    if (displayPrivateIndex >= 0)
	freeDisplayPrivateIndex (displayPrivateIndex);
}

static CompBool
highlightcontentInitObject (CompPlugin *p,
		     CompObject *o)
{
    static InitPluginObjectProc dispTab[] = {
	(InitPluginObjectProc) 0, /* InitCore */
	(InitPluginObjectProc) highlightcontentInitDisplay,
	(InitPluginObjectProc) highlightcontentInitScreen
    };

    RETURN_DISPATCH (o, dispTab, ARRAY_SIZE (dispTab), TRUE, (p, o));
}

static void
highlightcontentFiniObject (CompPlugin *p,
		     CompObject *o)
{
    static FiniPluginObjectProc dispTab[] = {
	(FiniPluginObjectProc) 0, /* FiniCore */
	(FiniPluginObjectProc) highlightcontentFiniDisplay,
	(FiniPluginObjectProc) highlightcontentFiniScreen
    };

    DISPATCH (o, dispTab, ARRAY_SIZE (dispTab), (p, o));
}

CompPluginVTable highlightcontentVTable = {
    "highlightcontent",
    0,
    highlightcontentInit,
    highlightcontentFini,
    highlightcontentInitObject,
    highlightcontentFiniObject,
    0,
    0
};

CompPluginVTable *
getCompPluginInfo (void)
{
    return &highlightcontentVTable;
}
