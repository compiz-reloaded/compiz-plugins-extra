#include "group.h"

/**
 *
 * Beryl group plugin
 *
 * group.c
 *
 * Copyright : (C) 2006 by Patrick Niklaus, Roi Cohen, Danny Baumann
 * Authors: Patrick Niklaus <patrick.niklaus@googlemail.com>
 *          Roi Cohen       <roico@beryl-project.org>
 *          Danny Baumann   <maniac@beryl-project.org>
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

//Compiz-only
#include <stdarg.h>
Bool
screenGrabExist (CompScreen *s, ...)
{
    va_list ap;
    char    *name;
    int	    i;

    for (i = 0; i < s->maxGrab; i++)
    {
	if (s->grabs[i].active)
	{
	    va_start (ap, s);

	    name = va_arg (ap, char *);
	    while (name)
	    {
		if (strcmp (name, s->grabs[i].name) == 0)
		    break;

		name = va_arg (ap, char *);
	    }

	    va_end (ap);

	    if (name)
		return TRUE;
	}
    }

    return FALSE;
}

/*
 * groupFindWindowIndex
 *
 * Description:
 * Activates a window after a certain time a slot has been dragged over it.
 *
 */
static Bool
groupDragHoverTimeout(void* closure)
{
	CompWindow *w = (CompWindow *) closure;

	if (!w)
		return FALSE;

	GROUP_SCREEN(w->screen);

	activateWindow(w);
	gs->dragHoverTimeoutHandle = 0;

	return FALSE;
}

/*
 * groupFindWindowIndex
 *
 */
int
groupFindWindowIndex(CompWindow *w, GroupSelection *g)
{
	int i;

	for(i = 0; i < g->nWins; i++)
	{
		if(g->windows[i]->id == w->id)
			return i;
	}

	return -1;
}

/*
 * groupFindGroupByID
 *
 */
GroupSelection* groupFindGroupByID(CompScreen *s, long int id)
{
	GroupSelection *group;
	GROUP_SCREEN(s);

	for (group = gs->groups; group; group = group->next) {
		if (group->identifier == id)
			break;
	}

	return group;
}

/*
 * groupCheckWindowProperty
 *
 */
Bool groupCheckWindowProperty(CompWindow *w, long int *id, Bool *tabbed, GLushort *color)
{
	Atom type;
	int fmt;
	unsigned long nitems, exbyte;
	long int *data;

	GROUP_DISPLAY(w->screen->display);

	if (XGetWindowProperty(w->screen->display->display, w->id,
						   gd->groupWinPropertyAtom, 0, 5,
						   False, XA_CARDINAL, &type, &fmt,
						   &nitems, &exbyte,
						   (unsigned char **)&data) == Success)
	{
		if (type == XA_CARDINAL && fmt == 32 && nitems == 5)
		{
			if (id)
				*id = data[0];
			if (tabbed)
				*tabbed = (Bool)data[1];
			if (color) {
				color[0] = (GLushort) data[2];
				color[1] = (GLushort) data[3];
				color[2] = (GLushort) data[4];
			}

			XFree(data);
			return TRUE;
		}
		else if (fmt != 0)
			XFree(data);
	}
	return FALSE;
}

/*
 * groupUpdateWindowProperty
 *
 */
void groupUpdateWindowProperty(CompWindow *w)
{
	GROUP_WINDOW(w);
	GROUP_DISPLAY(w->screen->display);

	if (gw->group) {
		long int buffer[5];

		buffer[0] = gw->group->identifier;
		buffer[1] = (gw->slot) ? TRUE : FALSE;

		/* group color RGB */
		buffer[2] = gw->group->color[0];
		buffer[3] = gw->group->color[1];
		buffer[4] = gw->group->color[2];

		XChangeProperty(w->screen->display->display,
				w->id, gd->groupWinPropertyAtom, XA_CARDINAL,
				32, PropModeReplace, (unsigned char *) buffer, 5);
	} else {
		XDeleteProperty(w->screen->display->display,
				w->id, gd->groupWinPropertyAtom);
	}

}

/*
 * groupGrabScreen
 *
 */
void groupGrabScreen(CompScreen * s, GroupScreenGrabState newState)
{
	GROUP_SCREEN(s);

	if ((gs->grabState != newState) && gs->grabIndex) {
		removeScreenGrab(s, gs->grabIndex, NULL);
		gs->grabIndex = 0;
	}

	if (newState == ScreenGrabSelect) {
		gs->grabIndex = pushScreenGrab(s, None, "group");
	} else if (newState == ScreenGrabTabDrag) {
		gs->grabIndex = pushScreenGrab(s, None, "group-drag");
	}

	gs->grabState = newState;
}

/*
 * groupSyncWindows
 *
 */
void groupSyncWindows(GroupSelection *group)
{
	int i;
	for (i = 0; i < group->nWins; i++)
		syncWindowPosition(group->windows[i]);
}

/*
 * groupRaiseWindows
 *
 */
void
groupRaiseWindows(CompWindow * top, GroupSelection *group)
{
	int i;
	for (i = 0; i < group->nWins; i++) {
		CompWindow *w = group->windows[i];
		if (w->id == top->id)
			continue;
		restackWindowBelow(w, top);
	}
}

/*
 * groupMinimizeWindows
 *
 */
static void
groupMinimizeWindows(CompWindow *top, GroupSelection *group, Bool minimize)
{
	int i;
	for (i = 0; i < group->nWins; i++) {
		CompWindow *w = group->windows[i];
		if (w->id == top->id)
			continue;

		if (minimize)
			minimizeWindow(w);
		else
			unminimizeWindow(w);
	}
}

/*
 * groupShadeWindows
 *
 */
static void
groupShadeWindows(CompWindow *top, GroupSelection *group, Bool shade)
{
	int i;
	unsigned int state;

	for (i = 0; i < group->nWins; i++) {
		CompWindow *w = group->windows[i];
		if (w->id == top->id)
			continue;

		if (shade)
			state = w->state | CompWindowStateShadedMask;
		else
			state = w->state & ~CompWindowStateShadedMask;

		changeWindowState(w, state);
		updateWindowAttributes(w, CompStackingUpdateModeNone);
	}
}

/*
 * groupDeleteGroupWindow
 *
 * Note: allowRegroup is need for a special case when groupAddWindowToGroup
 *       calls this function.
 *
 */
void groupDeleteGroupWindow(CompWindow * w, Bool allowRegroup)
{
	GROUP_WINDOW(w);
	GROUP_SCREEN(w->screen);
	GroupSelection *group = gw->group;

	if (!group)
		return;

	if (group->tabBar) {
		if (gw->slot)
		{
			if (gs->draggedSlot && gs->dragged && gs->draggedSlot->window->id == w->id)
				groupUnhookTabBarSlot(group->tabBar, gw->slot, FALSE);
			else
				groupDeleteTabBarSlot(group->tabBar, gw->slot);
		}

		if(!gw->ungroup && group->nWins > 1)
		{
			if(HAS_TOP_WIN(group))
			{
				// TODO: maybe add the IS_ANIMATED to the topTab for better constraining...

				CompWindow *topTab = TOP_TAB(group);
				GroupWindow *gtw = GET_GROUP_WINDOW(topTab, gs);
				int oldX = gw->orgPos.x;
				int oldY = gw->orgPos.y;

				gw->orgPos.x = group->oldTopTabCenterX - WIN_WIDTH(w) / 2;
				gw->orgPos.y = group->oldTopTabCenterY - WIN_HEIGHT(w) / 2;

				gw->destination.x = group->oldTopTabCenterX - WIN_WIDTH(w)/2 + gw->mainTabOffset.x -
					gtw->mainTabOffset.x;
				gw->destination.y = group->oldTopTabCenterY - WIN_HEIGHT(w)/2 + gw->mainTabOffset.y -
					gtw->mainTabOffset.y;

				gw->mainTabOffset.x = oldX;
				gw->mainTabOffset.y = oldY;

				gw->animateState |= IS_ANIMATED;

				gw->tx = gw->ty = gw->xVelocity = gw->yVelocity = 0.0f;
			}

			// Although when there is no top-tab, it will never really animate anything,
			// if we don't start the animation, the window will never get remvoed.
			groupStartTabbingAnimation(group, FALSE);

			group->ungroupState = UngroupSingle;
			gw->ungroup = TRUE;

			return;
		}
	}

	if (group->nWins && group->windows) {
		CompWindow **buf = group->windows;

		group->windows = (CompWindow **) calloc(group->nWins - 1, sizeof(CompWindow *));

		int counter = 0;
		int i;
		for (i = 0; i < group->nWins; i++) {
			if (buf[i]->id == w->id)
				continue;
			group->windows[counter++] = buf[i];
		}
		group->nWins = counter;

		if (group->nWins == 1)
		{
			damageWindowOutputExtents(group->windows[0]);	// Glow was removed from this window too.
			updateWindowOutputExtents(group->windows[0]);
		}

		if (group->nWins == 1 && groupGetAutoUngroup(w->screen)) {
			if (group->changeTab) {
				/* a change animation is pending: this most
				   likely means that a window must be moved
				   back onscreen, so we do that here */
				CompWindow *lw = group->windows[0];

				gs->queued = TRUE;
				groupSetWindowVisibility(lw, TRUE);
				moveWindow(lw, group->oldTopTabCenterX - WIN_X(lw) - WIN_WIDTH(lw) / 2,
					group->oldTopTabCenterY - WIN_Y(lw) - WIN_HEIGHT(lw) / 2, TRUE, TRUE);
				syncWindowPosition(lw);
				gs->queued = FALSE;
			}
			groupDeleteGroup(group);
		} else if (group->nWins <= 0) {
			free(group->windows);
			group->windows = NULL;
			groupDeleteGroup(group);
		}

		free(buf);

		damageWindowOutputExtents(w);
		gw->group = NULL;
		updateWindowOutputExtents(w);
		groupUpdateWindowProperty(w);

		if (allowRegroup && groupGetAutotabCreate(w->screen) &&
		    matchEval(groupGetWindowMatch(w->screen), w)) {
			groupAddWindowToGroup(w, NULL, 0);
			groupTabGroup(w);
		}
	}
}

/*
 * groupDeleteGroup
 *
 */
void groupDeleteGroup(GroupSelection *group)
{
	GROUP_SCREEN(group->screen);

	if (group->windows != NULL) {
		if (group->tabBar) {
			groupUntabGroup(group);
			group->ungroupState = UngroupAll;
			return;
		}

		int i;
		for (i = 0; i < group->nWins; i++) {
			CompWindow *cw = group->windows[i];
			GROUP_WINDOW(cw);

			damageWindowOutputExtents(cw);
			gw->group = NULL;
			updateWindowOutputExtents(cw);
			groupUpdateWindowProperty(cw);

			if (groupGetAutotabCreate(group->screen) &&
			    matchEval(groupGetWindowMatch(group->screen), cw)) {
				groupAddWindowToGroup(cw, NULL, 0);
				groupTabGroup(cw);
			}
		}
		free(group->windows);
		group->windows = NULL;
	} else if (group->tabBar)
		groupDeleteTabBar(group);

	GroupSelection *prev = group->prev;
	GroupSelection *next = group->next;

	// relink stack
	if (prev || next) {
		if (prev) {
			if (next)
				prev->next = next;
			else {
				prev->next = NULL;
			}
		}
		if (next) {
			if (prev)
				next->prev = prev;
			else {
				next->prev = NULL;
				gs->groups = next;
			}
		}
	} else {
		gs->groups = NULL;
	}
	free(group);
}

/*
 * groupAddWindowToGroup
 *
 */
void
groupAddWindowToGroup(CompWindow * w, GroupSelection *group, long int initialIdent)
{
	GROUP_SCREEN(w->screen);
	GROUP_WINDOW(w);

	if (group && gw->group == group)
		return;

	if (gw->group)
	{
		gw->ungroup = TRUE;	//This will prevent setting up animations on the previous group.
		groupDeleteGroupWindow(w, FALSE);
		gw->ungroup = FALSE;
	}

	if (group) {
		group->windows = (CompWindow **) realloc(group->windows, sizeof(CompWindow *) * (group->nWins + 1));
		group->windows[group->nWins] = w;
		group->nWins++;

		gw->group = group;
		updateWindowOutputExtents(w);

		groupUpdateWindowProperty(w);

		if(group->nWins == 2)
			updateWindowOutputExtents(group->windows[0]);	// First window in the group got its glow too...

		if (group->tabBar && group->topTab)
		{
			CompWindow *topTab = TOP_TAB(group);

			if(!gw->slot)
				groupCreateSlot(group, w);

			gw->destination.x = WIN_X(topTab) + (WIN_WIDTH(topTab) / 2) - (WIN_WIDTH(w) / 2);
			gw->destination.y = WIN_Y(topTab) + (WIN_HEIGHT(topTab) / 2) - (WIN_HEIGHT(w) / 2);
			gw->mainTabOffset.x = WIN_X(w) - gw->destination.x;
			gw->mainTabOffset.y = WIN_Y(w) - gw->destination.y;
			gw->orgPos.x = WIN_X(w);
			gw->orgPos.y = WIN_Y(w);

			gw->tx = gw->ty = gw->xVelocity = gw->yVelocity = 0.0f;

			gw->animateState = IS_ANIMATED;

			groupStartTabbingAnimation(group, TRUE);

			addWindowDamage(w);
		}
	} else {
		GroupSelection *g = malloc(sizeof(GroupSelection));

		g->windows = (CompWindow **) calloc(1, sizeof(CompWindow *));
		g->windows[0] = w;
		g->screen = w->screen;
		g->nWins = 1;
		g->topTab = NULL;
		g->prevTopTab = NULL;
		g->nextTopTab = NULL;
		g->activateTab = NULL;
		g->doTabbing = FALSE;
		g->changeAnimationTime = 0;
		g->changeAnimationDirection = 0;
		g->changeState = PaintOff;
		g->tabbingState = PaintOff;
		g->changeTab = FALSE;
		g->ungroupState = UngroupNone;
		g->tabBar = NULL;

		g->grabWindow = None;
		g->grabMask = 0;

		g->inputPrevention = None;
		g->ipwMapped = FALSE;

		g->oldTopTabCenterX = 0;
		g->oldTopTabCenterY = 0;

		// glow color
		g->color[0] = (int)(rand() / (((double)RAND_MAX + 1)/ 0xffff));
		g->color[1] = (int)(rand() / (((double)RAND_MAX + 1)/ 0xffff));
		g->color[2] = (int)(rand() / (((double)RAND_MAX + 1)/ 0xffff));
		g->color[3] = 0xFFFF;

		if (initialIdent)
			g->identifier = initialIdent;
		else {
			GroupSelection *tg;
			Bool invalidID = FALSE;

			g->identifier = gs->groups ? gs->groups->identifier : 0;
			do {
				invalidID = FALSE;
				for (tg = gs->groups; tg; tg = tg->next)
				{
					if (tg->identifier == g->identifier) {
						invalidID = TRUE;

						g->identifier++;
						break;
					}
				}
			} while (invalidID);
		}

		// relink stack
		if (gs->groups) {
			gs->groups->prev = g;
			g->next = gs->groups;
			g->prev = NULL;
			gs->groups = g;
		} else {
			g->prev = NULL;
			g->next = NULL;
			gs->groups = g;
		}

		gw->group = g;

		groupUpdateWindowProperty(w);
	}
}

/*
 * groupGroupWindows
 *
 */
Bool
groupGroupWindows(CompDisplay * d, CompAction * action,
		  CompActionState state, CompOption * option, int nOption)
{
	GROUP_DISPLAY(d);

	if (gd->tmpSel.nWins > 0) {
		int i;
		CompWindow *cw;
		GroupSelection *group = NULL;
		Bool tabbed = FALSE;

		for(i = 0; i < gd->tmpSel.nWins; i++)
		{
			cw = gd->tmpSel.windows[i];
			GROUP_WINDOW(cw);

			if (gw->group)
			{
				if(!tabbed || group->tabBar)
					group = gw->group;

				if(group->tabBar)
					tabbed = TRUE;
			}
		}

		// we need to do one first to get the pointer of a new group
		cw = gd->tmpSel.windows[0];
		GROUP_WINDOW (cw);

		groupAddWindowToGroup(cw, group, 0);
		addWindowDamage(cw);

		gw->inSelection = FALSE;

		group = gw->group;
		for (i = 1; i < gd->tmpSel.nWins; i++) {
			cw = gd->tmpSel.windows[i];
			GROUP_WINDOW (cw);

			groupAddWindowToGroup(cw, group, 0);
			addWindowDamage(cw);

			gw->inSelection = FALSE;
		}

		// exit selection
		free(gd->tmpSel.windows);
		gd->tmpSel.windows = NULL;
		gd->tmpSel.nWins = 0;
	}

	return FALSE;
}

/*
 * groupUnGroupWindows
 *
 */
Bool
groupUnGroupWindows(CompDisplay * d, CompAction * action,
		    CompActionState state, CompOption * option,
		    int nOption)
{
	CompWindow *cw = findWindowAtDisplay(d, d->activeWindow);

	if (!cw)
		return FALSE;

	GROUP_WINDOW(cw);

	if (gw->group) {
		groupDeleteGroup(gw->group);
	}

	return FALSE;
}

/*
 * groupRemoveWindow
 *
 */
Bool
groupRemoveWindow(CompDisplay * d, CompAction * action,
		  CompActionState state, CompOption * option, int nOption)
{
	CompWindow *cw = findWindowAtDisplay(d, d->activeWindow);
	if (!cw)
		return FALSE;
	GROUP_WINDOW(cw);

	if (gw->group)
		groupDeleteGroupWindow(cw, TRUE);

	return FALSE;
}

/*
 * groupCloseWindows
 *
 */
Bool
groupCloseWindows(CompDisplay * d, CompAction * action,
		  CompActionState state, CompOption * option, int nOption)
{
	CompWindow *w = findWindowAtDisplay(d, d->activeWindow);
	if (!w)
		return FALSE;
	GROUP_WINDOW(w);

	if (gw->group) {

		int nWins = gw->group->nWins;
		int i;
		for (i = 0; i < nWins; i++) {
			CompWindow *cw = gw->group->windows[i];
			closeWindow(cw, getCurrentTimeFromDisplay(d));
		}
	}

	return FALSE;
}

/*
 * groupChangeColor
 *
 */
Bool
groupChangeColor(CompDisplay * d, CompAction * action,
		     CompActionState state, CompOption * option,
		     int nOption)
{
	CompWindow *w = findWindowAtDisplay(d, d->activeWindow);
	if (!w)
		return FALSE;
	GROUP_WINDOW(w);

	if (gw->group) {
		gw->group->color[0] = (int)(rand() / (((double)RAND_MAX + 1)/ 0xffff));
		gw->group->color[1] = (int)(rand() / (((double)RAND_MAX + 1)/ 0xffff));
		gw->group->color[2] = (int)(rand() / (((double)RAND_MAX + 1)/ 0xffff));

		groupRenderTopTabHighlight(gw->group);

		damageScreen(w->screen);
	}

	return FALSE;
}

/*
 * groupSetIgnore
 *
 */
Bool
groupSetIgnore(CompDisplay * d, CompAction * action, CompActionState state,
	       CompOption * option, int nOption)
{
	GROUP_DISPLAY(d);

	gd->ignoreMode = TRUE;

	if (state & CompActionStateInitKey)
		action->state |= CompActionStateTermKey;

	return FALSE;
}

/*
 * groupUnsetIgnore
 *
 */
Bool
groupUnsetIgnore(CompDisplay * d, CompAction * action,
		 CompActionState state, CompOption * option, int nOption)
{
	GROUP_DISPLAY(d);

	gd->ignoreMode = FALSE;

	action->state &= ~CompActionStateTermKey;

	return FALSE;
}

/*
 * groupHandleButtonPressEvent
 *
 */
static void
groupHandleButtonPressEvent(CompDisplay *d, XEvent *event)
{
	int xRoot = event->xbutton.x_root;
	int yRoot = event->xbutton.y_root;
	int button = event->xbutton.button;

	CompScreen *s = findScreenAtDisplay(d, event->xbutton.root);

	if (!s)
		return;

	GROUP_SCREEN(s);
	GroupSelection *group;

	for (group = gs->groups; group; group = group->next) {
		if ((group->inputPrevention == event->xbutton.window) && group->tabBar) {
			switch (button) {
			case Button1:
				{
					GroupTabBarSlot *slot;

					for (slot = group->tabBar->slots; slot; slot = slot->next) {
						if (XPointInRegion (slot->region, xRoot, yRoot)) {
							gs->draggedSlot = slot;
							gs->dragged = FALSE; // The slot isn't dragged yet...

							gs->prevX = xRoot;
							gs->prevY = yRoot;

							if (gs->grabState == ScreenGrabNone) {
								if (!otherScreenGrabExist(s, "group", "group-drag"))
									groupGrabScreen(s, ScreenGrabTabDrag);
							}
						}
					}
				}
				break;
			case Button4:
			case Button5:
				{
					CompWindow *topTab = NULL;
					GroupWindow *gw;


					if (group->nextTopTab)
						topTab = NEXT_TOP_TAB(group);
					else if (group->topTab)
						 // If there are no tabbing animations, topTab is never NULL.
						topTab = TOP_TAB(group);

					if (!topTab)
						return;

					gw = GET_GROUP_WINDOW(topTab, gs);

					if (button == Button4) {
						if (gw->slot->prev)
							groupChangeTab(gw->slot->prev, RotateLeft);
						else
							groupChangeTab(gw->group->tabBar->revSlots, RotateLeft);
					} else {
						if (gw->slot->next)
							groupChangeTab(gw->slot->next, RotateRight);
						else
							groupChangeTab(gw->group->tabBar->slots, RotateRight);
					}
				}
				break;
			}

			break;
		}
	}
}

/*
 * groupHandleButtonReleaseEvent
 *
 */
static void
groupHandleButtonReleaseEvent(CompDisplay *d, XEvent *event)
{
	GroupSelection *group;

	if (event->xbutton.button != 1)
		return;

	CompScreen *s = findScreenAtDisplay(d, event->xbutton.root);
	if (!s)
		return;

	GROUP_SCREEN(s);

	if(!gs->draggedSlot)
		return;

	if(!gs->dragged) {
		groupChangeTab(gs->draggedSlot, RotateUncertain);
		gs->draggedSlot = NULL;

		if (gs->grabState == ScreenGrabTabDrag)
			groupGrabScreen(s, ScreenGrabNone);

		return;
	}

	GROUP_WINDOW(gs->draggedSlot->window);

	int vx, vy;
	Region newRegion = XCreateRegion();
	XUnionRegion(newRegion, gs->draggedSlot->region, newRegion);

	groupGetDrawOffsetForSlot(gs->draggedSlot, &vx, &vy);
	XOffsetRegion(newRegion, vx, vy);

	Bool inserted = FALSE;
	Bool wasInTabBar = FALSE;
	for(group = gs->groups; group; group = group->next) {
		Bool inTabBar;

		if (!group->tabBar || !HAS_TOP_WIN(group))
			continue;

		// create clipping region
		Region clip = groupGetClippingRegion(TOP_TAB(group));

		Region buf = XCreateRegion();
		XIntersectRegion(newRegion, group->tabBar->region, buf);
		XSubtractRegion(buf, clip, buf);
		XDestroyRegion(clip);

		inTabBar = !XEmptyRegion(buf);

		XDestroyRegion(buf);

		if(!inTabBar)
			continue;

		wasInTabBar = TRUE;
		GroupTabBarSlot *slot;

		for(slot = group->tabBar->slots; slot; slot = slot->next) {
			if(slot == gs->draggedSlot)
				continue;

			Region slotRegion = XCreateRegion();
			XRectangle rect;
			Bool inSlot;

			if(slot->prev && slot->prev != gs->draggedSlot)
				rect.x = slot->prev->region->extents.x2;
			else if(slot->prev && slot->prev == gs->draggedSlot && gs->draggedSlot->prev)
				rect.x = gs->draggedSlot->prev->region->extents.x2;
			else
				rect.x = group->tabBar->region->extents.x1;

			rect.y = slot->region->extents.y1;

			if(slot->next && slot->next != gs->draggedSlot)
				rect.width = slot->next->region->extents.x1 - rect.x;
			else if(slot->next && slot->next == gs->draggedSlot && gs->draggedSlot->next)
				rect.width = gs->draggedSlot->next->region->extents.x1 - rect.x;
			else
				rect.width = group->tabBar->region->extents.x2;

			rect.height = slot->region->extents.y2 - slot->region->extents.y1;

			XUnionRectWithRegion(&rect, slotRegion, slotRegion);

			Region buf = XCreateRegion();
			XIntersectRegion(newRegion, slotRegion, buf);

			inSlot = !XEmptyRegion(buf);

			XDestroyRegion(buf);
			XDestroyRegion(slotRegion);

			if (!inSlot)
				continue;

			GroupTabBarSlot *tmpDraggedSlot = gs->draggedSlot;

			if(group != gw->group) {
				GroupSelection *tmpGroup = gw->group;

				// if the dragged window is not the top tab, move it onscreen
				if (!IS_TOP_TAB(gs->draggedSlot->window, tmpGroup) && tmpGroup->topTab) {
					CompWindow *tw = tmpGroup->topTab->window;

					tmpGroup->oldTopTabCenterX = WIN_X(tw) + WIN_WIDTH(tw) / 2;
					tmpGroup->oldTopTabCenterY = WIN_Y(tw) + WIN_HEIGHT(tw) / 2;

					gs->queued = TRUE;
					groupSetWindowVisibility(gs->draggedSlot->window, TRUE);
					moveWindow(gs->draggedSlot->window,
							gw->group->oldTopTabCenterX - WIN_X(gs->draggedSlot->window) - WIN_WIDTH(gs->draggedSlot->window) / 2,
							gw->group->oldTopTabCenterY - WIN_Y(gs->draggedSlot->window) - WIN_HEIGHT(gs->draggedSlot->window) / 2,
							TRUE, TRUE);
					syncWindowPosition(gs->draggedSlot->window);
					gs->queued = FALSE;
				}

				// Change the group.
				groupAddWindowToGroup(gs->draggedSlot->window, group, 0);
			} else
				groupUnhookTabBarSlot(group->tabBar, gs->draggedSlot, TRUE);

			// for re-calculating the tab-bar including the dragged window
			gs->draggedSlot = NULL;
			gs->dragged = FALSE;
			inserted = TRUE;

			if((tmpDraggedSlot->region->extents.x1 + tmpDraggedSlot->region->extents.x2 + (2 * vx)) / 2 >
				    (slot->region->extents.x1 + slot->region->extents.x2) / 2)
				groupInsertTabBarSlotAfter(group->tabBar, tmpDraggedSlot, slot);
			else
				groupInsertTabBarSlotBefore(group->tabBar, tmpDraggedSlot, slot);

			// Hide tab-bars.
			GroupSelection *tmpGroup;
			for(tmpGroup = gs->groups; tmpGroup; tmpGroup = tmpGroup->next)
			{
				if (group == tmpGroup)
					groupTabSetVisibility (tmpGroup, TRUE, 0);
				else
					groupTabSetVisibility(tmpGroup, FALSE, PERMANENT);
			}

			break;
		}

		if (inserted)
			break;
	}

	if (newRegion)
		XDestroyRegion(newRegion);

	if (!inserted) {
		CompWindow *draggedSlotWindow = gs->draggedSlot->window;
		GroupSelection *tmpGroup;

		for(tmpGroup = gs->groups; tmpGroup; tmpGroup = tmpGroup->next)
			groupTabSetVisibility(tmpGroup, FALSE, PERMANENT);

		gs->draggedSlot = NULL;
		gs->dragged = FALSE;

		if(groupGetDndUngroupWindow(s) && !wasInTabBar)
			groupDeleteGroupWindow(draggedSlotWindow, TRUE);

		else if (gw->group && gw->group->topTab) {
			groupRecalcTabBarPos(gw->group,
				(gw->group->tabBar->region->extents.x1 + gw->group->tabBar->region->extents.x2) / 2,
				gw->group->tabBar->region->extents.x1, gw->group->tabBar->region->extents.x2);
		}

		// to remove the painted slot
		damageScreen(s);
	}

	if (gs->grabState == ScreenGrabTabDrag)
		groupGrabScreen(s, ScreenGrabNone);

	if (gs->dragHoverTimeoutHandle) {
		compRemoveTimeout(gs->dragHoverTimeoutHandle);
		gs->dragHoverTimeoutHandle = 0;
	}
}

/*
 * groupHandleMotionEvent
 *
 */

/* the radius to determine if it was a click or a drag */
#define RADIUS 5

static void
groupHandleMotionEvent(CompScreen * s, int xRoot, int yRoot)
{
	GROUP_SCREEN(s);

	if (gs->grabState == ScreenGrabTabDrag) {
		int dx, dy;
		REGION reg;
		Region draggedRegion = gs->draggedSlot->region;

		reg.rects = &reg.extents;
		reg.numRects = 1;

		dx = xRoot - gs->prevX;
		dy = yRoot - gs->prevY;

		if(gs->dragged || abs(dx) > RADIUS || abs(dy) > RADIUS) {
			gs->prevX = xRoot;
			gs->prevY = yRoot;

			if(!gs->dragged) {
				GroupSelection *group;
				GROUP_WINDOW(gs->draggedSlot->window);

				gs->dragged = TRUE;

				for(group = gs->groups; group; group = group->next)
					groupTabSetVisibility(group, TRUE, PERMANENT);

				groupRecalcTabBarPos(gw->group, (gw->group->tabBar->region->extents.x1 + gw->group->tabBar->region->extents.x2) / 2,
		    						 gw->group->tabBar->region->extents.x1, gw->group->tabBar->region->extents.x2);
			}


			int vx,vy;
			groupGetDrawOffsetForSlot(gs->draggedSlot, &vx, &vy);

			// (border = 5) + (buffer = 2) == (damage buffer = 7)
			//TODO: respect border option.
			reg.extents.x1 = draggedRegion->extents.x1 - 7 + vx;
			reg.extents.y1 = draggedRegion->extents.y1 - 7 + vy;
			reg.extents.x2 = draggedRegion->extents.x2 + 7 + vx;
			reg.extents.y2 = draggedRegion->extents.y2 + 7 + vy;
			damageScreenRegion(s, &reg);

			XOffsetRegion (gs->draggedSlot->region, dx, dy);
			gs->draggedSlot->springX = (gs->draggedSlot->region->extents.x1 + gs->draggedSlot->region->extents.x2) / 2;

			reg.extents.x1 = draggedRegion->extents.x1 - 7 + vx;
			reg.extents.y1 = draggedRegion->extents.y1 - 7 + vy;
			reg.extents.x2 = draggedRegion->extents.x2 + 7 + vx;
			reg.extents.y2 = draggedRegion->extents.y2 + 7 + vy;
			damageScreenRegion(s, &reg);
		}
	} else if (gs->grabState == ScreenGrabSelect) {
    	groupDamageSelectionRect(s, xRoot, yRoot);
	}
}

/*
 * groupHandleEvent
 *
 */
void groupHandleEvent(CompDisplay * d, XEvent * event)
{
	GROUP_DISPLAY(d);

	switch (event->type) {
	case MotionNotify:
		{
			CompScreen *s = findScreenAtDisplay(d, event->xmotion.root);
			if (s)
				groupHandleMotionEvent(s, pointerX, pointerY);

			break;
		}

	case ButtonPress:
		groupHandleButtonPressEvent(d, event);
		break;

	case ButtonRelease:
		groupHandleButtonReleaseEvent(d, event);
		break;

	case MapNotify:
		{
		    CompWindow *cw;
		    CompWindow *w = findWindowAtDisplay(d, event->xmap.window);
		    if (!w)
				break;

		    for (cw = w->screen->windows; cw; cw = cw->next)
		    {
				if (w->id == cw->frame)
				{
				    GROUP_WINDOW(cw);
					if (gw->windowHideInfo)
						XUnmapWindow(cw->screen->display->display, cw->frame);
				}
			}
		}
		break;

	case UnmapNotify:
		{
			CompWindow *w = findWindowAtDisplay(d, event->xunmap.window);
			if (!w)
				break;
			GROUP_WINDOW(w);

			if (w->pendingUnmaps) {
				if (w->shaded) {
					gw->windowState = WindowShaded;

					if (gw->group && groupGetShadeAll(w->screen))
						groupShadeWindows(w, gw->group, TRUE);
				} else if (w->minimized) {
					gw->windowState = WindowMinimized;

					if (gw->group && groupGetMinimizeAll(w->screen))
						groupMinimizeWindows(w, gw->group, TRUE);
				}
			}

			if (gw->group) {
				if (gw->group->tabBar && IS_TOP_TAB(w, gw->group)) {
					/* on unmap of the top tab, hide the tab bar and the
					   input prevention window */
					groupTabSetVisibility(gw->group, FALSE, PERMANENT);
				}
				if (!w->pendingUnmaps) {
					//close event
					gw->ungroup = TRUE;	// This will prevent setting up animations on group.
					groupDeleteGroupWindow(w, FALSE);
					gw->ungroup = FALSE;
					damageScreen(w->screen);
				}
			}
			break;
		}

	case ClientMessage:
		if (event->xclient.message_type == d->winActiveAtom) {
			CompWindow *w = findWindowAtDisplay(d, event->xclient.window);
			if (!w)
				return;

			GROUP_WINDOW(w);

			if (gw->group && gw->group->tabBar && HAS_TOP_WIN(gw->group)) {
				CompWindow *tw = TOP_TAB(gw->group);

				if (w->id != tw->id) {
					/* if a non top-tab has been activated, switch to the
					   top-tab instead - but only if is visible */
					if (tw->shaded) {
						changeWindowState(tw, tw->state & ~CompWindowStateShadedMask);
						updateWindowAttributes(tw, CompStackingUpdateModeNone);
					} else if (tw->minimized)
						unminimizeWindow(tw);

					if (!(tw->state & CompWindowStateHiddenMask)) {
						if (!gw->group->changeTab)
							gw->group->activateTab = gw->slot;
						sendWindowActivationRequest(tw->screen, tw->id);
					}
				}
			}
		}
		break;

	default:
		if (event->type == d->shapeEvent + ShapeNotify) {
			XShapeEvent *se = (XShapeEvent*)event;
			if (se->kind == ShapeInput) {
				CompWindow *w = findWindowAtDisplay(d, se->window);

				if (w) {
					GROUP_WINDOW(w);

					if (gw->windowHideInfo)
						groupClearWindowInputShape(w, gw->windowHideInfo);
				}
			}
		}
		break;
	}

	UNWRAP(gd, d, handleEvent);
	(*d->handleEvent) (d, event);
	WRAP(gd, d, handleEvent, groupHandleEvent);

	switch (event->type) {
	case FocusIn:
		{
			CompWindow *w = findWindowAtDisplay(d, event->xfocus.window);
			if (!w)
				break;

			GROUP_WINDOW(w);
			if (gw->group && !gw->group->tabBar) {
				if (groupGetRaiseAll(w->screen))
					groupRaiseWindows(w, gw->group);
			}
		}
		break;

	case PropertyNotify:
		if (event->xproperty.atom == d->winActiveAtom) {
			CompWindow *w = findWindowAtDisplay(d, d->activeWindow);
			if (!w)
				break;

			GROUP_WINDOW(w);

			if (gw->group && gw->group->activateTab) {
				groupChangeTab(gw->group->activateTab, RotateUncertain);
				gw->group->activateTab = NULL;
			}
		} else if (event->xproperty.atom == d->wmNameAtom) {
			CompWindow *w = findWindowAtDisplay(d, d->activeWindow);
			if (!w)
				break;

			GROUP_WINDOW(w);

			if (gw->group && gw->group->tabBar) {
				// make sure we are using the updated name
				groupRenderWindowTitle(gw->group);
			}
		}
		break;

	case EnterNotify:
		{
			CompWindow *w = findWindowAtDisplay(d, event->xcrossing.window);
			if (!w)
				break;

			GROUP_WINDOW(w);
			GROUP_SCREEN(w->screen);

			if (gs->showDelayTimeoutHandle)
				compRemoveTimeout(gs->showDelayTimeoutHandle);

			if (w->id != w->screen->grabWindow)
				groupUpdateTabBars(w->screen, w->id);

			if (gw->group) {
				GROUP_SCREEN(w->screen);

				if (gs->draggedSlot && gs->dragged && IS_TOP_TAB(w, gw->group)) {
					int hoverTime = groupGetDragHoverTime(w->screen) * 1000;
					if (gs->dragHoverTimeoutHandle)
						compRemoveTimeout(gs->dragHoverTimeoutHandle);

					if (hoverTime > 0)
						gs->dragHoverTimeoutHandle =
							compAddTimeout(hoverTime, groupDragHoverTimeout, w);
				}
			}
		}

	case ConfigureNotify:
		{
			CompWindow *w = findWindowAtDisplay(d, event->xconfigure.window);

			if (!w)
				break;

			GROUP_WINDOW(w);

			if (gw->group && gw->group->tabBar && IS_TOP_TAB(w, gw->group) && gw->group->inputPrevention)
			{
			    XWindowChanges xwc;

			    xwc.stack_mode = Above;
			    xwc.sibling = w->id;

		    	    XConfigureWindow (w->screen->display->display, gw->group->inputPrevention,
					      CWSibling | CWStackMode, &xwc);
			}
		}
		break;


	default:
		break;
	}
}

/*
 * groupGetOutputExtentsForWindow
 *
 */
void
groupGetOutputExtentsForWindow (CompWindow * w, CompWindowExtents * output)
{
	GROUP_SCREEN (w->screen);
	GROUP_WINDOW (w);

	UNWRAP (gs, w->screen, getOutputExtentsForWindow);
	(*w->screen->getOutputExtentsForWindow) (w, output);
	WRAP (gs, w->screen, getOutputExtentsForWindow,
		groupGetOutputExtentsForWindow);

	if (gw->group && gw->group->nWins > 1)
	{
		int glowSize = groupGetGlowSize(w->screen);

		if (glowSize > output->left)
			output->left = glowSize;
		if (glowSize > output->right)
			output->right = glowSize;
		if (glowSize > output->top)
			output->top = glowSize;
		if (glowSize > output->bottom)
			output->bottom = glowSize;
	}
}

/*
 * groupWindowResizeNotify
 *
 */
void groupWindowResizeNotify(CompWindow * w, int dx, int dy, int dwidth, int dheight)
{
	GROUP_SCREEN(w->screen);
	GROUP_DISPLAY(w->screen->display);
	GROUP_WINDOW(w);

	if (gw->group && !gd->ignoreMode) {
		if (((gw->lastState & MAXIMIZE_STATE) != (w->state & MAXIMIZE_STATE)) &&
		    groupGetMaximizeUnmaximizeAll(w->screen))
		{
			int i;
			for (i = 0; i < gw->group->nWins; i++) {
				CompWindow *cw = gw->group->windows[i];
				if (!cw)
					continue;

				if (cw->id == w->id)
					continue;

				maximizeWindow(cw, w->state & MAXIMIZE_STATE);
			}
		} else if ((gw->group->grabWindow == w->id) && groupGetResizeAll(w->screen)) {
			int i;
			for (i = 0; i < gw->group->nWins; i++) {
				CompWindow *cw =  gw->group->windows[i];
				if (!cw)
					continue;

				if (cw->state & MAXIMIZE_STATE)
					continue;

				if (cw->id == w->id)
					continue;

				int nx = 0;
				int ny = 0;

				if (groupGetRelativeDistance(w->screen)) {
					int distX = cw->serverX - (w->serverX - dx);
					int distY = cw->serverY - (w->serverY - dy);
					int ndistX = distX * ((float) w->serverWidth / (float) (w->serverWidth - dwidth));
					int ndistY = distY * ((float) w->serverHeight / (float) (w->serverHeight - dheight));
					nx = w->serverX + ndistX;
					ny = w->serverY + ndistY;
				} else {
					nx = cw->serverX + dx;
					ny = cw->serverY + dy;
				}

				int nwidth = cw->serverWidth + dwidth;
				int nheight = cw->serverHeight + dheight;

				constrainNewWindowSize(cw, nwidth, nheight, &nwidth, &nheight);

				XWindowChanges xwc;

				xwc.x = nx;
				xwc.y = ny;
				xwc.width = nwidth;
				xwc.height = nheight;

				configureXWindow(cw, CWX | CWY | CWWidth | CWHeight, &xwc);
			}
		}
	}

	UNWRAP(gs, w->screen, windowResizeNotify);
	(*w->screen->windowResizeNotify) (w, dx, dy, dwidth, dheight);
	WRAP(gs, w->screen, windowResizeNotify, groupWindowResizeNotify);

	if (gw->glowQuads)
		groupComputeGlowQuads (w, &gs->glowTexture.matrix);

	if (gw->group && gw->group->tabBar &&
	    (gw->group->tabBar->state != PaintOff) && IS_TOP_TAB(w, gw->group)) {
		groupRecalcTabBarPos(gw->group, pointerX,
			WIN_X(w), WIN_X(w) + WIN_WIDTH(w));
	}

}

/*
 * groupWindowMoveNotify
 *
 */
void
groupWindowMoveNotify(CompWindow * w, int dx, int dy, Bool immediate)
{
	GROUP_SCREEN(w->screen);
	GROUP_DISPLAY(w->screen->display);
	GROUP_WINDOW(w);

	UNWRAP(gs, w->screen, windowMoveNotify);
	(*w->screen->windowMoveNotify) (w, dx, dy, immediate);
	WRAP(gs, w->screen, windowMoveNotify, groupWindowMoveNotify);

	if (gw->glowQuads)
		groupComputeGlowQuads (w, &gs->glowTexture.matrix);

	if(!gw->group || gs->queued)
		return;

	/* FIXME: we need a reliable, 100% safe way to detect window
			  moves caused by viewport changes here */
	Bool viewportChange =
		((dx && !(dx % w->screen->width)) || (dy && !(dy % w->screen->height)));

	if (viewportChange && (gw->animateState & IS_ANIMATED)) {
		gw->destination.x += dx;
		gw->destination.y += dy;
	}

	if (gw->group->tabBar && IS_TOP_TAB(w, gw->group)) {
		GroupTabBarSlot *slot;
		GroupTabBar *bar = gw->group->tabBar;

		bar->rightSpringX += dx;
		bar->leftSpringX += dx;

		groupMoveTabBarRegion (gw->group,
				       (groupGetSpringModelOnMove (w->screen)) ? 0 : dx, dy, TRUE);

		for(slot = bar->slots; slot; slot = slot->next) {
			if (groupGetSpringModelOnMove(w->screen))
				XOffsetRegion (slot->region, 0, dy);
			else
				XOffsetRegion (slot->region, dx, dy);
			slot->springX += dx;
		}

		return;
	}

	if (gw->group->doTabbing || gd->ignoreMode ||
	    (gw->group->grabWindow != w->id) ||
		!(gw->group->grabMask & CompWindowGrabMoveMask) ||
	    !groupGetMoveAll(w->screen))
		return;

	int i;

	for (i = 0; i < gw->group->nWins; i++) {
		CompWindow *cw = gw->group->windows[i];
		if (!cw)
			continue;

		if (cw->id == w->id)
			continue;

		GROUP_WINDOW(cw);

		if (cw->state & MAXIMIZE_STATE) {
			if (viewportChange) {
				gw->needsPosSync = TRUE;
				groupEnqueueMoveNotify (cw, -dx, -dy, immediate, TRUE);
			}
		} else if (!viewportChange) {
			gw->needsPosSync = TRUE;
			groupEnqueueMoveNotify (cw, dx, dy, immediate, FALSE);
		}
	}
}

void
groupWindowGrabNotify(CompWindow * w,
                       int x, int y, unsigned int state, unsigned int mask)
{
	GROUP_SCREEN(w->screen);
	GROUP_DISPLAY(w->screen->display);
	GROUP_WINDOW(w);

	if (gw->group && !gd->ignoreMode && !gs->queued) {
		if (gw->group->tabBar)
			groupTabSetVisibility(gw->group, FALSE, 0);
		else {
			int i;
			for (i = 0; i < gw->group->nWins; i++) {
				CompWindow *cw = gw->group->windows[i];
				if (!cw)
					continue;

				if (cw->id != w->id)
					groupEnqueueGrabNotify (cw, x, y, state, mask);
			}
		}

		gw->group->grabWindow = w->id;
		gw->group->grabMask = mask;

	}

	UNWRAP(gs, w->screen, windowGrabNotify);
	(*w->screen->windowGrabNotify) (w, x, y, state, mask);
	WRAP(gs, w->screen, windowGrabNotify, groupWindowGrabNotify);
}

void groupWindowUngrabNotify(CompWindow * w)
{
	GROUP_SCREEN(w->screen);
	GROUP_DISPLAY(w->screen->display);
	GROUP_WINDOW(w);

	if (gw->group && !gd->ignoreMode && !gs->queued) {
		if (!gw->group->tabBar) {
			int i;

			groupDequeueMoveNotifies(w->screen);

			for (i = 0; i < gw->group->nWins; i++) {
				CompWindow *cw = gw->group->windows[i];
				if (!cw)
					continue;

				if (cw->id != w->id) {
					GROUP_WINDOW(cw);

					if (gw->needsPosSync) {
						syncWindowPosition(cw);
						gw->needsPosSync = FALSE;
					}
					groupEnqueueUngrabNotify (cw);
				}
			}
		}

		gw->group->grabWindow = None;
		gw->group->grabMask = 0;
	}

	UNWRAP(gs, w->screen, windowUngrabNotify);
	(*w->screen->windowUngrabNotify) (w);
	WRAP(gs, w->screen, windowUngrabNotify, groupWindowUngrabNotify);
}

Bool groupDamageWindowRect(CompWindow * w, Bool initial, BoxPtr rect)
{

	GROUP_SCREEN(w->screen);
	GROUP_WINDOW(w);
	Bool status;

	UNWRAP(gs, w->screen, damageWindowRect);
	status = (*w->screen->damageWindowRect) (w,initial,rect);
	WRAP(gs, w->screen, damageWindowRect, groupDamageWindowRect);

	if (initial) {
		if (groupGetAutotabCreate(w->screen) &&
			matchEval(groupGetWindowMatch(w->screen), w))
		{
			if (!gw->group && (gw->windowState == WindowNormal)) {
				groupAddWindowToGroup(w, NULL, 0);
				groupTabGroup(w);
			}
		}

		if ((gw->windowState == WindowMinimized) && gw->group) {
			if (groupGetMinimizeAll(w->screen))
				groupMinimizeWindows(w, gw->group, FALSE);
		} else if ((gw->windowState == WindowShaded) && gw->group) {
			if (groupGetShadeAll(w->screen))
				groupShadeWindows(w, gw->group, FALSE);
		}

		gw->windowState = WindowNormal;
	}

	if (gw->slot)
	{
		int vx, vy;
		Region reg;

		groupGetDrawOffsetForSlot (gw->slot, &vx, &vy);
		if (vx || vy)
		{
			reg = XCreateRegion ();
			XUnionRegion (reg, gw->slot->region, reg);
			XOffsetRegion (reg, vx, vy);
		}
		else
			reg = gw->slot->region;

		damageScreenRegion (w->screen, reg);

		if (vx || vy)
			XDestroyRegion (reg);
	}

	return status;

}

void groupWindowStateChangeNotify(CompWindow *w)
{
	 GROUP_SCREEN(w->screen);
	 GROUP_WINDOW(w);

	 gw->lastState = w->lastState;

	 UNWRAP(gs, w->screen, windowStateChangeNotify);
	 (*w->screen->windowStateChangeNotify) (w);
	 WRAP(gs, w->screen, windowStateChangeNotify, groupWindowStateChangeNotify);
}
