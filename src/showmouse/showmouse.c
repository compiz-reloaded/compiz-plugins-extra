/*
 *
 * Compiz show mouse pointer plugin
 *
 * showmouse.c
 *
 * Copyright : (C) 2008 by Dennis Kasprzyk
 * E-mail    : onestone@compiz.org
 *
 * Copyright (C) 2016 by Hypra
 * E-mail    contact@hypra.fr
 * Added mouse guides.
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

#include <math.h>
#include <string.h>

#include <compiz-core.h>
#include <compiz-mousepoll.h>

#include <X11/Xatom.h>
#include <X11/extensions/Xfixes.h>
#include <X11/extensions/Xrender.h>
#include <X11/extensions/shape.h>

#include <cairo/cairo-xlib.h>

#include "showmouse_options.h"
#include "showmouse_tex.h"

#define GET_SHOWMOUSE_DISPLAY(d)                                  \
    ((ShowmouseDisplay *) (d)->base.privates[displayPrivateIndex].ptr)

#define SHOWMOUSE_DISPLAY(d)                      \
    ShowmouseDisplay *sd = GET_SHOWMOUSE_DISPLAY (d)

#define GET_SHOWMOUSE_SCREEN(s, sd)                                   \
    ((ShowmouseScreen *) (s)->base.privates[(sd)->screenPrivateIndex].ptr)

#define SHOWMOUSE_SCREEN(s)                                                      \
    ShowmouseScreen *ss = GET_SHOWMOUSE_SCREEN (s, GET_SHOWMOUSE_DISPLAY (s->display))


typedef struct _Particle
{
    float life;			// particle life
    float fade;			// fade speed
    float width;		// particle width
    float height;		// particle height
    float w_mod;		// particle size modification during life
    float h_mod;		// particle size modification during life
    float r;			// red value
    float g;			// green value
    float b;			// blue value
    float a;			// alpha value
    float x;			// X position
    float y;			// Y position
    float z;			// Z position
    float xi;			// X direction
    float yi;			// Y direction
    float zi;			// Z direction
    float xg;			// X gravity
    float yg;			// Y gravity
    float zg;			// Z gravity
    float xo;			// orginal X position
    float yo;			// orginal Y position
    float zo;			// orginal Z position
} Particle;

typedef struct _ParticleSystem
{
    int      numParticles;
    Particle *particles;
    float    slowdown;
    GLuint   tex;
    Bool     active;
    int      x, y;
    float    darken;
    GLuint   blendMode;

    // Moved from drawParticles to get rid of spurious malloc's
    GLfloat *vertices_cache;
    int     vertex_cache_count;
    GLfloat *coords_cache;
    int     coords_cache_count;
    GLfloat *colors_cache;
    int     color_cache_count;
    GLfloat *dcolors_cache;
    int     dcolors_cache_count;
} ParticleSystem;


static int displayPrivateIndex = 0;

typedef struct _ShowmouseDisplay
{
    int  screenPrivateIndex;

    MousePollFunc *mpFunc;

    HandleEventProc handleEvent;
}
ShowmouseDisplay;

typedef struct _ShowmouseScreen
{
    int posX;
    int posY;

    Bool active;

    ParticleSystem *ps;

    float rot;

    struct
    {
	/* offset of our window relative to the root, in order to avoid
	 * offset between crosshairs and actual mouse in case our
	 * window didn't get placed at 0,0 */
	int xOffset;
	int yOffset;

	Window window;
	cairo_surface_t *surface;
    } overlay;

    PositionPollingHandle pollHandle;
	
    PreparePaintScreenProc preparePaintScreen;
    DonePaintScreenProc    donePaintScreen;
    PaintOutputProc        paintOutput;
}
ShowmouseScreen;

static void
initParticles (int numParticles, ParticleSystem * ps)
{
    if (ps->particles)
	free(ps->particles);
    ps->particles    = calloc(numParticles, sizeof(Particle));
    ps->tex          = 0;
    ps->numParticles = numParticles;
    ps->slowdown     = 1;
    ps->active       = FALSE;

    // Initialize cache
    ps->vertices_cache      = NULL;
    ps->colors_cache        = NULL;
    ps->coords_cache        = NULL;
    ps->dcolors_cache       = NULL;
    ps->vertex_cache_count  = 0;
    ps->color_cache_count   = 0;
    ps->coords_cache_count  = 0;
    ps->dcolors_cache_count = 0;

    Particle *part = ps->particles;
    int i;
    for (i = 0; i < numParticles; i++, part++)
	part->life = 0.0f;
}

static void
drawParticles (CompScreen * s, ParticleSystem * ps)
{
    glPushMatrix();

    glEnable(GL_BLEND);
    if (ps->tex)
    {
	glBindTexture(GL_TEXTURE_2D, ps->tex);
	glEnable(GL_TEXTURE_2D);
    }
    glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

    /* Check that the cache is big enough */
    if (ps->numParticles > ps->vertex_cache_count)
    {
	ps->vertices_cache =
	    realloc(ps->vertices_cache,
		    ps->numParticles * 4 * 3 * sizeof(GLfloat));
	ps->vertex_cache_count = ps->numParticles;
    }

    if (ps->numParticles > ps->coords_cache_count)
    {
	ps->coords_cache =
	    realloc(ps->coords_cache,
		    ps->numParticles * 4 * 2 * sizeof(GLfloat));
	ps->coords_cache_count = ps->numParticles;
    }

    if (ps->numParticles > ps->color_cache_count)
    {
	ps->colors_cache =
	    realloc(ps->colors_cache,
		    ps->numParticles * 4 * 4 * sizeof(GLfloat));
	ps->color_cache_count = ps->numParticles;
    }

    if (ps->darken > 0)
    {
	if (ps->dcolors_cache_count < ps->numParticles)
	{
	    ps->dcolors_cache =
		realloc(ps->dcolors_cache,
			ps->numParticles * 4 * 4 * sizeof(GLfloat));
	    ps->dcolors_cache_count = ps->numParticles;
	}
    }

    GLfloat *dcolors  = ps->dcolors_cache;
    GLfloat *vertices = ps->vertices_cache;
    GLfloat *coords   = ps->coords_cache;
    GLfloat *colors   = ps->colors_cache;

    int cornersSize = sizeof (GLfloat) * 8;
    int colorSize   = sizeof (GLfloat) * 4;

    GLfloat cornerCoords[8] = {0.0, 0.0,
			       0.0, 1.0,
			       1.0, 1.0,
			       1.0, 0.0};

    int numActive = 0;

    Particle *part = ps->particles;
    int i;
    for (i = 0; i < ps->numParticles; i++, part++)
    {
	if (part->life > 0.0f)
	{
	    numActive += 4;

	    float w = part->width / 2;
	    float h = part->height / 2;

	    w += (w * part->w_mod) * part->life;
	    h += (h * part->h_mod) * part->life;

	    vertices[0] = part->x - w;
	    vertices[1] = part->y - h;
	    vertices[2] = part->z;

	    vertices[3] = part->x - w;
	    vertices[4] = part->y + h;
	    vertices[5] = part->z;

	    vertices[6] = part->x + w;
	    vertices[7] = part->y + h;
	    vertices[8] = part->z;

	    vertices[9]  = part->x + w;
	    vertices[10] = part->y - h;
	    vertices[11] = part->z;

	    vertices += 12;

	    memcpy (coords, cornerCoords, cornersSize);

	    coords += 8;

	    colors[0] = part->r;
	    colors[1] = part->g;
	    colors[2] = part->b;
	    colors[3] = part->life * part->a;
	    memcpy (colors + 4, colors, colorSize);
	    memcpy (colors + 8, colors, colorSize);
	    memcpy (colors + 12, colors, colorSize);

	    colors += 16;

	    if (ps->darken > 0)
	    {
		dcolors[0] = part->r;
		dcolors[1] = part->g;
		dcolors[2] = part->b;
		dcolors[3] = part->life * part->a * ps->darken;
		memcpy (dcolors + 4, dcolors, colorSize);
		memcpy (dcolors + 8, dcolors, colorSize);
		memcpy (dcolors + 12, dcolors, colorSize);

		dcolors += 16;
	    }
	}
    }

    glEnableClientState(GL_COLOR_ARRAY);

    glTexCoordPointer(2, GL_FLOAT, 2 * sizeof(GLfloat), ps->coords_cache);
    glVertexPointer(3, GL_FLOAT, 3 * sizeof(GLfloat), ps->vertices_cache);

    // darken the background
    if (ps->darken > 0)
    {
	glBlendFunc(GL_ZERO, GL_ONE_MINUS_SRC_ALPHA);
	glColorPointer(4, GL_FLOAT, 4 * sizeof(GLfloat), ps->dcolors_cache);
	glDrawArrays(GL_QUADS, 0, numActive);
    }
    // draw particles
    glBlendFunc(GL_SRC_ALPHA, ps->blendMode);

    glColorPointer(4, GL_FLOAT, 4 * sizeof(GLfloat), ps->colors_cache);

    glDrawArrays(GL_QUADS, 0, numActive);

    glDisableClientState(GL_COLOR_ARRAY);

    glPopMatrix();
    glColor4usv(defaultColor);
    screenTexEnvMode(s, GL_REPLACE);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_BLEND);
}

static void
updateParticles (ParticleSystem * ps, float time)
{
    int i;
    Particle *part;
    float speed    = (time / 50.0);
    float slowdown = ps->slowdown * (1 - MAX(0.99, time / 1000.0)) * 1000;

    ps->active = FALSE;

    part = ps->particles;

    for (i = 0; i < ps->numParticles; i++, part++)
    {
	if (part->life > 0.0f)
	{
	    // move particle
	    part->x += part->xi / slowdown;
	    part->y += part->yi / slowdown;
	    part->z += part->zi / slowdown;

	    // modify speed
	    part->xi += part->xg * speed;
	    part->yi += part->yg * speed;
	    part->zi += part->zg * speed;

	    // modify life
	    part->life -= part->fade * speed;
	    ps->active  = TRUE;
	}
    }
}

static void
finiParticles (ParticleSystem * ps)
{
    free(ps->particles);
    if (ps->tex)
	glDeleteTextures(1, &ps->tex);

    if (ps->vertices_cache)
	free(ps->vertices_cache);
    if (ps->colors_cache)
	free(ps->colors_cache);
    if (ps->coords_cache)
	free(ps->coords_cache);
    if (ps->dcolors_cache)
	free(ps->dcolors_cache);
}

static void
genNewParticles(CompScreen     *s,
		ParticleSystem *ps,
		int            time)
{
    SHOWMOUSE_SCREEN(s);

    Bool rColor     = showmouseGetRandom (s);
    float life      = showmouseGetLife (s);
    float lifeNeg   = 1 - life;
    float fadeExtra = 0.2f * (1.01 - life);
    float max_new   = ps->numParticles * ((float)time / 50) * (1.05 - life);

    unsigned short *c = showmouseGetColor (s);

    float colr1 = (float)c[0] / 0xffff;
    float colg1 = (float)c[1] / 0xffff;
    float colb1 = (float)c[2] / 0xffff;
    float colr2 = 1.0 / 4.0 * (float)c[0] / 0xffff;
    float colg2 = 1.0 / 4.0 * (float)c[1] / 0xffff;
    float colb2 = 1.0 / 4.0 * (float)c[2] / 0xffff;
    float cola  = (float)c[3] / 0xffff;
    float rVal;

    float partw = showmouseGetSize (s) * 5;
    float parth = partw;

    Particle *part = ps->particles;
    int i, j;

    float pos[10][2];
    int nE       = MIN (10, showmouseGetEmitters (s));
    float rA     = (2 * M_PI) / nE;
    int radius   = showmouseGetRadius (s);
    for (i = 0; i < nE; i++)
    {
	pos[i][0]  = sin (ss->rot + (i * rA)) * radius;
	pos[i][0] += ss->posX;
	pos[i][1]  = cos (ss->rot + (i * rA)) * radius;
	pos[i][1] += ss->posY;
    }

    for (i = 0; i < ps->numParticles && max_new > 0; i++, part++)
    {
	if (part->life <= 0.0f)
	{
	    // give gt new life
	    rVal = (float)(random() & 0xff) / 255.0;
	    part->life = 1.0f;
	    part->fade = rVal * lifeNeg + fadeExtra; // Random Fade Value

	    // set size
	    part->width = partw;
	    part->height = parth;
	    rVal = (float)(random() & 0xff) / 255.0;
	    part->w_mod = part->h_mod = -1;

	    // choose random position

	    j        = random() % nE;
	    part->x  = pos[j][0];
	    part->y  = pos[j][1];
	    part->z  = 0.0;
	    part->xo = part->x;
	    part->yo = part->y;
	    part->zo = part->z;

	    // set speed and direction
	    rVal     = (float)(random() & 0xff) / 255.0;
	    part->xi = ((rVal * 20.0) - 10.0f);
	    rVal     = (float)(random() & 0xff) / 255.0;
	    part->yi = ((rVal * 20.0) - 10.0f);
	    part->zi = 0.0f;

	    if (rColor)
	    {
		// Random colors! (aka Mystical Fire)
		rVal    = (float)(random() & 0xff) / 255.0;
		part->r = rVal;
		rVal    = (float)(random() & 0xff) / 255.0;
		part->g = rVal;
		rVal    = (float)(random() & 0xff) / 255.0;
		part->b = rVal;
	    }
	    else
	    {
		rVal    = (float)(random() & 0xff) / 255.0;
		part->r = colr1 - rVal * colr2;
		part->g = colg1 - rVal * colg2;
		part->b = colb1 - rVal * colb2;
	    }
	    // set transparancy
	    part->a = cola;

	    // set gravity
	    part->xg = 0.0f;
	    part->yg = 0.0f;
	    part->zg = 0.0f;

	    ps->active = TRUE;
	    max_new   -= 1;
	}
    }
}


/* copied from Wallpaper plugin */
static Visual *
findArgbVisual (Display *dpy,
		int     screen)
{
    XVisualInfo         temp;
    int                 nvi;

    temp.screen  = screen;
    temp.depth   = 32;
    temp.class   = TrueColor;

    XVisualInfo *xvi = XGetVisualInfo (dpy,
				       VisualScreenMask |
				       VisualDepthMask  |
				       VisualClassMask,
				       &temp,
				       &nvi);
    if (!xvi)
	return 0;

    Visual            *visual = 0;
    XRenderPictFormat *format;

    for (int i = 0; i < nvi; ++i)
    {
	format = XRenderFindVisualFormat (dpy, xvi[i].visual);

	if (format->type == PictTypeDirect && format->direct.alphaMask)
	{
	    visual = xvi[i].visual;
	    break;
	}
    }

    XFree (xvi);

    return visual;
}

/*
 * Shapes the crosshairs on the cairo context.  This can be used for
 * painting, clipping or anything else that requires a Cairo shape.
 */
static void
shapeCrosshair (CompScreen *s,
		cairo_t *cr,
		const int posX,
		const int posY)
{
    SHOWMOUSE_SCREEN (s);
    int thickness = showmouseGetGuideThickness (s);
    int empty_radius = showmouseGetGuideEmptyRadius (s);
    int x = posX - ss->overlay.xOffset;
    int y = posY - ss->overlay.yOffset;

    cairo_rectangle (cr,
		     0,
		     y - thickness / 2,
		     x - empty_radius,
		     thickness);
    cairo_rectangle (cr,
		     x + empty_radius,
		     y - thickness / 2,
		     s->width - x - empty_radius,
		     thickness);
    cairo_rectangle (cr,
		     x - thickness / 2,
		     0,
		     thickness,
		     y - empty_radius);
    cairo_rectangle (cr,
		     x - thickness / 2,
		     y + empty_radius,
		     thickness,
		     s->height - y - empty_radius);
}

static void
paintCrosshair (CompScreen *s,
		cairo_t *cr,
		const int newPosX,
		const int newPosY)
{
    SHOWMOUSE_SCREEN (s);
    unsigned short *color = showmouseGetGuideColor (s);

    shapeCrosshair (s, cr, ss->posX, ss->posY);
    cairo_set_operator (cr, CAIRO_OPERATOR_CLEAR);
    cairo_fill (cr);

    shapeCrosshair (s, cr, newPosX, newPosY);
    cairo_set_source_rgba (cr,
			   color[0] / 65535.0,
			   color[1] / 65535.0,
			   color[2] / 65535.0,
			   color[3] / 65535.0);
    cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
    cairo_fill (cr);
}

static void
clearOverlayWindow (CompScreen *s)
{
    SHOWMOUSE_SCREEN (s);
    cairo_t *cr = cairo_create (ss->overlay.surface);

    cairo_set_operator (cr, CAIRO_OPERATOR_CLEAR);
    cairo_paint (cr);
    paintCrosshair (s, cr, ss->posX, ss->posY);
    cairo_destroy (cr);
}

static void
showOverlayWindow (CompScreen *s)
{
    SHOWMOUSE_SCREEN (s);

    if (!ss->overlay.window)
    {
	Display		    *dpy = s->display->display;
	XSizeHints	     xsh;
	XWMHints	     xwmh;
	XClassHint           xch;
	Atom		     state[] = {
	    s->display->winStateAboveAtom,
	    s->display->winStateStickyAtom,
	    s->display->winStateSkipTaskbarAtom,
	    s->display->winStateSkipPagerAtom,
	};
	XSetWindowAttributes attr;
	Visual		    *visual;
	XserverRegion	     region;

	visual = findArgbVisual (dpy, s->screenNum);
	if (!visual)
	    return;

	xsh.flags       = PSize | PPosition | PWinGravity;
	xsh.width       = s->width;
	xsh.height      = s->height;
	xsh.win_gravity = StaticGravity;

	xwmh.flags = InputHint;
	xwmh.input = 0;

	xch.res_name  = (char *)"compiz";
	xch.res_class = (char *)"mouselocalizer-window";

	attr.background_pixel = 0;
	attr.border_pixel     = 0;
	attr.colormap	      = XCreateColormap (dpy, s->root,
						 visual, AllocNone);
	attr.override_redirect = TRUE;

	ss->overlay.window =
	    XCreateWindow (dpy, s->root,
			   0, 0,
			   (unsigned) xsh.width, (unsigned) xsh.height, 0,
			   32, InputOutput, visual,
			   CWBackPixel | CWBorderPixel | CWColormap | CWOverrideRedirect, &attr);

	XSelectInput (dpy, ss->overlay.window, ExposureMask | StructureNotifyMask);

	XSetWMProperties (dpy, ss->overlay.window, NULL, NULL,
			  programArgv, programArgc,
			  &xsh, &xwmh, &xch);

	XChangeProperty (dpy, ss->overlay.window,
			 s->display->winStateAtom,
			 XA_ATOM, 32, PropModeReplace,
			 (unsigned char *) state,
			 sizeof state / sizeof *state);

	XChangeProperty (dpy, ss->overlay.window,
			 s->display->winTypeAtom,
			 XA_ATOM, 32, PropModeReplace,
			 (unsigned char *) &s->display->winTypeUtilAtom, 1);

	setWindowProp (s->display, ss->overlay.window,
		       s->display->winDesktopAtom, 0xffffffff);

	/* make the window an output-only window by having no shape for input */
	region = XFixesCreateRegion (dpy, NULL, 0);
	XFixesSetWindowShapeRegion (dpy, ss->overlay.window, ShapeBounding, 0, 0, None);
	XFixesSetWindowShapeRegion (dpy, ss->overlay.window, ShapeInput, 0, 0, region);
	XFixesDestroyRegion (dpy, region);

	ss->overlay.surface = cairo_xlib_surface_create (dpy,
							 ss->overlay.window, visual,
							 xsh.width, xsh.height);
    }

    /* we need a valid initial mouse position right away, possibly
     * before next Compiz paint, so retrieve it */
    if (!ss->pollHandle)
    {
	SHOWMOUSE_DISPLAY (s->display);

	(*sd->mpFunc->getCurrentPosition) (s, &ss->posX, &ss->posY);
    }

    XMapWindow (s->display->display, ss->overlay.window);

    /* in case we re-show, we don't always get an EXPOSE event, so
     * force an initial redraw */
    clearOverlayWindow (s);
}

static void
hideOverlayWindow (CompScreen *s)
{
    SHOWMOUSE_SCREEN (s);

    if (ss->overlay.window)
	XUnmapWindow (s->display->display, ss->overlay.window);
}

static CompScreen *
findScreenForXWindow (CompDisplay *d,
		      Window xid)
{
    XWindowAttributes attr;

    /* FIXME: isn't there a cheaper way to find the CompScreen? */
    if (XGetWindowAttributes (d->display, xid, &attr))
	return findScreenAtDisplay (d, attr.root);

    return NULL;
}

static void
showmouseHandleEvent (CompDisplay *d,
		      XEvent *event)
{
    CompScreen *s;
    SHOWMOUSE_DISPLAY (d);

    UNWRAP (sd, d, handleEvent);
    (*d->handleEvent) (d, event);
    WRAP (sd, d, handleEvent, showmouseHandleEvent);

    switch (event->type)
    {
    case Expose:
	s = findScreenForXWindow (d, event->xexpose.window);
	if (s)
	{
	    SHOWMOUSE_SCREEN (s);

	    if (ss->active && showmouseGetCrosshair (s) &&
		event->xexpose.window == ss->overlay.window)
	    {
		cairo_t *cr = cairo_create (ss->overlay.surface);

		cairo_rectangle (cr, event->xexpose.x, event->xexpose.y,
				 event->xexpose.width, event->xexpose.height);
		cairo_clip (cr);
		paintCrosshair (s, cr, ss->posX, ss->posY);
		cairo_destroy (cr);
	    }
	}
	break;

    case ConfigureNotify:
	s = findScreenForXWindow (d, event->xconfigure.window);
	if (s)
	{
	    SHOWMOUSE_SCREEN (s);

	    if (event->xconfigure.window == ss->overlay.window)
	    {
		ss->overlay.xOffset = event->xconfigure.x;
		ss->overlay.yOffset = event->xconfigure.y;
	    }
	}
	break;

    case MapNotify:
	s = findScreenForXWindow (d, event->xmap.window);
	if (s)
	{
	    SHOWMOUSE_SCREEN (s);

	    if (event->xmap.window == ss->overlay.window)
	    {
		XWindowAttributes attr;

		XGetWindowAttributes (event->xmap.display, event->xmap.window, &attr);
		ss->overlay.xOffset = attr.x;
		ss->overlay.yOffset = attr.y;
	    }
	    else if (ss->overlay.window)
		XRaiseWindow (s->display->display, ss->overlay.window);
	}
	break;
    }
}

static void
damageRegion (CompScreen *s)
{
    REGION   r;
    int      i;
    Particle *p;
    float    w, h, x1, x2, y1, y2;

    SHOWMOUSE_SCREEN (s);

    if (!ss->ps)
	return;

    x1 = s->width;
    x2 = 0;
    y1 = s->height;
    y2 = 0;

    p = ss->ps->particles;

    for (i = 0; i < ss->ps->numParticles; i++, p++)
    {
	w = p->width / 2;
	h = p->height / 2;

	w += (w * p->w_mod) * p->life;
	h += (h * p->h_mod) * p->life;
	
	x1 = MIN (x1, p->x - w);
	x2 = MAX (x2, p->x + w);
	y1 = MIN (y1, p->y - h);
	y2 = MAX (y2, p->y + h);
    }

    r.rects = &r.extents;
    r.numRects = r.size = 1;

    r.extents.x1 = floor (x1);
    r.extents.x2 = ceil (x2);
    r.extents.y1 = floor (y1);
    r.extents.y2 = ceil (y2);

    damageScreenRegion (s, &r);
}

static void
positionUpdate (CompScreen *s,
		int        x,
		int        y)
{
    SHOWMOUSE_SCREEN (s);

    if (showmouseGetCrosshair (s))
    {
	cairo_t *cr = cairo_create (ss->overlay.surface);

	paintCrosshair (s, cr, x, y);
	cairo_destroy (cr);
    }

    ss->posX = x;
    ss->posY = y;
}


static void
showmousePreparePaintScreen (CompScreen *s,
			     int        time)
{
    SHOWMOUSE_SCREEN (s);
    SHOWMOUSE_DISPLAY (s->display);

    if (ss->active && !ss->pollHandle)
    {
	(*sd->mpFunc->getCurrentPosition) (s, &ss->posX, &ss->posY);
	ss->pollHandle = (*sd->mpFunc->addPositionPolling) (s, positionUpdate);
    }

    if (ss->active && !ss->ps && showmouseGetParticles (s))
    {
	ss->ps = calloc(1, sizeof(ParticleSystem));
	if (!ss->ps)
	{
	    UNWRAP (ss, s, preparePaintScreen);
	    (*s->preparePaintScreen) (s, time);
	    WRAP (ss, s, preparePaintScreen, showmousePreparePaintScreen);
	    return;
	}
	initParticles(showmouseGetNumParticles (s), ss->ps);

	ss->ps->slowdown = showmouseGetSlowdown (s);
	ss->ps->darken = showmouseGetDarken (s);
	ss->ps->blendMode = (showmouseGetBlend(s)) ? GL_ONE :
			    GL_ONE_MINUS_SRC_ALPHA;

	glGenTextures(1, &ss->ps->tex);
	glBindTexture(GL_TEXTURE_2D, ss->ps->tex);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 32, 32, 0,
			GL_RGBA, GL_UNSIGNED_BYTE, starTex);
	glBindTexture(GL_TEXTURE_2D, 0);
    }

    if (ss->active && showmouseGetParticles (s))
	ss->rot = fmod (ss->rot + (((float)time / 1000.0) * 2 * M_PI *
			showmouseGetRotationSpeed (s)), 2 * M_PI);

    if (ss->ps && ss->ps->active)
	updateParticles (ss->ps, time);

    if (ss->ps && ss->ps->active)
	damageRegion (s);

    if (ss->ps && ss->active && showmouseGetParticles (s))
	genNewParticles (s, ss->ps, time);

    UNWRAP (ss, s, preparePaintScreen);
    (*s->preparePaintScreen) (s, time);
    WRAP (ss, s, preparePaintScreen, showmousePreparePaintScreen);
}

static void
showmouseDonePaintScreen (CompScreen *s)
{
    SHOWMOUSE_SCREEN (s);
    SHOWMOUSE_DISPLAY (s->display);

    if (ss->ps && ss->ps->active)
	damageRegion (s);

    if (!ss->active && ss->pollHandle)
    {
	(*sd->mpFunc->removePositionPolling) (s, ss->pollHandle);
	ss->pollHandle = 0;
    }

    if ((!ss->active || !showmouseGetParticles (s)) &&
	ss->ps && !ss->ps->active)
    {
	finiParticles (ss->ps);
	free (ss->ps);
	ss->ps = NULL;
    }

    UNWRAP (ss, s, donePaintScreen);
    (*s->donePaintScreen) (s);
    WRAP (ss, s, donePaintScreen, showmouseDonePaintScreen);
}

static Bool
showmousePaintOutput (CompScreen              *s,
		      const ScreenPaintAttrib *sa,
		      const CompTransform     *transform,
		      Region                  region,
		      CompOutput              *output,
		      unsigned int            mask)
{
    Bool           status;
    CompTransform  sTransform = *transform;

    SHOWMOUSE_SCREEN (s);

    UNWRAP (ss, s, paintOutput);
    status = (*s->paintOutput) (s, sa, transform, region, output, mask);
    WRAP (ss, s, paintOutput, showmousePaintOutput);

    if (!ss->ps || !ss->ps->active)
	return status;

    transformToScreenSpace (s, output, -DEFAULT_Z_CAMERA, &sTransform);

    glPushMatrix ();
    glLoadMatrixf (sTransform.m);

    drawParticles (s, ss->ps);

    glPopMatrix();

    glColor4usv (defaultColor);

    return status;
}

static Bool
showmouseTerminate (CompDisplay     *d,
		    CompAction      *action,
		    CompActionState state,
		    CompOption      *option,
		    int             nOption)
{
    CompScreen *s;
    Window     xid;

    xid = getIntOptionNamed (option, nOption, "root", 0);

    s = findScreenAtDisplay (d, xid);
    if (s)
    {
	SHOWMOUSE_SCREEN (s);

	ss->active = FALSE;
	damageRegion (s);

	hideOverlayWindow (s);

	return TRUE;
    }
    return FALSE;
}

static Bool
showmouseInitiate (CompDisplay     *d,
		   CompAction      *action,
		   CompActionState state,
		   CompOption      *option,
		   int             nOption)
{
    CompScreen *s;
    Window     xid;

    xid    = getIntOptionNamed (option, nOption, "root", 0);

    s = findScreenAtDisplay (d, xid);
    if (s)
    {
	SHOWMOUSE_SCREEN (s);

	if (ss->active)
	    return showmouseTerminate (d, action, state, option, nOption);

	ss->active = TRUE;

	if (showmouseGetCrosshair (s))
	    showOverlayWindow (s);

	return TRUE;
    }
    return FALSE;
}

static void
guideOptionNotify (CompScreen            *s,
		   CompOption            *option,
		   ShowmouseScreenOptions num)
{
    switch (num)
    {
    case ShowmouseScreenOptionCrosshair:
    {
	SHOWMOUSE_SCREEN (s);

	if (ss->active && showmouseGetCrosshair (s))
	    showOverlayWindow (s);
	else
	    hideOverlayWindow (s);
	break;
    }

    default:
	if (showmouseGetCrosshair (s))
	    clearOverlayWindow (s);
	break;
    }
}


static Bool
showmouseInitScreen (CompPlugin *p,
		     CompScreen *s)
{
    SHOWMOUSE_DISPLAY (s->display);

    ShowmouseScreen *ss = (ShowmouseScreen *) calloc (1, sizeof (ShowmouseScreen) );

    if (!ss)
	return FALSE;

    s->base.privates[sd->screenPrivateIndex].ptr = ss;

    WRAP (ss, s, paintOutput, showmousePaintOutput);
    WRAP (ss, s, preparePaintScreen, showmousePreparePaintScreen);
    WRAP (ss, s, donePaintScreen, showmouseDonePaintScreen);

    ss->active = FALSE;

    ss->pollHandle = 0;

    ss->ps  = NULL;
    ss->rot = 0;

    showmouseSetCrosshairNotify (s, guideOptionNotify);
    showmouseSetGuideThicknessNotify (s, guideOptionNotify);
    showmouseSetGuideEmptyRadiusNotify (s, guideOptionNotify);
    showmouseSetGuideColorNotify (s, guideOptionNotify);

    return TRUE;
}


static void
showmouseFiniScreen (CompPlugin *p,
		     CompScreen *s)
{
    SHOWMOUSE_SCREEN (s);
    SHOWMOUSE_DISPLAY (s->display);

    //Restore the original function
    UNWRAP (ss, s, paintOutput);
    UNWRAP (ss, s, preparePaintScreen);
    UNWRAP (ss, s, donePaintScreen);

    if (ss->pollHandle)
	(*sd->mpFunc->removePositionPolling) (s, ss->pollHandle);

    if (ss->active || (ss->ps && ss->ps->active))
	damageScreen (s);

    if (ss->overlay.window)
	XDestroyWindow (s->display->display, ss->overlay.window);
    if (ss->overlay.surface)
	cairo_surface_destroy (ss->overlay.surface);

    //Free the pointer
    free (ss);
}

static Bool
showmouseInitDisplay (CompPlugin  *p,
		      CompDisplay *d)
{
    //Generate a showmouse display
    ShowmouseDisplay *sd;
    int              index;

    if (!checkPluginABI ("core", CORE_ABIVERSION) ||
        !checkPluginABI ("mousepoll", MOUSEPOLL_ABIVERSION))
	return FALSE;

    if (!getPluginDisplayIndex (d, "mousepoll", &index))
	return FALSE;

    sd = (ShowmouseDisplay *) malloc (sizeof (ShowmouseDisplay));

    if (!sd)
	return FALSE;
 
    //Allocate a private index
    sd->screenPrivateIndex = allocateScreenPrivateIndex (d);

    //Check if its valid
    if (sd->screenPrivateIndex < 0)
    {
	//Its invalid so free memory and return
	free (sd);
	return FALSE;
    }

    sd->mpFunc = d->base.privates[index].ptr;

    showmouseSetInitiateInitiate (d, showmouseInitiate);
    showmouseSetInitiateTerminate (d, showmouseTerminate);
    showmouseSetInitiateButtonInitiate (d, showmouseInitiate);
    showmouseSetInitiateButtonTerminate (d, showmouseTerminate);
    showmouseSetInitiateEdgeInitiate (d, showmouseInitiate);
    showmouseSetInitiateEdgeTerminate (d, showmouseTerminate);

    //Record the display
    d->base.privates[displayPrivateIndex].ptr = sd;

    WRAP (sd, d, handleEvent, showmouseHandleEvent);

    return TRUE;
}

static void
showmouseFiniDisplay (CompPlugin  *p,
		      CompDisplay *d)
{
    SHOWMOUSE_DISPLAY (d);

    UNWRAP (sd, d, handleEvent);

    //Free the private index
    freeScreenPrivateIndex (d, sd->screenPrivateIndex);
    //Free the pointer
    free (sd);
}



static Bool
showmouseInit (CompPlugin * p)
{
    displayPrivateIndex = allocateDisplayPrivateIndex();

    if (displayPrivateIndex < 0)
	return FALSE;

    return TRUE;
}

static void
showmouseFini (CompPlugin * p)
{
    if (displayPrivateIndex >= 0)
	freeDisplayPrivateIndex (displayPrivateIndex);
}

static CompBool
showmouseInitObject (CompPlugin *p,
		     CompObject *o)
{
    static InitPluginObjectProc dispTab[] = {
	(InitPluginObjectProc) 0, /* InitCore */
	(InitPluginObjectProc) showmouseInitDisplay,
	(InitPluginObjectProc) showmouseInitScreen
    };

    RETURN_DISPATCH (o, dispTab, ARRAY_SIZE (dispTab), TRUE, (p, o));
}

static void
showmouseFiniObject (CompPlugin *p,
		     CompObject *o)
{
    static FiniPluginObjectProc dispTab[] = {
	(FiniPluginObjectProc) 0, /* FiniCore */
	(FiniPluginObjectProc) showmouseFiniDisplay,
	(FiniPluginObjectProc) showmouseFiniScreen
    };

    DISPATCH (o, dispTab, ARRAY_SIZE (dispTab), (p, o));
}

CompPluginVTable showmouseVTable = {
    "showmouse",
    0,
    showmouseInit,
    showmouseFini,
    showmouseInitObject,
    showmouseFiniObject,
    0,
    0
};

CompPluginVTable *
getCompPluginInfo (void)
{
    return &showmouseVTable;
}
