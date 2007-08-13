/* Compiz Gotovp plugin
 * Copyright (c) 2007 Robert Carr <racarr@opencompositing.org>
 * gotovp.c
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <X11/keysymdef.h>

#include <compiz.h>

#include "gotovp_options.h"

static int displayPrivateIndex;

/* number-to-keysym mapping */
static const KeySym numberKeySyms[3][10] = {
	/* number key row */
	{ XK_0, XK_1, XK_2, XK_3, XK_4, XK_5, XK_6, XK_7, XK_8, XK_9 },
	/* number keypad with activated NumLock */
	{ XK_KP_0, XK_KP_1, XK_KP_2, XK_KP_3, XK_KP_4,
	  XK_KP_5, XK_KP_6, XK_KP_7, XK_KP_8, XK_KP_9 },
	/* number keypad without NumLock */
	{ XK_KP_Insert, XK_KP_End, XK_KP_Down, XK_KP_Next, XK_KP_Left,
	  XK_KP_Begin, XK_KP_Right, XK_KP_Home, XK_KP_Up, XK_KP_Prior }
};

typedef struct _GotovpDisplay
{
	HandleEventProc handleEvent;

	CompScreen * activeScreen;
	int destination;
} GotovpDisplay;

#define GET_GOTOVP_DISPLAY(d)						\
	((GotovpDisplay *) (d)->privates[displayPrivateIndex].ptr)
#define GOTOVP_DISPLAY(d)				\
	GotovpDisplay * gd = GET_GOTOVP_DISPLAY(d)

static void
gotovpHandleEvent (CompDisplay *d, XEvent * event)
{
	GOTOVP_DISPLAY (d);
	CompScreen *s;

	switch (event->type)
	{
	case KeyPress:
		s = findScreenAtDisplay (d, event->xkey.root);
		if (s && (s == gd->activeScreen))
		{
			KeySym pressedKeySym;
			unsigned int mods;
			int i, row;

			pressedKeySym = XLookupKeysym (&event->xkey, 0);
			mods = keycodeToModifiers (d, event->xkey.keycode);
			if (mods & CompNumLockMask)
				row = 1; /* use first row of lookup table */
			else
				row = 2;

			for (i = 0; i < 10; i++)
			{
				/* first try to handle normal number keys */
				if (numberKeySyms[0][i] == pressedKeySym)
				{
					gd->destination *= 10;
					gd->destination += i;
					break;
				}
				else
				{
					if (numberKeySyms[row][i] == pressedKeySym)
					{
						gd->destination *= 10;
						gd->destination += i;
						break;
					}
				}
			}
		}
	}

	UNWRAP (gd, d, handleEvent);
	(*d->handleEvent) (d, event);
	WRAP (gd, d, handleEvent, gotovpHandleEvent);
}

static void
gotovpGotoViewport (CompScreen *s, int x, int y)
{
	XEvent xev;

	xev.xclient.type         = ClientMessage;
	xev.xclient.display      = s->display->display;
	xev.xclient.format       = 32;
	xev.xclient.message_type = s->display->desktopViewportAtom;
	xev.xclient.window       = s->root;
	xev.xclient.data.l[0]    = x * s->width;
	xev.xclient.data.l[1]    = y * s->height;
	xev.xclient.data.l[2]    = 0;
	xev.xclient.data.l[3]    = 0;
	xev.xclient.data.l[4]    = 0;

	XSendEvent (s->display->display, s->root, FALSE,
				SubstructureRedirectMask | SubstructureNotifyMask, &xev);
}

static Bool
gotovpBegin (CompDisplay *d,
			 CompAction * action,
			 CompActionState state,
			 CompOption * option, int nOption)
{
	GOTOVP_DISPLAY(d);

	if (!gd->activeScreen)
	{
	    Window xid;

		xid = getIntOptionNamed (option, nOption, "root", 0);
		gd->activeScreen = findScreenAtDisplay (d, xid);
		gd->destination = 0;

		if (state & CompActionStateInitKey)
			action->state |= CompActionStateTermKey;

		return TRUE;
	}

	return FALSE;
}

static Bool
gotovpTerm (CompDisplay *d,
		   	CompAction * action,
			CompActionState state,
			CompOption * option, int nOption)
{
	GOTOVP_DISPLAY(d);
	CompScreen *s = gd->activeScreen;
	int nx, ny;

	if (!s)
		return FALSE;

	gd->activeScreen = 0;

	if (gd->destination < 1 || gd->destination > (s->hsize * s->vsize))
		return FALSE;

	nx = (gd->destination - 1 ) % s->hsize;
	ny = (gd->destination - 1 ) / s->hsize;

	gotovpGotoViewport (s, nx, ny);

	return FALSE;
}

static Bool
gotovpSwitchTo (CompDisplay     *d,
		CompAction      *action,
		CompActionState state,
		CompOption      *option,
		int             nOption)
{
    GOTOVP_DISPLAY(d);

    int i;

    for (i = GotovpDisplayOptionSwitchTo1; i <= GotovpDisplayOptionSwitchTo2; i++)
    {
       if (action == &gotovpGetDisplayOption(d, i)->value.action)
       {
	  Window xid = getIntOptionNamed (option, nOption, "root", 0);
	  gd->activeScreen = findScreenAtDisplay (d, xid);
	  gd->destination = i - GotovpDisplayOptionSwitchTo1 + 1;
           break;
       }
    }
    return gotovpTerm(d,action,state,option,nOption);
}


static Bool
gotovpInitDisplay (CompPlugin *p,
				   CompDisplay *d)
{
	GotovpDisplay * gd;
	gd = malloc (sizeof (GotovpDisplay));

	if (!gd)
		return FALSE;

	gd->activeScreen = 0;

	WRAP (gd, d, handleEvent, gotovpHandleEvent);

	d->privates[displayPrivateIndex].ptr = gd;

	gotovpSetBeginInitiate (d, gotovpBegin);
	gotovpSetBeginTerminate (d, gotovpTerm);
	gotovpSetSwitchTo1Initiate (d, gotovpSwitchTo);
	gotovpSetSwitchTo2Initiate (d, gotovpSwitchTo);
	gotovpSetSwitchTo3Initiate (d, gotovpSwitchTo);
	gotovpSetSwitchTo4Initiate (d, gotovpSwitchTo);
	gotovpSetSwitchTo5Initiate (d, gotovpSwitchTo);
	gotovpSetSwitchTo6Initiate (d, gotovpSwitchTo);
	gotovpSetSwitchTo7Initiate (d, gotovpSwitchTo);
	gotovpSetSwitchTo8Initiate (d, gotovpSwitchTo);
	gotovpSetSwitchTo9Initiate (d, gotovpSwitchTo);
	gotovpSetSwitchTo10Initiate (d, gotovpSwitchTo);
	gotovpSetSwitchTo11Initiate (d, gotovpSwitchTo);
	gotovpSetSwitchTo12Initiate (d, gotovpSwitchTo);
	
	return TRUE;
}

static void
gotovpFiniDisplay(CompPlugin *p,
				  CompDisplay *d)
{
	GOTOVP_DISPLAY (d);

	UNWRAP (gd, d, handleEvent);

	free (gd);
}

static Bool
gotovpInit (CompPlugin *p)
{
	displayPrivateIndex = allocateDisplayPrivateIndex ();
	if (displayPrivateIndex < 0)
		return FALSE;

	return TRUE;
}

static void
gotovpFini (CompPlugin *p)
{
	freeDisplayPrivateIndex(displayPrivateIndex);
}

static int
gotovpGetVersion (CompPlugin *p,
				  int version)
{
	return ABIVERSION;
}


static CompPluginVTable gotovpVTable =
{
	"gotovp",
	gotovpGetVersion,
	0,
	gotovpInit,
	gotovpFini,
	gotovpInitDisplay,
	gotovpFiniDisplay,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0
};

CompPluginVTable * getCompPluginInfo (void)
{
	return &gotovpVTable;
}

