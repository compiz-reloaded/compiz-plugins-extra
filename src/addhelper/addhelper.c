/**
 * Beryl  ADD Helper. Makes it easier to concentrate.
 *
 * Copyright (c) 2007 Kristian Lyngstøl <kristian@beryl-project.org>
 * Ported and highly modified by Patrick Niklaus <marex@beryl-project.org>
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
 * This plugin provides a toggle-feature that dims all but the active
 * window. This makes it easier for people with lousy concentration
 * to focus. Like me.
 * 
 * Please note any major changes to the code in this header with who you
 * are and what you did. 
 *
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <X11/Xatom.h>
#include <X11/extensions/Xrender.h>
#include <compiz.h>

#include "addhelper_options.h"

#define GET_ADD_DISPLAY(d)                            \
	((AddHelperDisplay *) (d)->privates[displayPrivateIndex].ptr)
#define ADD_DISPLAY(d)                                \
    AddHelperDisplay *ad = GET_ADD_DISPLAY (d)
#define GET_ADD_SCREEN(s, ad)                         \
	((AddHelperScreen *) (s)->privates[(ad)->screenPrivateIndex].ptr)
#define ADD_SCREEN(s)                                 \
	AddHelperScreen *as = GET_ADD_SCREEN (s, GET_ADD_DISPLAY (s->display))
#define GET_ADD_WINDOW(w, as) \
	((AddHelperWindow *) (w)->privates[ (as)->windowPrivateIndex].ptr)
#define ADD_WINDOW(w) \
	AddHelperWindow *aw = GET_ADD_WINDOW (w, GET_ADD_SCREEN  (w->screen, GET_ADD_DISPLAY (w->screen->display)))

static int displayPrivateIndex = 0;

typedef struct _AddHelperDisplay
{
	int screenPrivateIndex;
	int active_screen;

	GLushort opacity;
	GLushort brightness;
	GLushort saturation;

	CompMatch *match;
	Bool toggle;
	Window lastActive;

	HandleEventProc handleEvent;
} AddHelperDisplay;

typedef struct _AddHelperScreen
{
	int windowPrivateIndex;

	PaintWindowProc paintWindow;
} AddHelperScreen;

typedef struct _AddHelperWindow
{
	Bool dim;
} AddHelperWindow;

/* Walk through all windows of the screen and adjust them if they
 * are not the active window. If reset is true, this will reset
 * the windows, including the active. Otherwise, it will dim 
 * and reset the active. 
 */
static void walk_windows(CompDisplay *d)
{
	ADD_DISPLAY(d);
	
	CompScreen *s;
	CompWindow *w;
	for (s = d->screens; s; s = s->next)
	{
		for (w = s->windows; w; w = w->next)
		{
			ADD_WINDOW(w);

			aw->dim = FALSE;

			if (!ad->toggle)
				continue;

			if (w->id == d->activeWindow)
				continue;

			if(w->invisible || w->destroyed || w->hidden || w->minimized)
				continue;

			if (!matchEval(ad->match, w))
				continue;

			aw->dim = TRUE;
		}

		damageScreen(s);
	}
}

/* Checks if the window is dimmed and, if so, paints it with the modified
 * paint attributes.
 */
static Bool addhelperPaintWindow(CompWindow *w, const WindowPaintAttrib *attrib,
								  const CompTransform *transform,
								  Region region, unsigned int mask)
{
	ADD_SCREEN(w->screen);
	ADD_DISPLAY(w->screen->display);
	ADD_WINDOW(w);
	Bool status;

	if (aw->dim)
	{
		// copy the paint attribute
		WindowPaintAttrib wAttrib = *attrib;

		// applies the lowest value
		wAttrib.opacity = MIN(attrib->opacity, ad->opacity);
		wAttrib.brightness = MIN(attrib->brightness, ad->brightness);
		wAttrib.saturation = MIN(attrib->saturation, ad->saturation);

		// continue painting with the modified attribute
		UNWRAP(as, w->screen, paintWindow);
		status = (*w->screen->paintWindow) (w, &wAttrib, transform, region, mask);
		WRAP(as, w->screen, paintWindow, addhelperPaintWindow);
	}
	else
	{
		// the window is not dimmed, so its painted normal
		UNWRAP(as, w->screen, paintWindow);
		status = (*w->screen->paintWindow) (w, attrib, transform, region, mask);
		WRAP(as, w->screen, paintWindow, addhelperPaintWindow);
	}

	return status;
}

/* Takes the inital event. 
 * This checks for focus change and acts on it.
 */
static void addhelperHandleEvent(CompDisplay * d, XEvent * event)
{
	ADD_DISPLAY(d);

	UNWRAP(ad, d, handleEvent);
	(*d->handleEvent) (d, event);
	WRAP(ad, d, handleEvent, addhelperHandleEvent);
	
	if (!ad->toggle)
		return;

	if (event->type == PropertyNotify && ad->lastActive != d->activeWindow)
	{
		walk_windows(d);
		ad->lastActive = d->activeWindow;
	}
}

/* Configuration, initialization, boring stuff. ----------------------- */

/* Takes the action and toggles us.
 */
static Bool addhelperToggle(CompDisplay * d, CompAction * ac,
						   CompActionState state, CompOption * option,
						   int nOption)
{
	ADD_DISPLAY(d);

	ad->toggle = !ad->toggle;
	walk_windows(d);

	return TRUE;
}

/* Change notify for bcop */
static void addhelperDisplayOptionChanged(CompDisplay *d, CompOption *opt, AddhelperDisplayOptions num)
{
	ADD_DISPLAY(d);

	switch (num)
	{
	case AddhelperDisplayOptionBrightness:
		ad->brightness = (addhelperGetBrightness(d) * 0xffff) / 100;
		break;
	case AddhelperDisplayOptionSaturation:
		ad->saturation = (addhelperGetSaturation(d) * 0xffff) / 100;
		break;
	case AddhelperDisplayOptionOpacity:
		ad->opacity = (addhelperGetOpacity(d) * 0xffff) / 100;
		break;
	case AddhelperDisplayOptionWindowTypes:
		ad->match = addhelperGetWindowTypes(d);
		break;
	default:
		break;
	}

}

static Bool addhelperInitWindow(CompPlugin * p, CompWindow * w)
{
	ADD_SCREEN(w->screen);

	AddHelperWindow *aw = (AddHelperWindow *) malloc(sizeof(AddHelperWindow));

	w->privates[as->windowPrivateIndex].ptr = aw;

	aw->dim = FALSE;

	return TRUE;
}

static void addhelperFiniWindow(CompPlugin * p, CompWindow * w)
{
	ADD_WINDOW(w);

	free(aw);
}

static Bool addhelperInitScreen(CompPlugin * p, CompScreen * s)
{
	ADD_DISPLAY(s->display);

	AddHelperScreen *as = (AddHelperScreen*) malloc(sizeof(AddHelperScreen));

	as->windowPrivateIndex = allocateWindowPrivateIndex(s);
	if (as->windowPrivateIndex < 0)
	{
		free(as);
		return FALSE;
	}

	WRAP(as, s, paintWindow, addhelperPaintWindow);

	s->privates[ad->screenPrivateIndex].ptr = as;

	return TRUE;
}

static void addhelperFiniScreen(CompPlugin * p, CompScreen * s)
{
	ADD_SCREEN(s);

	UNWRAP(as, s, paintWindow);
	
	free(as);
}

static Bool addhelperInitDisplay(CompPlugin * p, CompDisplay * d)
{
	AddHelperDisplay *ad = (AddHelperDisplay *) malloc(sizeof(AddHelperDisplay));

	ad->screenPrivateIndex = allocateScreenPrivateIndex(d);
	if (ad->screenPrivateIndex < 0)
	{
		free(ad);
		return FALSE;
	}

	d->privates[displayPrivateIndex].ptr = ad;

	addhelperSetToggleInitiate(d, addhelperToggle);
	addhelperSetBrightnessNotify(d, addhelperDisplayOptionChanged);
	addhelperSetOpacityNotify(d, addhelperDisplayOptionChanged);
	addhelperSetSaturationNotify(d, addhelperDisplayOptionChanged);

	ad->active_screen = d->screens->screenNum;
	ad->toggle = FALSE;
	ad->match = addhelperGetWindowTypes(d);
	ad->brightness = (addhelperGetBrightness(d) * 0xffff) / 100;
	ad->opacity = (addhelperGetOpacity(d) * 0xffff) / 100;
	ad->saturation = (addhelperGetSaturation(d) * 0xffff) / 100;
	ad->lastActive = None;

	WRAP(ad, d, handleEvent, addhelperHandleEvent);

	return TRUE;
}

static void addhelperFiniDisplay(CompPlugin * p, CompDisplay * d)
{
	ADD_DISPLAY(d);

	UNWRAP(ad, d, handleEvent);

	freeScreenPrivateIndex(d, ad->screenPrivateIndex);
	free(ad);
}

static Bool addhelperInit(CompPlugin * p)
{
	displayPrivateIndex = allocateDisplayPrivateIndex();
	if (displayPrivateIndex < 0)
		return FALSE;
	return TRUE;
}

static void addhelperFini(CompPlugin * p)
{
	if (displayPrivateIndex >= 0)
		freeDisplayPrivateIndex(displayPrivateIndex);
}

static int addhelperGetVersion(CompPlugin *p, int version)
{
	return ABIVERSION;
}

CompPluginVTable addhelperVTable = {
	"addhelper",
	addhelperGetVersion,
	0,
	addhelperInit,
	addhelperFini,
	addhelperInitDisplay,
	addhelperFiniDisplay,
	addhelperInitScreen,
	addhelperFiniScreen,
	addhelperInitWindow,
	addhelperFiniWindow,
	0, // addhelperGetDisplayOptions
	0, // addhelperSetDisplayOptions
	0, // addhelperGetScreenOptions,
	0, // addhelperSetScreenOptions,
};

CompPluginVTable *getCompPluginInfo(void)
{
	return &addhelperVTable;
}
