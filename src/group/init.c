/**
 *
 * Compiz group plugin
 *
 * init.c
 *
 * Copyright : (C) 2006-2007 by Patrick Niklaus, Roi Cohen, Danny Baumann
 * Authors: Patrick Niklaus <patrick.niklaus@googlemail.com>
 *          Roi Cohen       <roico.beryl@gmail.com>
 *          Danny Baumann   <maniac@opencompositing.org>
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
 **/

#include "group-internal.h"
#include "group_glow.h"

int groupDisplayPrivateIndex;

static const GlowTextureProperties glowTextureProperties[2] = {
	/* GlowTextureRectangular */
	{glowTexRect, 32, 21},
	/* GlowTextureRing */
	{glowTexRing, 32, 16}
};

static void
groupScreenOptionChanged (CompScreen         *s,
						  CompOption         *opt,
						  GroupScreenOptions num)
{
	GroupSelection *group;

	GROUP_SCREEN (s);

	switch (num)
	{
	case GroupScreenOptionTabBaseColor:
	case GroupScreenOptionTabHighlightColor:
	case GroupScreenOptionTabBorderColor:
	case GroupScreenOptionTabStyle:
	case GroupScreenOptionBorderRadius:
	case GroupScreenOptionBorderWidth:
		for (group = gs->groups; group; group = group->next)
			if (group->tabBar)
				groupRenderTabBarBackground (group);
		break;
	case GroupScreenOptionTabbarFontSize:
	case GroupScreenOptionTabbarFontColor:
		for (group = gs->groups; group; group = group->next)
			groupRenderWindowTitle (group);
		break;
	case GroupScreenOptionThumbSize:
	case GroupScreenOptionThumbSpace:
		for (group = gs->groups; group; group = group->next)
			if (group->tabBar)
				groupRecalcTabBarPos (group,
									  (group->tabBar->region->extents.x1 +
								  	   group->tabBar->region->extents.x2) / 2,
							 		  group->tabBar->region->extents.x1,
						 			  group->tabBar->region->extents.x2);
		break;
	case GroupScreenOptionGlow:
	case GroupScreenOptionGlowSize:
		{
			CompWindow *w;

			groupRecomputeGlow (s);
			for (w = s->windows; w; w = w->next)
			{
				GROUP_WINDOW (w);

				if (gw->glowQuads)
				{
					damageWindowOutputExtents (w);
					updateWindowOutputExtents (w);
					damageWindowOutputExtents (w);
				}
			}
			break;
		}
	case GroupScreenOptionGlowType:
		{
			GroupGlowTypeEnum glowType;

			GROUP_DISPLAY (s->display);
			glowType = groupGetGlowType (s);

			finiTexture (s, &gs->glowTexture);
			initTexture (s, &gs->glowTexture);

			imageDataToTexture (s, &gs->glowTexture,
								gd->glowTextureProperties[glowType].textureData,
								gd->glowTextureProperties[glowType].textureSize,
								gd->glowTextureProperties[glowType].textureSize,
								GL_RGBA, GL_UNSIGNED_BYTE);

			if (groupGetGlow (s) && gs->groups)
			{
				groupRecomputeGlow (s);
				damageScreen (s);
			}
			break;
		}

	default:
		break;
	}
}

/*
 * groupInitDisplay
 *
 */
static Bool
groupInitDisplay (CompPlugin  *p,
				  CompDisplay *d)
{
	GroupDisplay *gd;

	gd = malloc (sizeof (GroupDisplay));
	if (!gd)
		return FALSE;

	gd->screenPrivateIndex = allocateScreenPrivateIndex (d);
	if (gd->screenPrivateIndex < 0)
	{
		free (gd);
		return FALSE;
	}

	gd->glowTextureProperties = (GlowTextureProperties*) glowTextureProperties;
	gd->ignoreMode = FALSE;
	gd->groupWinPropertyAtom = XInternAtom (d->display, "_COMPIZ_GROUP", 0);

	WRAP (gd, d, handleEvent, groupHandleEvent);

	groupSetSelectInitiate (d, groupSelect);
	groupSetSelectTerminate (d, groupSelectTerminate);
	groupSetSelectSingleInitiate (d, groupSelectSingle);
	groupSetGroupInitiate (d, groupGroupWindows);
	groupSetUngroupInitiate (d, groupUnGroupWindows);
	groupSetTabmodeInitiate (d, groupInitTab);
	groupSetChangeTabLeftInitiate (d, groupChangeTabLeft);
	groupSetChangeTabRightInitiate (d, groupChangeTabRight);
	groupSetRemoveInitiate (d, groupRemoveWindow);
	groupSetCloseInitiate (d, groupCloseWindows);
	groupSetIgnoreInitiate (d, groupSetIgnore);
	groupSetIgnoreTerminate (d, groupUnsetIgnore);
	groupSetChangeColorInitiate (d, groupChangeColor);

	d->privates[groupDisplayPrivateIndex].ptr = gd;

	srand (time (NULL));

	return TRUE;
}

/*
 * groupFiniDisplay
 *
 */
static void
groupFiniDisplay (CompPlugin  *p,
				  CompDisplay *d)
{
	GROUP_DISPLAY (d);

	freeScreenPrivateIndex (d, gd->screenPrivateIndex);

	UNWRAP (gd, d, handleEvent);

	free (gd);
}

/*
 * groupInitScreen
 *
 */
static Bool
groupInitScreen (CompPlugin *p,
				 CompScreen *s)
{
	GroupScreen       *gs;
	GroupGlowTypeEnum glowType;

	GROUP_DISPLAY (s->display);

	gs = malloc (sizeof (GroupScreen));
	if (!gs)
		return FALSE;

	gs->windowPrivateIndex = allocateWindowPrivateIndex (s);
	if (gs->windowPrivateIndex < 0)
	{
		free (gs);
		return FALSE;
	}

	WRAP (gs, s, windowMoveNotify, groupWindowMoveNotify);
	WRAP (gs, s, windowResizeNotify, groupWindowResizeNotify);
	WRAP (gs, s, getOutputExtentsForWindow, groupGetOutputExtentsForWindow);
	WRAP (gs, s, preparePaintScreen, groupPreparePaintScreen);
	WRAP (gs, s, paintOutput, groupPaintOutput);
	WRAP (gs, s, drawWindow, groupDrawWindow);
	WRAP (gs, s, paintWindow, groupPaintWindow);
	WRAP (gs, s, paintTransformedOutput, groupPaintTransformedOutput);
	WRAP (gs, s, donePaintScreen, groupDonePaintScreen);
	WRAP (gs, s, windowGrabNotify, groupWindowGrabNotify);
	WRAP (gs, s, windowUngrabNotify, groupWindowUngrabNotify);
	WRAP (gs, s, damageWindowRect, groupDamageWindowRect);
	WRAP (gs, s, windowStateChangeNotify, groupWindowStateChangeNotify);

	s->privates[gd->screenPrivateIndex].ptr = gs;

	groupSetTabHighlightColorNotify (s, groupScreenOptionChanged);
	groupSetTabBaseColorNotify (s, groupScreenOptionChanged);
	groupSetTabBorderColorNotify (s, groupScreenOptionChanged);
	groupSetTabbarFontSizeNotify (s, groupScreenOptionChanged);
	groupSetTabbarFontColorNotify (s, groupScreenOptionChanged);
	groupSetGlowNotify (s, groupScreenOptionChanged);
	groupSetGlowTypeNotify (s, groupScreenOptionChanged);
	groupSetGlowSizeNotify (s, groupScreenOptionChanged);
	groupSetTabStyleNotify (s, groupScreenOptionChanged);
	groupSetThumbSizeNotify (s, groupScreenOptionChanged);
	groupSetThumbSpaceNotify (s, groupScreenOptionChanged);
	groupSetBorderWidthNotify (s, groupScreenOptionChanged);
	groupSetBorderRadiusNotify (s, groupScreenOptionChanged);

	gs->groups = NULL;

	gs->tmpSel.windows = NULL;
	gs->tmpSel.nWins = 0;

	gs->grabIndex = 0;
	gs->grabState = ScreenGrabNone;
	gs->queued = FALSE;
	gs->tabBarVisible = FALSE;
	gs->lastHoveredGroup = NULL;

	gs->pendingMoves = NULL;
	gs->pendingGrabs = NULL;
	gs->pendingUngrabs = NULL;

	gs->draggedSlot = NULL;
	gs->dragged = FALSE;
	gs->dragHoverTimeoutHandle = 0;
	gs->prevX = 0;
	gs->prevY = 0;

	gs->screenActions = (CHECK_WINDOW_PROPERTIES | APPLY_AUTO_TABBING);

	initTexture (s, &gs->glowTexture);

	glowType = groupGetGlowType (s);
	imageDataToTexture (s, &gs->glowTexture,
						glowTextureProperties[glowType].textureData,
						glowTextureProperties[glowType].textureSize,
						glowTextureProperties[glowType].textureSize,
						GL_RGBA, GL_UNSIGNED_BYTE);

	return TRUE;
}

/*
 * groupFiniScreen
 *
 */
static void
groupFiniScreen (CompPlugin *p,
				 CompScreen *s)
{
	GROUP_SCREEN (s);

	if (gs->groups)
	{
		GroupSelection *group, *nextGroup;

		for(group = gs->groups; group;)
		{
			if (group->tabBar)
			{
				GroupTabBarSlot *slot, *nextSlot;

				for (slot = group->tabBar->slots; slot;)
				{
					if (slot->region)
						XDestroyRegion (slot->region);

					nextSlot = slot->next;
					free (slot);
					slot = nextSlot;
				}

				groupDestroyCairoLayer (group->screen,
										group->tabBar->textLayer);
				groupDestroyCairoLayer (group->screen,
										group->tabBar->bgLayer);
				groupDestroyCairoLayer (group->screen,
										group->tabBar->selectionLayer);

				if (group->inputPrevention)
					XDestroyWindow (s->display->display,
									group->inputPrevention);

				if (group->tabBar->region)
					XDestroyRegion (group->tabBar->region);

				free (group->tabBar);
			}

			nextGroup = group->next;
			free (group);
			group = nextGroup;
		}
	}

	if (gs->tmpSel.windows)
		free (gs->tmpSel.windows);

	freeWindowPrivateIndex (s, gs->windowPrivateIndex);

	UNWRAP (gs, s, windowMoveNotify);
	UNWRAP (gs, s, windowResizeNotify);
	UNWRAP (gs, s, getOutputExtentsForWindow);
	UNWRAP (gs, s, preparePaintScreen);
	UNWRAP (gs, s, paintOutput);
	UNWRAP (gs, s, drawWindow);
	UNWRAP (gs, s, paintWindow);
	UNWRAP (gs, s, paintTransformedOutput);
	UNWRAP (gs, s, donePaintScreen);
	UNWRAP (gs, s, windowGrabNotify);
	UNWRAP (gs, s, windowUngrabNotify);
	UNWRAP (gs, s, damageWindowRect);
	UNWRAP (gs, s, windowStateChangeNotify);

	finiTexture (s, &gs->glowTexture);
	free (gs);
}

/*
 * groupInitWindow
 *
 */
static Bool groupInitWindow(CompPlugin * p, CompWindow * w)
{
	GroupWindow *gw;

	GROUP_SCREEN (w->screen);

	gw = malloc (sizeof (GroupWindow));
	if (!gw)
		return FALSE;

	gw->group = NULL;
	gw->inSelection = FALSE;
	gw->needsPosSync = FALSE;

	/* for tab */
	gw->oldWindowState = getWindowState (w->screen->display, w->id);
	gw->animateState = 0;
	gw->ungroup = FALSE;
	gw->slot = NULL;
	gw->tx = gw->ty = 0;
	gw->xVelocity = gw->yVelocity = 0;
	gw->orgPos.x = 0;
	gw->orgPos.y = 0;
	gw->mainTabOffset.x = 0;
	gw->mainTabOffset.y = 0;
	gw->destination.x = 0;
	gw->destination.y = 0;

	gw->windowHideInfo = NULL;

	if (w->minimized)
		gw->windowState = WindowMinimized;
	else if (w->shaded)
		gw->windowState = WindowShaded;
	else
		gw->windowState = WindowNormal;

	gw->lastState = w->state;

	w->privates[gs->windowPrivateIndex].ptr = gw;

	gw->glowQuads = NULL;
	groupComputeGlowQuads (w, &gs->glowTexture.matrix);

	return TRUE;
}

/*
 * groupFiniWindow
 *
 */
static void
groupFiniWindow (CompPlugin *p,
				 CompWindow *w)
{
	GROUP_WINDOW(w);

	if (gw->windowHideInfo)
		groupSetWindowVisibility (w, TRUE);

	if (gw->glowQuads)
		free (gw->glowQuads);

	free (gw);
}

/*
 * groupInit
 *
 */
static Bool
groupInit (CompPlugin *p)
{
	groupDisplayPrivateIndex = allocateDisplayPrivateIndex ();
	if (groupDisplayPrivateIndex < 0)
		return FALSE;

	return TRUE;
}

/*
 * groupFini
 *
 */
static void
groupFini (CompPlugin *p)
{
	freeDisplayPrivateIndex (groupDisplayPrivateIndex);
}

static int
groupGetVersion (CompPlugin *plugin,
				 int        version)
{
    return ABIVERSION;
}

/*
 * groupVTable
 *
 */
CompPluginVTable groupVTable = {
	"group",
	groupGetVersion,
	0,
	groupInit,
	groupFini,
	groupInitDisplay,
	groupFiniDisplay,
	groupInitScreen,
	groupFiniScreen,
	groupInitWindow,
	groupFiniWindow,
	0,
	0,
	0,
	0
};

/*
 * getCompPluginInfo
 *
 */
CompPluginVTable*
getCompPluginInfo (void)
{
	return &groupVTable;
}
