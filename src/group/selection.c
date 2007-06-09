#include "group.h"

/**
 *
 * Beryl group plugin
 *
 * selection.c
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

/*
 * groupWindowInRegion
 *
 */
static Bool
groupWindowInRegion(CompWindow *w, Region src, float precision)
{
	Region buf = XCreateRegion();
	XIntersectRegion(w->region, src, buf);

	// buf area
	int i;
	int area = 0;
	BOX *box;
	for(i = 0; i < buf->numRects; i++) {
		box = &buf->rects[i];
		area += (box->x2 - box->x1) * (box->y2 - box->y1); // width * height
	}

	XDestroyRegion(buf);

	if (area >= WIN_WIDTH(w) * WIN_HEIGHT(w) * precision) {
		XSubtractRegion(src, w->region, src);
		return TRUE;
	}

	return FALSE;
}

/*
 * groupFindGroupInWindows
 *
 */
static Bool groupFindGroupInWindows(GroupSelection *group, CompWindow **windows, int nWins)
{
	int i;
	for (i = 0; i < nWins; i++) {
		CompWindow *cw = windows[i];
		GROUP_WINDOW(cw);
		if (gw->group == group)
			return TRUE;
	}

	return FALSE;
}

/*
 * groupFindWindowsInRegion
 *
 */
CompWindow **groupFindWindowsInRegion(CompScreen * s, Region reg, int *c)
{
	float precision = groupGetSelectPrecision(s) / 100.0f;

	CompWindow **ret = NULL;
	int count = 0;
	CompWindow *w;
	for (w = s->reverseWindows; w; w = w->prev) {
		if (matchEval(groupGetWindowMatch(s), w) &&
		    !w->invisible &&
		    groupWindowInRegion(w, reg, precision))
		{
			GROUP_WINDOW(w);
			if (gw->group && groupFindGroupInWindows(gw->group, ret, count))
				continue;

			if (count == 0)	{
				ret = calloc(1, sizeof(CompWindow));
				ret[0] = w;
			} else {
				ret = realloc(ret, sizeof(CompWindow) * (count + 1));
				ret[count] = w;
			}

			count++;
		}
	}

	(*c) = count;
	return ret;
}

/*
 * groupDeleteSelectionWindow
 *
 */
void groupDeleteSelectionWindow(CompDisplay * d, CompWindow * w)
{
	GROUP_DISPLAY(d);

	if (gd->tmpSel.nWins > 0 && gd->tmpSel.windows) {
		CompWindow **buf = gd->tmpSel.windows;
		gd->tmpSel.windows = (CompWindow **) calloc(gd->tmpSel.nWins - 1, sizeof(CompWindow *));

		int counter = 0;
		int i;
		for (i = 0; i < gd->tmpSel.nWins; i++) {
			if (buf[i]->id == w->id)
				continue;
			gd->tmpSel.windows[counter++] = buf[i];
		}

		gd->tmpSel.nWins = counter;
		free(buf);
	}
}

/*
 * groupAddWindowToSelection
 *
 */
void groupAddWindowToSelection(CompDisplay * d, CompWindow * w)
{
	GROUP_DISPLAY(d);

	gd->tmpSel.windows = (CompWindow **) realloc(gd->tmpSel.windows, sizeof(CompWindow *) * (gd->tmpSel.nWins + 1));

	gd->tmpSel.windows[gd->tmpSel.nWins] = w;
	gd->tmpSel.nWins++;
}

/*
 * groupSelectWindow
 *
 */
void groupSelectWindow(CompDisplay * d, CompWindow * w)
{
	GROUP_DISPLAY(d);
	GROUP_WINDOW(w);

	// select singe window
	if (matchEval(groupGetWindowMatch(w->screen), w) &&
	    !w->invisible && !gw->inSelection && !gw->group) {

		groupAddWindowToSelection(d, w);

		gw->inSelection = TRUE;
		addWindowDamage(w);
	}
	// unselect single window
	else if (matchEval(groupGetWindowMatch(w->screen), w) &&
		 !w->invisible && gw->inSelection && !gw->group) {

		groupDeleteSelectionWindow(d, w);
		gw->inSelection = FALSE;
		addWindowDamage(w);
	}
	// select group
	else if (matchEval(groupGetWindowMatch(w->screen), w) &&
		 !w->invisible && !gw->inSelection && gw->group) {

		int i;
		for (i = 0; i < gw->group->nWins; i++) {
			CompWindow *cw = gw->group->windows[i];
			GROUP_WINDOW(cw);

			groupAddWindowToSelection(d, cw);

			gw->inSelection = TRUE;
			addWindowDamage(cw);
		}
	}
	// Unselect group
	else if (matchEval(groupGetWindowMatch(w->screen), w) &&
		 !w->invisible && gw->inSelection && gw->group) {

		GroupSelection *group = gw->group;
		CompWindow **buf = gd->tmpSel.windows;
		gd->tmpSel.windows = (CompWindow **) calloc(gd->tmpSel.nWins - gw->group->nWins, sizeof(CompWindow *));

		int counter = 0;
		int i;
		for (i = 0; i < gd->tmpSel.nWins; i++) {
			CompWindow *cw = buf[i];
			GROUP_WINDOW(cw);

			if (gw->group == group) {
				gw->inSelection = FALSE;
				addWindowDamage(cw);
				continue;
			}

			gd->tmpSel.windows[counter++] = buf[i];
		}
		gd->tmpSel.nWins = counter;
		free(buf);
	}

}

/*
 * groupSelectSingle
 *
 */
Bool
groupSelectSingle(CompDisplay * d, CompAction * action,
		  CompActionState state, CompOption * option, int nOption)
{
	CompWindow *w = (CompWindow *) findWindowAtDisplay(d, d->activeWindow);

	if (w)
		groupSelectWindow(d, w);

	return TRUE;
}

/*
 * groupSelect
 *
 */
Bool
groupSelect(CompDisplay * d, CompAction * action, CompActionState state,
	    CompOption * option, int nOption)
{
	CompWindow *w = (CompWindow *) findWindowAtDisplay(d, d->activeWindow);
	if (!w)
		return FALSE;

	GROUP_SCREEN(w->screen);

	if (gs->grabState == ScreenGrabNone) {
		groupGrabScreen(w->screen, ScreenGrabSelect);

		if (state & CompActionStateInitKey)
			action->state |= CompActionStateTermKey;

		if (state & CompActionStateInitButton)
			action->state |= CompActionStateTermButton;

		gs->x1 = gs->x2 = pointerX;
		gs->y1 = gs->y2 = pointerY;
	}

	return TRUE;
}

/*
 * groupSelectTerminate
 *
 */
Bool
groupSelectTerminate(CompDisplay * d, CompAction * action,
		     CompActionState state, CompOption * option,
		     int nOption)
{
	CompScreen *s;
	Window xid;

	xid = getIntOptionNamed(option, nOption, "root", 0);

	for (s = d->screens; s; s = s->next) {
		if (xid && s->root != xid)
			continue;
		break;
	}

	if (s) {
		GROUP_SCREEN(s);

		if (gs->grabState == ScreenGrabSelect) {
			groupGrabScreen(s, ScreenGrabNone);

			if (gs->x1 != gs->x2 && gs->y1 != gs->y2) {
				Region reg = XCreateRegion();
				XRectangle rect;

				rect.x = MIN(gs->x1, gs->x2) - 2;
				rect.y = MIN(gs->y1, gs->y2) - 2;
				rect.width  = MAX(gs->x1, gs->x2) - MIN(gs->x1, gs->x2) + 4;
				rect.height = MAX(gs->y1, gs->y2) - MIN(gs->y1, gs->y2) + 4;
				XUnionRectWithRegion(&rect, reg, reg);

				damageScreenRegion(s, reg);

				int count;
				CompWindow **ws = groupFindWindowsInRegion(s, reg, &count);

				if (ws) {
					// select windows
					int i;
					for (i = 0; i < count; i++) {
						CompWindow *cw = ws[i];

						groupSelectWindow(d, cw);
					}
					if (groupGetAutoGroup(s)) {
						groupGroupWindows(d, NULL, 0, NULL, 0);
					}
					free(ws);
				}

				XDestroyRegion(reg);
			}
		}

	}

	action->state &=
	    ~(CompActionStateTermKey | CompActionStateTermButton);

	return FALSE;
}

/*
 * groupDamageSelectionRect
 *
 */
void
groupDamageSelectionRect(CompScreen* s, int xRoot, int yRoot)
{
	GROUP_SCREEN(s);
	REGION reg;

	reg.rects = &reg.extents;
	reg.numRects = 1;

	reg.extents.x1 = MIN(gs->x1, gs->x2) - 5;
	reg.extents.y1 = MIN(gs->y1, gs->y2) - 5;
	reg.extents.x2 = MAX(gs->x1, gs->x2) + 5;
	reg.extents.y2 = MAX(gs->y1, gs->y2) + 5;
	damageScreenRegion(s, &reg);

	gs->x2 = xRoot;
	gs->y2 = yRoot;

	reg.extents.x1 = MIN(gs->x1, gs->x2) - 5;
	reg.extents.y1 = MIN(gs->y1, gs->y2) - 5;
	reg.extents.x2 = MAX(gs->x1, gs->x2) + 5;
	reg.extents.y2 = MAX(gs->y1, gs->y2) + 5;
	damageScreenRegion(s, &reg);
}
