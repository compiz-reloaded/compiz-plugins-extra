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

#include <canberra.h>

#include <compiz-core.h>
#include "bell_options.h"


static int displayPrivateIndex;

typedef struct _BellDisplay
{
    ca_context *canberraContext;
}
BellDisplay;

#define GET_BELL_DISPLAY(d) \
    ((BellDisplay *) (d)->base.privates[displayPrivateIndex].ptr)
#define BELL_DISPLAY(d) \
    BellDisplay *bd = GET_BELL_DISPLAY(d);


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

    bellSetBellInitiate (d, bell);

    d->base.privates[displayPrivateIndex].ptr = bd;

    return TRUE;
}

static void
bellFiniDisplay (CompPlugin  *p,
		 CompDisplay *d)
{
    BELL_DISPLAY (d);

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
