/*
 *
 * Compiz widget handling plugin
 *
 * widget.c
 *
 * Copyright : (C) 2007 by Danny Baumann
 * E-mail    : dannybaumann@web.de
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

#include <stdlib.h>
#include <string.h>
#include <compiz-core.h>
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

    Window lastActiveWindow;

    Atom compizWidgetAtom;
} WidgetDisplay;

typedef struct _WidgetScreen
{
    int windowPrivateIndex;

    PreparePaintScreenProc preparePaintScreen;
    DonePaintScreenProc    donePaintScreen;
    PaintWindowProc        paintWindow;
    FocusWindowProc        focusWindow;

    WidgetState state;

    int fadeTime;

    int    grabIndex;
    Cursor cursor;
} WidgetScreen;

typedef struct _WidgetWindow
{
    Bool                isWidget;
    Bool                wasUnmapped;
    Bool                oldManaged;
    CompWindow          *parentWidget;
    CompTimeoutHandle   matchUpdateHandle;
    CompTimeoutHandle   widgetStatusUpdateHandle;
    WidgetPropertyState propertyState;
} WidgetWindow;

#define GET_WIDGET_DISPLAY(d)                                  \
    ((WidgetDisplay *) (d)->base.privates[displayPrivateIndex].ptr)

#define WIDGET_DISPLAY(d)                    \
    WidgetDisplay *wd = GET_WIDGET_DISPLAY (d)

#define GET_WIDGET_SCREEN(s, wd)                                   \
    ((WidgetScreen *) (s)->base.privates[(wd)->screenPrivateIndex].ptr)

#define WIDGET_SCREEN(s)                                                  \
    WidgetScreen *ws = GET_WIDGET_SCREEN (s, GET_WIDGET_DISPLAY (s->display))

#define GET_WIDGET_WINDOW(w, ws)                                   \
    ((WidgetWindow *) (w)->base.privates[(ws)->windowPrivateIndex].ptr)

#define WIDGET_WINDOW(w)                                       \
    WidgetWindow *ww = GET_WIDGET_WINDOW  (w,                    \
		       GET_WIDGET_SCREEN  (w->screen,            \
		       GET_WIDGET_DISPLAY (w->screen->display)))

static void
widgetUpdateTreeStatus (CompWindow *w)
{
    CompWindow   *p;
    WidgetWindow *pww;

    WIDGET_SCREEN (w->screen);
    WIDGET_WINDOW (w);

    /* first clear out every reference to our window */
    for (p = w->screen->windows; p; p = p->next)
    {
	pww = GET_WIDGET_WINDOW (p, ws);
	if (pww->parentWidget == w)
	    pww->parentWidget = NULL;
    }

    if (w->destroyed)
	return;

    if (!ww->isWidget)
	return;

    for (p = w->screen->windows; p; p = p->next)
    {
	Window clientLeader;

	if (p->attrib.override_redirect)
	    clientLeader = getClientLeader (p);
	else
	    clientLeader = p->clientLeader;

	if ((clientLeader == w->clientLeader) && (w->id != p->id))
	{
	    WIDGET_SCREEN (w->screen);

	    pww = GET_WIDGET_WINDOW (p, ws);
	    pww->parentWidget = w;
	}
    }
}

static Bool
widgetUpdateWidgetStatus (CompWindow *w)
{
    Bool isWidget, retval, managed;

    WIDGET_WINDOW (w);

    switch (ww->propertyState) {
    case PropertyWidget:
	isWidget = TRUE;
	break;
    case PropertyNoWidget:
	isWidget = FALSE;
	break;
    default:
	managed = w->managed || ww->oldManaged;
	if (!managed || (w->wmType & CompWindowTypeDesktopMask))
	    isWidget = FALSE;
	else
	    isWidget = matchEval (widgetGetMatch (w->screen), w);
	break;
    }

    retval = (!isWidget && ww->isWidget) || (isWidget && !ww->isWidget);
    ww->isWidget = isWidget;

    return retval;
}

static Bool
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
				 0, 1L, FALSE, AnyPropertyType, &retType,
				 &format, &nitems, &remain, &data);

    if (result == Success && data)
    {
	if (nitems && format == 32)
	{
	    unsigned long int *retData = (unsigned long int *) data;
	    if (*retData)
		ww->propertyState = PropertyWidget;
	    else
		ww->propertyState = PropertyNoWidget;
	}

	XFree (data);
    }
    else
	ww->propertyState = PropertyNotSet;

    return widgetUpdateWidgetStatus (w);
}

static void
widgetUpdateWidgetMapState (CompWindow *w,
			    Bool       map)
{
    WIDGET_WINDOW (w);

    if (map && ww->wasUnmapped)
    {
	XMapWindow (w->screen->display->display, w->id);
	raiseWindow (w);
	ww->wasUnmapped = FALSE;
	w->managed = ww->oldManaged;
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
	    ww->oldManaged = w->managed;
	}
    }
}

static void
widgetSetWidgetLayerMapState (CompScreen *s,
			      Bool       map)
{
    CompWindow   *w, *highest = NULL;
    unsigned int highestActiveNum = 0;

    WIDGET_DISPLAY (s->display);

    for (w = s->windows; w; w = w->next)
    {
	WIDGET_WINDOW (w);

	if (!ww->isWidget)
	    continue;

	if (w->activeNum > highestActiveNum)
	{
	    highest = w;
	    highestActiveNum = w->activeNum;
	}

	widgetUpdateWidgetMapState (w, map);
    }

    if (map && highest)
    {
	if (!wd->lastActiveWindow)
	    wd->lastActiveWindow = s->display->activeWindow;
	moveInputFocusToWindow (highest);
    }
    else if (!map)
    {
	w = findWindowAtDisplay (s->display, wd->lastActiveWindow);
	wd->lastActiveWindow = None;
	if (w)
	    moveInputFocusToWindow (w);
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

		widgetUpdateTreeStatus (w);

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
	case StateFadeOut:
	    widgetSetWidgetLayerMapState (s, TRUE);
	    ws->fadeTime = 1000.0f * widgetGetFadeTime (s);
	    ws->state = StateFadeIn;
	    break;
	case StateOn:
	case StateFadeIn:
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
widgetEndWidgetMode (CompScreen *s,
		     CompWindow *closedWidget)
{
    CompOption o;

    WIDGET_SCREEN (s);

    if (ws->state != StateOn && ws->state != StateFadeIn)
	return;

    if (closedWidget)
    {
	CompWindow *w;
	/* end widget mode if the closed widget was the last one */

	WIDGET_WINDOW (closedWidget);
	if (!ww->isWidget)
	    return;

	for (w = s->windows; w; w = w->next)
	{
	    WIDGET_WINDOW (w);
	    if (w == closedWidget)
		continue;
	    if (ww->isWidget)
		return;
	}
    }

    o.type    = CompOptionTypeInt;
    o.name    = "root";
    o.value.i = s->root;

    widgetToggle (s->display, NULL, 0, &o, 1);
}

static void
widgetHandleEvent (CompDisplay *d,
		   XEvent      *event)
{
    CompScreen *s;
    CompWindow *w = NULL;

    switch (event->type)
    {
    case DestroyNotify:
	/* We need to get the CompWindow * for event->xdestroywindow.window
	   here because in the (*d->handleEvent) call below, that CompWindow's
	   id will become 1, so findWindowAtDisplay won't be able to find the
	   CompWindow after that. */
	w = findWindowAtDisplay (d, event->xdestroywindow.window);
	break;
    }

    WIDGET_DISPLAY (d);

    UNWRAP (wd, d, handleEvent);
    (*d->handleEvent) (d, event);
    WRAP (wd, d, handleEvent, widgetHandleEvent);

    switch (event->type)
    {
    case PropertyNotify:
	if (event->xproperty.atom == wd->compizWidgetAtom)
	{
	    w = findWindowAtDisplay (d, event->xproperty.window);
	    if (w)
	    {
		if (widgetUpdateWidgetPropertyState (w))
		{
		    Bool map;

		    WIDGET_SCREEN (w->screen);
		    WIDGET_WINDOW (w);

		    map = !ww->isWidget || (ws->state != StateOff);
		    widgetUpdateWidgetMapState (w, map);
		    widgetUpdateTreeStatus (w);
		    (*d->matchPropertyChanged) (d, w);
		}
	    }
	}
	else if (event->xproperty.atom == d->wmClientLeaderAtom)
	{
	    w = findWindowAtDisplay (d, event->xproperty.window);
	    if (w)
	    {
		WIDGET_WINDOW (w);

		if (ww->isWidget)
		    widgetUpdateTreeStatus (w);
		else if (ww->parentWidget)
		    widgetUpdateTreeStatus (ww->parentWidget);
	    }
	}
	break;
    case ButtonPress:
	/* terminate widget mode if a non-widget window was clicked */
	s = findScreenAtDisplay (d, event->xbutton.root);
	if (s && widgetGetEndOnClick (s))
	{
	    WIDGET_SCREEN (s);
	    if (ws->state == StateOn)
	    {
		w = findWindowAtScreen (s, event->xbutton.window);
		if (w && w->managed)
		{
		    WIDGET_WINDOW (w);

		    if (!ww->isWidget && !ww->parentWidget)
			widgetEndWidgetMode (s, NULL);
		}
	    }
	}
	break;
    case MapNotify:
	w = findWindowAtDisplay (d, event->xmap.window);
	if (w)
	{
	    WIDGET_WINDOW (w);
	    WIDGET_SCREEN (w->screen);

	    widgetUpdateWidgetStatus (w);
	    if (ww->isWidget)
		widgetUpdateWidgetMapState (w, ws->state != StateOff);
	}
	break;
    case UnmapNotify:
	w = findWindowAtDisplay (d, event->xunmap.window);
	if (w)
	{
	    widgetUpdateTreeStatus (w);
	    widgetEndWidgetMode (w->screen, w);
	}
	break;
    case DestroyNotify:
	if (w)
	{
	    widgetUpdateTreeStatus (w);
	    widgetEndWidgetMode (w->screen, w);
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
    {
	WIDGET_SCREEN (w->screen);

	widgetUpdateTreeStatus (w);
	widgetUpdateWidgetMapState (w, ws->state != StateOff);
	(*w->screen->display->matchPropertyChanged) (w->screen->display, w);
    }

    ww->matchUpdateHandle = 0;
    return FALSE;
}

static Bool
widgetUpdateStatus (void *closure)
{
    CompWindow *w = (CompWindow *) closure;
    Window     clientLeader;

    WIDGET_WINDOW (w);
    WIDGET_SCREEN (w->screen);

    if (widgetUpdateWidgetPropertyState (w))
	widgetUpdateWidgetMapState (w, (ws->state != StateOff));

    if (w->attrib.override_redirect)
	clientLeader = getClientLeader (w);
    else
	clientLeader = w->clientLeader;

    if (ww->isWidget)
    {
	widgetUpdateTreeStatus (w);
    }
    else if (clientLeader)
    {
	CompWindow *lw;

	lw = findWindowAtScreen (w->screen, clientLeader);
	if (lw)
	{
	    WidgetWindow *lww;
	    lww = GET_WIDGET_WINDOW (lw, ws);

	    if (lww->isWidget)
		ww->parentWidget = lw;
	    else if (lww->parentWidget)
		ww->parentWidget = lww->parentWidget;
	}
    }

    ww->widgetStatusUpdateHandle = 0;
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
    if (!ww->matchUpdateHandle)
	ww->matchUpdateHandle = compAddTimeout (0, 0, widgetUpdateMatch,
						(void *) w);

    UNWRAP (wd, d, matchPropertyChanged);
    (*d->matchPropertyChanged) (d, w);
    WRAP (wd, d, matchPropertyChanged, widgetMatchPropertyChanged);
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

	if (!ww->isWidget && !ww->parentWidget)
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

static Bool
widgetFocusWindow (CompWindow *w)
{
    CompScreen *s = w->screen;
    Bool       status;

    WIDGET_SCREEN (s);
    WIDGET_WINDOW (w);

    if (ws->state != StateOff && !ww->isWidget && !ww->parentWidget)
    {
	status = FALSE;
    }
    else
    {
	UNWRAP (ws, s, focusWindow);
	status = (*s->focusWindow) (w);
	WRAP (ws, s, focusWindow, widgetFocusWindow);
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
	    if (ws->grabIndex)
	    {
		removeScreenGrab (s, ws->grabIndex, NULL);
		ws->grabIndex = 0;
	    }

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

		    widgetUpdateTreeStatus (w);
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

    if (!checkPluginABI ("core", CORE_ABIVERSION))
	return FALSE;

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
    wd->lastActiveWindow = None;

    d->base.privates[displayPrivateIndex].ptr = wd;

    widgetSetToggleKeyInitiate (d, widgetToggle);
    widgetSetToggleButtonInitiate (d, widgetToggle);
    widgetSetToggleEdgeInitiate (d, widgetToggle);

    WRAP (wd, d, handleEvent, widgetHandleEvent);
    WRAP (wd, d, matchPropertyChanged, widgetMatchPropertyChanged);
    WRAP (wd, d, matchExpHandlerChanged, widgetMatchExpHandlerChanged);
    WRAP (wd, d, matchInitExp, widgetMatchInitExp);

    /* one shot timeout to which will register the expression handler
       after all screens and windows have been initialized */
    compAddTimeout (0, 0, widgetRegisterExpHandler, (void *) d);

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

    if (d->base.parent)
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

    s->base.privates[wd->screenPrivateIndex].ptr = ws;

    WRAP (ws, s, focusWindow, widgetFocusWindow);
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

    UNWRAP (ws, s, focusWindow);
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
    ww->propertyState = PropertyNotSet;
    ww->parentWidget = NULL;
    ww->wasUnmapped = FALSE;
    ww->oldManaged = FALSE;
    ww->matchUpdateHandle = 0;

    w->base.privates[ws->windowPrivateIndex].ptr = ww;

    ww->widgetStatusUpdateHandle = compAddTimeout (0, 0, widgetUpdateStatus,
						   (void *) w);

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

    if (ww->widgetStatusUpdateHandle)
	compRemoveTimeout (ww->widgetStatusUpdateHandle);

    free (ww);
}

static CompBool
widgetInitObject (CompPlugin *p,
		  CompObject *o)
{
    static InitPluginObjectProc dispTab[] = {
	(InitPluginObjectProc) 0, /* InitCore */
	(InitPluginObjectProc) widgetInitDisplay,
	(InitPluginObjectProc) widgetInitScreen,
	(InitPluginObjectProc) widgetInitWindow
    };

    RETURN_DISPATCH (o, dispTab, ARRAY_SIZE (dispTab), TRUE, (p, o));
}

static void
widgetFiniObject (CompPlugin *p,
		  CompObject *o)
{
    static FiniPluginObjectProc dispTab[] = {
	(FiniPluginObjectProc) 0, /* FiniCore */
	(FiniPluginObjectProc) widgetFiniDisplay,
	(FiniPluginObjectProc) widgetFiniScreen,
	(FiniPluginObjectProc) widgetFiniWindow
    };

    DISPATCH (o, dispTab, ARRAY_SIZE (dispTab), (p, o));
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

static CompPluginVTable widgetVTable = {
    "widget",
    0,
    widgetInit,
    widgetFini,
    widgetInitObject,
    widgetFiniObject,
    0,
    0
};

CompPluginVTable*
getCompPluginInfo (void)
{
    return &widgetVTable;
}

