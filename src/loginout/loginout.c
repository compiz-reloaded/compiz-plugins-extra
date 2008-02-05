/*
 * Compiz login/logout effect plugin
 *
 * loginout.c
 *
 * Copyright : (C) 2008 by Dennis Kasprzyk
 * E-mail    : onestone@opencompositing.org
 *
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

#include <compiz-core.h>

#include "loginout_options.h"

static int displayPrivateIndex = 0;

typedef struct _LoginoutDisplay
{
    MatchExpHandlerChangedProc matchExpHandlerChanged;
    MatchPropertyChangedProc   matchPropertyChanged;

    int screenPrivateIndex;

    Atom kdeLogoutInfoAtom;
}
LoginoutDisplay;

typedef struct _LoginoutScreen
{
    int windowPrivateIndex;

    PreparePaintScreenProc preparePaintScreen;
    DonePaintScreenProc    donePaintScreen;
    PaintWindowProc        paintWindow;
    DrawWindowProc         drawWindow;

    int numLoginWin;
    int numLogoutWin;

    float brightness;
    float saturation;
    float opacity;

    float in;
    float out;
}
LoginoutScreen;

typedef struct _LoginoutWindow {
    Bool login;
    Bool logout;
} LoginoutWindow;

#define GET_LOGINOUT_DISPLAY(d)                                  \
    ((LoginoutDisplay *) (d)->base.privates[displayPrivateIndex].ptr)

#define LOGINOUT_DISPLAY(d)                                      \
    LoginoutDisplay *ld = GET_LOGINOUT_DISPLAY (d)

#define GET_LOGINOUT_SCREEN(s, ld)                               \
    ((LoginoutScreen *) (s)->base.privates[(ld)->screenPrivateIndex].ptr)

#define LOGINOUT_SCREEN(s)                                       \
    LoginoutScreen *ls = GET_LOGINOUT_SCREEN (s, GET_LOGINOUT_DISPLAY (s->display))

#define GET_LOGINOUT_WINDOW(w, ls)					 \
    ((LoginoutWindow *) (w)->base.privates[(ls)->windowPrivateIndex].ptr)

#define LOGINOUT_WINDOW(w)					     \
    LoginoutWindow *lw = GET_LOGINOUT_WINDOW  (w,		     \
			 GET_LOGINOUT_SCREEN  (w->screen,	     \
			 GET_LOGINOUT_DISPLAY (w->screen->display)))

static void
loginoutUpdateWindowMatch (CompWindow *w)
{
    Bool curr;

    LOGINOUT_WINDOW (w);
    LOGINOUT_SCREEN (w->screen);

    curr = matchEval (loginoutGetInMatch (w->screen), w);
    if (curr != lw->login)
    {
	lw->login = curr;
	if (curr)
	    ls->numLoginWin++;
	else
	    ls->numLoginWin--;
	damageScreen (w->screen);
    }
    curr = matchEval (loginoutGetOutMatch (w->screen), w);
    if (curr != lw->logout)
    {
	lw->logout = curr;
	if (curr)
	    ls->numLogoutWin++;
	else
	    ls->numLogoutWin--;
	damageScreen (w->screen);
    }
}

static void
loginoutScreenOptionChanged (CompScreen          *s,
			     CompOption          *opt,
			     LoginoutScreenOptions num)
{
    CompWindow *w;

    switch (num)
    {
    case LoginoutScreenOptionInMatch:
    case LoginoutScreenOptionOutMatch:
	for (w = s->windows; w; w = w->next)
	    loginoutUpdateWindowMatch (w);

	damageScreen (s);
	break;

    default:
	damageScreen (s);
	break;
    }
}

static void
loginoutMatchExpHandlerChanged (CompDisplay *d)
{
    CompScreen *s;
    CompWindow *w;

    LOGINOUT_DISPLAY (d);

    UNWRAP (ld, d, matchExpHandlerChanged);
    (*d->matchExpHandlerChanged) (d);
    WRAP (ld, d, matchExpHandlerChanged, loginoutMatchExpHandlerChanged);

    /* match options are up to date after the call to matchExpHandlerChanged */
    for (s = d->screens; s; s = s->next)
    {
	for (w = s->windows; w; w = w->next)
	    loginoutUpdateWindowMatch (w);
    }
}

static void
loginoutMatchPropertyChanged (CompDisplay *d,
			    CompWindow  *w)
{
    LOGINOUT_DISPLAY (d);

    loginoutUpdateWindowMatch (w);

    UNWRAP (ld, d, matchPropertyChanged);
    (*d->matchPropertyChanged) (d, w);
    WRAP (ld, d, matchPropertyChanged, loginoutMatchPropertyChanged);
}

static Bool
loginoutPaintWindow (CompWindow              *w,
		     const WindowPaintAttrib *attrib,
		     const CompTransform     *transform,
		     Region                  region,
		     unsigned int            mask)
{
    CompScreen *s = w->screen;
    Bool status;

    LOGINOUT_WINDOW (w);
    LOGINOUT_SCREEN (s);

    if ((ls->in > 0.0 || ls->out > 0.0) && !lw->login && !lw->logout &&
	!(w->wmType & CompWindowTypeDesktopMask) && ls->opacity < 1.0)
	mask |= PAINT_WINDOW_TRANSLUCENT_MASK;

    UNWRAP (ls, s, paintWindow);
    status = (*s->paintWindow) (w, attrib, transform, region, mask);
    WRAP (ls, s, paintWindow, loginoutPaintWindow);

    return status;
}

static Bool
loginoutDrawWindow (CompWindow           *w,
		    const CompTransform  *transform,
		    const FragmentAttrib *fragment,
		    Region	         region,
		    unsigned int	 mask)
{
    Bool       status;
    CompScreen *s = w->screen;

    LOGINOUT_WINDOW (w);
    LOGINOUT_SCREEN (s);

    if ((ls->in > 0.0 || ls->out > 0.0) && !lw->login && !lw->logout)
    {
	FragmentAttrib fA = *fragment;

	if (!(w->wmType & CompWindowTypeDesktopMask))
	    fA.opacity = fragment->opacity * ls->opacity;
	
	fA.brightness = fragment->brightness * ls->brightness;
	fA.saturation = fragment->saturation * ls->saturation;
	
	UNWRAP (ls, s, drawWindow);
	status = (*s->drawWindow) (w, transform, &fA, region, mask);
	WRAP (ls, s, drawWindow, loginoutDrawWindow);
    }
    else
    {
	UNWRAP (ls, s, drawWindow);
	status = (*s->drawWindow) (w, transform, fragment, region, mask);
	WRAP (ls, s, drawWindow, loginoutDrawWindow);
    }
    return status;
}

static void
loginoutPreparePaintScreen (CompScreen *s,
			    int        ms)
{
    LOGINOUT_SCREEN (s);

    float val, val2;

    val = ((float)ms / 1000.0) / loginoutGetInTime (s);

    if (ls->numLoginWin)
	ls->in = MIN (1.0, ls->in + val);
    else 
	ls->in = MAX (0.0, ls->in - val);

    val = ((float)ms / 1000.0) / loginoutGetOutTime (s);

    if (ls->numLogoutWin)
	ls->out = MIN (1.0, ls->out + val);
    else 
	ls->out = MAX (0.0, ls->out - val);

    if (ls->in > 0.0 || ls->out > 0.0)
    {
	val  = (ls->in * loginoutGetInOpacity (s) / 100.0) + (1.0 - ls->in);
	val2 = (ls->out * loginoutGetOutOpacity (s) / 100.0) + (1.0 - ls->out);
	ls->opacity = MIN (val, val2);

	val  = (ls->in * loginoutGetInSaturation (s) / 100.0) + (1.0 - ls->in);
	val2 = (ls->out * loginoutGetOutSaturation (s) / 100.0) +
	       (1.0 - ls->out);
	ls->saturation = MIN (val, val2);

	val  = (ls->in * loginoutGetInBrightness (s) / 100.0) +
	       (1.0 - ls->in);
	val2 = (ls->out * loginoutGetOutBrightness (s) / 100.0) +
	       (1.0 - ls->out);
	ls->brightness = MIN (val, val2);
    }

    UNWRAP (ls, s, preparePaintScreen);
    (*s->preparePaintScreen) (s, ms);
    WRAP (ls, s, preparePaintScreen, loginoutPreparePaintScreen);
}

static void
loginoutDonePaintScreen (CompScreen * s)
{
    LOGINOUT_SCREEN (s);

    if ((ls->in > 0.0 && ls->in < 1.0) || (ls->out > 0.0 && ls->out < 1.0))
	damageScreen (s);

    UNWRAP (ls, s, donePaintScreen);
    (*s->donePaintScreen) (s);
    WRAP (ls, s, donePaintScreen, loginoutDonePaintScreen);
}

static Bool
loginoutInitDisplay (CompPlugin  *p,
		     CompDisplay *d)
{
    LoginoutDisplay *ld;

    if (!checkPluginABI ("core", CORE_ABIVERSION))
	return FALSE;

    ld = malloc (sizeof (LoginoutDisplay) );

    if (!ld)
	return FALSE;

    ld->screenPrivateIndex = allocateScreenPrivateIndex (d);

    if (ld->screenPrivateIndex < 0)
    {
	free (ld);
	return FALSE;
    }

    d->base.privates[displayPrivateIndex].ptr = ld;

    ld->kdeLogoutInfoAtom = XInternAtom (d->display,
					 "_KWIN_LOGOUT_EFFECT", 0);

    WRAP (ld, d, matchExpHandlerChanged, loginoutMatchExpHandlerChanged);
    WRAP (ld, d, matchPropertyChanged, loginoutMatchPropertyChanged);

    return TRUE;
}

static void
loginoutFiniDisplay (CompPlugin  *p,
		     CompDisplay *d)
{
    LOGINOUT_DISPLAY (d);
    freeScreenPrivateIndex (d, ld->screenPrivateIndex);

    UNWRAP (ld, d, matchExpHandlerChanged);
    UNWRAP (ld, d, matchPropertyChanged);

    free (ld);
}


static Bool
loginoutInitScreen (CompPlugin *p,
		  CompScreen *s)
{
    LoginoutScreen *ls;

    LOGINOUT_DISPLAY (s->display);

    ls = malloc (sizeof (LoginoutScreen) );

    if (!ls)
	return FALSE;

    ls->windowPrivateIndex = allocateWindowPrivateIndex (s);

    if (ls->windowPrivateIndex < 0)
    {
	free (ls);
	return FALSE;
    }

    loginoutSetInMatchNotify (s, loginoutScreenOptionChanged);
    loginoutSetOutMatchNotify (s, loginoutScreenOptionChanged);

    s->base.privates[ld->screenPrivateIndex].ptr = ls;

    ls->numLoginWin  = 0;
    ls->numLogoutWin = 0;

    ls->saturation = 1.0;
    ls->brightness = 1.0;
    ls->opacity    = 1.0;

    ls->in  = 0.0;
    ls->out = 0.0;

    WRAP (ls, s, preparePaintScreen, loginoutPreparePaintScreen);
    WRAP (ls, s, donePaintScreen, loginoutDonePaintScreen);
    WRAP (ls, s, paintWindow, loginoutPaintWindow);
    WRAP (ls, s, drawWindow, loginoutDrawWindow);

    /* This is a temporary solution until an official spec will be released */
    XChangeProperty (s->display->display, s->wmSnSelectionWindow,
		     ld->kdeLogoutInfoAtom, ld->kdeLogoutInfoAtom, 8,
		     PropModeReplace,
		     (unsigned char*)&ld->kdeLogoutInfoAtom, 1);


    return TRUE;
}


static void
loginoutFiniScreen (CompPlugin *p,
		    CompScreen *s)
{
    LOGINOUT_SCREEN (s);
    LOGINOUT_DISPLAY (s->display);

    freeWindowPrivateIndex (s, ls->windowPrivateIndex);

    UNWRAP (ls, s, preparePaintScreen);
    UNWRAP (ls, s, donePaintScreen);
    UNWRAP (ls, s, paintWindow);
    UNWRAP (ls, s, drawWindow);

    XDeleteProperty (s->display->display, s->wmSnSelectionWindow,
		     ld->kdeLogoutInfoAtom);

    free (ls);
}

static Bool
loginoutInitWindow (CompPlugin *p,
		    CompWindow *w)
{
    LoginoutWindow *lw;

    LOGINOUT_SCREEN (w->screen);

    lw = malloc (sizeof (LoginoutWindow));
    if (!lw)
	return FALSE;

    lw->login  = FALSE;
    lw->logout = FALSE;

    w->base.privates[ls->windowPrivateIndex].ptr = lw;

    loginoutUpdateWindowMatch (w);

    return TRUE;
}

static void
loginoutFiniWindow (CompPlugin *p,
		    CompWindow *w)
{
    LOGINOUT_WINDOW (w);
    LOGINOUT_SCREEN (w->screen);

    if (lw->login)
    {
	ls->numLoginWin--;
	damageScreen (w->screen);
    }
    if (lw->logout)
    {
	ls->numLogoutWin--;
	damageScreen (w->screen);
    }

    free (lw);
}

static Bool
loginoutInit (CompPlugin *p)
{
    displayPrivateIndex = allocateDisplayPrivateIndex ();

    if (displayPrivateIndex < 0)
	return FALSE;

    return TRUE;
}

static void
loginoutFini (CompPlugin *p)
{
    if (displayPrivateIndex >= 0)
	freeDisplayPrivateIndex (displayPrivateIndex);
}

static CompBool
loginoutInitObject (CompPlugin *p,
		    CompObject *o)
{
    static InitPluginObjectProc dispTab[] = {
	(InitPluginObjectProc) 0, /* InitCore */
	(InitPluginObjectProc) loginoutInitDisplay,
	(InitPluginObjectProc) loginoutInitScreen,
	(InitPluginObjectProc) loginoutInitWindow
    };

    RETURN_DISPATCH (o, dispTab, ARRAY_SIZE (dispTab), TRUE, (p, o));
}

static void
loginoutFiniObject (CompPlugin *p,
		    CompObject *o)
{
    static FiniPluginObjectProc dispTab[] = {
	(FiniPluginObjectProc) 0, /* FiniCore */
	(FiniPluginObjectProc) loginoutFiniDisplay,
	(FiniPluginObjectProc) loginoutFiniScreen,
	(FiniPluginObjectProc) loginoutFiniWindow
    };

    DISPATCH (o, dispTab, ARRAY_SIZE (dispTab), (p, o));
}

CompPluginVTable loginoutVTable = {
    "loginout",
    0,
    loginoutInit,
    loginoutFini,
    loginoutInitObject,
    loginoutFiniObject,
    0,
    0
};

CompPluginVTable *
getCompPluginInfo (void)
{
    return &loginoutVTable;
}
