/*
 *
 * Compiz bell plugin
 *
 * bell.c
 *
 * Copyright (c) 2011 Emily Strickland <emily@zubon.org>
 * Copyright (c) 2018 Colomban Wendling <cwendling@hypra.fr>
 *
 * Authors:
 * Emily Strickland <emily@zubon.org>
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
    BELL_DISPLAY (d);
    int error;

    if ((error = ca_context_change_props (bd->canberraContext,
					  CA_PROP_WINDOW_X11_DISPLAY,
					  DisplayString (d->display),
					  /*
					  CA_PROP_WINDOW_X11_SCREEN,
					  CA_PROP_WINDOW_ID,
					  ...*/
					  NULL)) < 0)
    {
	compLogMessage ("bell", CompLogLevelWarn, "couldn't update properties - %s",
			ca_strerror (error));
    }
    if ((error = ca_context_play (bd->canberraContext, 0,
				  CA_PROP_EVENT_ID, "bell",
				  CA_PROP_CANBERRA_CACHE_CONTROL, "permanent",
				  NULL)) < 0)
    {
	compLogMessage ("bell", CompLogLevelWarn, "couldn't play bell - %s",
			ca_strerror (error));
    }

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
					  NULL)) < 0 ||
	(error = ca_context_open (bd->canberraContext)) < 0)
    {
        compLogMessage ("bell", CompLogLevelWarn, "couldn't initialize canberra - %s",
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
