/**
 * Beryl Trailfocus - take three
 *
 * Copyright (c) 2006 Kristian Lyngstøl <kristian@beryl-project.org>
 * Ported to Compiz and BCOP usage by Danny Baumann <maniac@beryl-project.org>
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
 * This version is completly rewritten from scratch with opacify as a 
 * basic template. The original trailfocus was written by: 
 * François Ingelrest <Athropos@gmail.com> and rewritten by:
 * Dennis Kasprzyk <onestone@beryl-project.org>
 * 
 *
 * Trailfocus modifies the opacity, brightness and saturation on a window 
 * based on when it last had focus. 
 *
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <X11/Xatom.h>
#include <X11/extensions/Xrender.h>
#include <compiz.h>
#include "trailfocus_options.h"

#define GET_TRAILFOCUS_DISPLAY(d)                            \
	((TrailfocusDisplay *) (d)->privates[displayPrivateIndex].ptr)
#define TRAILFOCUS_DISPLAY(d)                                \
    TrailfocusDisplay *td = GET_TRAILFOCUS_DISPLAY (d)
#define GET_TRAILFOCUS_SCREEN(s, td)                         \
	((TrailfocusScreen *) (s)->privates[(td)->screenPrivateIndex].ptr)
#define TRAILFOCUS_SCREEN(s)                                 \
	TrailfocusScreen *ts = GET_TRAILFOCUS_SCREEN (s, GET_TRAILFOCUS_DISPLAY (s->display))
#define GET_TRAILFOCUS_WINDOW(w, ts)                         \
	((TrailfocusWindow *) (w)->privates[(ts)->windowPrivateIndex].ptr)
#define TRAILFOCUS_WINDOW(w)                                 \
	TrailfocusWindow *tw = GET_TRAILFOCUS_WINDOW (w, GET_TRAILFOCUS_SCREEN(w->screen, GET_TRAILFOCUS_DISPLAY (w->screen->display)))

static int displayPrivateIndex = 0;

typedef struct _TrailfocusDisplay
{
	int screenPrivateIndex;
	HandleEventProc handleEvent;
} TrailfocusDisplay;

typedef struct _TfWindowAttributes
{
	GLushort opacity;
	GLushort brightness;
	GLushort saturation;
} TfAttrib;

typedef struct _TrailfocusScreen
{
	int windowPrivateIndex;

	Window *win;
	TfAttrib *inc;
	Bool initialized;

	PaintWindowProc paintWindow;
} TrailfocusScreen;

typedef struct _TrailfocusWindow
{
	Bool isTfWindow;
	TfAttrib attribs;
}  TrailfocusWindow;

/* Core trailfocus functions. These do the real work. ---------------*/

/* Walks through the window-list and sets the opacity-levels for
 * all windows. The inner loop will result in ts->win[i] either
 * representing a recently focused window, or the least
 * focused window.
 */
static void setWindows(CompScreen * s)
{
	CompWindow *w;
	Bool wasTfWindow;

	TRAILFOCUS_SCREEN(s);
	int i = 0;
	int winMax = trailfocusGetWindowsCount(s);

	for (w = s->windows; w; w = w->next)
	{
		TRAILFOCUS_WINDOW(w);
		wasTfWindow = tw->isTfWindow;
		tw->isTfWindow = TRUE;

		if (w->invisible || w->hidden || w->minimized)
			tw->isTfWindow = FALSE;
		else if (!matchEval(trailfocusGetWindowMatch(s), w))
			tw->isTfWindow = FALSE;

		if (wasTfWindow && !tw->isTfWindow)
			addWindowDamage(w);

		if (tw->isTfWindow)
		{
			for (i = 0; i < winMax; i++)
				if (w->id == ts->win[i])
					break;

			if (!wasTfWindow ||
				(memcmp(&tw->attribs, &ts->inc[i], sizeof(TfAttrib)) != 0))
			{
				addWindowDamage(w);
			}

			tw->attribs = ts->inc[i];
		}
	}
}

/* Push a new window-id on the trailfocus window-stack (not to be
 * confused with the real window stack).  Only keep one copy of a
 * window on the stack. If the window allready exist on the stack,
 * move it to the top.
 */
static CompScreen *pushWindow(CompDisplay * d, Window id)
{
	int i;
	int tmp, winMax;
	CompWindow *w;

	w = findWindowAtDisplay(d, id);
	if (!w)
		return NULL;

	TRAILFOCUS_SCREEN(w->screen);

	if (!matchEval(trailfocusGetWindowMatch(w->screen), w))
		return NULL;

	tmp = winMax = trailfocusGetWindowsCount(w->screen);
	for (i = 0; i < winMax; i++)
		if (ts->win[i] == id)
			break;

	if (i == 0)
		return NULL;

	for (; i > 0; i--)
		ts->win[i] = ts->win[i - 1];

	ts->win[0] = id;
	return w->screen;
}

/* Find a window on a screen.... Unlike the findWindowAtScreen which
 * core provides, we don't intend to search for the same window several
 * times in a row so we optimize for* the normal situation of searching for 
 * a window only once in a row.
 */
static CompWindow *findWindow(CompScreen * s, Window id)
{
	CompWindow *w;

	for (w = s->windows; w; w = w->next)
		if (w->id == id)
			return w;

	return NULL;
}

/* Walks through the existing stack and removes windows that should
 * (no longer) be there. Used for option-change.
 */
static void cleanList(CompScreen * s)
{
	TRAILFOCUS_SCREEN(s);
	CompWindow *w;
	int i, j, length;
	int winMax;

	winMax = trailfocusGetWindowsCount(s);

	for (i = 0; i < winMax; i++)
	{
		w = findWindow(s, ts->win[i]);
		if (!w || !matchEval(trailfocusGetWindowMatch(s), w))
			ts->win[i] = 0;
	}

	length = winMax;
	for (i = 0; i < length; i++)
	{
		if (!ts->win[i])
		{
			for (j = i; j < length - 1; j++)
				ts->win[j] = ts->win[j + 1];
			length--;
		}
	}
	for (; length < winMax; length++)
		ts->win[length] = 0;
}

/* Handles the event if it was a FocusIn event.  */
static void trailfocusHandleEvent(CompDisplay * d, XEvent * event)
{
	TRAILFOCUS_DISPLAY(d);
	CompScreen *s;

	switch (event->type)
	{
	case FocusIn:
		s = pushWindow(d, event->xfocus.window);
		if (s)
			setWindows(s);
		break;
	default:
		break;
	}

	UNWRAP(td, d, handleEvent);
	(*d->handleEvent) (d, event);
	WRAP(td, d, handleEvent, trailfocusHandleEvent);
}

/* Settings changed. Reallocate rs->inc and re-populate it and the
 * rest of the TrailfocusScreen (-wMask).
 */
static void recalculateAttributes(CompScreen * s)
{
	TRAILFOCUS_SCREEN(s);
	TfAttrib tmp, min, max;
	int i;
	int start;
	int winMax;

	start = trailfocusGetWindowsStart(s) - 1;
	winMax = trailfocusGetWindowsCount(s);

	if (start >= winMax)
	{
		compLogMessage (s->display, "trailfocus", CompLogLevelWarn,
						"Attempting to define start higher than max windows.");
		start = winMax - 1;
	}

	min.opacity = trailfocusGetMinOpacity(s) * OPAQUE / 100;
	min.brightness = trailfocusGetMinBrightness(s) * OPAQUE / 100;
	min.saturation = trailfocusGetMinSaturation(s) * OPAQUE / 100;
	max.opacity = trailfocusGetMaxOpacity(s) * OPAQUE / 100;
	max.brightness = trailfocusGetMaxBrightness(s) * OPAQUE / 100;
	max.saturation = trailfocusGetMaxSaturation(s) * OPAQUE / 100;

	ts->win = realloc(ts->win, sizeof(Window) * (winMax + 1));
	ts->inc = realloc(ts->inc, sizeof(TfAttrib) * (winMax + 1));

	tmp.opacity = (max.opacity - min.opacity) / ((winMax - start));
	tmp.brightness =
			(max.brightness - min.brightness) / ((winMax - start));
	tmp.saturation =
			(max.saturation - min.saturation) / ((winMax - start));

	for (i = 0; i < start; ++i)
		ts->inc[i] = max;

	for (i = 0; i + start <= winMax; i++)
	{
		ts->inc[i + start].opacity = max.opacity - (tmp.opacity * i);
		ts->inc[i + start].brightness = max.brightness - (tmp.brightness * i);
		ts->inc[i + start].saturation = max.saturation - (tmp.saturation * i);
		ts->win[i + start] = 0;
	}
//  ts->inc[i+start] = min;
}

static Bool trailfocusPaintWindow(CompWindow *w, const WindowPaintAttrib *attrib,
								  const CompTransform *transform,
								  Region region, unsigned int mask)
{
	Bool status;
	TRAILFOCUS_WINDOW(w);
	TRAILFOCUS_SCREEN(w->screen);

	if (!ts->initialized)
	{
		setWindows(w->screen);
		ts->initialized = TRUE;
	}

	if (tw->isTfWindow)
	{
		WindowPaintAttrib wAttrib = *attrib;

		wAttrib.opacity = MIN(attrib->opacity, tw->attribs.opacity);
		wAttrib.brightness = MIN(attrib->brightness, tw->attribs.brightness);
		wAttrib.saturation = MIN(attrib->saturation, tw->attribs.brightness);

		UNWRAP(ts, w->screen, paintWindow);
		status = (*w->screen->paintWindow) (w, &wAttrib, transform, region, mask);
		WRAP(ts, w->screen, paintWindow, trailfocusPaintWindow);
	}
	else
	{
		UNWRAP(ts, w->screen, paintWindow);
		status = (*w->screen->paintWindow) (w, attrib, transform, region, mask);
		WRAP(ts, w->screen, paintWindow, trailfocusPaintWindow);
	}

	return status;
}

/* Configuration, initliazation, boring stuff. ----------------------- */

static void trailfocusScreenOptionChanged(CompScreen *s, CompOption *opt, TrailfocusScreenOptions num)
{
	switch (num)
	{
		case TrailfocusScreenOptionMinOpacity:
		case TrailfocusScreenOptionMaxOpacity:
		case TrailfocusScreenOptionMinSaturation:
		case TrailfocusScreenOptionMaxSaturation:
		case TrailfocusScreenOptionMinBrightness:
		case TrailfocusScreenOptionMaxBrightness:
		case TrailfocusScreenOptionWindowsStart:
		case TrailfocusScreenOptionWindowsCount:
			recalculateAttributes(s);
			break;
		default:
			break;
	}

	cleanList(s);
	pushWindow(s->display, s->display->activeWindow);
	setWindows(s);
}

static Bool trailfocusInitWindow(CompPlugin *p, CompWindow *w)
{
	TRAILFOCUS_SCREEN(w->screen);

	TrailfocusWindow *tw = (TrailfocusWindow *) calloc(1, sizeof(TrailfocusWindow));
	w->privates[ts->windowPrivateIndex].ptr = tw;

	tw->isTfWindow = FALSE;

	return TRUE;
}

static void trailfocusFiniWindow(CompPlugin *p, CompWindow *w)
{
	TRAILFOCUS_WINDOW(w);

	free(tw);
}

/* Remember to reset windows to some sane value when we unload */
static void trailfocusFiniScreen(CompPlugin * p, CompScreen * s)
{
	TRAILFOCUS_SCREEN(s);

	if (ts->win)
		free(ts->win);
	if (ts->inc)
		free(ts->inc);

	UNWRAP(ts, s, paintWindow);

	free(ts);
}

/* Remember to populate the TrailFocus screen properly, and push the
 * active window on the stack, then set windows.
 */
static Bool trailfocusInitScreen(CompPlugin * p, CompScreen * s)
{
	TRAILFOCUS_DISPLAY(s->display);

	TrailfocusScreen *ts =
			(TrailfocusScreen *) calloc(1, sizeof(TrailfocusScreen));

	ts->windowPrivateIndex = allocateWindowPrivateIndex(s);
	if (ts->windowPrivateIndex < 0)
	{
		free(ts);
		return FALSE;
	}

	trailfocusSetWindowMatchNotify(s, trailfocusScreenOptionChanged);
	trailfocusSetWindowsCountNotify(s, trailfocusScreenOptionChanged);
	trailfocusSetWindowsStartNotify(s, trailfocusScreenOptionChanged);
	trailfocusSetMinOpacityNotify(s, trailfocusScreenOptionChanged);
	trailfocusSetMaxOpacityNotify(s, trailfocusScreenOptionChanged);
	trailfocusSetMinSaturationNotify(s, trailfocusScreenOptionChanged);
	trailfocusSetMaxSaturationNotify(s, trailfocusScreenOptionChanged);
	trailfocusSetMinBrightnessNotify(s, trailfocusScreenOptionChanged);
	trailfocusSetMaxBrightnessNotify(s, trailfocusScreenOptionChanged);

	s->privates[td->screenPrivateIndex].ptr = ts;

	WRAP(ts, s, paintWindow, trailfocusPaintWindow);

	recalculateAttributes(s);
	pushWindow(s->display, s->display->activeWindow);

	ts->initialized = FALSE;

	return TRUE;
}

static Bool trailfocusInitDisplay(CompPlugin * p, CompDisplay * d)
{
	TrailfocusDisplay *td =
			(TrailfocusDisplay *) malloc(sizeof(TrailfocusDisplay));

	td->screenPrivateIndex = allocateScreenPrivateIndex(d);
	if (td->screenPrivateIndex < 0)
	{
		free(td);
		return FALSE;
	}

	d->privates[displayPrivateIndex].ptr = td;
	WRAP(td, d, handleEvent, trailfocusHandleEvent);
	return TRUE;
}

static void trailfocusFiniDisplay(CompPlugin * p, CompDisplay * d)
{
	TRAILFOCUS_DISPLAY(d);

	UNWRAP(td, d, handleEvent);

	freeScreenPrivateIndex(d, td->screenPrivateIndex);
	free(td);
}

static Bool trailfocusInit(CompPlugin * p)
{
	displayPrivateIndex = allocateDisplayPrivateIndex();
	if (displayPrivateIndex < 0)
		return FALSE;
	return TRUE;
}

static void trailfocusFini(CompPlugin * p)
{
	if (displayPrivateIndex >= 0)
		freeDisplayPrivateIndex(displayPrivateIndex);
}

static int trailfocusGetVersion(CompPlugin *p, int version)
{
	return ABIVERSION;
}

CompPluginVTable trailfocusVTable = {
	"trailfocus",
	trailfocusGetVersion,
	0,
	trailfocusInit,
	trailfocusFini,
	trailfocusInitDisplay,
	trailfocusFiniDisplay,
	trailfocusInitScreen,
	trailfocusFiniScreen,
	trailfocusInitWindow,
	trailfocusFiniWindow,
	0,							// trailfocusGetDisplayOptions,
	0,							// trailfocusSetDisplayOptions,
	0, 
	0,
	0,
	0,
	0,
	0
};

CompPluginVTable *getCompPluginInfo(void)
{
	return &trailfocusVTable;
}
