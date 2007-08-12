/*
 *
 * Compiz widget handling plugin
 *
 * widget.c
 *
 * Copyright : (C) 2007 by Danny Baumann
 * E-mail    : maniac@opencompositing.org
 *
 * Idea based on widget.c:
 * Copyright : (C) 2006 Quinn Storm
 * E-mail    : livinglatexkali@gmail.com
 *
 * Copyright : (C) 2007 Mike Dransfield
 * E-mail    : mike@blueroot.co.uk
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

#include <string.h>
#include <compiz.h>
#include <X11/Xatom.h>
#include <X11/cursorfont.h>
#include "widget_options.h"

static int displayPrivateIndex;

typedef enum _WidgetState
{
    StateOff = 0,
    StateFadeIn,
    StateOn,
    StateFadeOut
} WidgetState;

typedef enum _WidgetPropertyState
{
    PropertyNotSet = 0,
    PropertyWidget,
    PropertyNoWidget
} WidgetPropertyState;

typedef struct _WidgetDisplay
{
    int screenPrivateIndex;

    HandleEventProc            handleEvent;
    MatchPropertyChangedProc   matchPropertyChanged;
    MatchExpHandlerChangedProc matchExpHandlerChanged;
    MatchInitExpProc           matchInitExp;

    Atom compizWidgetAtom;
} WidgetDisplay;

typedef struct _WidgetScreen
{
    int windowPrivateIndex;

    PreparePaintScreenProc preparePaintScreen;
    DonePaintScreenProc    donePaintScreen;
    PaintWindowProc        paintWindow;
    WindowAddNotifyProc    windowAddNotify;

    WidgetState state;

    int fadeTime;

    int    grabIndex;
    Cursor cursor;
} WidgetScreen;

typedef struct _WidgetWindow
{
    Bool                isWidget;
    Bool                wasUnmapped;
    CompTimeoutHandle   matchUpdateHandle;
    WidgetPropertyState propertyState;
} WidgetWindow;

#define GET_WIDGET_DISPLAY(d)                                  \
    ((WidgetDisplay *) (d)->privates[displayPrivateIndex].ptr)

#define WIDGET_DISPLAY(d)                    \
    WidgetDisplay *wd = GET_WIDGET_DISPLAY (d)

#define GET_WIDGET_SCREEN(s, wd)                                   \
    ((WidgetScreen *) (s)->privates[(wd)->screenPrivateIndex].ptr)

#define WIDGET_SCREEN(s)                                                  \
    WidgetScreen *ws = GET_WIDGET_SCREEN (s, GET_WIDGET_DISPLAY (s->display))

#define GET_WIDGET_WINDOW(w, ws)                                   \
    ((WidgetWindow *) (w)->privates[(ws)->windowPrivateIndex].ptr)

#define WIDGET_WINDOW(w)                                       \
    WidgetWindow *ww = GET_WIDGET_WINDOW  (w,                    \
		       GET_WIDGET_SCREEN  (w->screen,            \
		       GET_WIDGET_DISPLAY (w->screen->display)))

static Bool
widgetUpdateWidgetStatus (CompWindow *w)
{
    Bool isWidget, retval;

    WIDGET_WINDOW (w);

    switch (ww->propertyState) {
    case PropertyWidget:
	isWidget = TRUE;
	break;
    case PropertyNoWidget:
	isWidget = FALSE;
	break;
    default:
	isWidget = matchEval (widgetGetMatch (w->screen), w);
	break;
    }

    retval = (!isWidget && ww->isWidget) || (isWidget && !ww->isWidget);
    ww->isWidget = isWidget;

    return retval;
}

static void
widgetUpdateWidgetPropertyState (CompWindow *w)
{
    CompDisplay   *d = w->screen->display;
    Atom          retType;
    int           format, result;
    unsigned long nitems, remain;
    unsigned char *data = NULL;

    WIDGET_DISPLAY (d);
    WIDGET_WINDOW (w);

    result = XGetWindowProperty (d->display, w->id, wd->compizWidgetAtom,
				 0, 1L, FALSE, XA_CARDINAL, &retType,
				 &format, &nitems, &remain, &data);

    if (result == Success && nitems && data)
    {
	if (*data)
	    ww->propertyState = PropertyWidget;
	else
	    ww->propertyState = PropertyNoWidget;

	XFree (data);
    }
    else
	ww->propertyState = PropertyNotSet;

    widgetUpdateWidgetStatus (w);
}

static void
widgetUpdateWidgetMapState (CompWindow *w,
			    Bool       map)
{
    WIDGET_WINDOW (w);

    if (map && ww->wasUnmapped)
    {
	XMapRaised (w->screen->display->display, w->id);
	ww->wasUnmapped = FALSE;
    }
    else if (!map && !ww->wasUnmapped)
    {
	/* never set ww->wasUnmapped on previously unmapped windows -
	   it might happen that we map windows when entering the
	   widget mode which aren't supposed to be unmapped */
	if (w->attrib.map_state == IsViewable)
	{
	    XUnmapWindow (w->screen->display->display, w->id);
	    ww->wasUnmapped = TRUE;
	}
    }
}

static void
widgetSetWidgetLayerMapState (CompScreen *s,
			      Bool       map)
{
    CompWindow *w;

    for (w = s->windows; w; w = w->next)
    {
	WIDGET_WINDOW (w);

	if (!ww->isWidget)
	    continue;

	widgetUpdateWidgetMapState (w, map);
    }
}

static Bool
widgetRegisterExpHandler (void *closure)
{
    CompDisplay *d = (CompDisplay *) closure;

    (*d->matchExpHandlerChanged) (d);

    return FALSE;
}

static Bool
widgetMatchExpEval (CompDisplay *d,
		    CompWindow  *w,
		    CompPrivate private)
{
    WIDGET_WINDOW (w);

    return ((private.val && ww->isWidget) || (!private.val && !ww->isWidget));
}

static void
widgetMatchInitExp (CompDisplay  *d,
		    CompMatchExp *exp,
		    const char   *value)
{
    WIDGET_DISPLAY (d);

    if (strncmp (value, "widget=", 7) == 0)
    {
	exp->fini     = NULL;
	exp->eval     = widgetMatchExpEval;
	exp->priv.val = strtol (value + 7, NULL, 0);
    }
    else
    {
	UNWRAP (wd, d, matchInitExp);
	(*d->matchInitExp) (d, exp, value);
	WRAP (wd, d, matchInitExp, widgetMatchInitExp);
    }
}

static void
widgetMatchExpHandlerChanged (CompDisplay *d)
{
    CompScreen *s;
    CompWindow *w;

    WIDGET_DISPLAY (d);

    UNWRAP (wd, d, matchExpHandlerChanged);
    (*d->matchExpHandlerChanged) (d);
    WRAP (wd, d, matchExpHandlerChanged, widgetMatchExpHandlerChanged);

    /* match options are up to date after the call to matchExpHandlerChanged */
    for (s = d->screens; s; s = s->next)
    {
	for (w = s->windows; w; w = w->next)
	    if (widgetUpdateWidgetStatus (w))
	    {
		Bool map;

		WIDGET_SCREEN (s);
		WIDGET_WINDOW (w);

		map = !ww->isWidget || (ws->state != StateOff);
		widgetUpdateWidgetMapState (w, map);

		(*d->matchPropertyChanged) (d, w);
	    }
    }
}

static Bool
widgetToggle (CompDisplay     *d,
	      CompAction      *action,
	      CompActionState state,
	      CompOption      *option,
	      int             nOption)
{
    Window     xid;
    CompScreen *s;

    xid = getIntOptionNamed (option, nOption, "root", 0);
    s   = findScreenAtDisplay (d, xid);

    if (s)
    {
	WIDGET_SCREEN (s);

	switch (ws->state) {
	case StateOff:
	    widgetSetWidgetLayerMapState (s, TRUE);
	    ws->fadeTime = 1000.0f * widgetGetFadeTime (s);
	    ws->state = StateFadeIn;
	    break;
	case StateFadeIn:
	    ws->fadeTime = (1000.0f * widgetGetFadeTime (s)) - ws->fadeTime;
	    ws->state = StateFadeOut;
	    break;
	case StateFadeOut:
	    ws->fadeTime = (1000.0f * widgetGetFadeTime (s)) - ws->fadeTime;
	    ws->state = StateFadeIn;
	    break;
	case StateOn:
	    widgetSetWidgetLayerMapState (s, FALSE);
	    ws->fadeTime = 1000.0f * widgetGetFadeTime (s);
	    ws->state = StateFadeOut;
	    break;
	}

	if (!ws->grabIndex)
	    ws->grabIndex = pushScreenGrab (s, ws->cursor, "widget");

	damageScreen (s);

	return TRUE;
    }

    return FALSE;
}

static void
widgetHandleEvent (CompDisplay *d,
		   XEvent      *event)
{
    WIDGET_DISPLAY (d);

    UNWRAP (wd, d, handleEvent);
    (*d->handleEvent) (d, event);
    WRAP (wd, d, handleEvent, widgetHandleEvent);

    switch (event->type)
    {
    case PropertyNotify:
	if (event->xproperty.atom == wd->compizWidgetAtom)
	{
	    CompWindow *w;

	    w = findWindowAtDisplay (d, event->xproperty.window);
	    if (w)
	    {
		widgetUpdateWidgetPropertyState (w);
		(*d->matchPropertyChanged) (d, w);
	    }
	}
	break;
    case ButtonPress:
	{
	    CompScreen *s;

	    /* terminate widget mode if a non-widget window
	       was clicked */
	    s = findScreenAtDisplay (d, event->xbutton.root);
	    if (s)
	    {
		WIDGET_SCREEN (s);
		if (ws->state == StateOn)
		{
		    CompWindow *w;
		    w = findWindowAtScreen (s, event->xbutton.window);
		    if (w && w->managed)
		    {
			WIDGET_WINDOW (w);

			if (!ww->isWidget)
			{
			    CompOption o;

			    o.type    = CompOptionTypeInt;
			    o.name    = "root";
			    o.value.i = s->root;

			    widgetToggle (d, NULL, 0, &o, 1);
			}
		    }
		}
	    }
	}
	break;
    case MapNotify:
	{
	    CompWindow *w;

	    w = findWindowAtDisplay (d, event->xmap.window);
	    if (w)
	    {
		WIDGET_WINDOW (w);
		WIDGET_SCREEN (w->screen);

		if (ww->isWidget)
		    widgetUpdateWidgetMapState (w, ws->state != StateOff);
	    }
	}
	break;
    }
}

static Bool
widgetUpdateMatch (void *closure)
{
    CompWindow *w = (CompWindow *) closure;

    WIDGET_WINDOW (w);

    if (widgetUpdateWidgetStatus (w))
	(*w->screen->display->matchPropertyChanged) (w->screen->display, w);

    ww->matchUpdateHandle = 0;
    return FALSE;
}


static void
widgetMatchPropertyChanged (CompDisplay *d,
			    CompWindow  *w)
{
    WIDGET_DISPLAY (d);
    WIDGET_WINDOW (w);

    /* one shot timeout which will update the widget status (timer
       is needed because we don't want to call wrapped functions
       recursively) */
    ww->matchUpdateHandle = compAddTimeout (0, widgetUpdateMatch, (void *) w);

    UNWRAP (wd, d, matchPropertyChanged);
    (*d->matchPropertyChanged) (d, w);
    WRAP (wd, d, matchPropertyChanged, widgetMatchPropertyChanged);
}

static void
widgetWindowAddNotify (CompWindow *w)
{
    WIDGET_SCREEN (w->screen);
    WIDGET_WINDOW (w);

    if (ww->isWidget)
	widgetUpdateWidgetMapState (w, (ws->state != StateOff));

    UNWRAP (ws, w->screen, windowAddNotify);
    (*w->screen->windowAddNotify) (w);
    WRAP (ws, w->screen, windowAddNotify, widgetWindowAddNotify);
}

static Bool
widgetPaintWindow (CompWindow              *w,
		   const WindowPaintAttrib *attrib,
		   const CompTransform     *transform,
		   Region                  region,
		   unsigned int            mask)
{
    Bool       status;
    CompScreen *s = w->screen;

    WIDGET_SCREEN (s);

    if (ws->state != StateOff)
    {
	WindowPaintAttrib wAttrib = *attrib;
	float             fadeProgress;

	WIDGET_WINDOW (w);

	if (ws->state == StateOn)
	    fadeProgress = 1.0f;
	else
	{
	    fadeProgress = widgetGetFadeTime (s);
	    if (fadeProgress)
		fadeProgress = (float) ws->fadeTime / (1000.0f * fadeProgress);
	    fadeProgress = 1.0f - fadeProgress;
	}

	if (!ww->isWidget)
	{
	    float progress;

	    if ((ws->state == StateFadeIn) || (ws->state == StateOn))
		fadeProgress = 1.0f - fadeProgress;

	    progress = widgetGetBgSaturation (s) / 100.0f;
	    progress += (1.0f - progress) * fadeProgress;
	    wAttrib.saturation = (float) wAttrib.saturation * progress;

	    progress = widgetGetBgBrightness (s) / 100.0f;
	    progress += (1.0f - progress) * fadeProgress;

	    wAttrib.brightness = (float) wAttrib.brightness * progress;
	}

	UNWRAP (ws, s, paintWindow);
	status = (*s->paintWindow) (w, &wAttrib, transform, region, mask);
	WRAP (ws, s, paintWindow, widgetPaintWindow);
    }
    else
    {
	UNWRAP (ws, s, paintWindow);
	status = (*s->paintWindow) (w, attrib, transform, region, mask);
	WRAP (ws, s, paintWindow, widgetPaintWindow);
    }

    return status;
}

static void
widgetPreparePaintScreen (CompScreen  *s,
			  int         msSinceLastPaint)
{
    WIDGET_SCREEN (s);

    if ((ws->state == StateFadeIn) || (ws->state == StateFadeOut))
    {
	ws->fadeTime -= msSinceLastPaint;
	ws->fadeTime = MAX (ws->fadeTime, 0);
    }

    UNWRAP (ws, s, preparePaintScreen);
    (*s->preparePaintScreen) (s, msSinceLastPaint);
    WRAP (ws, s, preparePaintScreen, widgetPreparePaintScreen);
}

static void
widgetDonePaintScreen (CompScreen *s)
{
    WIDGET_SCREEN (s);

    if ((ws->state == StateFadeIn) || (ws->state == StateFadeOut))
    {
	if (ws->fadeTime)
	    damageScreen (s);
	else
	{
	    removeScreenGrab (s, ws->grabIndex, NULL);
	    ws->grabIndex = 0;

	    if (ws->state == StateFadeIn)
		ws->state = StateOn;
	    else
		ws->state = StateOff;
	}
    }

    UNWRAP (ws, s, donePaintScreen);
    (*s->donePaintScreen) (s);
    WRAP (ws, s, donePaintScreen, widgetDonePaintScreen);
}

static void
widgetScreenOptionChanged (CompScreen           *s,
			   CompOption           *opt,
			   WidgetScreenOptions  num)
{
    switch (num) {
    case WidgetScreenOptionMatch:
	{
	    CompWindow *w;
	    for (w = s->windows; w; w = w->next)
		if (widgetUpdateWidgetStatus (w))
		{
		    Bool map;

		    WIDGET_SCREEN (s);
		    WIDGET_WINDOW (w);

		    map = !ww->isWidget || (ws->state != StateOff);
		    widgetUpdateWidgetMapState (w, map);

		    (*s->display->matchPropertyChanged) (s->display, w);
		}
	}
	break;
    default:
	break;
    }
}

static Bool
widgetInitDisplay (CompPlugin  *p,
		   CompDisplay *d)
{
    WidgetDisplay *wd;

    wd = malloc (sizeof (WidgetDisplay));
    if (!wd)
      return FALSE;

    wd->screenPrivateIndex = allocateScreenPrivateIndex (d);
    if (wd->screenPrivateIndex < 0)
    {
	free (wd);
	return FALSE;
    }

    wd->compizWidgetAtom = XInternAtom(d->display, "_COMPIZ_WIDGET", FALSE);

    d->privates[displayPrivateIndex].ptr = wd;

    widgetSetToggleInitiate (d, widgetToggle);

    WRAP (wd, d, handleEvent, widgetHandleEvent);
    WRAP (wd, d, matchPropertyChanged, widgetMatchPropertyChanged);
    WRAP (wd, d, matchExpHandlerChanged, widgetMatchExpHandlerChanged);
    WRAP (wd, d, matchInitExp, widgetMatchInitExp);

    /* one shot timeout to which will register the expression handler
       after all screens and windows have been initialized */
    compAddTimeout (0, widgetRegisterExpHandler, (void *) d);

    return TRUE;
}

static void
widgetFiniDisplay (CompPlugin  *p,
		   CompDisplay *d)
{
    WIDGET_DISPLAY (d);

    freeScreenPrivateIndex (d, wd->screenPrivateIndex);

    UNWRAP (wd, d, handleEvent);
    UNWRAP (wd, d, matchPropertyChanged);
    UNWRAP (wd, d, matchExpHandlerChanged);
    UNWRAP (wd, d, matchInitExp);

    (*d->matchExpHandlerChanged) (d);

    free (wd);
}

static Bool
widgetInitScreen (CompPlugin *p,
		  CompScreen *s)
{
    WidgetScreen *ws;

    WIDGET_DISPLAY (s->display);

    ws = malloc (sizeof (WidgetScreen));
    if (!ws)
        return FALSE;

    ws->windowPrivateIndex = allocateWindowPrivateIndex (s);
    if (ws->windowPrivateIndex < 0)
    {
        free (ws);
        return FALSE;
    }

    ws->state     = StateOff;
    ws->cursor    = XCreateFontCursor (s->display->display, XC_watch);
    ws->grabIndex = 0;
    ws->fadeTime  = 0;

    widgetSetMatchNotify (s, widgetScreenOptionChanged);

    s->privates[wd->screenPrivateIndex].ptr = ws;

    WRAP (ws, s, paintWindow, widgetPaintWindow);
    WRAP (ws, s, preparePaintScreen, widgetPreparePaintScreen);
    WRAP (ws, s, donePaintScreen, widgetDonePaintScreen);

    return TRUE;
}

static void
widgetFiniScreen (CompPlugin *p,
		  CompScreen *s)
{
    WIDGET_SCREEN (s);

    UNWRAP (ws, s, paintWindow);
    UNWRAP (ws, s, preparePaintScreen);
    UNWRAP (ws, s, donePaintScreen);

    freeWindowPrivateIndex (s, ws->windowPrivateIndex);

    if (ws->cursor)
	XFreeCursor (s->display->display, ws->cursor);

    free (ws);
}

static Bool
widgetInitWindow (CompPlugin *p,
		  CompWindow *w)
{
    WidgetWindow *ww;

    WIDGET_SCREEN (w->screen);

    ww = malloc (sizeof (WidgetWindow));
    if (!ww)
        return FALSE;

    ww->isWidget = FALSE;
    ww->wasUnmapped = FALSE;
    ww->matchUpdateHandle = 0;

    w->privates[ws->windowPrivateIndex].ptr = ww;

    widgetUpdateWidgetPropertyState (w);

    return TRUE;
}

static void
widgetFiniWindow (CompPlugin *p,
		  CompWindow *w)
{
    WIDGET_WINDOW (w);

    if (ww->wasUnmapped)
	widgetUpdateWidgetMapState (w, TRUE);

    if (ww->matchUpdateHandle)
	compRemoveTimeout (ww->matchUpdateHandle);

    free (ww);
}

static Bool
widgetInit (CompPlugin *p)
{
    displayPrivateIndex = allocateDisplayPrivateIndex ();
    if (displayPrivateIndex < 0)
	return FALSE;

    return TRUE;
}

static void
widgetFini (CompPlugin *p)
{
    freeDisplayPrivateIndex (displayPrivateIndex);
}

static int
widgetGetVersion (CompPlugin *plugin,
		  int        version)
{
    return ABIVERSION;
}

static CompPluginVTable widgetVTable = {
    "widget",
    widgetGetVersion,
    NULL,
    widgetInit,
    widgetFini,
    widgetInitDisplay,
    widgetFiniDisplay,
    widgetInitScreen,
    widgetFiniScreen,
    widgetInitWindow,
    widgetFiniWindow,
    NULL,
    NULL,
    NULL,
    NULL
};

CompPluginVTable*
getCompPluginInfo (void)
{
    return &widgetVTable;
}

