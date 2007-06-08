#include "group.h"

/**
 *
 * Beryl group plugin
 *
 * paint.c
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
 * groupPaintThumb - taken from switcher and modified for tab bar
 *
 */
void groupPaintThumb(GroupSelection *group, GroupTabBarSlot *slot, const CompTransform *transform, int targetOpacity)
{
	AddWindowGeometryProc oldAddWindowGeometry;

	CompWindow *w = slot->window;
	
	int tw, th;
	tw = slot->region->extents.x2 - slot->region->extents.x1;
	th = slot->region->extents.y2 - slot->region->extents.y1;

	/* Wrap drawWindowGeometry to make sure the general
	   drawWindowGeometry function is used */
	oldAddWindowGeometry = w->screen->addWindowGeometry;
	w->screen->addWindowGeometry = addWindowGeometry;
	
	WindowPaintAttrib sAttrib = w->paint;

	// animate fade
	if (group && group->tabBar->state == PaintFadeIn)
		sAttrib.opacity -= sAttrib.opacity * group->tabBar->animationTime / 
				   (groupGetFadeTime(w->screen) * 1000);
	else if (group && group->tabBar->state == PaintFadeOut)
		sAttrib.opacity = sAttrib.opacity * group->tabBar->animationTime / 
				  (groupGetFadeTime(w->screen) * 1000);

	sAttrib.opacity = sAttrib.opacity * targetOpacity / 0xffff;

	if (w->mapNum) {

		if (WIN_WIDTH(w) > tw)
			sAttrib.xScale = (float) tw / WIN_WIDTH(w);
		else
			sAttrib.xScale = 1.0f;

		if (WIN_HEIGHT(w) > th)
			sAttrib.yScale = (float) tw / WIN_HEIGHT(w);
		else
			sAttrib.yScale = 1.0f;

		if (sAttrib.xScale < sAttrib.yScale)
			sAttrib.yScale = sAttrib.xScale;
		else
			sAttrib.xScale = sAttrib.yScale;

/*		FIXME: do some more work on the highlight on hover feature
		// Highlight on hover
		if (group && group->tabBar && group->tabBar->hoveredSlot == slot) {
			sAttrib.saturation = 0;
			sAttrib.brightness /= 1.25f;
		}*/

		int vx, vy;
		groupGetDrawOffsetForSlot(slot, &vx, &vy);

		sAttrib.xTranslate = slot->region->extents.x1 - w->attrib.x + vx;
		sAttrib.yTranslate = slot->region->extents.y1 - w->attrib.y + vy;

		FragmentAttrib fragment;
		CompTransform wTransform = *transform;

		initFragmentAttrib(&fragment, &sAttrib);

		matrixTranslate(&wTransform, WIN_X(w), WIN_Y(w), 0.0f);
		matrixScale(&wTransform, sAttrib.xScale, sAttrib.yScale, 0.0f);
		matrixTranslate(&wTransform, 
				sAttrib.xTranslate / sAttrib.xScale - WIN_X(w),
				sAttrib.yTranslate / sAttrib.yScale - WIN_Y(w),
				0.0f);

		glPushMatrix();
		glLoadMatrixf(wTransform.m);

		(w->screen->drawWindow) (w, &wTransform, &fragment, &infiniteRegion,
			PAINT_WINDOW_TRANSFORMED_MASK | PAINT_WINDOW_TRANSLUCENT_MASK);

		glPopMatrix();
	}
	
	w->screen->addWindowGeometry = oldAddWindowGeometry;
}

/*
 * groupRenderTopTabHighlight
 *
 */
void groupRenderTopTabHighlight(GroupSelection *group)
{
	GroupTabBar *bar;
	cairo_t *cr;

	if (!group->tabBar || !HAS_TOP_WIN(group) || !group->tabBar->selectionLayer || !group->tabBar->selectionLayer->cairo)
	    return;

	bar = group->tabBar;

	int width = group->topTab->region->extents.x2 - group->topTab->region->extents.x1 + 10;
	int height = group->topTab->region->extents.y2 - group->topTab->region->extents.y1 + 10;

	bar->selectionLayer = groupRebuildCairoLayer(group->screen, bar->selectionLayer, width, height);
	if (!bar->selectionLayer)
		return;

	cr = bar->selectionLayer->cairo;
	
	// fill
	cairo_set_line_width(cr, 2);
	cairo_set_source_rgba(cr, 
		(group->color[0] / 65535.0f),
		(group->color[1] / 65535.0f),
		(group->color[2] / 65535.0f),
		(group->color[3] / (65535.0f*2)));

	cairo_move_to(cr, 0, 0);
	cairo_rectangle(cr, 0, 0, width, height);
	
	// fill
	cairo_fill_preserve(cr);

	// outline
	cairo_set_source_rgba(cr, 
		(group->color[0] / 65535.0f),
		(group->color[1] / 65535.0f),
		(group->color[2] / 65535.0f),
		(group->color[3] / 65535.0f));
	cairo_stroke(cr);
}

/*
 * groupRenderTabBarBackground
 *
 */
void groupRenderTabBarBackground(GroupSelection *group)
{
	GroupTabBar *bar;
	GroupCairoLayer *layer;
	cairo_t *cr;

	if (!group->tabBar || !HAS_TOP_WIN(group) || !group->tabBar->bgLayer || !group->tabBar->bgLayer->cairo)
	    return;

	bar = group->tabBar;

	int width, height, radius;
	width = bar->region->extents.x2 - bar->region->extents.x1;
	height = bar->region->extents.y2 - bar->region->extents.y1;
	radius = groupGetBorderRadius(group->screen);

	if (width > bar->bgLayer->texWidth)
		width = bar->bgLayer->texWidth;
	
	if (radius > width / 2)
		radius = width / 2;

	layer = bar->bgLayer;
	cr = layer->cairo;
	
	groupClearCairoLayer(layer);

	float r, g, b, a;

	int border_width = groupGetBorderWidth(group->screen);
	cairo_set_line_width(cr, border_width);

	cairo_save(cr);

	double x0, y0, x1, y1;
	x0 = border_width/2.0;
	y0 = border_width/2.0;
	x1 = width  - border_width/2.0;
	y1 = height - border_width/2.0;
	cairo_move_to(cr, x0 + radius, y0);
	cairo_arc(cr, x1 - radius, y0 + radius, radius, M_PI * 1.5, M_PI * 2.0);
	cairo_arc(cr, x1 - radius, y1 - radius, radius, 0.0, M_PI * 0.5);
	cairo_arc(cr, x0 + radius, y1 - radius, radius, M_PI * 0.5, M_PI);
	cairo_arc(cr, x0 + radius, y0 + radius, radius, M_PI, M_PI * 1.5);

	cairo_close_path (cr);

	switch (groupGetTabStyle(group->screen))
	{
		case TabStyleSimple:
		{
			// base color
			r = groupGetTabBaseColorRed(group->screen) / 65535.0f;
			g = groupGetTabBaseColorGreen(group->screen) / 65535.0f;
			b = groupGetTabBaseColorBlue(group->screen) / 65535.0f;
			a = groupGetTabBaseColorAlpha(group->screen) / 65535.0f;
			cairo_set_source_rgba(cr, r, g, b, a);
			
			cairo_fill_preserve(cr);
			break;
		}

		case TabStyleGradient:
		{
			// fill
			cairo_pattern_t *pattern;
			pattern = cairo_pattern_create_linear(0, 0, width, height);

			// highlight color
			r = groupGetTabHighlightColorRed(group->screen) / 65535.0f;
			g = groupGetTabHighlightColorGreen(group->screen) / 65535.0f;
			b = groupGetTabHighlightColorBlue(group->screen) / 65535.0f;
			a = groupGetTabHighlightColorAlpha(group->screen) / 65535.0f;
			cairo_pattern_add_color_stop_rgba(pattern, 0.0f, r, g, b, a);

			// base color
			r = groupGetTabBaseColorRed(group->screen) / 65535.0f;
			g = groupGetTabBaseColorGreen(group->screen) / 65535.0f;
			b = groupGetTabBaseColorBlue(group->screen) / 65535.0f;
			a = groupGetTabBaseColorAlpha(group->screen) / 65535.0f;
			cairo_pattern_add_color_stop_rgba(pattern, 1.0f, r, g, b, a);
	
			cairo_set_source(cr, pattern);
			cairo_fill_preserve(cr);
			cairo_pattern_destroy(pattern);
			break;
		}

		case TabStyleGlass:
		{
			cairo_pattern_t *pattern;

			cairo_save(cr);

			// clip width rounded rectangle
			cairo_clip(cr);

			// ===== HIGHLIGHT =====

			// make draw the shape for the highlight and create a pattern for it
			cairo_rectangle(cr, 0, 0, width, height/2);
			pattern = cairo_pattern_create_linear(0, 0, 0, height);

			// highlight color
			r = groupGetTabHighlightColorRed(group->screen) / 65535.0f;
			g = groupGetTabHighlightColorGreen(group->screen) / 65535.0f;
			b = groupGetTabHighlightColorBlue(group->screen) / 65535.0f;
			a = groupGetTabHighlightColorAlpha(group->screen) / 65535.0f;
			cairo_pattern_add_color_stop_rgba(pattern, 0.0f, r, g, b, a);

			// base color
			r = groupGetTabBaseColorRed(group->screen) / 65535.0f;
			g = groupGetTabBaseColorGreen(group->screen) / 65535.0f;
			b = groupGetTabBaseColorBlue(group->screen) / 65535.0f;
			a = groupGetTabBaseColorAlpha(group->screen) / 65535.0f;
			cairo_pattern_add_color_stop_rgba(pattern, 0.6f, r, g, b, a);
	
			cairo_set_source(cr, pattern);
			cairo_fill(cr);
			cairo_pattern_destroy(pattern);

			// ==== SHADOW =====

			// make draw the shape for the show and create a pattern for it
			cairo_rectangle(cr, 0, height/2, width, height);
			pattern = cairo_pattern_create_linear(0, 0, 0, height);

			// we don't want to use a full highlight here so we mix the colors
			r = (groupGetTabHighlightColorRed(group->screen) + 
			     groupGetTabBaseColorRed(group->screen)) / (2 * 65535.0f);
			g = (groupGetTabHighlightColorGreen(group->screen) + 
			     groupGetTabBaseColorGreen(group->screen)) / (2 * 65535.0f);
			b = (groupGetTabHighlightColorBlue(group->screen) + 
			     groupGetTabBaseColorBlue(group->screen)) / (2 * 65535.0f);
			a = (groupGetTabHighlightColorAlpha(group->screen) + 
			     groupGetTabBaseColorAlpha(group->screen)) / (2 * 65535.0f);
			cairo_pattern_add_color_stop_rgba(pattern, 1.0f, r, g, b, a);

			// base color
			r = groupGetTabBaseColorRed(group->screen) / 65535.0f;
			g = groupGetTabBaseColorGreen(group->screen) / 65535.0f;
			b = groupGetTabBaseColorBlue(group->screen) / 65535.0f;
			a = groupGetTabBaseColorAlpha(group->screen) / 65535.0f;
			cairo_pattern_add_color_stop_rgba(pattern, 0.5f, r, g, b, a);
	
			cairo_set_source(cr, pattern);
			cairo_fill(cr);
			cairo_pattern_destroy(pattern);
			
			cairo_restore(cr);

			// draw shape again for the outline
			cairo_move_to (cr, x0 + radius, y0);
			cairo_arc (cr, x1 - radius, y0 + radius, radius, M_PI * 1.5, M_PI * 2.0);
			cairo_arc (cr, x1 - radius, y1 - radius, radius, 0.0, M_PI * 0.5);
			cairo_arc (cr, x0 + radius, y1 - radius, radius, M_PI * 0.5, M_PI);
			cairo_arc (cr, x0 + radius, y0 + radius, radius, M_PI, M_PI * 1.5);

			break;
		}

		case TabStyleMetal:
		{
			// fill
			cairo_pattern_t *pattern;
			pattern = cairo_pattern_create_linear(0, 0, 0, height);

			// base color #1
			r = groupGetTabBaseColorRed(group->screen) / 65535.0f;
			g = groupGetTabBaseColorGreen(group->screen) / 65535.0f;
			b = groupGetTabBaseColorBlue(group->screen) / 65535.0f;
			a = groupGetTabBaseColorAlpha(group->screen) / 65535.0f;
			cairo_pattern_add_color_stop_rgba(pattern, 0.0f, r, g, b, a);

			// highlight color
			r = groupGetTabHighlightColorRed(group->screen) / 65535.0f;
			g = groupGetTabHighlightColorGreen(group->screen) / 65535.0f;
			b = groupGetTabHighlightColorBlue(group->screen) / 65535.0f;
			a = groupGetTabHighlightColorAlpha(group->screen) / 65535.0f;
			cairo_pattern_add_color_stop_rgba(pattern, 0.55f, r, g, b, a);

			// base color #2
			r = groupGetTabBaseColorRed(group->screen) / 65535.0f;
			g = groupGetTabBaseColorGreen(group->screen) / 65535.0f;
			b = groupGetTabBaseColorBlue(group->screen) / 65535.0f;
			a = groupGetTabBaseColorAlpha(group->screen) / 65535.0f;
			cairo_pattern_add_color_stop_rgba(pattern, 1.0f, r, g, b, a);
	
			cairo_set_source(cr, pattern);
			cairo_fill_preserve(cr);
			cairo_pattern_destroy(pattern);
			break;
		}

		case TabStyleMurrina:
		{
			cairo_pattern_t *pattern;
			cairo_save(cr);

			// clip width rounded rectangle
			cairo_clip_preserve(cr);

			// ==== TOP ====

			x0 = border_width/2.0;
			y0 = border_width/2.0;
			x1 = width  - border_width/2.0;
			y1 = height - border_width/2.0;
			radius = (y1 - y0) / 2;

			// setup pattern
			pattern = cairo_pattern_create_linear(0, 0, 0, height);
			
			// we don't want to use a full highlight here so we mix the colors
			r = (groupGetTabHighlightColorRed(group->screen) + 
			     groupGetTabBaseColorRed(group->screen)) / (2 * 65535.0f);
			g = (groupGetTabHighlightColorGreen(group->screen) + 
			     groupGetTabBaseColorGreen(group->screen)) / (2 * 65535.0f);
			b = (groupGetTabHighlightColorBlue(group->screen) + 
			     groupGetTabBaseColorBlue(group->screen)) / (2 * 65535.0f);
			a = (groupGetTabHighlightColorAlpha(group->screen) + 
			     groupGetTabBaseColorAlpha(group->screen)) / (2 * 65535.0f);
			cairo_pattern_add_color_stop_rgba(pattern, 0.0f, r, g, b, a);

			// highlight color
			r = groupGetTabHighlightColorRed(group->screen) / 65535.0f;
			g = groupGetTabHighlightColorGreen(group->screen) / 65535.0f;
			b = groupGetTabHighlightColorBlue(group->screen) / 65535.0f;
			a = groupGetTabHighlightColorAlpha(group->screen) / 65535.0f;
			cairo_pattern_add_color_stop_rgba(pattern, 1.0f, r, g, b, a);

			cairo_set_source(cr, pattern);
	
			cairo_fill(cr);
			cairo_pattern_destroy(pattern);

			// ==== BOTTOM =====

			x0 = border_width/2.0;
			y0 = border_width/2.0;
			x1 = width  - border_width/2.0;
			y1 = height - border_width/2.0;
			radius = (y1 - y0) / 2;

			cairo_move_to(cr, x1, y1);
			cairo_line_to(cr, x1, y0);
			cairo_arc(cr, x1 - radius, y0, radius, 0.0, M_PI * 0.5);
			cairo_arc_negative(cr, x0 + radius, y1, radius, M_PI * 1.5, M_PI);
			cairo_close_path (cr);

			// setup pattern
			pattern = cairo_pattern_create_linear(0, 0, 0, height);

			// base color
			r = groupGetTabBaseColorRed(group->screen) / 65535.0f;
			g = groupGetTabBaseColorGreen(group->screen) / 65535.0f;
			b = groupGetTabBaseColorBlue(group->screen) / 65535.0f;
			a = groupGetTabBaseColorAlpha(group->screen) / 65535.0f;
			cairo_pattern_add_color_stop_rgba(pattern, 0.0f, r, g, b, a);

			// we don't want to use a full highlight here so we mix the colors
			r = (groupGetTabHighlightColorRed(group->screen) + 
			     groupGetTabBaseColorRed(group->screen)) / (2 * 65535.0f);
			g = (groupGetTabHighlightColorGreen(group->screen) + 
			     groupGetTabBaseColorGreen(group->screen)) / (2 * 65535.0f);
			b = (groupGetTabHighlightColorBlue(group->screen) + 
			     groupGetTabBaseColorBlue(group->screen)) / (2 * 65535.0f);
			a = (groupGetTabHighlightColorAlpha(group->screen) + 
			     groupGetTabBaseColorAlpha(group->screen)) / (2 * 65535.0f);
			cairo_pattern_add_color_stop_rgba(pattern, 1.0f, r, g, b, a);

			cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
			cairo_set_source(cr, pattern);
			cairo_fill(cr);
			cairo_pattern_destroy(pattern);
			cairo_set_operator(cr, CAIRO_OPERATOR_OVER);

			cairo_restore(cr);

			// draw shape again for the outline
			x0 = border_width/2.0;
			y0 = border_width/2.0;
			x1 = width  - border_width/2.0;
			y1 = height - border_width/2.0;
			radius = groupGetBorderRadius(group->screen);
			cairo_move_to(cr, x0 + radius, y0);
			cairo_arc(cr, x1 - radius, y0 + radius, radius, M_PI * 1.5, M_PI * 2.0);
			cairo_arc(cr, x1 - radius, y1 - radius, radius, 0.0, M_PI * 0.5);
			cairo_arc(cr, x0 + radius, y1 - radius, radius, M_PI * 0.5, M_PI);
			cairo_arc(cr, x0 + radius, y0 + radius, radius, M_PI, M_PI * 1.5);

			break;
		}

		default:
			break;
	}

	// outline
	r = groupGetTabBorderColorRed(group->screen) / 65535.0f;
	g = groupGetTabBorderColorGreen(group->screen) / 65535.0f;
	b = groupGetTabBorderColorBlue(group->screen) / 65535.0f;
	a = groupGetTabBorderColorAlpha(group->screen) / 65535.0f;
	cairo_set_source_rgba(cr, r, g, b, a);
	if (bar->bgAnimation != AnimationNone)
		cairo_stroke_preserve(cr);
	else
		cairo_stroke(cr);

	switch (bar->bgAnimation)
	{
		case AnimationPulse:
		{
			double animationProgress = bar->bgAnimationTime / (groupGetPulseTime(group->screen) * 1000.0);
			double alpha = fabs(sin(PI*3 * animationProgress)) * 0.75;
			if (alpha <= 0)
				break;
				
			cairo_save(cr);
			cairo_clip(cr);
			cairo_set_operator(cr, CAIRO_OPERATOR_XOR);
			cairo_rectangle(cr, 0.0, 0.0, width, height);
			cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, alpha);
			cairo_fill(cr);
			cairo_restore(cr);
			break;
		}

		case AnimationReflex:
		{
			double animationProgress = bar->bgAnimationTime / (groupGetReflexTime(group->screen) * 1000.0);
			double posX = (width+240) * animationProgress;
			double alpha = sin(PI * animationProgress) * 0.75;
			if (alpha <= 0)
				break;

			cairo_save(cr);
			cairo_clip(cr);
			cairo_translate(cr, posX - 120.0, 0.0);
			cairo_move_to(cr, 60.0, 0.0);
			cairo_line_to(cr, 120.0, 0.0);
			cairo_line_to(cr, 60.0, height);
			cairo_line_to(cr, 0.0, height);
			cairo_close_path(cr);

			cairo_pattern_t *pattern;
			pattern = cairo_pattern_create_linear(0.0, 0.0, 0.0, height);
			cairo_pattern_add_color_stop_rgba(pattern, 0.0f, 1.0, 1.0, 1.0, alpha);
			cairo_pattern_add_color_stop_rgba(pattern, 1.0f, 1.0, 1.0, 1.0, 0.0);
			cairo_set_source(cr, pattern);
			cairo_fill(cr);
			cairo_restore(cr);
			break;
		}

		case AnimationNone:
		default:
			break;
	}

	cairo_restore(cr);
}

/*
 * groupRenderWindowTitle
 *
 */
void groupRenderWindowTitle(GroupSelection *group)
{
	GroupTabBar *bar;
	GroupCairoLayer *layer;
	void *data = NULL;

	if (!group->tabBar || !HAS_TOP_WIN(group) || !group->tabBar->textLayer)
	    return;

	bar = group->tabBar;

	int width = bar->region->extents.x2 - bar->region->extents.x1;
	int height = bar->region->extents.y2 - bar->region->extents.y1;

	bar->textLayer = groupRebuildCairoLayer(group->screen, bar->textLayer, width, height);
	layer = bar->textLayer;
	if (!layer)
		return;

	int font_size = groupGetTabbarFontSize(group->screen);
	
	CompTextAttrib text_attrib;
	text_attrib.family = "Sans";
	text_attrib.size = font_size;
	text_attrib.style = TEXT_STYLE_BOLD;
	text_attrib.color[0] = groupGetTabbarFontColorRed(group->screen);
	text_attrib.color[1] = groupGetTabbarFontColorGreen(group->screen);
	text_attrib.color[2] = groupGetTabbarFontColorBlue(group->screen);
	text_attrib.color[3] = groupGetTabbarFontColorAlpha(group->screen);
	text_attrib.ellipsize = TRUE;

	text_attrib.maxwidth = width;
	text_attrib.maxheight = height;
	text_attrib.screen = group->screen;
	text_attrib.renderMode = TextRenderWindowTitle;
	text_attrib.data = (void*)((bar->textSlot && bar->textSlot->window) ? bar->textSlot->window->id : 0);

	int stride;

	if (!((*group->screen->display->fileToImage)(group->screen->display, "TextToPixmap", 
					(const char*) &text_attrib, &width, 
					&height, &stride, &data))) 
	{
		/* getting the pixmap failed, so create an empty one */
		Pixmap emptyPixmap = XCreatePixmap(group->screen->display->display, 
				group->screen->root, width, height, 32);

		if (emptyPixmap) {
			XGCValues gcv;
			gcv.foreground = 0x00000000;
			gcv.plane_mask = 0xffffffff;

			GC gc = XCreateGC(group->screen->display->display, emptyPixmap, 
					GCForeground, &gcv);

			XFillRectangle(group->screen->display->display, emptyPixmap, gc, 
					0, 0, width, height);

			XFreeGC(group->screen->display->display, gc);

			data = (void*) emptyPixmap;
		}
	}

	layer->texWidth = width;
	layer->texHeight = height;
	layer->pixmap = (Pixmap) data;

	if(data)
		bindPixmapToTexture(group->screen, &layer->texture, (Pixmap) data, layer->texWidth, layer->texHeight, 32);
}

/*
 * groupPaintTabBar
 *
 */
void groupPaintTabBar(GroupSelection * group, const WindowPaintAttrib *wAttrib, 
	const CompTransform *transform, unsigned int mask, Region clipRegion)
{
	if (!group || !HAS_TOP_WIN(group) || !group->tabBar || (group->tabBar->state == PaintOff))
		return;

	CompWindow *topTab = TOP_TAB(group);
	CompScreen *s = group->screen;
	GroupTabBarSlot *slot;
	GroupTabBar *bar = group->tabBar;

	GROUP_SCREEN(s);

	int i;
	int alpha;
	float w_scale;
	float h_scale;
	GroupCairoLayer *layer;

	REGION box;

	WindowPaintAttrib attrib = *wAttrib;
	attrib.opacity = OPAQUE;
	attrib.saturation = COLOR;
	attrib.brightness = BRIGHT;

#define PAINT_BG     0
#define PAINT_SEL    1
#define PAINT_THUMBS 2
#define PAINT_TEXT   3
#define PAINT_MAX    4

	box.rects = &box.extents;
	box.numRects = 1;

	glEnable(GL_BLEND);
	glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

	for (i = 0; i < PAINT_MAX; i++) {
		alpha = 0xffff;

		if (bar->state == PaintFadeIn) 
			alpha -= alpha * bar->animationTime /
				(groupGetFadeTime(s) * 1000);
		else if (bar->state == PaintFadeOut) 
			alpha = alpha * bar->animationTime /
				(groupGetFadeTime(s) * 1000);

		switch (i) {
			case PAINT_BG:
				layer = bar->bgLayer;

				h_scale = 1.0f;
				w_scale = 1.0f;

				// handle the repaint of the background
				int newWidth = bar->region->extents.x2 - bar->region->extents.x1;

				if (layer && (newWidth > layer->texWidth))
					newWidth = layer->texWidth;

				w_scale = (double) (bar->region->extents.x2 - bar->region->extents.x1) / (double) newWidth;
				if (newWidth != bar->oldWidth || bar->bgAnimation)
					groupRenderTabBarBackground(group);
				bar->oldWidth = newWidth;
				box.extents = bar->region->extents;
			break;

			case PAINT_SEL:
				if (group->topTab != gs->draggedSlot) {
					layer = bar->selectionLayer;

					h_scale = 1.0f;
					w_scale = 1.0f;

					box.extents.x1 = group->topTab->region->extents.x1 - 5;
					box.extents.x2 = group->topTab->region->extents.x2 + 5;
					box.extents.y1 = group->topTab->region->extents.y1 - 5;
					box.extents.y2 = group->topTab->region->extents.y2 + 5;
				} else
					layer = NULL;
			break;

			case PAINT_THUMBS:
				glColor4usv(defaultColor);
				glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
				glDisable(GL_BLEND);

				GLenum oldTextureFilter = s->display->textureFilter;

				if (groupGetMipmaps(s))
					s->display->textureFilter = GL_LINEAR_MIPMAP_LINEAR;

				for(slot = bar->slots; slot; slot = slot->next)
				{
					if(slot != gs->draggedSlot || !gs->dragged)
						groupPaintThumb(group, slot, transform, wAttrib->opacity);
				}

				s->display->textureFilter = oldTextureFilter;

				glEnable(GL_BLEND);
				glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

				layer = NULL;
				w_scale = 1.0f;
				h_scale = 1.0f;
			break;

			case PAINT_TEXT:
				if (bar->textLayer && (bar->textLayer->state != PaintOff)) {
					layer = bar->textLayer;
					
					h_scale = 1.0f;
					w_scale = 1.0f;

					box.extents.x1 = bar->region->extents.x1 + 5;
					box.extents.x2 = bar->region->extents.x1 + bar->textLayer->texWidth + 5;
					box.extents.y1 = bar->region->extents.y2 - bar->textLayer->texHeight - 5;
					box.extents.y2 = bar->region->extents.y2 - 5;

					if (box.extents.x2 > bar->region->extents.x2)
						box.extents.x2 = bar->region->extents.x2;

					// recalculate the alpha again...
					if (bar->textLayer->state == PaintFadeIn) 
						alpha -= alpha * bar->textLayer->animationTime /
							(groupGetFadeTextTime(s) * 1000);
					else if (group->tabBar->textLayer->state == PaintFadeOut) 
						alpha = alpha * bar->textLayer->animationTime /
							(groupGetFadeTextTime(s) * 1000);
				} else
					layer = NULL;
			break;

			default:
				layer = NULL;
				w_scale = 1.0f;
				h_scale = 1.0f;
			break;
		}

		if (layer) {
			CompMatrix matrix = layer->texture.matrix;

			// remove the old x1 and y1 so we have a relative value
			box.extents.x2 -= box.extents.x1;
			box.extents.y2 -= box.extents.y1;
			box.extents.x1 = (box.extents.x1 - topTab->attrib.x) / w_scale + topTab->attrib.x;
			box.extents.y1 = (box.extents.y1 - topTab->attrib.y) / h_scale + topTab->attrib.y;
			// now add the new x1 and y1 so we have a absolute value again,
			// also we don't want to stretch the texture...
			if (box.extents.x2*w_scale < layer->texWidth)
				box.extents.x2 += box.extents.x1;
			else
				box.extents.x2 = box.extents.x1 + layer->texWidth;
			if (box.extents.y2*h_scale < layer->texHeight)
				box.extents.y2 += box.extents.y1;
			else 
				box.extents.y2 = box.extents.y1 + layer->texHeight;
			

			matrix.x0 -= box.extents.x1 * matrix.xx;
			matrix.y0 -= box.extents.y1 * matrix.yy;

			attrib.xScale = w_scale;
			attrib.yScale = h_scale;

			topTab->vCount = 0;


			addWindowGeometry(topTab, &matrix, 1, &box, clipRegion);

			if (topTab->vCount)
			{
				FragmentAttrib fragment;
				CompTransform wTransform = *transform;

				matrixTranslate (&wTransform, WIN_X(topTab), WIN_Y(topTab), 0.0f);
				matrixScale (&wTransform, attrib.xScale, attrib.yScale, 0.0f);
				matrixTranslate (&wTransform,
					attrib.xTranslate / attrib.xScale - WIN_X(topTab),
					attrib.yTranslate / attrib.yScale - WIN_Y(topTab), 0.0f);

				glPushMatrix();
				glLoadMatrixf(wTransform.m);

				initFragmentAttrib(&fragment, &attrib);

				screenTexEnvMode (s, GL_MODULATE);

				alpha = alpha * wAttrib->opacity / 0xffff;
				glColor4us(alpha, alpha, alpha, alpha);

				(*group->screen->drawWindowTexture) (topTab, &layer->texture, 
						&fragment, mask | PAINT_WINDOW_TRANSLUCENT_MASK | 
						PAINT_WINDOW_TRANSFORMED_MASK);

				glPopMatrix();

				screenTexEnvMode(s, GL_REPLACE);
				glColor4usv(defaultColor);
			}
		}
	}

	glColor4usv(defaultColor);
	glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
	glDisable(GL_BLEND);
}

/*
 * groupPaintSelectionOutline
 *
 */
static void
groupPaintSelectionOutline (CompScreen *s, const ScreenPaintAttrib *sa, 
			    const CompTransform *transform,
			    CompOutput *output, Bool transformed)
{
	GROUP_SCREEN(s);

	int x1, x2, y1, y2;

	x1 = MIN(gs->x1, gs->x2);
	y1 = MIN(gs->y1, gs->y2);
	x2 = MAX(gs->x1, gs->x2);
	y2 = MAX(gs->y1, gs->y2);

	if (gs->grabState == ScreenGrabSelect) {
		CompTransform sTransform = *transform;

		if (transformed) {
			(s->applyScreenTransform) (s, sa, output, &sTransform);
			transformToScreenSpace (s, output, -sa->zTranslate, &sTransform);
		} else
			transformToScreenSpace (s, output, -DEFAULT_Z_CAMERA, &sTransform);

		glPushMatrix();
		glLoadMatrixf(sTransform.m);

		glDisableClientState(GL_TEXTURE_COORD_ARRAY);
		glEnable(GL_BLEND);

		glColor4usv(groupGetFillColorOption(s)->value.c);
		glRecti(x1, y2, x2, y1);

		glLineWidth(3);
		glEnable(GL_LINE_SMOOTH);
		glColor4usv(groupGetLineColorOption(s)->value.c);
		glBegin(GL_LINE_LOOP);
		glVertex2i(x1, y1);
		glVertex2i(x2, y1);
		glVertex2i(x2, y2);
		glVertex2i(x1, y2);
		glEnd();
		glDisable(GL_LINE_SMOOTH);
		glLineWidth(1); // back to default

		glColor4usv(defaultColor);
		glDisable(GL_BLEND);
		glEnableClientState(GL_TEXTURE_COORD_ARRAY);
		glPopMatrix();
	}
}

/*
 * groupPreparePaintScreen
 *
 */
void groupPreparePaintScreen(CompScreen * s, int msSinceLastPaint)
{
	GROUP_SCREEN(s);
	GroupSelection *group;

	UNWRAP(gs, s, preparePaintScreen);
	(*s->preparePaintScreen) (s, msSinceLastPaint);
	WRAP(gs, s, preparePaintScreen, groupPreparePaintScreen);

	for (group = gs->groups; group; group = group->next)
	{
		GroupTabBar *bar = group->tabBar;
	
		if (group->changeState != PaintOff)
			group->changeAnimationTime -= msSinceLastPaint;

		if (!bar)
			continue;
	
		groupApplyForces(s, bar, (gs->dragged)? gs->draggedSlot: NULL);
		groupApplySpeeds(s, group, msSinceLastPaint);

		groupHandleHoverDetection(group);
		groupHandleTabBarFade(group, msSinceLastPaint);
		groupHandleTextFade(group, msSinceLastPaint);
		groupHandleTabBarAnimation(group, msSinceLastPaint);
	}
	
	groupHandleScreenActions(s);

	groupHandleChanges(s);
	groupDrawTabAnimation(s, msSinceLastPaint);

	groupDequeueMoveNotifies (s);
	groupDequeueGrabNotifies (s);
	groupDequeueUngrabNotifies (s);
}

/*
 * groupPaintOutput
 *
 */
Bool
groupPaintOutput(CompScreen * s,
		 const ScreenPaintAttrib * sAttrib,
		 const CompTransform *transform,
		 Region region, CompOutput *output,
		 unsigned int mask)
{
	GROUP_SCREEN(s);
	GroupSelection *group;
	Bool status;

	gs->painted = FALSE;
	gs->vpX = s->x;
	gs->vpY = s->y;

	for (group = gs->groups; group; group = group->next)
	{
		if (group->changeState != PaintOff)
			mask |= PAINT_SCREEN_WITH_TRANSFORMED_WINDOWS_MASK;
	}

	if (gs->tabBarVisible) 
			mask |= PAINT_SCREEN_WITH_TRANSFORMED_WINDOWS_MASK;

	UNWRAP(gs, s, paintOutput);
	status = (*s->paintOutput) (s, sAttrib, transform, region, output, mask);
	WRAP(gs, s, paintOutput, groupPaintOutput);

	if (status && !gs->painted) {
		if ((gs->grabState == ScreenGrabTabDrag) && gs->draggedSlot) {
			GROUP_WINDOW(gs->draggedSlot->window);

			CompTransform wTransform = *transform;

			transformToScreenSpace(s, output, -DEFAULT_Z_CAMERA, &wTransform);
			
			glPushMatrix();
			glLoadMatrixf(wTransform.m);
		
			// prevent tab bar drawing..
			PaintState state = gw->group->tabBar->state;
			gw->group->tabBar->state = PaintOff;
			groupPaintThumb(NULL, gs->draggedSlot, &wTransform, 0xffff);
			gw->group->tabBar->state = state;

			glPopMatrix();
		} else  if (gs->grabState == ScreenGrabSelect) {
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
groupPaintTransformedOutput(CompScreen * s, const ScreenPaintAttrib * sa,
			    const CompTransform *transform,
			    Region region, CompOutput *output, 
			    unsigned int mask)
{
	GROUP_SCREEN(s);

	UNWRAP(gs, s, paintTransformedOutput);
	(*s->paintTransformedOutput) (s, sa, transform, region, output, mask);
	WRAP(gs, s, paintTransformedOutput, groupPaintTransformedOutput);
	
	if ((gs->vpX == s->x) && (gs->vpY == s->y)) {
		gs->painted = TRUE;

		if ((gs->grabState == ScreenGrabTabDrag) && gs->draggedSlot && gs->dragged) {
			CompTransform wTransform = *transform;

			(s->applyScreenTransform) (s, sa, output, &wTransform);
			transformToScreenSpace(s, output, -sa->zTranslate, &wTransform);
			glPushMatrix();
			glLoadMatrixf(wTransform.m);
	
			groupPaintThumb(NULL, gs->draggedSlot, &wTransform, 0xffff);
	
			glPopMatrix();
		} else if (gs->grabState == ScreenGrabSelect) {
			groupPaintSelectionOutline (s, sa, transform, output, TRUE);
		}
	}
}

void groupRecomputeGlow (CompScreen *s)
{
	GROUP_SCREEN(s);
	CompWindow *w;

	for (w = s->windows; w; w = w->next)
		groupComputeGlowQuads (w, &gs->glowTexture.matrix);
}

/*
 * groupDonePaintScreen
 *
 */
void groupDonePaintScreen(CompScreen * s)
{
	GROUP_SCREEN(s);
	GroupSelection *group;

	UNWRAP(gs, s, donePaintScreen);
	(*s->donePaintScreen) (s);
	WRAP(gs, s, donePaintScreen, groupDonePaintScreen);

	for(group = gs->groups; group; group = group->next)
	{
		if (group->doTabbing)
			damageScreen(s);

		if (group->changeState != PaintOff)
			damageScreen(s);

		if (group->tabBar)
		{
			if ((group->tabBar->state == PaintFadeIn) ||
				(group->tabBar->state == PaintFadeOut))
			{
				groupDamageTabBarRegion (group);
			}

			if (group->tabBar->textLayer)
			{
				if ((group->tabBar->textLayer->state == PaintFadeIn) ||
					(group->tabBar->textLayer->state == PaintFadeOut))
				{
					groupDamageTabBarRegion (group);
				}
			}
		}
	}
}

void
groupComputeGlowQuads (CompWindow *w, CompMatrix *matrix)
{
	GROUP_WINDOW(w);

	BoxRec *box;
	CompMatrix *quadMatrix;

	if (groupGetGlow(w->screen) && matrix) {
		if (!gw->glowQuads)
			gw->glowQuads = malloc (NUM_GLOWQUADS * sizeof(GlowQuad));
		if (!gw->glowQuads)
			return;
	} else {
		if (gw->glowQuads) {
			free (gw->glowQuads);
			gw->glowQuads = NULL;
		}
		return;
	}

	GROUP_DISPLAY(w->screen->display);

	int glowSize = groupGetGlowSize(w->screen);
	GroupGlowTypeEnum glowType = groupGetGlowType(w->screen);
	int glowOffset = (glowSize * gd->glowTextureProperties[glowType].glowOffset /
		      gd->glowTextureProperties[glowType].textureSize) + 1; 

	/* Top left corner */
	box = &gw->glowQuads[GLOWQUAD_TOPLEFT].box;
	gw->glowQuads[GLOWQUAD_TOPLEFT].matrix = *matrix;
	quadMatrix = &gw->glowQuads[GLOWQUAD_TOPLEFT].matrix;

	box->x1 = WIN_REAL_X(w) - glowSize + glowOffset;
	box->y1 = WIN_REAL_Y(w) - glowSize + glowOffset;
	box->x2 = WIN_REAL_X(w) + glowOffset;
	box->y2 = WIN_REAL_Y(w) + glowOffset;

	quadMatrix->xx = 1.0f / glowSize;
	quadMatrix->yy = -1.0f / glowSize;
	quadMatrix->x0 = -(box->x1 * quadMatrix->xx);
	quadMatrix->y0 = 1.0 -(box->y1 * quadMatrix->yy);

	box->x2 = MIN(WIN_REAL_X(w) + glowOffset, WIN_REAL_X(w) + (WIN_REAL_WIDTH(w) / 2));
	box->y2 = MIN(WIN_REAL_Y(w) + glowOffset, WIN_REAL_Y(w) + (WIN_REAL_HEIGHT(w) / 2));

	/* Top right corner */
	box = &gw->glowQuads[GLOWQUAD_TOPRIGHT].box;
	gw->glowQuads[GLOWQUAD_TOPRIGHT].matrix = *matrix;
	quadMatrix = &gw->glowQuads[GLOWQUAD_TOPRIGHT].matrix;

	box->x1 = WIN_REAL_X(w) + WIN_REAL_WIDTH(w) - glowOffset;
	box->y1 = WIN_REAL_Y(w) - glowSize + glowOffset;
	box->x2 = WIN_REAL_X(w) + WIN_REAL_WIDTH(w) + glowSize - glowOffset;
	box->y2 = WIN_REAL_Y(w) + glowOffset;

	quadMatrix->xx = -1.0f / glowSize;
	quadMatrix->yy = -1.0f / glowSize;
	quadMatrix->x0 = 1.0 - (box->x1 * quadMatrix->xx);
	quadMatrix->y0 = 1.0 - (box->y1 * quadMatrix->yy);

	box->x1 = MAX(WIN_REAL_X(w) + WIN_REAL_WIDTH(w) - glowOffset, 
		WIN_REAL_X(w) + (WIN_REAL_WIDTH(w) / 2));
	box->y2 = MIN(WIN_REAL_Y(w) + glowOffset, 
		WIN_REAL_Y(w) + (WIN_REAL_HEIGHT(w) / 2));

	/* Bottom left corner */
	box = &gw->glowQuads[GLOWQUAD_BOTTOMLEFT].box;
	gw->glowQuads[GLOWQUAD_BOTTOMLEFT].matrix = *matrix;
	quadMatrix = &gw->glowQuads[GLOWQUAD_BOTTOMLEFT].matrix;

	box->x1 = WIN_REAL_X(w) - glowSize + glowOffset;
	box->y1 = WIN_REAL_Y(w) + WIN_REAL_HEIGHT(w) - glowOffset;
	box->x2 = WIN_REAL_X(w) + glowOffset;
	box->y2 = WIN_REAL_Y(w) + WIN_REAL_HEIGHT(w) + glowSize - glowOffset;

	quadMatrix->xx = 1.0f / glowSize;
	quadMatrix->yy = 1.0f / glowSize;
	quadMatrix->x0 = -(box->x1 * quadMatrix->xx);
	quadMatrix->y0 = -(box->y1 * quadMatrix->yy);

	box->y1 = MAX(WIN_REAL_Y(w) + WIN_REAL_HEIGHT(w) - glowOffset,
		WIN_REAL_Y(w) + (WIN_REAL_HEIGHT(w) / 2));
	box->x2 = MIN(WIN_REAL_X(w) + glowOffset, 
		WIN_REAL_X(w) + (WIN_REAL_WIDTH(w) / 2));

	/* Bottom right corner */
	box = &gw->glowQuads[GLOWQUAD_BOTTOMRIGHT].box;
	gw->glowQuads[GLOWQUAD_BOTTOMRIGHT].matrix = *matrix;
	quadMatrix = &gw->glowQuads[GLOWQUAD_BOTTOMRIGHT].matrix;

	box->x1 = WIN_REAL_X(w) + WIN_REAL_WIDTH(w) - glowOffset;
	box->y1 = WIN_REAL_Y(w) + WIN_REAL_HEIGHT(w) - glowOffset;
	box->x2 = WIN_REAL_X(w) + WIN_REAL_WIDTH(w) + glowSize - glowOffset;
	box->y2 = WIN_REAL_Y(w) + WIN_REAL_HEIGHT(w) + glowSize - glowOffset;

	quadMatrix->xx = -1.0f / glowSize;
	quadMatrix->yy = 1.0f / glowSize;
	quadMatrix->x0 = 1.0 - (box->x1 * quadMatrix->xx);
	quadMatrix->y0 = -(box->y1 * quadMatrix->yy);

	box->x1 = MAX(WIN_REAL_X(w) + WIN_REAL_WIDTH(w) - glowOffset, 
		WIN_REAL_X(w) + (WIN_REAL_WIDTH(w) / 2));
	box->y1 = MAX(WIN_REAL_Y(w) + WIN_REAL_HEIGHT(w) - glowOffset,
		WIN_REAL_Y(w) + (WIN_REAL_HEIGHT(w) / 2));

	/* Top edge */
	box = &gw->glowQuads[GLOWQUAD_TOP].box;
	gw->glowQuads[GLOWQUAD_TOP].matrix = *matrix;
	quadMatrix = &gw->glowQuads[GLOWQUAD_TOP].matrix;

	box->x1 = WIN_REAL_X(w) + glowOffset;
	box->y1 = WIN_REAL_Y(w) - glowSize + glowOffset;
	box->x2 = WIN_REAL_X(w) + WIN_REAL_WIDTH(w) - glowOffset;
	box->y2 = WIN_REAL_Y(w) + glowOffset;

	quadMatrix->xx = 0.0f;
	quadMatrix->yy = -1.0f / glowSize;
	quadMatrix->x0 = 1.0;
	quadMatrix->y0 = 1.0 - (box->y1 * quadMatrix->yy);

	/* Bottom edge */
	box = &gw->glowQuads[GLOWQUAD_BOTTOM].box;
	gw->glowQuads[GLOWQUAD_BOTTOM].matrix = *matrix;
	quadMatrix = &gw->glowQuads[GLOWQUAD_BOTTOM].matrix;

	box->x1 = WIN_REAL_X(w) + glowOffset;
	box->y1 = WIN_REAL_Y(w) + WIN_REAL_HEIGHT(w) - glowOffset;
	box->x2 = WIN_REAL_X(w) + WIN_REAL_WIDTH(w) - glowOffset;
	box->y2 = WIN_REAL_Y(w) + WIN_REAL_HEIGHT(w) + glowSize - glowOffset;

	quadMatrix->xx = 0.0f;
	quadMatrix->yy = 1.0f / glowSize;
	quadMatrix->x0 = 1.0;
	quadMatrix->y0 = -(box->y1 * quadMatrix->yy);

	/* Left edge */
	box = &gw->glowQuads[GLOWQUAD_LEFT].box;
	gw->glowQuads[GLOWQUAD_LEFT].matrix = *matrix;
	quadMatrix = &gw->glowQuads[GLOWQUAD_LEFT].matrix;

	box->x1 = WIN_REAL_X(w) - glowSize + glowOffset;
	box->y1 = WIN_REAL_Y(w) + glowOffset;
	box->x2 = WIN_REAL_X(w) + glowOffset;
	box->y2 = WIN_REAL_Y(w) + WIN_REAL_HEIGHT(w) - glowOffset;

	quadMatrix->xx = 1.0f / glowSize;
	quadMatrix->yy = 0.0f;
	quadMatrix->x0 = -(box->x1 * quadMatrix->xx);
	quadMatrix->y0 = 0.0;

	/* Right edge */
	box = &gw->glowQuads[GLOWQUAD_RIGHT].box;
	gw->glowQuads[GLOWQUAD_RIGHT].matrix = *matrix;
	quadMatrix = &gw->glowQuads[GLOWQUAD_RIGHT].matrix;

	box->x1 = WIN_REAL_X(w) + WIN_REAL_WIDTH(w) - glowOffset;
	box->y1 = WIN_REAL_Y(w) + glowOffset;
	box->x2 = WIN_REAL_X(w) + WIN_REAL_WIDTH(w) + glowSize - glowOffset;
	box->y2 = WIN_REAL_Y(w) + WIN_REAL_HEIGHT(w) - glowOffset;

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
groupDrawWindow(CompWindow * w,
		const CompTransform *transform,
		const FragmentAttrib *attrib,
		Region region, unsigned int mask)
{
	Bool status;
	GROUP_WINDOW(w);
	GROUP_SCREEN(w->screen);

	if (gw->group && (gw->group->nWins > 1) && gw->glowQuads)
	{
		if (mask & PAINT_WINDOW_TRANSFORMED_MASK)
			region = &infiniteRegion;

		if (region->numRects) {
			REGION box;
			int i;

			box.rects = &box.extents;
			box.numRects = 1;

			w->vCount = w->indexCount = 0;

			for (i = 0; i < NUM_GLOWQUADS; i++) {
				box.extents = gw->glowQuads[i].box;

				if (box.extents.x1 < box.extents.x2 &&
				    box.extents.y1 < box.extents.y2) {
					(*w->screen->addWindowGeometry) (w, 
						&gw->glowQuads[i].matrix, 1, &box, region);
				}
			}

			if (w->vCount) {
				FragmentAttrib fAttrib = *attrib;
				
				GLushort color[3] = {gw->group->color[0], gw->group->color[1], gw->group->color[2]};
				
				// Apply brightness to color.
				color[0] *= (float)attrib->brightness / 0xffff;
				color[1] *= (float)attrib->brightness / 0xffff;
				color[2] *= (float)attrib->brightness / 0xffff;
				
				// Apply saturation to color.
				GLushort avarage = (color[0] + color[1] + color[2]) / 3;
				
				color[0] = avarage + (color[0] - avarage) * attrib->saturation / 0xffff;
				color[1] = avarage + (color[1] - avarage) * attrib->saturation / 0xffff;
				color[2] = avarage + (color[2] - avarage) * attrib->saturation / 0xffff;

				fAttrib.opacity = OPAQUE;
				fAttrib.saturation = COLOR;
				fAttrib.brightness = BRIGHT;

				screenTexEnvMode (w->screen, GL_MODULATE);
				//glBlendFunc(GL_SRC_ALPHA, GL_ONE); - maybe add an option for that...
				glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
				glColor4us(color[0], color[1], color[2],
						attrib->opacity);

				/* we use PAINT_WINDOW_TRANSFORMED_MASK here to force
				   the usage of a good texture filter */
				(*w->screen->drawWindowTexture) (w, &gs->glowTexture, 
					&fAttrib, mask | PAINT_WINDOW_BLEND_MASK | 
					PAINT_WINDOW_TRANSLUCENT_MASK |
					PAINT_WINDOW_TRANSFORMED_MASK);

				glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
				screenTexEnvMode (w->screen, GL_REPLACE);
				glColor4usv(defaultColor);
			}
		}
	}
	
	UNWRAP(gs, w->screen, drawWindow);
	status = (*w->screen->drawWindow) (w, transform, attrib, region, mask);
	WRAP(gs, w->screen, drawWindow, groupDrawWindow);
	
	return status;
}

/*
 * groupPaintWindow
 *
 */
Bool
groupPaintWindow(CompWindow * w,
		const WindowPaintAttrib * attrib,
		const CompTransform *transform,
		Region region, unsigned int mask)
{
	Bool status;
	Bool doRotate;
	GROUP_SCREEN(w->screen);
	GROUP_WINDOW(w);

	WindowPaintAttrib gAttrib = *attrib;
	CompTransform wTransform = *transform;

	if (gw->inSelection) {
		int opacity = groupGetSelectOpacity(w->screen);
		int saturation = groupGetSelectSaturation(w->screen);
		int brightness = groupGetSelectBrightness(w->screen);

		opacity = OPAQUE * opacity / 100;
		saturation = COLOR * saturation / 100;
		brightness = BRIGHT * brightness / 100;

		gAttrib.opacity = opacity;
		gAttrib.saturation = saturation;
		gAttrib.brightness = brightness;
	} else if (gw->group && gw->group->tabbingState != PaintOff &&
		(gw->animateState & (IS_ANIMATED | FINISHED_ANIMATION))) {
		//fade the window out 
		float opacity;
		
		int origDistanceX = (gw->orgPos.x - gw->destination.x);
		int origDistanceY = (gw->orgPos.y - gw->destination.y);
		float origDistance = sqrt(pow(origDistanceX, 2) + pow(origDistanceY,2));
		
		float distanceX = (WIN_X(w) - gw->destination.x);
		float distanceY = (WIN_Y(w) - gw->destination.y);
		float distance = sqrt(pow(distanceX, 2) + pow(distanceY, 2));
		
		if(distance > origDistance) 
			opacity = 100.0f;
		else {
			if(!origDistanceX && !origDistanceY) {
				if (IS_TOP_TAB(w, gw->group) && (gw->group->tabbingState == PaintFadeIn)) 
					opacity = 100.0f;
				else
					opacity = 0.0f;
			} else 
				opacity = 100.0f * distance / origDistance;

			if (gw->group->tabbingState == PaintFadeOut) 
				opacity = 100.0f - opacity;
		}

		gAttrib.opacity = gAttrib.opacity * opacity / 100; 
	}            
	
	doRotate = gw->group && (gw->group->changeState != PaintOff) &&
		(IS_TOP_TAB(w, gw->group) || IS_PREV_TOP_TAB(w, gw->group));

	if (doRotate)
	{
		float rotateAngle;
		float timeLeft = gw->group->changeAnimationTime;
		float animationProgress;
		float animWidth, animHeight;
		float animScaleX, animScaleY;
		
		if(gw->group->changeState == PaintFadeIn)
			timeLeft += groupGetChangeAnimationTime(w->screen) * 500.0f;
		
		animationProgress = (1 - (timeLeft / (groupGetChangeAnimationTime(w->screen) * 1000.0f))); // 0 at the beginning, 1 at the end.
		
		rotateAngle = animationProgress * 180.0f;
		if (IS_TOP_TAB(w, gw->group))
			rotateAngle += 180.0f;

		if (gw->group->changeAnimationDirection < 0)
			rotateAngle *= -1.0f;
			
		animWidth = (1 - animationProgress) * WIN_REAL_WIDTH(PREV_TOP_TAB(gw->group)) + animationProgress * WIN_REAL_WIDTH(TOP_TAB(gw->group));
		animHeight = (1 - animationProgress) * WIN_REAL_HEIGHT(PREV_TOP_TAB(gw->group)) + animationProgress * WIN_REAL_HEIGHT(TOP_TAB(gw->group));

		animScaleX = animWidth / WIN_REAL_WIDTH(w);
		animScaleY = animHeight / WIN_REAL_HEIGHT(w);

		matrixScale(&wTransform, 1.0f, 1.0f, 1.0f / w->screen->width);
		matrixTranslate(&wTransform, WIN_REAL_X(w) + WIN_REAL_WIDTH(w) / 2.0f,
		                             WIN_REAL_Y(w) + WIN_REAL_HEIGHT(w) / 2.0f, 0.0f);
		matrixRotate(&wTransform, rotateAngle, 0.0f, 1.0f, 0.0f);
		matrixScale(&wTransform, animScaleX, animScaleY, 1.0f);
		matrixTranslate(&wTransform, -(WIN_REAL_X(w) + WIN_REAL_WIDTH(w) / 2.0f),
		                             -(WIN_REAL_Y(w) + WIN_REAL_HEIGHT(w) / 2.0f), 0.0f);

		glPushMatrix();
		glLoadMatrixf(wTransform.m);
		
		mask |= PAINT_WINDOW_TRANSFORMED_MASK;
	}

	if (gw->windowHideInfo)
		mask |= PAINT_WINDOW_NO_CORE_INSTANCE_MASK;

	UNWRAP(gs, w->screen, paintWindow);
	
	status = (*w->screen->paintWindow) (w, &gAttrib, &wTransform, region, mask);
	
	if (gw->group && gw->group->tabBar) {
		if (HAS_TOP_WIN(gw->group) && IS_TOP_TAB(w, gw->group)) {
			if ((gw->group->changeState == PaintOff) || (gw->group->changeState == PaintFadeOut)) 
				groupPaintTabBar(gw->group, attrib, &wTransform, mask, region);
		} else if (IS_PREV_TOP_TAB(w, gw->group)) {
			if (gw->group->changeState == PaintFadeIn)
				groupPaintTabBar(gw->group, attrib, &wTransform, mask, region);
		}
	}

	WRAP(gs, w->screen, paintWindow, groupPaintWindow);
	
	if(doRotate)
		glPopMatrix();

	return status;
}
