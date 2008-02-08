/*
 * Notification plugin for compiz
 *
 * Copyright (C) 2007 Mike Dransfield (mike (at) blueroot.co.uk)
 * Maintained by Danny Baumann <dannybaumann (at) web.de>
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */


#include <libnotify/notify.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <compiz-core.h>

#define NOTIFY_DISPLAY_OPTION_TIMEOUT   0
#define NOTIFY_DISPLAY_OPTION_MAX_LEVEL 1
#define NOTIFY_DISPLAY_OPTION_NUM       2

#define IMAGE_DIR ".compiz/images"

#define NOTIFY_TIMEOUT_DEFAULT -1
#define NOTIFY_TIMEOUT_NEVER    0

static int displayPrivateIndex;

static CompMetadata notifyMetadata;

typedef struct _NotifyDisplay {
    LogMessageProc logMessage;

    int        timeout;
    CompOption opt[NOTIFY_DISPLAY_OPTION_NUM];
} NotifyDisplay;

#define GET_NOTIFY_DISPLAY(d)				       \
    ((NotifyDisplay *) (d)->base.privates[displayPrivateIndex].ptr)

#define NOTIFY_DISPLAY(d)		       \
    NotifyDisplay *nd = GET_NOTIFY_DISPLAY (d)

#define NUM_OPTIONS(s) (sizeof ((s)->opt) / sizeof (CompOption))


static void
notifyLogMessage (CompDisplay  *d,
		  const char   *component,
		  CompLogLevel level,
		  const char   *message)
{
    NotifyNotification *n;
    char               *logLevel, iconFile[256], *iconUri, *homeDir;
    int                maxLevel;

    NOTIFY_DISPLAY (d);

    maxLevel = nd->opt[NOTIFY_DISPLAY_OPTION_MAX_LEVEL].value.i;
    if (level > maxLevel)
    {
	UNWRAP (nd, d, logMessage);
	(*d->logMessage) (d, component, level, message);
	WRAP (nd, d, logMessage, notifyLogMessage);

	return;
    }

    homeDir = getenv ("HOME");
    if (!homeDir)
	return;

    snprintf (iconFile, 256, "%s/%s/%s", homeDir, IMAGE_DIR, "compiz.png");

    iconUri = malloc (sizeof (char) * strlen (iconFile) + 8);
    if (!iconUri)
	return;

    sprintf (iconUri, "file://%s", iconFile);

    logLevel = (char *) logLevelToString (level);

    n = notify_notification_new (logLevel,
                                 message,
                                 iconUri, NULL);

    notify_notification_set_timeout (n, nd->timeout);

    switch (level)
    {
    case CompLogLevelFatal:
    case CompLogLevelError:
	notify_notification_set_urgency (n, NOTIFY_URGENCY_CRITICAL);
	break;
    case CompLogLevelWarn:
	notify_notification_set_urgency (n, NOTIFY_URGENCY_NORMAL);
	break;
    default:
	notify_notification_set_urgency (n, NOTIFY_URGENCY_LOW);
	break;
    }

    if (!notify_notification_show (n, NULL))
	fprintf (stderr, "failed to send notification\n");

    g_object_unref (G_OBJECT (n));
    free (iconUri);

    UNWRAP (nd, d, logMessage);
    (*d->logMessage) (d, component, level, message);
    WRAP (nd, d, logMessage, notifyLogMessage);
}

static const CompMetadataOptionInfo notifyDisplayOptionInfo[] = {
    { "timeout", "int", "<min>-1</min><max>30</max><default>-1</default>", 0, 0 },
    { "max_log_level", "int", "<default>1</default>", 0, 0 }
};

static Bool
notifyInitDisplay (CompPlugin  *p,
		   CompDisplay *d)
{
    NotifyDisplay *nd;

    if (!checkPluginABI ("core", CORE_ABIVERSION))
	return FALSE;

    nd = malloc (sizeof (NotifyDisplay));
    if (!nd)
	return FALSE;

    if (!compInitDisplayOptionsFromMetadata (d,
					     &notifyMetadata,
					     notifyDisplayOptionInfo,
					     nd->opt,
					     NOTIFY_DISPLAY_OPTION_NUM))
    {
	free (nd);
	return FALSE;
    }

    notify_init ("compiz");
    nd->timeout = 2000;

    d->base.privates[displayPrivateIndex].ptr = nd;

    WRAP (nd, d, logMessage, notifyLogMessage);

    return TRUE;
}

static void
notifyFiniDisplay (CompPlugin  *p,
		   CompDisplay *d)
{
    NOTIFY_DISPLAY (d);

    UNWRAP (nd, d, logMessage);

    if (notify_is_initted ())
	notify_uninit ();

    free (nd);
}

static Bool
notifyInit (CompPlugin *p)
{
    if (!compInitPluginMetadataFromInfo (&notifyMetadata,
					 p->vTable->name,
					 notifyDisplayOptionInfo,
					 NOTIFY_DISPLAY_OPTION_NUM,
					 0, 0))
	return FALSE;

    displayPrivateIndex = allocateDisplayPrivateIndex ();
    if (displayPrivateIndex < 0)
    {
	compFiniMetadata (&notifyMetadata);
	return FALSE;
    }

    compAddMetadataFromFile (&notifyMetadata, p->vTable->name);

    return TRUE;
}

static CompBool
notifyInitObject (CompPlugin *p,
		  CompObject *o)
{
    static InitPluginObjectProc dispTab[] = {
    	(InitPluginObjectProc) 0, /* InitCore */
	(InitPluginObjectProc) notifyInitDisplay
    };

    RETURN_DISPATCH (o, dispTab, ARRAY_SIZE (dispTab), TRUE, (p, o));
}

static void
notifyFiniObject (CompPlugin *p,
		  CompObject *o)
{
    static FiniPluginObjectProc dispTab[] = {
	(FiniPluginObjectProc) 0, /* FiniCore */
	(FiniPluginObjectProc) notifyFiniDisplay
    };

    DISPATCH (o, dispTab, ARRAY_SIZE (dispTab), (p, o));
}

static CompOption *
notifyGetDisplayOptions (CompPlugin   *p,
			 CompDisplay  *display,
			 int	      *count)
{
    NOTIFY_DISPLAY (display);

    *count = NUM_OPTIONS (nd);
    return nd->opt;
}

static Bool
notifySetDisplayOption (CompPlugin      *p,
			CompDisplay     *display,
			const char      *name,
			CompOptionValue *value)
{
    CompOption *o;
    int	       index;

    NOTIFY_DISPLAY (display);

    o = compFindOption (nd->opt, NUM_OPTIONS (nd), name, &index);
    if (!o)
	return FALSE;

    switch (index) {
    case NOTIFY_DISPLAY_OPTION_TIMEOUT:
	if (compSetIntOption (o, value))
	{
	    if (value->i == -1)
		nd->timeout = value->i;
	    else
		nd->timeout = value->i * 1000;
	    return TRUE;
	}
    default:
	if (compSetOption (o, value))
	    return TRUE;
	break;
    }

    return FALSE;
}

static CompOption *
notifyGetObjectOptions (CompPlugin *plugin,
			CompObject *object,
			int	   *count)
{
    static GetPluginObjectOptionsProc dispTab[] = {
	(GetPluginObjectOptionsProc) 0, /* GetCoreOptions */
	(GetPluginObjectOptionsProc) notifyGetDisplayOptions
    };

    RETURN_DISPATCH (object, dispTab, ARRAY_SIZE (dispTab),
		     (void *) (*count = 0), (plugin, object, count));
}

static CompBool
notifySetObjectOption (CompPlugin      *plugin,
		       CompObject      *object,
		       const char      *name,
		       CompOptionValue *value)
{
    static SetPluginObjectOptionProc dispTab[] = {
	(SetPluginObjectOptionProc) 0, /* SetCoreOption */
	(SetPluginObjectOptionProc) notifySetDisplayOption
    };

    RETURN_DISPATCH (object, dispTab, ARRAY_SIZE (dispTab), FALSE,
		     (plugin, object, name, value));
}

static void
notifyFini (CompPlugin *p)
{
    freeDisplayPrivateIndex (displayPrivateIndex);
    compFiniMetadata (&notifyMetadata);
}

static CompMetadata *
notifyGetMetadata (CompPlugin *plugin)
{
    return &notifyMetadata;
}

static CompPluginVTable notifyVTable = {
    "notification",
    notifyGetMetadata,
    notifyInit,
    notifyFini,
    notifyInitObject,
    notifyFiniObject,
    notifyGetObjectOptions,
    notifySetObjectOption
};

CompPluginVTable *
getCompPluginInfo20070830 (void)
{
    return &notifyVTable;
}
