/*
 *
 * Compiz scale window title filter plugin
 *
 * scalefilter.c
 *
 * Copyright : (C) 2007 by Danny Baumann
 * E-mail    : dannybaumann@web.de
 *
 * Copyright : (C) 2006 Diogo Ferreira
 * E-mail    : diogo@underdev.org
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
 *
 */

#define _GNU_SOURCE
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <locale.h>

#include <X11/Xlib.h>
#include <X11/keysymdef.h>
#include <X11/XKBlib.h>

#include <compiz-core.h>
#include <compiz-scale.h>
#include <compiz-text.h>

#include "scalefilter_options.h"

static int displayPrivateIndex;
static int scaleDisplayPrivateIndex;

#define MAX_FILTER_SIZE 32
#define MAX_FILTER_STRING_LEN (MAX_FILTER_SIZE + 1)
#define MAX_FILTER_TEXT_LEN (MAX_FILTER_SIZE + 8)

typedef struct _ScaleFilterInfo {
    CompTimeoutHandle timeoutHandle;

    CompTextData *textData;

    unsigned int outputDevice;

    CompMatch match;
    CompMatch *origMatch;

    wchar_t filterString[2 * MAX_FILTER_STRING_LEN];
    int     filterStringLength;
} ScaleFilterInfo;

typedef struct _ScaleFilterDisplay {
    int screenPrivateIndex;

    XIM xim;
    XIC xic;

    TextFunc *textFunc;

    HandleEventProc       handleEvent;
    HandleCompizEventProc handleCompizEvent;
} ScaleFilterDisplay;

typedef struct _ScaleFilterScreen {
    PaintOutputProc                   paintOutput;
    ScaleSetScaledPaintAttributesProc setScaledPaintAttributes;

    CompMatch scaleMatch;
    Bool      matchApplied;

    ScaleFilterInfo *filterInfo;
} ScaleFilterScreen;

#define GET_FILTER_DISPLAY(d)				          \
    ((ScaleFilterDisplay *) (d)->base.privates[displayPrivateIndex].ptr)

#define FILTER_DISPLAY(d)		          \
    ScaleFilterDisplay *fd = GET_FILTER_DISPLAY (d)

#define GET_FILTER_SCREEN(s, fd)				              \
    ((ScaleFilterScreen *) (s)->base.privates[(fd)->screenPrivateIndex].ptr)

#define FILTER_SCREEN(s)		                    \
    ScaleFilterScreen *fs = GET_FILTER_SCREEN (s,           \
			    GET_FILTER_DISPLAY (s->display))

static void
scalefilterFreeFilterText (CompScreen *s)
{
    FILTER_SCREEN (s);
    FILTER_DISPLAY (s->display);

    if (!fs->filterInfo)
	return;

    if (!fs->filterInfo->textData)
	return;

    (fd->textFunc->finiTextData) (s, fs->filterInfo->textData);
    fs->filterInfo->textData = NULL;
}

static void
scalefilterRenderFilterText (CompScreen *s)
{
    CompTextAttrib attrib;
    int            x1, x2, y1, y2;
    int            width, height;
    REGION         reg;
    char           buffer[2 * MAX_FILTER_STRING_LEN];

    FILTER_SCREEN (s);
    FILTER_DISPLAY (s->display);

    if (!fs->filterInfo)
	return;

    x1 = s->outputDev[fs->filterInfo->outputDevice].region.extents.x1;
    x2 = s->outputDev[fs->filterInfo->outputDevice].region.extents.x2;
    y1 = s->outputDev[fs->filterInfo->outputDevice].region.extents.y1;
    y2 = s->outputDev[fs->filterInfo->outputDevice].region.extents.y2;

    reg.rects    = &reg.extents;
    reg.numRects = 1;

    /* damage the old draw rectangle */
    if (fs->filterInfo->textData)
    {
	width  = fs->filterInfo->textData->width +
	         (2 * scalefilterGetBorderSize (s));
	height = fs->filterInfo->textData->height +
	         (2 * scalefilterGetBorderSize (s));

	reg.extents.x1 = x1 + ((x2 - x1) / 2) - (width / 2) - 1;
	reg.extents.x2 = reg.extents.x1 + width + 1;
	reg.extents.y1 = y1 + ((y2 - y1) / 2) - (height / 2) - 1;
	reg.extents.y2 = reg.extents.y1 + height + 1;

	damageScreenRegion (s, &reg);
    }

    scalefilterFreeFilterText (s);

    if (!scalefilterGetFilterDisplay (s))
	return;

    if (fs->filterInfo->filterStringLength == 0)
	return;

    if (!fd->textFunc)
	return;

    attrib.maxWidth = x2 - x1;
    attrib.maxHeight = y2 - y1;

    attrib.family = scalefilterGetFontFamily(s);
    attrib.size = scalefilterGetFontSize (s);
    attrib.color[0] = scalefilterGetFontColorRed (s);
    attrib.color[1] = scalefilterGetFontColorGreen (s);
    attrib.color[2] = scalefilterGetFontColorBlue (s);
    attrib.color[3] = scalefilterGetFontColorAlpha (s);

    attrib.flags = CompTextFlagWithBackground | CompTextFlagEllipsized;
    if (scalefilterGetFontBold (s))
	attrib.flags |= CompTextFlagStyleBold;

    attrib.bgHMargin = scalefilterGetBorderSize (s);
    attrib.bgVMargin = scalefilterGetBorderSize (s);
    attrib.bgColor[0] = scalefilterGetBackColorRed (s);
    attrib.bgColor[1] = scalefilterGetBackColorGreen (s);
    attrib.bgColor[2] = scalefilterGetBackColorBlue (s);
    attrib.bgColor[3] = scalefilterGetBackColorAlpha (s);

    wcstombs (buffer, fs->filterInfo->filterString, MAX_FILTER_STRING_LEN);

    fs->filterInfo->textData = (fd->textFunc->renderText) (s, buffer, &attrib);

    /* damage the new draw rectangle */
    if (fs->filterInfo->textData)
    {
	width  = fs->filterInfo->textData->width;
	height = fs->filterInfo->textData->height;

	reg.extents.x1 = x1 + ((x2 - x1) / 2) - (width / 2) - 1;
	reg.extents.x2 = reg.extents.x1 + width + 1;
	reg.extents.y1 = y1 + ((y2 - y1) / 2) - (height / 2) - 1;
	reg.extents.y2 = reg.extents.y1 + height + 1;

	damageScreenRegion (s, &reg);
    }
}

static void
scalefilterDrawFilterText (CompScreen *s,
			   CompOutput *output)
{
    int        ox1, ox2, oy1, oy2;
    float      x, y, width, height;

    FILTER_DISPLAY (s->display);
    FILTER_SCREEN (s);

    width = fs->filterInfo->textData->width;
    height = fs->filterInfo->textData->height;

    ox1 = output->region.extents.x1;
    ox2 = output->region.extents.x2;
    oy1 = output->region.extents.y1;
    oy2 = output->region.extents.y2;

    x = floor (ox1 + ((ox2 - ox1) / 2) - (width / 2));
    y = floor (oy1 + ((oy2 - oy1) / 2) + (height / 2));

    (fd->textFunc->drawText) (s, fs->filterInfo->textData, x, y, 1.0f);
}

static void
scalefilterUpdateFilter (CompScreen *s,
	   		 CompMatch  *match)
{
    char         filterMatch[2 * MAX_FILTER_TEXT_LEN];
    unsigned int offset;
    char         *filterType;

    FILTER_SCREEN (s);

    matchFini (match);
    matchInit (match);

    filterType = (scalefilterGetFilterCaseInsensitive (s)) ? "ititle=" :
	                                                     "title=";

    strncpy (filterMatch, filterType, MAX_FILTER_TEXT_LEN);
    offset = strlen (filterType);

    wcstombs (filterMatch + offset, fs->filterInfo->filterString,
	      MAX_FILTER_STRING_LEN);
    matchAddExp (match, 0, filterMatch);
    matchAddGroup (match, MATCH_OP_AND_MASK, &fs->scaleMatch);
    matchUpdate (s->display, match);
}

static void
scalefilterRelayout (CompScreen *s)
{
    CompOption o[1];
    CompAction *action;

    SCALE_DISPLAY (s->display);

    action = &sd->opt[SCALE_DISPLAY_OPTION_RELAYOUT].value.action;

    o[0].type    = CompOptionTypeInt;
    o[0].name    = "root";
    o[0].value.i = s->root;

    if (action->initiate)
    {
	if ((*action->initiate) (s->display, NULL, 0, o, 1))
	    damageScreen (s);
    }
}

static void
scalefilterInitFilterInfo (CompScreen *s)
{
    ScaleFilterInfo *info;

    FILTER_SCREEN (s);
    SCALE_SCREEN (s);

    if (!fs->filterInfo)
	return;

    info = fs->filterInfo;
    memset (info->filterString, 0, sizeof (info->filterString));
    info->filterStringLength = 0;

    info->textData = NULL;

    info->timeoutHandle = 0;

    info->outputDevice = s->currentOutputDev;

    matchInit (&info->match);
    matchCopy (&info->match, &fs->scaleMatch);

    info->origMatch  = ss->currentMatch;
    ss->currentMatch = &info->match;
}

static void
scalefilterFiniFilterInfo (CompScreen *s,
			   Bool       freeTimeout)
{
    FILTER_SCREEN (s);

    scalefilterFreeFilterText (s);

    matchFini (&fs->filterInfo->match);

    if (freeTimeout && fs->filterInfo->timeoutHandle)
	compRemoveTimeout (fs->filterInfo->timeoutHandle);

    free (fs->filterInfo);
    fs->filterInfo = NULL;
}

static Bool
scalefilterFilterTimeout (void *closure)
{
    CompScreen *s = (CompScreen *) closure;

    FILTER_SCREEN (s);
    SCALE_SCREEN (s);

    if (fs->filterInfo)
    {
	ss->currentMatch = fs->filterInfo->origMatch;
	scalefilterFiniFilterInfo (s, FALSE);
	scalefilterRelayout (s);
    }

    return FALSE;
}

static Bool
scalefilterRemoveFilter (CompScreen *s)
{
    Bool retval = FALSE;

    FILTER_SCREEN (s);
    SCALE_SCREEN (s);

    if (fs->filterInfo)
    {
	/* in input mode: drop current filter */
	ss->currentMatch = fs->filterInfo->origMatch;
	scalefilterFiniFilterInfo (s, TRUE);
	retval = TRUE;
    }
    else if (fs->matchApplied)
    {
	/* remove filter applied previously
	   if currently not in input mode */
	matchFini (&ss->match);
	matchInit (&ss->match);
	matchCopy (&ss->match, &fs->scaleMatch);
	matchUpdate (s->display, &ss->match);

	ss->currentMatch = &ss->match;
	fs->matchApplied = FALSE;
	retval = TRUE;
    }

    return retval;
}

static void
scalefilterHandleWindowRemove (CompDisplay *d,
			       Window      id)
{
    CompWindow *w;

    w = findWindowAtDisplay (d, id);
    if (w)
    {
	SCALE_SCREEN (w->screen);

	if (ss->state != SCALE_STATE_NONE && ss->state != SCALE_STATE_IN)
	{
	    if (ss->nWindows == 1 && ss->windows[0] == w)
	    {
		scalefilterRemoveFilter (w->screen);
	    }
	}
    }
}

static void
scalefilterDoRelayout (CompScreen *s)
{
    FILTER_SCREEN (s);

    scalefilterRenderFilterText (s);

    if (fs->filterInfo)
	scalefilterUpdateFilter (s, &fs->filterInfo->match);

    scalefilterRelayout (s);
}

static Bool
scalefilterHandleSpecialKeyPress (CompScreen *s,
				  XKeyEvent  *event,
				  Bool       *drop)
{
    ScaleFilterInfo *info;
    KeySym          ks;
    Bool            retval = FALSE;
    Bool            needRelayout = FALSE;

    FILTER_SCREEN (s);

    info = fs->filterInfo;
    ks   = XkbKeycodeToKeysym (s->display->display, event->keycode, 0, 0);

    if (ks == XK_Escape)
    {
	/* Escape key - drop current filter or remove filter applied
	   previously if currently not in input mode */
	if (scalefilterRemoveFilter (s))
	{
	    needRelayout = TRUE;
	    *drop        = TRUE;
	}
	retval = TRUE;
    }
    else if (ks == XK_Return)
    {
	if (info && info->filterStringLength > 0)
	{
	    SCALE_SCREEN (s);

	    /* Return key - apply current filter persistently */

	    matchFini (&ss->match);
	    matchInit (&ss->match);
	    matchCopy (&ss->match, &info->match);
	    matchUpdate (s->display, &ss->match);

	    ss->currentMatch = &ss->match;
	    fs->matchApplied = TRUE;
	    /* let return pass (and thus end scale) if only
	       one window is left */
	    *drop            = ss->nWindows > 1;
	    needRelayout     = TRUE;
	    scalefilterFiniFilterInfo (s, TRUE);
	}
	retval = TRUE;
    }
    else if (ks == XK_BackSpace)
    {
	if (info && info->filterStringLength > 0)
	{
	    /* remove last character in string */
	    info->filterString[--(info->filterStringLength)] = '\0';
	    needRelayout = TRUE;
	}
	retval = TRUE;
    }

    if (needRelayout)
	scalefilterDoRelayout (s);

    return retval;
}

static void
scalefilterHandleTextKeyPress (CompScreen *s,
			       XKeyEvent  *event)
{
    CompDisplay     *d = s->display;
    ScaleFilterInfo *info;
    Bool            needRelayout = FALSE;
    int             count, timeout;
    char            buffer[10];
    wchar_t         wbuffer[10];
    KeySym          ks;
    unsigned int    mods;

    FILTER_DISPLAY (d);
    FILTER_SCREEN (s);

    /* ignore key presses with modifiers (except Shift and
       ModeSwitch AKA AltGr) */
    mods =  event->state & ~d->ignoredModMask & ~d->modMask[CompModModeSwitch];
    if (mods & ~ShiftMask)
	return;

    info = fs->filterInfo;
    memset (buffer, 0, sizeof (buffer));
    memset (wbuffer, 0, sizeof (wbuffer));

    if (fd->xic)
    {
	Status status;

	XSetICFocus (fd->xic);
	count = Xutf8LookupString (fd->xic, event, buffer, 9, &ks, &status);
	XUnsetICFocus (fd->xic);
    }
    else
    {
	count = XLookupString (event, buffer, 9, &ks, NULL);
    }

    mbstowcs (wbuffer, buffer, 9);

    if (count > 0)
    {
	if (!info)
	{
	    fs->filterInfo = info = malloc (sizeof (ScaleFilterInfo));
	    scalefilterInitFilterInfo (s);
	}
	else if (info->timeoutHandle)
	{
	    compRemoveTimeout (info->timeoutHandle);
	    info->timeoutHandle = 0;
	}

	if (info)
	{
	    timeout = scalefilterGetTimeout (s);
	    if (timeout > 0)
		info->timeoutHandle = compAddTimeout (timeout,
						      (float) timeout * 1.2,
						      scalefilterFilterTimeout,
						      s);

	    if (info->filterStringLength < MAX_FILTER_SIZE)
	    {
		info->filterString[info->filterStringLength++] = wbuffer[0];
		info->filterString[info->filterStringLength] = '\0';
		needRelayout = TRUE;
	    }
	}
    }

    if (needRelayout)
	scalefilterDoRelayout (s);
}

static void
scalefilterHandleEvent (CompDisplay *d,
	 		XEvent      *event)
{
    CompScreen *s;
    int        grabIndex;
    Bool       dropEvent = FALSE;

    FILTER_DISPLAY (d);

    switch (event->type) {
    case KeyPress:
	s = findScreenAtDisplay (d, event->xkey.root);
	if (s)
	{
	    SCALE_SCREEN (s);
	    grabIndex = ss->grabIndex;
	    if (grabIndex)
		if (scalefilterHandleSpecialKeyPress (s, &event->xkey,
						      &dropEvent))
		{
		    /* don't attempt to process text input later on
		       if the input was a special key */
		    grabIndex = 0;
		}
	}
	break;
    case UnmapNotify:
	scalefilterHandleWindowRemove (d, event->xunmap.window);
	break;
    case DestroyNotify:
	scalefilterHandleWindowRemove (d, event->xdestroywindow.window);
	break;
    default:
	break;
    }

    if (!dropEvent)
    {
	UNWRAP (fd, d, handleEvent);
	(*d->handleEvent) (d, event);
	WRAP (fd, d, handleEvent, scalefilterHandleEvent);
    }

    switch (event->type) {
    case KeyPress:
	if (s && grabIndex && !dropEvent)
	{
	    SCALE_SCREEN (s);

	    if (ss->grabIndex == grabIndex)
		scalefilterHandleTextKeyPress (s, &event->xkey);
	}
	break;
    }
}

static void
scalefilterHandleCompizEvent (CompDisplay *d,
	 		      const char  *pluginName,
	 		      const char  *eventName,
	 		      CompOption  *option,
	 		      int         nOption)
{
    FILTER_DISPLAY (d);

    UNWRAP (fd, d, handleCompizEvent);
    (*d->handleCompizEvent) (d, pluginName, eventName, option, nOption);
    WRAP (fd, d, handleCompizEvent, scalefilterHandleCompizEvent);

    if ((strcmp (pluginName, "scale") == 0) &&
	(strcmp (eventName, "activate") == 0))
    {
	Window     xid;
	CompScreen *s;

	xid = getIntOptionNamed (option, nOption, "root", 0);
	s   = findScreenAtDisplay (d, xid);
	if (s)
	{
	    Bool activated;
	    FILTER_SCREEN (s);
	    SCALE_SCREEN (s);

	    activated = getBoolOptionNamed (option, nOption, "active", FALSE);
	    if (activated)
	    {
		matchFini (&fs->scaleMatch);
		matchInit (&fs->scaleMatch);
		matchCopy (&fs->scaleMatch, ss->currentMatch);
		matchUpdate (d, &fs->scaleMatch);
	    }
	    else if (fs->filterInfo)
	    {
    		ss->currentMatch = fs->filterInfo->origMatch;
		scalefilterFiniFilterInfo (s, TRUE);
	    }

	    fs->matchApplied = FALSE;
	}
    }
}

static Bool
scalefilterPaintOutput (CompScreen              *s,
			const ScreenPaintAttrib *sAttrib,
			const CompTransform     *transform,
			Region                  region,
			CompOutput              *output,
			unsigned int            mask)
{
    Bool status;

    FILTER_SCREEN (s);

    UNWRAP (fs, s, paintOutput);
    status = (*s->paintOutput) (s, sAttrib, transform, region, output, mask);
    WRAP (fs, s, paintOutput, scalefilterPaintOutput);

    if (status && fs->filterInfo && fs->filterInfo->textData)
    {
	if (output->id == ~0 || output->id == fs->filterInfo->outputDevice)
	{
	    CompTransform sTransform = *transform;
	    transformToScreenSpace (s, output, -DEFAULT_Z_CAMERA, &sTransform);

	    glPushMatrix ();
	    glLoadMatrixf (sTransform.m);

	    scalefilterDrawFilterText (s, output);

	    glPopMatrix ();
	}
    }

    return status;
}

static Bool
scalefilterSetScaledPaintAttributes (CompWindow        *w,
				     WindowPaintAttrib *attrib)
{
    Bool ret;

    FILTER_SCREEN (w->screen);
    SCALE_SCREEN (w->screen);

    UNWRAP (fs, ss, setScaledPaintAttributes);
    ret = (*ss->setScaledPaintAttributes) (w, attrib);
    WRAP (fs, ss, setScaledPaintAttributes,
	  scalefilterSetScaledPaintAttributes);

    if (fs->matchApplied ||
	(fs->filterInfo && fs->filterInfo->filterStringLength))
    {
	SCALE_WINDOW (w);

	if (ret && !sw->slot && ss->state != SCALE_STATE_IN)
	{
	    ret = FALSE;
    	    attrib->opacity = 0;
	}
    }

    return ret;
}

static void
scalefilterScreenOptionChanged (CompScreen               *s,
				CompOption               *opt,
	 			ScalefilterScreenOptions num)
{
    switch (num)
    {
	case ScalefilterScreenOptionFontFamily:
	case ScalefilterScreenOptionFontBold:
	case ScalefilterScreenOptionFontSize:
	case ScalefilterScreenOptionFontColor:
	case ScalefilterScreenOptionBackColor:
	    {
		FILTER_SCREEN (s);

		if (fs->filterInfo)
		    scalefilterRenderFilterText (s);
	    }
	    break;
	default:
	    break;
    }
}

static Bool
scalefilterInitDisplay (CompPlugin  *p,
			CompDisplay *d)
{
    ScaleFilterDisplay *fd;
    int                index;

    if (!checkPluginABI ("core", CORE_ABIVERSION))
	return FALSE;

    if (!checkPluginABI ("scale", SCALE_ABIVERSION))
	return FALSE;

    if (!getPluginDisplayIndex (d, "scale", &scaleDisplayPrivateIndex))
	return FALSE;

    fd = malloc (sizeof (ScaleFilterDisplay));
    if (!fd)
	return FALSE;

    fd->screenPrivateIndex = allocateScreenPrivateIndex (d);
    if (fd->screenPrivateIndex < 0)
    {
	free (fd);
	return FALSE;
    }

    fd->xim = XOpenIM (d->display, NULL, NULL, NULL);
    if (fd->xim)
	fd->xic = XCreateIC (fd->xim,
			     XNClientWindow, d->screens->root,
			     XNInputStyle,
			     XIMPreeditNothing  | XIMStatusNothing,
			     NULL);
    else
	fd->xic = NULL;

    if (fd->xic)
	setlocale (LC_CTYPE, "");

    if (checkPluginABI ("text", TEXT_ABIVERSION) &&
	getPluginDisplayIndex (d, "text", &index))
    {
	fd->textFunc = d->base.privates[index].ptr;
    }
    else
    {
	compLogMessage ("scalefilter", CompLogLevelWarn,
			"No compatible text plugin found.");
	fd->textFunc = NULL;
    }

    WRAP (fd, d, handleEvent, scalefilterHandleEvent);
    WRAP (fd, d, handleCompizEvent, scalefilterHandleCompizEvent);

    d->base.privates[displayPrivateIndex].ptr = fd;

    return TRUE;
}

static void
scalefilterFiniDisplay (CompPlugin  *p,
	    		CompDisplay *d)
{
    FILTER_DISPLAY (d);

    UNWRAP (fd, d, handleEvent);
    UNWRAP (fd, d, handleCompizEvent);

    if (fd->xic)
	XDestroyIC (fd->xic);
    if (fd->xim)
	XCloseIM (fd->xim);

    freeScreenPrivateIndex (d, fd->screenPrivateIndex);

    free (fd);
}

static Bool
scalefilterInitScreen (CompPlugin *p,
		       CompScreen *s)
{
    ScaleFilterScreen *fs;

    FILTER_DISPLAY (s->display);
    SCALE_SCREEN (s);

    fs = malloc (sizeof (ScaleFilterScreen));
    if (!fs)
	return FALSE;

    fs->filterInfo = NULL;
    matchInit (&fs->scaleMatch);
    fs->matchApplied = FALSE;

    WRAP (fs, s, paintOutput, scalefilterPaintOutput);
    WRAP (fs, ss, setScaledPaintAttributes,
	  scalefilterSetScaledPaintAttributes);

    scalefilterSetFontFamilyNotify (s, scalefilterScreenOptionChanged);
    scalefilterSetFontBoldNotify (s, scalefilterScreenOptionChanged);
    scalefilterSetFontSizeNotify (s, scalefilterScreenOptionChanged);
    scalefilterSetFontColorNotify (s, scalefilterScreenOptionChanged);
    scalefilterSetBackColorNotify (s, scalefilterScreenOptionChanged);

    s->base.privates[fd->screenPrivateIndex].ptr = fs;

    return TRUE;
}

static void
scalefilterFiniScreen (CompPlugin *p,
		       CompScreen *s)
{
    FILTER_SCREEN (s);
    SCALE_SCREEN (s);

    UNWRAP (fs, s, paintOutput);
    UNWRAP (fs, ss, setScaledPaintAttributes);

    matchFini (&fs->scaleMatch);

    if (fs->filterInfo)
    {
	ss->currentMatch = fs->filterInfo->origMatch;
	scalefilterFiniFilterInfo (s, TRUE);
    }

    free (fs);
}

static CompBool
scalefilterInitObject (CompPlugin *p,
		       CompObject *o)
{
    static InitPluginObjectProc dispTab[] = {
	(InitPluginObjectProc) 0, /* InitCore */
	(InitPluginObjectProc) scalefilterInitDisplay,
	(InitPluginObjectProc) scalefilterInitScreen
    };

    RETURN_DISPATCH (o, dispTab, ARRAY_SIZE (dispTab), TRUE, (p, o));
}

static void
scalefilterFiniObject (CompPlugin *p,
		       CompObject *o)
{
    static FiniPluginObjectProc dispTab[] = {
	(FiniPluginObjectProc) 0, /* FiniCore */
	(FiniPluginObjectProc) scalefilterFiniDisplay,
	(FiniPluginObjectProc) scalefilterFiniScreen
    };

    DISPATCH (o, dispTab, ARRAY_SIZE (dispTab), (p, o));
}

static Bool
scalefilterInit (CompPlugin *p)
{
    displayPrivateIndex = allocateDisplayPrivateIndex ();
    if (displayPrivateIndex < 0)
	return FALSE;

    return TRUE;
}

static void
scalefilterFini (CompPlugin *p)
{
    freeDisplayPrivateIndex (displayPrivateIndex);
}

CompPluginVTable scalefilterVTable = {
    "scalefilter",
    0,
    scalefilterInit,
    scalefilterFini,
    scalefilterInitObject,
    scalefilterFiniObject,
    0,
    0
};

CompPluginVTable *
getCompPluginInfo (void)
{
    return &scalefilterVTable;
}
