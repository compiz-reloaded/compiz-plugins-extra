/*
 * extrawm.c
 * Compiz extra WM actions plugins
 * Copyright: (C) 2007 Danny Baumann <maniac@beryl-project.org>
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
 *
 */

#include <stdio.h>
#include <stdlib.h>

#include <compiz-core.h>
#include "extrawm_options.h"

static Bool
activateWin (CompDisplay     *d,
	     CompAction      *action,
	     CompActionState state,
	     CompOption      *option,
	     int             nOption)
{
    CompWindow *w;
    Window     xid;

    xid = getIntOptionNamed (option, nOption, "window", 0);
    w = findWindowAtDisplay (d, xid);
    if (w)
  	sendWindowActivationRequest (w->screen, w->id);

    return TRUE;
}

static void 
fullscreenWindow (CompWindow *w,
		  int        state)
{
    unsigned int newState = w->state;
	
    if (w->attrib.override_redirect)
	return;

    /* It would be a bug, to put a shaded window to fullscreen. */
    if (w->shaded)
	return;

    state = constrainWindowState (state, w->actions);
    state &= CompWindowStateFullscreenMask;

    if (state == (w->state & CompWindowStateFullscreenMask))
	return;

    newState &= ~CompWindowStateFullscreenMask;
    newState |= state;

    changeWindowState (w, newState);
    recalcWindowType (w);
    recalcWindowActions (w);
    updateWindowAttributes (w, CompStackingUpdateModeNormal);
}

static Bool
toggleFullscreen (CompDisplay     *d,
		  CompAction      *action,
		  CompActionState state,
		  CompOption      *option,
		  int             nOption)
{
    CompWindow *w;
    Window     xid;

    xid = getIntOptionNamed (option, nOption, "window", 0);

    w = findTopLevelWindowAtDisplay (d, xid);
    if (w && (w->actions & CompWindowActionFullscreenMask))
	fullscreenWindow (w, w->state ^ CompWindowStateFullscreenMask);

    return TRUE;
}

static Bool
toggleRedirect (CompDisplay     *d,
		CompAction      *action,
		CompActionState state,
		CompOption      *option,
		int             nOption)
{
    CompWindow *w;
    Window     xid;

    xid = getIntOptionNamed (option, nOption, "window", 0);
    w = findTopLevelWindowAtDisplay (d, xid);
    if (w)
    {
	if (w->redirected)
	    unredirectWindow (w);
	else
	    redirectWindow (w);
    }

    return TRUE;
}

static Bool
toggleAlwaysOnTop (CompDisplay     *d,
		   CompAction      *action,
		   CompActionState state,
		   CompOption      *option,
		   int             nOption)
{
    CompWindow *w;
    Window     xid;

    xid = getIntOptionNamed (option, nOption, "window", 0);
    w = findTopLevelWindowAtDisplay (d, xid);
    if (w)
    {
	unsigned int newState;
	newState = w->state ^ CompWindowStateAboveMask;
	changeWindowState (w, newState);
	updateWindowAttributes (w, CompStackingUpdateModeNormal);
    }

    return TRUE;
}

static Bool
toggleSticky (CompDisplay     *d,
	      CompAction      *action,
	      CompActionState state,
	      CompOption      *option,
	      int             nOption)
{
    CompWindow *w;
    Window     xid;

    xid = getIntOptionNamed (option, nOption, "window", 0);
    w = findTopLevelWindowAtDisplay (d, xid);
    if (w && (w->actions & CompWindowActionStickMask))
    {
	unsigned int newState;
	newState = w->state ^ CompWindowStateStickyMask;
	changeWindowState (w, newState);
    }

    return TRUE;
}

static Bool
extraWMInit (CompPlugin *p)
{
    return TRUE;
}

static void
extraWMFini (CompPlugin *p)
{
}

static Bool
extraWMInitDisplay (CompPlugin  *p,
		    CompDisplay *d)
{
    if (!checkPluginABI ("core", CORE_ABIVERSION))
	return FALSE;

    extrawmSetToggleRedirectKeyInitiate (d, toggleRedirect);
    extrawmSetToggleAlwaysOnTopKeyInitiate (d, toggleAlwaysOnTop);
    extrawmSetToggleStickyKeyInitiate (d, toggleSticky);
    extrawmSetToggleFullscreenKeyInitiate (d, toggleFullscreen);
    extrawmSetActivateInitiate (d, activateWin);

    return TRUE;
}

static CompBool
extraWMInitObject (CompPlugin *p,
		   CompObject *o)
{
    static InitPluginObjectProc dispTab[] = {
	(InitPluginObjectProc) extraWMInitDisplay
    };

    RETURN_DISPATCH (o, dispTab, ARRAY_SIZE (dispTab), TRUE, (p, o));
}

CompPluginVTable extraWMVTable = {
    "extrawm",
    0,
    extraWMInit,
    extraWMFini,
    extraWMInitObject,
    0,
    0,
    0
};

CompPluginVTable*
getCompPluginInfo (void)
{
    return &extraWMVTable;
}
