/*
 *
 * Compiz bell plugin
 *
 * bell.c
 *
 * Copyright (c) 2011 Emily Strickland <emily@zubon.org>
 * Copyright (c) 2019 Colomban Wendling <cwendling@hypra.fr>
 *
 * Authors:
 * Emily Strickland <emily@zubon.org>
 * Colomban Wendling <cwendling@hypra.fr>
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
 **/

#include <string.h>
#include <stdlib.h>
#include <canberra.h>
#include <xsettings-client.h>

#include <compiz-core.h>
#include "bell_options.h"


static int displayPrivateIndex;

typedef struct _BellDisplay
{
    ca_context *canberraContext;
    /* In theory we could like to have an XSettings client on each
     * screen, but in practice most managers only bother setting up
     * on the default screen, so we do the same for the sake of
     * simplicity */
    XSettingsClient *xsettings_client;
    HandleEventProc handleEvent;
}
BellDisplay;

#define GET_BELL_DISPLAY(d) \
    ((BellDisplay *) (d)->base.privates[displayPrivateIndex].ptr)
#define BELL_DISPLAY(d) \
    BellDisplay *bd = GET_BELL_DISPLAY(d);

#ifndef CLAMP
#   define CLAMP(v, min, max) (((v) > (max)) ? (max) : (((v) < (min)) ? (min) : (v)))
#endif


static Bool
bell (CompDisplay     *d,
      CompAction      *action,
      CompActionState state,
      CompOption      *option,
      int             nOption)
{
    int r;
    CompWindow *w;
    ca_proplist *p;
    BELL_DISPLAY (d);

    if (ca_proplist_create (&p) < 0)
	return FALSE;

    ca_proplist_sets (p, CA_PROP_EVENT_ID, "bell-window-system");
    ca_proplist_sets (p, CA_PROP_EVENT_DESCRIPTION, "Bell event");
    ca_proplist_sets (p, CA_PROP_CANBERRA_CACHE_CONTROL, "permanent");

    w = findWindowAtDisplay (d, d->activeWindow);
    if (w)
    {
	ca_proplist_setf (p, CA_PROP_WINDOW_X11_SCREEN, "%d", w->screen->screenNum);
	ca_proplist_setf (p, CA_PROP_WINDOW_X11_XID, "%ld", w->id);
	ca_proplist_sets (p, CA_PROP_APPLICATION_NAME, w->resName);
	ca_proplist_setf (p, CA_PROP_WINDOW_DESKTOP, "%u", w->desktop);

	ca_proplist_setf (p, CA_PROP_WINDOW_X, "%d", w->attrib.x);
	ca_proplist_setf (p, CA_PROP_WINDOW_Y, "%d", w->attrib.y);
	ca_proplist_setf (p, CA_PROP_WINDOW_WIDTH, "%d", w->attrib.width);
	ca_proplist_setf (p, CA_PROP_WINDOW_HEIGHT, "%d", w->attrib.height);

	if (bellGetSpatialSounds (d))
	{
	    int x, y;

	    x = w->attrib.x + w->attrib.width / 2;
	    x = CLAMP (x, 0, w->screen->width);
	    y = w->attrib.y + w->attrib.height / 2;
	    y = CLAMP (y, 0, w->screen->height);
	    /* The convoluted format is to avoid locale-specific floating
	     * point formatting.  Taken from libcanberra-gtk. */
	    ca_proplist_setf (p, CA_PROP_WINDOW_HPOS, "%d.%03d",
			      (int) (x / w->screen->width),
			      (int) (x * 1000.0 / w->screen->width) % 1000);
	    ca_proplist_setf (p, CA_PROP_WINDOW_VPOS, "%d.%03d",
			      (int) (y / w->screen->height),
			      (int) (y * 1000.0 / w->screen->height) % 1000);

	    compLogMessage ("bell", CompLogLevelDebug,
			    "spatial: screen geometry: %dx%d%+d%+d",
			    w->screen->width, w->screen->height,
			    w->screen->x, w->screen->y);
	    compLogMessage ("bell", CompLogLevelDebug,
			    "spatial: window geometry: %dx%d%+d%+d (%d.%03d,%d.%03d)",
			    w->attrib.width, w->attrib.height,
			    w->attrib.x, w->attrib.y,
			    (int) (x / w->screen->width),
			    (int) (x * 1000.0 / w->screen->width) % 1000,
			    (int) (y / w->screen->height),
			    (int) (y * 1000.0 / w->screen->height) % 1000);
	}
    }

    compLogMessage ("bell", CompLogLevelDebug, "playing bell");
    if ((r = ca_context_play_full (bd->canberraContext, 1, p, NULL, NULL)) < 0)
    {
	compLogMessage ("bell", CompLogLevelWarn, "couldn't play bell: %s",
			ca_strerror (r));
    }

    ca_proplist_destroy (p);

    /* Allow other plugins to handle bell event */
    return FALSE;
}

static void
xsettings_notify (const char       *name,
		  XSettingsAction   action,
		  XSettingsSetting *setting,
		  void             *cb_data)
{
    BellDisplay *bd = cb_data;

    if (setting &&
	strcmp (setting->name, "Net/SoundThemeName") == 0 &&
	setting->type == XSETTINGS_TYPE_STRING)
    {
	compLogMessage ("bell", CompLogLevelDebug, "XSettings notify: %s=%s",
			name, setting->data.v_string);

	ca_context_change_props (bd->canberraContext,
				 CA_PROP_CANBERRA_XDG_THEME_NAME,
				 setting->data.v_string,
				 NULL);
    }
    else if (setting &&
	     strcmp (setting->name, "Net/EnableEventSounds") == 0 &&
	     setting->type == XSETTINGS_TYPE_INT)
    {
	compLogMessage ("bell", CompLogLevelDebug, "XSettings notify: %s=%d",
			name, setting->data.v_int);

	ca_context_change_props (bd->canberraContext,
				 CA_PROP_CANBERRA_ENABLE,
				 setting->data.v_int ? "1" : "0",
				 NULL);
    }
}

static void
bellHandleEvent (CompDisplay *d,
		 XEvent      *event)
{
    BELL_DISPLAY (d);

    xsettings_client_process_event (bd->xsettings_client, event);

    UNWRAP (bd, d, handleEvent);
    (*d->handleEvent) (d, event);
    WRAP (bd, d, handleEvent, bellHandleEvent);
}

static Bool
bellInitDisplay (CompPlugin  *p,
		 CompDisplay *d)
{
    BellDisplay *bd;
    int          error;

    if (!checkPluginABI ("core", CORE_ABIVERSION))
	return FALSE;

    bd = malloc (sizeof *bd);
    if (!bd)
	return FALSE;

    bd->canberraContext = NULL;
    if ((error = ca_context_create (&bd->canberraContext)) < 0 ||
	(error = ca_context_change_props (bd->canberraContext,
					  CA_PROP_APPLICATION_NAME,
					  "Compiz bell plugin",
					  CA_PROP_APPLICATION_ID,
					  "org.compiz.plugin.Bell",
					  CA_PROP_WINDOW_X11_DISPLAY,
					  DisplayString (d->display),
					  NULL)) < 0)
    {
        compLogMessage ("bell", CompLogLevelWarn, "couldn't initialize canberra: %s",
                        ca_strerror (error));
	if (bd->canberraContext)
	    ca_context_destroy (bd->canberraContext);
        free (bd);
	return FALSE;
    }

    bd->xsettings_client = xsettings_client_new (d->display,
						 DefaultScreen(d->display),
						 xsettings_notify,
						 NULL, bd);
    if (!bd->xsettings_client)
    {
	compLogMessage ("bell", CompLogLevelWarn, "couldn't allocate xsettings client");
	ca_context_destroy (bd->canberraContext);
	free (bd);
	return FALSE;
    }

    bellSetBellInitiate (d, bell);

    d->base.privates[displayPrivateIndex].ptr = bd;

    WRAP (bd, d, handleEvent, bellHandleEvent);

    return TRUE;
}

static void
bellFiniDisplay (CompPlugin  *p,
		 CompDisplay *d)
{
    BELL_DISPLAY (d);

    UNWRAP (bd, d, handleEvent);

    ca_context_destroy (bd->canberraContext);
    free (bd);
}

static Bool
bellInit (CompPlugin *p)
{
    displayPrivateIndex = allocateDisplayPrivateIndex();

    return displayPrivateIndex >= 0;
}

static void
bellFini (CompPlugin *p)
{
    freeDisplayPrivateIndex (displayPrivateIndex);
}

static CompBool
bellInitObject (CompPlugin *p,
		CompObject *o)
{
    static InitPluginObjectProc dispTab[] = {
	(InitPluginObjectProc) 0, /* InitCore */
	(InitPluginObjectProc) bellInitDisplay
    };

    RETURN_DISPATCH (o, dispTab, ARRAY_SIZE (dispTab), TRUE, (p, o));
}

static void
bellFiniObject (CompPlugin *p,
		CompObject *o)
{
    static FiniPluginObjectProc dispTab[] = {
	(FiniPluginObjectProc) 0, /* FiniCore */
	(FiniPluginObjectProc) bellFiniDisplay
    };

    DISPATCH (o, dispTab, ARRAY_SIZE (dispTab), (p, o));
}

CompPluginVTable bellVTable = {
    "bell",
    0,
    bellInit,
    bellFini,
    bellInitObject,
    bellFiniObject,
    0,
    0
};

CompPluginVTable *
getCompPluginInfo (void)
{
    return &bellVTable;
}
