/**
 *
 * Compiz group plugin
 *
 * paint.c
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

/*
 * groupPaintThumb - taken from switcher and modified for tab bar
 *
 */
static void
groupPaintThumb (GroupSelection      *group,
				 GroupTabBarSlot     *slot,
				 const CompTransform *transform,
				 int                 targetOpacity)
{
	CompWindow            *w = slot->window;
	AddWindowGeometryProc oldAddWindowGeometry;
	WindowPaintAttrib     wAttrib = w->paint;
	int                   tw, th;

	tw = slot->region->extents.x2 - slot->region->extents.x1;
	th = slot->region->extents.y2 - slot->region->extents.y1;

	/* Wrap drawWindowGeometry to make sure the general
	   drawWindowGeometry function is used */
	oldAddWindowGeometry = w->screen->addWindowGeometry;
	w->screen->addWindowGeometry = addWindowGeometry;

	/* animate fade */
	if (group && group->tabBar->state == PaintFadeIn)
	{
		wAttrib.opacity -= wAttrib.opacity * group->tabBar->animationTime /
			               (groupGetFadeTime (w->screen) * 1000);
	}
	else if (group && group->tabBar->state == PaintFadeOut)
	{
		wAttrib.opacity = wAttrib.opacity * group->tabBar->animationTime /
				          (groupGetFadeTime (w->screen) * 1000);
	}

	wAttrib.opacity = wAttrib.opacity * targetOpacity / OPAQUE;

	if (w->mapNum)
	{
		FragmentAttrib fragment;
		CompTransform  wTransform = *transform;
		int            width, height;
		int            vx, vy;

		width = w->width + w->output.left + w->output.right;
		height = w->height + w->output.top + w->output.bottom;

		if (width > tw)
			wAttrib.xScale = (float) tw / width;
		else
			wAttrib.xScale = 1.0f;
		if (height > th)
			wAttrib.yScale = (float) tw / height;
		else
			wAttrib.yScale = 1.0f;

		if (wAttrib.xScale < wAttrib.yScale)
			wAttrib.yScale = wAttrib.xScale;
		else
			wAttrib.xScale = wAttrib.yScale;

		/* FIXME: do some more work on the highlight on hover feature
		// Highlight on hover
		if (group && group->tabBar && group->tabBar->hoveredSlot == slot) {
			wAttrib.saturation = 0;
			wAttrib.brightness /= 1.25f;
		}*/

		groupGetDrawOffsetForSlot (slot, &vx, &vy);

		wAttrib.xTranslate = (slot->region->extents.x1 +
							  slot->region->extents.x2) / 2 + vx;
		wAttrib.yTranslate = slot->region->extents.y1 + vy;

		initFragmentAttrib (&fragment, &wAttrib);

		matrixTranslate (&wTransform,
						 wAttrib.xTranslate, wAttrib.yTranslate, 0.0f);
		matrixScale (&wTransform, wAttrib.xScale, wAttrib.yScale, 1.0f);
		matrixTranslate (&wTransform, -(WIN_X (w) + WIN_WIDTH (w) / 2),
						 -(WIN_Y (w) - w->output.top), 0.0f);

		glPushMatrix ();
		glLoadMatrixf (wTransform.m);

		(*w->screen->drawWindow) (w, &wTransform, &fragment, &infiniteRegion,
								  PAINT_WINDOW_TRANSFORMED_MASK |
								  PAINT_WINDOW_TRANSLUCENT_MASK);

		glPopMatrix ();
	}

	w->screen->addWindowGeometry = oldAddWindowGeometry;
}

/*
 * groupRenderTopTabHighlight
 *
 */
void
groupRenderTopTabHighlight (GroupSelection *group)
{
	GroupTabBar     *bar;
	GroupCairoLayer *layer;
	cairo_t         *cr;
	int             width, height;

	if (!group->tabBar || !HAS_TOP_WIN (group) ||
		!group->tabBar->selectionLayer ||
		!group->tabBar->selectionLayer->cairo)
	{
	    return;
	}

	bar = group->tabBar;
	width = group->topTab->region->extents.x2 -
		    group->topTab->region->extents.x1;
	height = group->topTab->region->extents.y2 -
		     group->topTab->region->extents.y1;

	bar->selectionLayer = groupRebuildCairoLayer (group->screen,
												  bar->selectionLayer,
												  width, height);
	if (!bar->selectionLayer)
		return;

	layer = bar->selectionLayer;
	cr = bar->selectionLayer->cairo;

	/* fill */
	cairo_set_line_width (cr, 2);
	cairo_set_source_rgba (cr,
						   (group->color[0] / 65535.0f),
						   (group->color[1] / 65535.0f),
						   (group->color[2] / 65535.0f),
						   (group->color[3] / (65535.0f*2)));

	cairo_move_to (cr, 0, 0);
	cairo_rectangle (cr, 0, 0, width, height);

	cairo_fill_preserve (cr);

	/* outline */
	cairo_set_source_rgba (cr,
						   (group->color[0] / 65535.0f),
						   (group->color[1] / 65535.0f),
						   (group->color[2] / 65535.0f),
						   (group->color[3] / 65535.0f));
	cairo_stroke (cr);

	imageBufferToTexture (group->screen,
						  &layer->texture, (char*) layer->buffer,
						  layer->texWidth, layer->texHeight);
}

/*
 * groupRenderTabBarBackground
 *
 */
void
groupRenderTabBarBackground(GroupSelection *group)
{
	GroupTabBar     *bar;
	GroupCairoLayer *layer;
	cairo_t         *cr;
	int             width, height, radius;
	int             borderWidth;
	float           r, g, b, a;
	double          x0, y0, x1, y1;

	if (!group->tabBar || !HAS_TOP_WIN (group) ||
		!group->tabBar->bgLayer ||
		!group->tabBar->bgLayer->cairo)
	{
	    return;
	}

	bar = group->tabBar;

	width = bar->region->extents.x2 - bar->region->extents.x1;
	height = bar->region->extents.y2 - bar->region->extents.y1;
	radius = groupGetBorderRadius (group->screen);

	if (width > bar->bgLayer->texWidth)
		width = bar->bgLayer->texWidth;

	if (radius > width / 2)
		radius = width / 2;

	layer = bar->bgLayer;
	cr = layer->cairo;

	groupClearCairoLayer (layer);

	borderWidth = groupGetBorderWidth (group->screen);
	cairo_set_line_width (cr, borderWidth);

	cairo_save (cr);

	x0 = borderWidth / 2.0f;
	y0 = borderWidth / 2.0f;
	x1 = width  - borderWidth / 2.0f;
	y1 = height - borderWidth / 2.0f;
	cairo_move_to (cr, x0 + radius, y0);
	cairo_arc (cr, x1 - radius, y0 + radius, radius, M_PI * 1.5, M_PI * 2.0);
	cairo_arc (cr, x1 - radius, y1 - radius, radius, 0.0, M_PI * 0.5);
	cairo_arc (cr, x0 + radius, y1 - radius, radius, M_PI * 0.5, M_PI);
	cairo_arc (cr, x0 + radius, y0 + radius, radius, M_PI, M_PI * 1.5);

	cairo_close_path  (cr);

	switch (groupGetTabStyle (group->screen))
	{
	case TabStyleSimple:
		{
			/* base color */
			r = groupGetTabBaseColorRed (group->screen) / 65535.0f;
			g = groupGetTabBaseColorGreen (group->screen) / 65535.0f;
			b = groupGetTabBaseColorBlue (group->screen) / 65535.0f;
			a = groupGetTabBaseColorAlpha (group->screen) / 65535.0f;
			cairo_set_source_rgba (cr, r, g, b, a);

			cairo_fill_preserve (cr);
			break;
		}

	case TabStyleGradient:
		{
			/* fill */
			cairo_pattern_t *pattern;
			pattern = cairo_pattern_create_linear (0, 0, width, height);

			/* highlight color */
			r = groupGetTabHighlightColorRed (group->screen) / 65535.0f;
			g = groupGetTabHighlightColorGreen (group->screen) / 65535.0f;
			b = groupGetTabHighlightColorBlue (group->screen) / 65535.0f;
			a = groupGetTabHighlightColorAlpha (group->screen) / 65535.0f;
			cairo_pattern_add_color_stop_rgba (pattern, 0.0f, r, g, b, a);

			/* base color */
			r = groupGetTabBaseColorRed (group->screen) / 65535.0f;
			g = groupGetTabBaseColorGreen (group->screen) / 65535.0f;
			b = groupGetTabBaseColorBlue (group->screen) / 65535.0f;
			a = groupGetTabBaseColorAlpha (group->screen) / 65535.0f;
			cairo_pattern_add_color_stop_rgba (pattern, 1.0f, r, g, b, a);

			cairo_set_source (cr, pattern);
			cairo_fill_preserve (cr);
			cairo_pattern_destroy (pattern);
			break;
		}

		case TabStyleGlass:
		{
			cairo_pattern_t *pattern;

			cairo_save (cr);

			/* clip width rounded rectangle */
			cairo_clip (cr);

			/* ===== HIGHLIGHT ===== */

			/* make draw the shape for the highlight and
			   create a pattern for it */
			cairo_rectangle (cr, 0, 0, width, height / 2);
			pattern = cairo_pattern_create_linear (0, 0, 0, height);

			/* highlight color */
			r = groupGetTabHighlightColorRed (group->screen) / 65535.0f;
			g = groupGetTabHighlightColorGreen (group->screen) / 65535.0f;
			b = groupGetTabHighlightColorBlue (group->screen) / 65535.0f;
			a = groupGetTabHighlightColorAlpha (group->screen) / 65535.0f;
			cairo_pattern_add_color_stop_rgba (pattern, 0.0f, r, g, b, a);

			/* base color */
			r = groupGetTabBaseColorRed (group->screen) / 65535.0f;
			g = groupGetTabBaseColorGreen (group->screen) / 65535.0f;
			b = groupGetTabBaseColorBlue (group->screen) / 65535.0f;
			a = groupGetTabBaseColorAlpha (group->screen) / 65535.0f;
			cairo_pattern_add_color_stop_rgba (pattern, 0.6f, r, g, b, a);

			cairo_set_source (cr, pattern);
			cairo_fill (cr);
			cairo_pattern_destroy (pattern);

			/* ==== SHADOW ===== */

			/* make draw the shape for the show and create a pattern for it */
			cairo_rectangle (cr, 0, height / 2, width, height);
			pattern = cairo_pattern_create_linear (0, 0, 0, height);

			/* we don't want to use a full highlight here
			   so we mix the colors */
			r = (groupGetTabHighlightColorRed (group->screen) +
			     groupGetTabBaseColorRed (group->screen)) / (2 * 65535.0f);
			g = (groupGetTabHighlightColorGreen (group->screen) +
			     groupGetTabBaseColorGreen (group->screen)) / (2 * 65535.0f);
			b = (groupGetTabHighlightColorBlue (group->screen) +
			     groupGetTabBaseColorBlue (group->screen)) / (2 * 65535.0f);
			a = (groupGetTabHighlightColorAlpha (group->screen) +
			     groupGetTabBaseColorAlpha (group->screen)) / (2 * 65535.0f);
			cairo_pattern_add_color_stop_rgba (pattern, 1.0f, r, g, b, a);

			/* base color */
			r = groupGetTabBaseColorRed (group->screen) / 65535.0f;
			g = groupGetTabBaseColorGreen (group->screen) / 65535.0f;
			b = groupGetTabBaseColorBlue (group->screen) / 65535.0f;
			a = groupGetTabBaseColorAlpha (group->screen) / 65535.0f;
			cairo_pattern_add_color_stop_rgba (pattern, 0.5f, r, g, b, a);

			cairo_set_source (cr, pattern);
			cairo_fill (cr);
			cairo_pattern_destroy (pattern);

			cairo_restore (cr);

			/* draw shape again for the outline */
			cairo_move_to (cr, x0 + radius, y0);
			cairo_arc (cr, x1 - radius, y0 + radius,
					   radius, M_PI * 1.5, M_PI * 2.0);
			cairo_arc (cr, x1 - radius, y1 - radius, radius, 0.0, M_PI * 0.5);
			cairo_arc (cr, x0 + radius, y1 - radius, radius, M_PI * 0.5, M_PI);
			cairo_arc (cr, x0 + radius, y0 + radius, radius, M_PI, M_PI * 1.5);

			break;
		}

		case TabStyleMetal:
		{
			/* fill */
			cairo_pattern_t *pattern;
			pattern = cairo_pattern_create_linear (0, 0, 0, height);

			/* base color #1 */
			r = groupGetTabBaseColorRed (group->screen) / 65535.0f;
			g = groupGetTabBaseColorGreen (group->screen) / 65535.0f;
			b = groupGetTabBaseColorBlue (group->screen) / 65535.0f;
			a = groupGetTabBaseColorAlpha (group->screen) / 65535.0f;
			cairo_pattern_add_color_stop_rgba (pattern, 0.0f, r, g, b, a);

			/* highlight color */
			r = groupGetTabHighlightColorRed (group->screen) / 65535.0f;
			g = groupGetTabHighlightColorGreen (group->screen) / 65535.0f;
			b = groupGetTabHighlightColorBlue (group->screen) / 65535.0f;
			a = groupGetTabHighlightColorAlpha (group->screen) / 65535.0f;
			cairo_pattern_add_color_stop_rgba (pattern, 0.55f, r, g, b, a);

			/* base color #2 */
			r = groupGetTabBaseColorRed (group->screen) / 65535.0f;
			g = groupGetTabBaseColorGreen (group->screen) / 65535.0f;
			b = groupGetTabBaseColorBlue (group->screen) / 65535.0f;
			a = groupGetTabBaseColorAlpha (group->screen) / 65535.0f;
			cairo_pattern_add_color_stop_rgba (pattern, 1.0f, r, g, b, a);

			cairo_set_source (cr, pattern);
			cairo_fill_preserve (cr);
			cairo_pattern_destroy (pattern);
			break;
		}

		case TabStyleMurrina:
		{
			double ratio, transX;
			cairo_pattern_t *pattern;

			cairo_save (cr);

			/* clip width rounded rectangle */
			cairo_clip_preserve (cr);

			/* ==== TOP ==== */

			x0 = borderWidth / 2.0;
			y0 = borderWidth / 2.0;
			x1 = width  - borderWidth / 2.0;
			y1 = height - borderWidth / 2.0;
			radius = (y1 - y0) / 2;

			/* setup pattern */
			pattern = cairo_pattern_create_linear (0, 0, 0, height);

			/* we don't want to use a full highlight here
			   so we mix the colors */
			r = (groupGetTabHighlightColorRed (group->screen) +
			     groupGetTabBaseColorRed (group->screen)) / (2 * 65535.0f);
			g = (groupGetTabHighlightColorGreen (group->screen) +
			     groupGetTabBaseColorGreen (group->screen)) / (2 * 65535.0f);
			b = (groupGetTabHighlightColorBlue (group->screen) +
			     groupGetTabBaseColorBlue (group->screen)) / (2 * 65535.0f);
			a = (groupGetTabHighlightColorAlpha (group->screen) +
			     groupGetTabBaseColorAlpha (group->screen)) / (2 * 65535.0f);
			cairo_pattern_add_color_stop_rgba (pattern, 0.0f, r, g, b, a);

			/* highlight color */
			r = groupGetTabHighlightColorRed (group->screen) / 65535.0f;
			g = groupGetTabHighlightColorGreen (group->screen) / 65535.0f;
			b = groupGetTabHighlightColorBlue (group->screen) / 65535.0f;
			a = groupGetTabHighlightColorAlpha (group->screen) / 65535.0f;
			cairo_pattern_add_color_stop_rgba (pattern, 1.0f, r, g, b, a);

			cairo_set_source (cr, pattern);

			cairo_fill (cr);
			cairo_pattern_destroy (pattern);

			/* ==== BOTTOM ===== */

			x0 = borderWidth / 2.0;
			y0 = borderWidth / 2.0;
			x1 = width  - borderWidth / 2.0;
			y1 = height - borderWidth / 2.0;
			radius = (y1 - y0) / 2;

			ratio = (double)width / (double)height;
			transX = width - (width * ratio);

			cairo_move_to (cr, x1, y1);
			cairo_line_to (cr, x1, y0);
			if (width < height)
			{
				cairo_translate (cr, transX, 0);
				cairo_scale (cr, ratio, 1.0);
			}
			cairo_arc (cr, x1 - radius, y0, radius, 0.0, M_PI * 0.5);
			if (width < height)
			{
				cairo_scale (cr, 1.0 / ratio, 1.0);
				cairo_translate (cr, -transX, 0);
				cairo_scale (cr, ratio, 1.0);
			}
			cairo_arc_negative (cr, x0 + radius, y1, radius, M_PI * 1.5, M_PI);
			cairo_close_path (cr);

			/* setup pattern */
			pattern = cairo_pattern_create_linear (0, 0, 0, height);

			/* base color */
			r = groupGetTabBaseColorRed (group->screen) / 65535.0f;
			g = groupGetTabBaseColorGreen (group->screen) / 65535.0f;
			b = groupGetTabBaseColorBlue (group->screen) / 65535.0f;
			a = groupGetTabBaseColorAlpha (group->screen) / 65535.0f;
			cairo_pattern_add_color_stop_rgba (pattern, 0.0f, r, g, b, a);

			/* we don't want to use a full highlight here
			   so we mix the colors */
			r = (groupGetTabHighlightColorRed (group->screen) +
			     groupGetTabBaseColorRed (group->screen)) / (2 * 65535.0f);
			g = (groupGetTabHighlightColorGreen (group->screen) +
			     groupGetTabBaseColorGreen (group->screen)) / (2 * 65535.0f);
			b = (groupGetTabHighlightColorBlue (group->screen) +
			     groupGetTabBaseColorBlue (group->screen)) / (2 * 65535.0f);
			a = (groupGetTabHighlightColorAlpha (group->screen) +
			     groupGetTabBaseColorAlpha (group->screen)) / (2 * 65535.0f);
			cairo_pattern_add_color_stop_rgba (pattern, 1.0f, r, g, b, a);

			cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
			cairo_set_source (cr, pattern);
			cairo_fill (cr);
			cairo_pattern_destroy (pattern);
			cairo_set_operator (cr, CAIRO_OPERATOR_OVER);

			cairo_restore (cr);

			/* draw shape again for the outline */
			x0 = borderWidth / 2.0;
			y0 = borderWidth / 2.0;
			x1 = width  - borderWidth / 2.0;
			y1 = height - borderWidth / 2.0;
			radius = groupGetBorderRadius (group->screen);
			cairo_move_to (cr, x0 + radius, y0);
			cairo_arc (cr, x1 - radius, y0 + radius,
					   radius, M_PI * 1.5, M_PI * 2.0);
			cairo_arc (cr, x1 - radius, y1 - radius, radius, 0.0, M_PI * 0.5);
			cairo_arc (cr, x0 + radius, y1 - radius, radius, M_PI * 0.5, M_PI);
			cairo_arc (cr, x0 + radius, y0 + radius, radius, M_PI, M_PI * 1.5);

			break;
		}

		default:
			break;
	}

	/* outline */
	r = groupGetTabBorderColorRed (group->screen) / 65535.0f;
	g = groupGetTabBorderColorGreen (group->screen) / 65535.0f;
	b = groupGetTabBorderColorBlue (group->screen) / 65535.0f;
	a = groupGetTabBorderColorAlpha (group->screen) / 65535.0f;
	cairo_set_source_rgba (cr, r, g, b, a);
	if (bar->bgAnimation != AnimationNone)
		cairo_stroke_preserve (cr);
	else
		cairo_stroke (cr);

	switch (bar->bgAnimation)
	{
	case AnimationPulse:
		{
			double animationProgress;
			double alpha;
			
			animationProgress = bar->bgAnimationTime /
		 		                (groupGetPulseTime (group->screen) * 1000.0);
			alpha = fabs (sin (PI * animationProgress)) * 0.75;
			if (alpha <= 0)
				break;

			cairo_save (cr);
			cairo_clip (cr);
			cairo_set_operator (cr, CAIRO_OPERATOR_XOR);
			cairo_rectangle (cr, 0.0, 0.0, width, height);
			cairo_set_source_rgba (cr, 1.0, 1.0, 1.0, alpha);
			cairo_fill (cr);
			cairo_restore (cr);
			break;
		}

		case AnimationReflex:
		{
			double          animationProgress;
			double          reflexWidth;
			double          posX, alpha;
			cairo_pattern_t *pattern;

			animationProgress = bar->bgAnimationTime /
				                (groupGetReflexTime (group->screen) * 1000.0);
			reflexWidth = (bar->nSlots / 2.0) * 30;
			posX = (width + reflexWidth * 2.0) * animationProgress;
			alpha = sin (PI * animationProgress) * 0.55;
			if (alpha <= 0)
				break;

			cairo_save (cr);
			cairo_clip (cr);
			pattern = cairo_pattern_create_linear (posX - reflexWidth,
												   0.0, posX, height);
			cairo_pattern_add_color_stop_rgba (pattern,
											   0.0f, 1.0, 1.0, 1.0, 0.0);
			cairo_pattern_add_color_stop_rgba (pattern,
											   0.5f, 1.0, 1.0, 1.0, alpha);
			cairo_pattern_add_color_stop_rgba (pattern,
											   1.0f, 1.0, 1.0, 1.0, 0.0);
			cairo_rectangle (cr, 0.0, 0.0, width, height);
			cairo_set_source (cr, pattern);
			cairo_fill (cr);
			cairo_restore (cr);
			break;
		}

		case AnimationNone:
		default:
			break;
	}

	cairo_restore (cr);
	imageBufferToTexture (group->screen,
						  &layer->texture, (char*) layer->buffer,
						  layer->texWidth, layer->texHeight);
}

/*
 * groupRenderWindowTitle
 *
 */
void
groupRenderWindowTitle (GroupSelection *group)
{
	GroupTabBar     *bar;
	GroupCairoLayer *layer;
	void            *data = NULL;
	int             width, height;
	int             stride;
	CompTextAttrib  textAttrib;
	CompDisplay     *d;

	if (!group->tabBar || !HAS_TOP_WIN(group) || !group->tabBar->textLayer)
	    return;

	bar = group->tabBar;

	width = bar->region->extents.x2 - bar->region->extents.x1;
	height = bar->region->extents.y2 - bar->region->extents.y1;

	bar->textLayer = groupRebuildCairoLayer (group->screen,
											 bar->textLayer, width, height);
	layer = bar->textLayer;
	if (!layer)
		return;

	textAttrib.family = "Sans";
	textAttrib.size = groupGetTabbarFontSize (group->screen);
	textAttrib.style = TEXT_STYLE_BOLD;
	textAttrib.color[0] = groupGetTabbarFontColorRed (group->screen);
	textAttrib.color[1] = groupGetTabbarFontColorGreen (group->screen);
	textAttrib.color[2] = groupGetTabbarFontColorBlue (group->screen);
	textAttrib.color[3] = groupGetTabbarFontColorAlpha (group->screen);
	textAttrib.ellipsize = TRUE;

	textAttrib.maxwidth = width;
	textAttrib.maxheight = height;
	textAttrib.screen = group->screen;
	textAttrib.renderMode = TextRenderWindowTitle;

	if (bar->textSlot && bar->textSlot->window)
		textAttrib.data = (void*) bar->textSlot->window->id;
	else
		textAttrib.data = 0;

	d = group->screen->display;
	if (!((*d->fileToImage) (d, TEXT_ID, (const char*) &textAttrib,
							 &width, &height, &stride, &data)))
	{
		/* getting the pixmap failed, so create an empty one */
		Pixmap emptyPixmap;
		emptyPixmap = XCreatePixmap (d->display, group->screen->root,
									 width, height, 32);

		if (emptyPixmap)
		{
			XGCValues gcv;
			GC        gc;

			gcv.foreground = 0x00000000;
			gcv.plane_mask = 0xffffffff;

			gc = XCreateGC(d->display, emptyPixmap,
						   GCForeground, &gcv);

			XFillRectangle (d->display, emptyPixmap, gc,
							0, 0, width, height);

			XFreeGC (d->display, gc);

			data = (void*) emptyPixmap;
		}
	}

	layer->texWidth = width;
	layer->texHeight = height;

	if (data)
		bindPixmapToTexture (group->screen, &layer->texture, (Pixmap) data,
							 layer->texWidth, layer->texHeight, 32);
}

/*
 * groupPaintTabBar
 *
 */
static void
groupPaintTabBar (GroupSelection          *group,
				  const WindowPaintAttrib *wAttrib,
				  const CompTransform     *transform,
	 			  unsigned int            mask,
				  Region                  clipRegion)
{
	CompWindow      *topTab = TOP_TAB (group);
	CompScreen      *s = group->screen;
	GroupTabBar     *bar = group->tabBar;
	GroupTabBarSlot *slot;
	int             i, alpha;
	float           wScale, hScale;
	GroupCairoLayer *layer;
	REGION          box;

	if (!group || !HAS_TOP_WIN (group) || !group->tabBar ||
		(group->tabBar->state == PaintOff))
	{
		return;
	}

	GROUP_SCREEN (s);

#define PAINT_BG     0
#define PAINT_SEL    1
#define PAINT_THUMBS 2
#define PAINT_TEXT   3
#define PAINT_MAX    4

	box.rects = &box.extents;
	box.numRects = 1;

	for (i = 0; i < PAINT_MAX; i++)
	{
		alpha = OPAQUE;

		if (bar->state == PaintFadeIn)
			alpha -= alpha * bar->animationTime /
				     (groupGetFadeTime (s) * 1000);
		else if (bar->state == PaintFadeOut)
			alpha = alpha * bar->animationTime /
				    (groupGetFadeTime (s) * 1000);

		wScale = hScale = 1.0f;
		layer = NULL;

		switch (i) {
		case PAINT_BG:
			{
				int newWidth;

				layer = bar->bgLayer;

				/* handle the repaint of the background */
				newWidth = bar->region->extents.x2 - bar->region->extents.x1;

				if (layer && (newWidth > layer->texWidth))
					newWidth = layer->texWidth;

				wScale = (double) (bar->region->extents.x2 -
								   bar->region->extents.x1) / (double) newWidth;

				/* FIXME: maybe move this over to groupResizeTabBarRegion -
				   the only problem is that we would have 2 redraws if
				   there is an animation */
				if (newWidth != bar->oldWidth || bar->bgAnimation)
					groupRenderTabBarBackground (group);

				bar->oldWidth = newWidth;
				box.extents = bar->region->extents;
				break;
			}

		case PAINT_SEL:
			if (group->topTab != gs->draggedSlot)
			{
				layer = bar->selectionLayer;
				box.extents = group->topTab->region->extents;
			}
			break;

		case PAINT_THUMBS:
			{
				GLenum oldTextureFilter;
				oldTextureFilter = s->display->textureFilter;

				if (groupGetMipmaps (s))
					s->display->textureFilter = GL_LINEAR_MIPMAP_LINEAR;

				for (slot = bar->slots; slot; slot = slot->next)
				{
					if (slot != gs->draggedSlot || !gs->dragged)
						groupPaintThumb (group, slot, transform,
										 wAttrib->opacity);
				}

				s->display->textureFilter = oldTextureFilter;
				break;
			}

		case PAINT_TEXT:
			if (bar->textLayer && (bar->textLayer->state != PaintOff))
			{
				layer = bar->textLayer;

				box.extents.x1 = bar->region->extents.x1 + 5;
				box.extents.x2 = bar->region->extents.x1 +
					             bar->textLayer->texWidth + 5;
				box.extents.y1 = bar->region->extents.y2 -
					             bar->textLayer->texHeight - 5;
				box.extents.y2 = bar->region->extents.y2 - 5;

				if (box.extents.x2 > bar->region->extents.x2)
					box.extents.x2 = bar->region->extents.x2;

				/* recalculate the alpha again for text fade... */
				if (layer->state == PaintFadeIn)
					alpha -= alpha * layer->animationTime /
						     (groupGetFadeTextTime(s) * 1000);
				else if (layer->state == PaintFadeOut)
					alpha = alpha * layer->animationTime /
						    (groupGetFadeTextTime(s) * 1000);
			}
			break;
		}

		if (layer)
		{
			CompMatrix matrix = layer->texture.matrix;

			/* remove the old x1 and y1 so we have a relative value */
			box.extents.x2 -= box.extents.x1;
			box.extents.y2 -= box.extents.y1;
			box.extents.x1 = (box.extents.x1 - topTab->attrib.x) / wScale +
				             topTab->attrib.x;
			box.extents.y1 = (box.extents.y1 - topTab->attrib.y) / hScale +
				             topTab->attrib.y;

			/* now add the new x1 and y1 so we have a absolute value again,
			   also we don't want to stretch the texture... */
			if (box.extents.x2 * wScale < layer->texWidth)
				box.extents.x2 += box.extents.x1;
			else
				box.extents.x2 = box.extents.x1 + layer->texWidth;

			if (box.extents.y2 * hScale < layer->texHeight)
				box.extents.y2 += box.extents.y1;
			else
				box.extents.y2 = box.extents.y1 + layer->texHeight;

			matrix.x0 -= box.extents.x1 * matrix.xx;
			matrix.y0 -= box.extents.y1 * matrix.yy;
			topTab->vCount = topTab->indexCount = 0;

			addWindowGeometry (topTab, &matrix, 1, &box, clipRegion);

			if (topTab->vCount)
			{
				FragmentAttrib fragment;
				CompTransform  wTransform = *transform;

				matrixTranslate (&wTransform,
								 WIN_X (topTab), WIN_Y (topTab), 0.0f);
				matrixScale (&wTransform, wScale, hScale, 1.0f);
				matrixTranslate (&wTransform,
								 wAttrib->xTranslate / wScale - WIN_X (topTab),
								 wAttrib->yTranslate / hScale - WIN_Y (topTab),
								 0.0f);

				glPushMatrix ();
				glLoadMatrixf (wTransform.m);

				alpha = alpha * ((float)wAttrib->opacity / OPAQUE);

				initFragmentAttrib (&fragment, wAttrib);
				fragment.opacity = alpha;

				(*s->drawWindowTexture) (topTab, &layer->texture,
								 		 &fragment, mask |
										 PAINT_WINDOW_BLEND_MASK |
										 PAINT_WINDOW_TRANSFORMED_MASK |
										 PAINT_WINDOW_TRANSLUCENT_MASK);

				glPopMatrix ();
			}
		}
	}
}

/*
 * groupPaintSelectionOutline
 *
 */
static void
groupPaintSelectionOutline (CompScreen              *s,
							const ScreenPaintAttrib *sa,
							const CompTransform     *transform,
							CompOutput              *output,
							Bool                    transformed)
{
	int x1, x2, y1, y2;

	GROUP_SCREEN (s);

	x1 = MIN (gs->x1, gs->x2);
	y1 = MIN (gs->y1, gs->y2);
	x2 = MAX (gs->x1, gs->x2);
	y2 = MAX (gs->y1, gs->y2);

	if (gs->grabState == ScreenGrabSelect)
	{
		CompTransform sTransform = *transform;

		if (transformed)
		{
			(*s->applyScreenTransform) (s, sa, output, &sTransform);
			transformToScreenSpace (s, output, -sa->zTranslate, &sTransform);
		} else
			transformToScreenSpace (s, output, -DEFAULT_Z_CAMERA, &sTransform);

		glPushMatrix ();
		glLoadMatrixf (sTransform.m);

		glDisableClientState (GL_TEXTURE_COORD_ARRAY);
		glEnable (GL_BLEND);

		glColor4usv (groupGetFillColorOption (s)->value.c);
		glRecti (x1, y2, x2, y1);

		glLineWidth (3);
		glEnable (GL_LINE_SMOOTH);
		glColor4usv (groupGetLineColorOption (s)->value.c);
		glBegin (GL_LINE_LOOP);
		glVertex2i (x1, y1);
		glVertex2i (x2, y1);
		glVertex2i (x2, y2);
		glVertex2i (x1, y2);
		glEnd ();
		glDisable (GL_LINE_SMOOTH);
		glLineWidth (1); /* back to default */

		glColor4usv (defaultColor);
		glDisable (GL_BLEND);
		glEnableClientState (GL_TEXTURE_COORD_ARRAY);
		glPopMatrix ();
	}
}

/*
 * groupPreparePaintScreen
 *
 */
void
groupPreparePaintScreen (CompScreen *s,
						 int        msSinceLastPaint)
{
	GroupSelection *group;

	GROUP_SCREEN (s);

	UNWRAP (gs, s, preparePaintScreen);
	(*s->preparePaintScreen) (s, msSinceLastPaint);
	WRAP (gs, s, preparePaintScreen, groupPreparePaintScreen);

	for (group = gs->groups; group; group = group->next)
	{
		GroupTabBar *bar = group->tabBar;

		if (group->changeState != PaintOff)
			group->changeAnimationTime -= msSinceLastPaint;

		if (!bar)
			continue;

		groupApplyForces (s, bar, (gs->dragged) ? gs->draggedSlot: NULL);
		groupApplySpeeds (s, group, msSinceLastPaint);

		groupHandleHoverDetection (group);
		groupHandleTabBarFade (group, msSinceLastPaint);
		groupHandleTextFade (group, msSinceLastPaint);
		groupHandleTabBarAnimation (group, msSinceLastPaint);
	}

	groupHandleScreenActions (s);

	groupHandleChanges (s);
	groupDrawTabAnimation (s, msSinceLastPaint);

	groupDequeueMoveNotifies (s);
	groupDequeueGrabNotifies (s);
	groupDequeueUngrabNotifies (s);
}

/*
 * groupPaintOutput
 *
 */
Bool
groupPaintOutput (CompScreen              *s,
				  const ScreenPaintAttrib *sAttrib,
				  const CompTransform     *transform,
				  Region                  region,
				  CompOutput              *output,
				  unsigned int            mask)
{
	GroupSelection *group;
	Bool           status;

	GROUP_SCREEN (s);
	GROUP_DISPLAY (s->display);

	gs->painted = FALSE;
	gs->vpX = s->x;
	gs->vpY = s->y;

	for (group = gs->groups; group; group = group->next)
	{
		if (group->changeState != PaintOff ||
			group->tabbingState != PaintOff)
		{
			mask |= PAINT_SCREEN_WITH_TRANSFORMED_WINDOWS_MASK;
		}
	}

	if (gs->tabBarVisible || gd->resizeInfo)
		mask |= PAINT_SCREEN_WITH_TRANSFORMED_WINDOWS_MASK;

	UNWRAP (gs, s, paintOutput);
	status = (*s->paintOutput) (s, sAttrib, transform, region, output, mask);
	WRAP (gs, s, paintOutput, groupPaintOutput);

	if (status && !gs->painted)
	{
		if ((gs->grabState == ScreenGrabTabDrag) && gs->draggedSlot)
		{
			CompTransform wTransform = *transform;
			PaintState state;

			GROUP_WINDOW (gs->draggedSlot->window);

			transformToScreenSpace (s, output, -DEFAULT_Z_CAMERA, &wTransform);

			glPushMatrix ();
			glLoadMatrixf (wTransform.m);

			/* prevent tab bar drawing.. */
			state = gw->group->tabBar->state;
			gw->group->tabBar->state = PaintOff;
			groupPaintThumb (NULL, gs->draggedSlot, &wTransform, OPAQUE);
			gw->group->tabBar->state = state;

			glPopMatrix ();
		}
		else  if (gs->grabState == ScreenGrabSelect)
		{
			groupPaintSelectionOutline (s, sAttrib, transform, output, FALSE);
		}
	}

	return status;
}

/*
 * groupaintTransformedOutput
 *
 */
void
groupPaintTransformedOutput (CompScreen              *s,
							 const ScreenPaintAttrib *sa,
							 const CompTransform     *transform,
							 Region                  region,
							 CompOutput              *output,
							 unsigned int            mask)
{
	GROUP_SCREEN (s);

	UNWRAP (gs, s, paintTransformedOutput);
	(*s->paintTransformedOutput) (s, sa, transform, region, output, mask);
	WRAP (gs, s, paintTransformedOutput, groupPaintTransformedOutput);

	if ((gs->vpX == s->x) && (gs->vpY == s->y))
	{
		gs->painted = TRUE;

		if ((gs->grabState == ScreenGrabTabDrag) &&
			gs->draggedSlot && gs->dragged)
		{
			CompTransform wTransform = *transform;

			(*s->applyScreenTransform) (s, sa, output, &wTransform);
			transformToScreenSpace (s, output, -sa->zTranslate, &wTransform);
			glPushMatrix ();
			glLoadMatrixf (wTransform.m);

			groupPaintThumb (NULL, gs->draggedSlot, &wTransform, OPAQUE);

			glPopMatrix ();
		}
		else if (gs->grabState == ScreenGrabSelect)
		{
			groupPaintSelectionOutline (s, sa, transform, output, TRUE);
		}
	}
}

void
groupRecomputeGlow (CompScreen *s)
{
	CompWindow *w;
	
	GROUP_SCREEN (s);

	for (w = s->windows; w; w = w->next)
		groupComputeGlowQuads (w, &gs->glowTexture.matrix);
}

/*
 * groupDonePaintScreen
 *
 */
void
groupDonePaintScreen (CompScreen *s)
{
	GroupSelection *group;

	GROUP_SCREEN (s);

	UNWRAP (gs, s, donePaintScreen);
	(*s->donePaintScreen) (s);
	WRAP (gs, s, donePaintScreen, groupDonePaintScreen);

	for (group = gs->groups; group; group = group->next)
	{
		if (group->doTabbing)
			damageScreen (s);

		if (group->changeState != PaintOff)
			damageScreen (s);

		if (group->tabBar)
		{
			Bool needDamage = FALSE;

			if ((group->tabBar->state == PaintFadeIn) ||
				(group->tabBar->state == PaintFadeOut))
			{
				needDamage = TRUE;
			}

			if (group->tabBar->textLayer)
			{
				if ((group->tabBar->textLayer->state == PaintFadeIn) ||
					(group->tabBar->textLayer->state == PaintFadeOut))
				{
					needDamage = TRUE;
				}
			}

			if (group->tabBar->bgAnimation)
				needDamage = TRUE;

			if (gs->draggedSlot)
				needDamage = TRUE;

			if (needDamage)
				groupDamageTabBarRegion (group);
		}
	}
}

void
groupComputeGlowQuads (CompWindow *w,
					   CompMatrix *matrix)
{
	BoxRec            *box;
	CompMatrix        *quadMatrix;
	int               glowSize, glowOffset;
	GroupGlowTypeEnum glowType;

	GROUP_WINDOW (w);

	if (groupGetGlow (w->screen) && matrix)
	{
		if (!gw->glowQuads)
			gw->glowQuads = malloc (NUM_GLOWQUADS * sizeof (GlowQuad));
		if (!gw->glowQuads)
			return;
	}
	else
	{
		if (gw->glowQuads)
		{
			free (gw->glowQuads);
			gw->glowQuads = NULL;
		}
		return;
	}

	GROUP_DISPLAY (w->screen->display);

	glowSize = groupGetGlowSize (w->screen);
	glowType = groupGetGlowType (w->screen);
	glowOffset = (glowSize * gd->glowTextureProperties[glowType].glowOffset /
				  gd->glowTextureProperties[glowType].textureSize) + 1;

	/* Top left corner */
	box = &gw->glowQuads[GLOWQUAD_TOPLEFT].box;
	gw->glowQuads[GLOWQUAD_TOPLEFT].matrix = *matrix;
	quadMatrix = &gw->glowQuads[GLOWQUAD_TOPLEFT].matrix;

	box->x1 = WIN_REAL_X (w) - glowSize + glowOffset;
	box->y1 = WIN_REAL_Y (w) - glowSize + glowOffset;
	box->x2 = WIN_REAL_X (w) + glowOffset;
	box->y2 = WIN_REAL_Y (w) + glowOffset;

	quadMatrix->xx = 1.0f / glowSize;
	quadMatrix->yy = -1.0f / glowSize;
	quadMatrix->x0 = -(box->x1 * quadMatrix->xx);
	quadMatrix->y0 = 1.0 -(box->y1 * quadMatrix->yy);

	box->x2 = MIN (WIN_REAL_X (w) + glowOffset,
				   WIN_REAL_X (w) + (WIN_REAL_WIDTH (w) / 2));
	box->y2 = MIN (WIN_REAL_Y (w) + glowOffset,
				   WIN_REAL_Y (w) + (WIN_REAL_HEIGHT (w) / 2));

	/* Top right corner */
	box = &gw->glowQuads[GLOWQUAD_TOPRIGHT].box;
	gw->glowQuads[GLOWQUAD_TOPRIGHT].matrix = *matrix;
	quadMatrix = &gw->glowQuads[GLOWQUAD_TOPRIGHT].matrix;

	box->x1 = WIN_REAL_X (w) + WIN_REAL_WIDTH (w) - glowOffset;
	box->y1 = WIN_REAL_Y (w) - glowSize + glowOffset;
	box->x2 = WIN_REAL_X (w) + WIN_REAL_WIDTH (w) + glowSize - glowOffset;
	box->y2 = WIN_REAL_Y (w) + glowOffset;

	quadMatrix->xx = -1.0f / glowSize;
	quadMatrix->yy = -1.0f / glowSize;
	quadMatrix->x0 = 1.0 - (box->x1 * quadMatrix->xx);
	quadMatrix->y0 = 1.0 - (box->y1 * quadMatrix->yy);

	box->x1 = MAX (WIN_REAL_X (w) + WIN_REAL_WIDTH (w) - glowOffset,
				   WIN_REAL_X (w) + (WIN_REAL_WIDTH (w) / 2));
	box->y2 = MIN (WIN_REAL_Y (w) + glowOffset,
				   WIN_REAL_Y (w) + (WIN_REAL_HEIGHT (w) / 2));

	/* Bottom left corner */
	box = &gw->glowQuads[GLOWQUAD_BOTTOMLEFT].box;
	gw->glowQuads[GLOWQUAD_BOTTOMLEFT].matrix = *matrix;
	quadMatrix = &gw->glowQuads[GLOWQUAD_BOTTOMLEFT].matrix;

	box->x1 = WIN_REAL_X (w) - glowSize + glowOffset;
	box->y1 = WIN_REAL_Y (w) + WIN_REAL_HEIGHT (w) - glowOffset;
	box->x2 = WIN_REAL_X (w) + glowOffset;
	box->y2 = WIN_REAL_Y (w) + WIN_REAL_HEIGHT (w) + glowSize - glowOffset;

	quadMatrix->xx = 1.0f / glowSize;
	quadMatrix->yy = 1.0f / glowSize;
	quadMatrix->x0 = -(box->x1 * quadMatrix->xx);
	quadMatrix->y0 = -(box->y1 * quadMatrix->yy);

	box->y1 = MAX (WIN_REAL_Y (w) + WIN_REAL_HEIGHT (w) - glowOffset,
				   WIN_REAL_Y (w) + (WIN_REAL_HEIGHT (w) / 2));
	box->x2 = MIN (WIN_REAL_X (w) + glowOffset,
				   WIN_REAL_X (w) + (WIN_REAL_WIDTH (w) / 2));

	/* Bottom right corner */
	box = &gw->glowQuads[GLOWQUAD_BOTTOMRIGHT].box;
	gw->glowQuads[GLOWQUAD_BOTTOMRIGHT].matrix = *matrix;
	quadMatrix = &gw->glowQuads[GLOWQUAD_BOTTOMRIGHT].matrix;

	box->x1 = WIN_REAL_X (w) + WIN_REAL_WIDTH (w) - glowOffset;
	box->y1 = WIN_REAL_Y (w) + WIN_REAL_HEIGHT (w) - glowOffset;
	box->x2 = WIN_REAL_X (w) + WIN_REAL_WIDTH (w) + glowSize - glowOffset;
	box->y2 = WIN_REAL_Y (w) + WIN_REAL_HEIGHT (w) + glowSize - glowOffset;

	quadMatrix->xx = -1.0f / glowSize;
	quadMatrix->yy = 1.0f / glowSize;
	quadMatrix->x0 = 1.0 - (box->x1 * quadMatrix->xx);
	quadMatrix->y0 = -(box->y1 * quadMatrix->yy);

	box->x1 = MAX (WIN_REAL_X (w) + WIN_REAL_WIDTH (w) - glowOffset,
				   WIN_REAL_X (w) + (WIN_REAL_WIDTH (w) / 2));
	box->y1 = MAX (WIN_REAL_Y (w) + WIN_REAL_HEIGHT (w) - glowOffset,
				   WIN_REAL_Y (w) + (WIN_REAL_HEIGHT (w) / 2));

	/* Top edge */
	box = &gw->glowQuads[GLOWQUAD_TOP].box;
	gw->glowQuads[GLOWQUAD_TOP].matrix = *matrix;
	quadMatrix = &gw->glowQuads[GLOWQUAD_TOP].matrix;

	box->x1 = WIN_REAL_X (w) + glowOffset;
	box->y1 = WIN_REAL_Y (w) - glowSize + glowOffset;
	box->x2 = WIN_REAL_X (w) + WIN_REAL_WIDTH (w) - glowOffset;
	box->y2 = WIN_REAL_Y (w) + glowOffset;

	quadMatrix->xx = 0.0f;
	quadMatrix->yy = -1.0f / glowSize;
	quadMatrix->x0 = 1.0;
	quadMatrix->y0 = 1.0 - (box->y1 * quadMatrix->yy);

	/* Bottom edge */
	box = &gw->glowQuads[GLOWQUAD_BOTTOM].box;
	gw->glowQuads[GLOWQUAD_BOTTOM].matrix = *matrix;
	quadMatrix = &gw->glowQuads[GLOWQUAD_BOTTOM].matrix;

	box->x1 = WIN_REAL_X (w) + glowOffset;
	box->y1 = WIN_REAL_Y (w) + WIN_REAL_HEIGHT (w) - glowOffset;
	box->x2 = WIN_REAL_X (w) + WIN_REAL_WIDTH (w) - glowOffset;
	box->y2 = WIN_REAL_Y (w) + WIN_REAL_HEIGHT (w) + glowSize - glowOffset;

	quadMatrix->xx = 0.0f;
	quadMatrix->yy = 1.0f / glowSize;
	quadMatrix->x0 = 1.0;
	quadMatrix->y0 = -(box->y1 * quadMatrix->yy);

	/* Left edge */
	box = &gw->glowQuads[GLOWQUAD_LEFT].box;
	gw->glowQuads[GLOWQUAD_LEFT].matrix = *matrix;
	quadMatrix = &gw->glowQuads[GLOWQUAD_LEFT].matrix;

	box->x1 = WIN_REAL_X (w) - glowSize + glowOffset;
	box->y1 = WIN_REAL_Y (w) + glowOffset;
	box->x2 = WIN_REAL_X (w) + glowOffset;
	box->y2 = WIN_REAL_Y (w) + WIN_REAL_HEIGHT (w) - glowOffset;

	quadMatrix->xx = 1.0f / glowSize;
	quadMatrix->yy = 0.0f;
	quadMatrix->x0 = -(box->x1 * quadMatrix->xx);
	quadMatrix->y0 = 0.0;

	/* Right edge */
	box = &gw->glowQuads[GLOWQUAD_RIGHT].box;
	gw->glowQuads[GLOWQUAD_RIGHT].matrix = *matrix;
	quadMatrix = &gw->glowQuads[GLOWQUAD_RIGHT].matrix;

	box->x1 = WIN_REAL_X (w) + WIN_REAL_WIDTH (w) - glowOffset;
	box->y1 = WIN_REAL_Y (w) + glowOffset;
	box->x2 = WIN_REAL_X (w) + WIN_REAL_WIDTH (w) + glowSize - glowOffset;
	box->y2 = WIN_REAL_Y (w) + WIN_REAL_HEIGHT (w) - glowOffset;

	quadMatrix->xx = -1.0f / glowSize;
	quadMatrix->yy = 0.0f;
	quadMatrix->x0 = 1.0 - (box->x1 * quadMatrix->xx);
	quadMatrix->y0 = 0.0;
}

/*
 * groupDrawWindow
 *
 */
Bool
groupDrawWindow(CompWindow           *w,
				const CompTransform  *transform,
				const FragmentAttrib *attrib,
				Region               region,
				unsigned int         mask)
{
	Bool       status;
	CompScreen *s = w->screen;

	GROUP_WINDOW (w);
	GROUP_SCREEN (s);

	if (gw->group && (gw->group->nWins > 1) && gw->glowQuads)
	{
		if (mask & PAINT_WINDOW_TRANSFORMED_MASK)
			region = &infiniteRegion;

		if (region->numRects)
		{
			REGION box;
			int    i;

			box.rects = &box.extents;
			box.numRects = 1;

			w->vCount = w->indexCount = 0;

			for (i = 0; i < NUM_GLOWQUADS; i++)
			{
				box.extents = gw->glowQuads[i].box;

				if (box.extents.x1 < box.extents.x2 &&
				   	box.extents.y1 < box.extents.y2)
				{
					(*s->addWindowGeometry) (w,
											 &gw->glowQuads[i].matrix,
											 1, &box, region);
				}
			}

			if (w->vCount)
			{
				FragmentAttrib fAttrib = *attrib;
				GLushort       average;
				GLushort       color[3] = {gw->group->color[0],
					                       gw->group->color[1],
										   gw->group->color[2]};

				/* Apply brightness to color. */
				color[0] *= (float)attrib->brightness / BRIGHT;
				color[1] *= (float)attrib->brightness / BRIGHT;
				color[2] *= (float)attrib->brightness / BRIGHT;

				/* Apply saturation to color. */
				average = (color[0] + color[1] + color[2]) / 3;
				color[0] = average +
					       (color[0] - average) * attrib->saturation / COLOR;
				color[1] = average +
					       (color[1] - average) * attrib->saturation / COLOR;
				color[2] = average +
					       (color[2] - average) * attrib->saturation / COLOR;

				fAttrib.opacity = OPAQUE;
				fAttrib.saturation = COLOR;
				fAttrib.brightness = BRIGHT;

				screenTexEnvMode (w->screen, GL_MODULATE);
				glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
				glColor4us (color[0], color[1], color[2], attrib->opacity);

				/* we use PAINT_WINDOW_TRANSFORMED_MASK here to force
				   the usage of a good texture filter */
				(*s->drawWindowTexture) (w, &gs->glowTexture, &fAttrib,
										 mask | PAINT_WINDOW_BLEND_MASK |
										 PAINT_WINDOW_TRANSLUCENT_MASK |
										 PAINT_WINDOW_TRANSFORMED_MASK);

				glBlendFunc (GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
				screenTexEnvMode (s, GL_REPLACE);
				glColor4usv (defaultColor);
			}
		}
	}

	UNWRAP (gs, s, drawWindow);
	status = (*s->drawWindow) (w, transform, attrib, region, mask);
	WRAP (gs, s, drawWindow, groupDrawWindow);

	return status;
}

void
groupGetStretchRectangle (CompWindow *w,
				   		  BoxPtr     pBox,
						  float      *xScaleRet,
						  float      *yScaleRet)
{
    BoxRec box;
	int    width, height;
	float  xScale, yScale;

	GROUP_WINDOW (w);

	box.x1 = gw->resizeGeometry->x - w->input.left;
    box.y1 = gw->resizeGeometry->y - w->input.top;
    box.x2 = gw->resizeGeometry->x +
  		     gw->resizeGeometry->width + w->serverBorderWidth * 2 +
			 w->input.right;

    if (w->shaded)
    {
		box.y2 = gw->resizeGeometry->y + w->height + w->input.bottom;
    }
    else
    {
		box.y2 = gw->resizeGeometry->y +
 			     gw->resizeGeometry->height + w->serverBorderWidth * 2 +
		   		 w->input.bottom;
    }

    width  = w->width  + w->input.left + w->input.right;
    height = w->height + w->input.top  + w->input.bottom;

    xScale = (width)  ? (box.x2 - box.x1) / (float) width  : 1.0f;
    yScale = (height) ? (box.y2 - box.y1) / (float) height : 1.0f;

    pBox->x1 = box.x1 - (w->output.left - w->input.left) * xScale;
    pBox->y1 = box.y1 - (w->output.top - w->input.top) * yScale;
    pBox->x2 = box.x2 + w->output.right * xScale;
    pBox->y2 = box.y2 + w->output.bottom * yScale;

	if (xScaleRet)
		*xScaleRet = xScale;
	if (yScaleRet)
		*yScaleRet = yScale;
}

void
groupDamagePaintRectangle (CompScreen *s,
						   BoxPtr     pBox)
{
    REGION reg;

    reg.rects    = &reg.extents;
    reg.numRects = 1;

    reg.extents = *pBox;

    reg.extents.x1 -= 1;
    reg.extents.y1 -= 1;
    reg.extents.x2 += 1;
    reg.extents.y2 += 1;

    damageScreenRegion (s, &reg);
}

/*
 * groupPaintWindow
 *
 */
Bool
groupPaintWindow (CompWindow              *w,
				  const WindowPaintAttrib *attrib,
				  const CompTransform     *transform,
				  Region                  region,
				  unsigned int            mask)
{
	Bool       status;
	Bool       doRotate, doTabbing, showTabbar;
	CompScreen *s = w->screen;

	GROUP_SCREEN (s);
	GROUP_WINDOW (w);

	doRotate = gw->group && (gw->group->changeState != PaintOff) &&
		       (IS_TOP_TAB (w, gw->group) || IS_PREV_TOP_TAB (w, gw->group));

	doTabbing = gw->group && (gw->group->tabbingState != PaintOff) &&
		        (gw->animateState & (IS_ANIMATED | FINISHED_ANIMATION)) &&
				!(IS_TOP_TAB (w, gw->group) &&
				  (gw->group->tabbingState == PaintFadeIn));

	showTabbar = gw->group && gw->group->tabBar &&
		         (((HAS_TOP_WIN (gw->group) && IS_TOP_TAB (w, gw->group)) &&
				   ((gw->group->changeState == PaintOff) ||
					(gw->group->changeState == PaintFadeOut))) ||
				  (IS_PREV_TOP_TAB (w, gw->group) &&
				   (gw->group->changeState == PaintFadeIn)));

	if (gw->windowHideInfo)
		mask |= PAINT_WINDOW_NO_CORE_INSTANCE_MASK;

	if (gw->inSelection || gw->resizeGeometry || doRotate || doTabbing || showTabbar)
	{
		WindowPaintAttrib wAttrib = *attrib;
		CompTransform     wTransform = *transform;
		float             animProgress = 0.0f;

		if (gw->inSelection)
		{
			wAttrib.opacity    = OPAQUE * groupGetSelectOpacity (s) / 100;
			wAttrib.saturation = COLOR * groupGetSelectSaturation (s) / 100;
			wAttrib.brightness = BRIGHT * groupGetSelectBrightness (s) / 100;
		}

		if (doTabbing)
		{
			/* fade the window out */
			float progress;
			int   distanceX, distanceY;
			float origDistance, distance;

			distanceX = (WIN_X (w) + gw->tx - gw->destination.x);
			distanceY = (WIN_Y (w) + gw->ty - gw->destination.y);
			distance = sqrt(pow (distanceX, 2) + pow (distanceY, 2));

			distanceX = (gw->orgPos.x - gw->destination.x);
			distanceY = (gw->orgPos.y - gw->destination.y);
			origDistance = sqrt (pow (distanceX, 2) + pow (distanceY, 2));

			if (!distanceX && !distanceY)
				progress = 1.0f;
			else
				progress = 1.0f - (distance / origDistance);

			animProgress = progress;

			progress = MAX (progress, 0.0f);
			if (gw->group->tabbingState == PaintFadeIn)
				progress = 1.0f - progress;

			wAttrib.opacity = (float)wAttrib.opacity * progress;
		}

		if (doRotate)
		{
			float timeLeft = gw->group->changeAnimationTime;
			int   animTime = groupGetChangeAnimationTime (s) * 500;

			if (gw->group->changeState == PaintFadeIn)
				timeLeft += animTime;

			/* 0 at the beginning, 1 at the end */
			animProgress = 1 - (timeLeft / (2 * animTime));
		}

		if (gw->resizeGeometry)
		{
			int    xOrigin, yOrigin;
			float  xScale, yScale;
			BoxRec box;

			groupGetStretchRectangle (w, &box, &xScale, &yScale);

			xOrigin = w->attrib.x - w->input.left;
			yOrigin = w->attrib.y - w->input.top;

			matrixTranslate (&wTransform, xOrigin, yOrigin, 0.0f);
			matrixScale (&wTransform, xScale, yScale, 1.0f);
			matrixTranslate (&wTransform,
							 (gw->resizeGeometry->x - w->attrib.x) / 
							 xScale - xOrigin,
							 (gw->resizeGeometry->y - w->attrib.y) /
							 yScale - yOrigin,
							 0.0f);

			mask |= PAINT_WINDOW_TRANSFORMED_MASK;
		}
		else if (doRotate || doTabbing)
		{
			float      animWidth, animHeight;
			float      animScaleX, animScaleY;
			CompWindow *morphBase, *morphTarget;

			if (doTabbing)
			{
				if (gw->group->tabbingState == PaintFadeIn)
				{
					morphBase   = w;
					morphTarget = TOP_TAB (gw->group);
				}
				else
				{
					morphTarget = w;
					if (HAS_TOP_WIN (gw->group))
						morphBase = TOP_TAB (gw->group);
					else
						morphBase = gw->group->lastTopTab;
				}
			}
			else
			{
				morphBase   = PREV_TOP_TAB (gw->group);
				morphTarget = TOP_TAB (gw->group);
			}

			animWidth = (1 - animProgress) * WIN_REAL_WIDTH (morphBase) +
						animProgress * WIN_REAL_WIDTH (morphTarget);
			animHeight = (1 - animProgress) * WIN_REAL_HEIGHT (morphBase) +
				         animProgress * WIN_REAL_HEIGHT (morphTarget);

			animWidth = MAX (1.0f, animWidth);
			animHeight = MAX (1.0f, animHeight);
			animScaleX = animWidth / WIN_REAL_WIDTH (w);
			animScaleY = animHeight / WIN_REAL_HEIGHT (w);

			if (doRotate)
				matrixScale (&wTransform, 1.0f, 1.0f, 1.0f / s->width);
	
			matrixTranslate (&wTransform,
							 WIN_REAL_X (w) + WIN_REAL_WIDTH (w) / 2.0f,
				 			 WIN_REAL_Y (w) + WIN_REAL_HEIGHT (w) / 2.0f,
							 0.0f);

			if (doRotate)
			{
				float rotateAngle = animProgress * 180.0f;
				if (IS_TOP_TAB (w, gw->group))
					rotateAngle += 180.0f;

				if (gw->group->changeAnimationDirection < 0)
					rotateAngle *= -1.0f;

				matrixRotate (&wTransform, rotateAngle, 0.0f, 1.0f, 0.0f);
			}

			if (doTabbing)
				matrixTranslate (&wTransform, 
								 gw->orgPos.x + gw->tx - WIN_X (w),
								 gw->orgPos.y + gw->ty - WIN_Y (w), 0.0f);

			matrixScale (&wTransform, animScaleX, animScaleY, 1.0f);

			matrixTranslate (&wTransform,
							 -(WIN_REAL_X (w) + WIN_REAL_WIDTH (w) / 2.0f),
				 			 -(WIN_REAL_Y (w) + WIN_REAL_HEIGHT (w) / 2.0f),
							 0.0f);

			mask |= PAINT_WINDOW_TRANSFORMED_MASK;
		}

		UNWRAP (gs, s, paintWindow);
		status = (*s->paintWindow) (w, &wAttrib, &wTransform, region, mask);

		if (showTabbar)
			groupPaintTabBar (gw->group, &wAttrib, &wTransform, mask, region);

		WRAP (gs, s, paintWindow, groupPaintWindow);
	}
	else
	{
		UNWRAP (gs, s, paintWindow);
		status = (*s->paintWindow) (w, attrib, transform, region, mask);
		WRAP (gs, s, paintWindow, groupPaintWindow);
	}

	return status;
}
