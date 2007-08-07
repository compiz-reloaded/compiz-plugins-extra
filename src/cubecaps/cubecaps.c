/*
 * Compiz Cube Caps plugin
 *
 * cubecaps.c
 *
 * Copyright : (C) 2007 Guillaume Seguin
 * E-mail    : guillaume@segu.in
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
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <signal.h>
#include <unistd.h>

#include <compiz.h>
#include <cube.h>
#include "cubecaps_options.h"

static int displayPrivateIndex;

static int cubeDisplayPrivateIndex;

typedef struct _CubeCapsDisplay
{
    int screenPrivateIndex;
} CubeCapsDisplay;

typedef struct _CubeCap
{
	int		    current;
	CompListValue	    *files;

	CompTexture	    texture;
	GLfloat		    tc[12];

	Bool		    scale;
	int		    pw;
	int		    ph;
} CubeCap;

typedef struct _CubeCapsScreen
{
    CubePaintTopProc	    paintTop;
    CubePaintBottomProc	    paintBottom;

    CubeCap		    topCap;
    CubeCap		    bottomCap;
} CubeCapsScreen;

#define GET_CUBECAPS_DISPLAY(d)						\
    ((CubeCapsDisplay *) (d)->privates[displayPrivateIndex].ptr)
#define CUBECAPS_DISPLAY(d)						\
    CubeCapsDisplay *ccd = GET_CUBECAPS_DISPLAY (d);

#define GET_CUBECAPS_SCREEN(s, ccd)					\
    ((CubeCapsScreen *) (s)->privates[(ccd)->screenPrivateIndex].ptr)
#define CUBECAPS_SCREEN(s)						\
    CubeCapsScreen *ccs = GET_CUBECAPS_SCREEN (s,			\
			  GET_CUBECAPS_DISPLAY (s->display))

/* Actual caps handling ----------------------------------------------------- */

/*
 * Initiate a CubeCap
 */
static void
cubecapsInitCap (CompScreen *s, CubeCap *cap)
{
    memset (cap->tc, 0, sizeof (cap->tc));

    initTexture (s, &cap->texture);

    cap->current    = 0;
    cap->files	    = NULL;

    cap->scale	    = FALSE;
    cap->pw	    = 0;
    cap->ph	    = 0;
}

/*
 * Prepare cap texture coordinates
 */
static void
cubecapsInitTextureCoords (CompScreen * s, CubeCap * cap,
			   unsigned int width, unsigned int height)
{
    float x1, x2, y1, y2;
    CompMatrix *matrix;

    if (!cap)
	return;

    matrix = &cap->texture.matrix;

    if (cap->scale)
    {
	x1 = 0.0f;
	y1 = 0.0f;
	x2 = width;
	y2 = height;
    }
    else
    {
	int bigscr, i;
	int bigWidth, bigHeight;

	CUBE_SCREEN(s);
	bigWidth = s->width;
	bigHeight = s->height;

	/* Scale the texture in a sane way for multi head too */
	if (s->nOutputDev > 1 && cs->moMode != CUBE_MOMODE_ONE)
	{
	    for (i = bigscr = 0; i < s->nOutputDev; i++)
		if (s->outputDev[i].width > s->outputDev[bigscr].width)
		    bigscr = i;
	    bigWidth = s->outputDev[bigscr].width;
	    bigHeight = s->outputDev[bigscr].height;
	}

	x1 = width / 2.0f - bigWidth / 2.0f;
	y1 = height / 2.0f - bigHeight / 2.0f;
	x2 = width / 2.0f + bigWidth / 2.0f;
	y2 = height / 2.0f + bigHeight / 2.0f;
    }

    cap->tc[0] = COMP_TEX_COORD_X (matrix, width / 2.0f);
    cap->tc[1] = COMP_TEX_COORD_Y (matrix, height / 2.0f);

    cap->tc[2] = COMP_TEX_COORD_X (matrix, x2);
    cap->tc[3] = COMP_TEX_COORD_Y (matrix, y1);

    cap->tc[4] = COMP_TEX_COORD_X (matrix, x1);
    cap->tc[5] = COMP_TEX_COORD_Y (matrix, y1);

    cap->tc[6] = COMP_TEX_COORD_X (matrix, x1);
    cap->tc[7] = COMP_TEX_COORD_Y (matrix, y2);

    cap->tc[8] = COMP_TEX_COORD_X (matrix, x2);
    cap->tc[9] = COMP_TEX_COORD_Y (matrix, y2);

    cap->tc[10] = COMP_TEX_COORD_X (matrix, x2);
    cap->tc[11] = COMP_TEX_COORD_Y (matrix, y1);
}

/*
 * Attempt to load current cap image (if any)
 */
static void
cubecapsLoadCap (CompScreen *s,
		 CubeCap    *cap)
{
    unsigned int    width, height;
    int		    pw, ph;

    CUBE_SCREEN (s);

    if (!cs->fullscreenOutput)
    {
	pw = s->width;
	ph = s->height;
    }
    else
    {
	pw = s->outputDev[0].width;
	ph = s->outputDev[0].height;
    }

    if (!cap->files || !cap->files->nValue || cap->pw != pw || cap->ph != ph)
    {
	finiTexture (s, &cap->texture);
	initTexture (s, &cap->texture);

	if (!cap->files || !cap->files->nValue)
	    return;
    }

    cap->current = cap->current % cap->files->nValue;

    if (!readImageToTexture (s, &cap->texture,
			     cap->files->value[cap->current].s,
			     &width, &height))
    {
	compLogMessage (s->display, "cubecaps", CompLogLevelWarn,
			"Failed to load image: %s",
			cap->files->value[cap->current].s);

	finiTexture (s, &cap->texture);
	initTexture (s, &cap->texture);

	return;
    }

    cubecapsInitTextureCoords (s, cap, width, height);
}

/*
 * Paint a cap
 */
static void
cubecapsPaintCap (CompScreen	    *s,
		  int		    offset,
		  CubeCap	    *capOutside,
		  CubeCap	    *capInside,
		  unsigned short    *colorOutside,
		  unsigned short    *colorInside)
{
    CubeCap *cap;
    unsigned short opacity;

    CUBE_SCREEN(s);

    opacity = cs->desktopOpacity;

    if (cs->invert == 1)
    {
	cap = capOutside;
	if (opacity == OPAQUE)
	    opacity = colorOutside[3];
	glColor4us (colorOutside[0],
		    colorOutside[1],
		    colorOutside[2],
		    opacity);
    }
    else if (cs->invert != 1)
    {
	cap = capInside;
	if (opacity == OPAQUE)
	    opacity = colorOutside[4];
	glColor4us (colorInside[0],
		    colorInside[1],
		    colorInside[2],
		    opacity);
    }

    glTranslatef (cs->outputXOffset, -cs->outputYOffset, 0.0f);
    glScalef (cs->outputXScale, cs->outputYScale, 1.0f);

    glVertexPointer (3, GL_FLOAT, 0, cs->vertices);

    /* It is not really a good idea to draw the cap texture when there are 
     * only three viewports */
    if (cap && cap->texture.name && s->hsize >= 4)
    {
	glEnable (GL_BLEND);
	/* Draw cap once and reset color so that image will get correctly
	 * blended, and for non-4-horizontal-viewports setups */
	if (opacity != OPAQUE)
	{
	    screenTexEnvMode (s, GL_MODULATE);
	    glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	    glDrawArrays (GL_TRIANGLE_FAN, offset, cs->nVertices >> 1);
	    glBlendFunc (GL_DST_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	}
	else
	{
	    /* Apply blend strategy to blend correctly color and image */
	    glDrawArrays (GL_TRIANGLE_FAN, offset, cs->nVertices >> 1);
	    glBlendFunc (GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
	}
	
	glColor4usv (defaultColor);

	enableTexture (s, &cap->texture, COMP_TEXTURE_FILTER_GOOD);

	/* Use CLAMP_TO_BORDER if available to avoid weird looking clamping 
	 * of non-scaled images (it also improves scaled images a bit but 
	 * that's much less obvious) */
	if (s->textureBorderClamp)
	{
	    glTexParameteri (cap->texture.target, GL_TEXTURE_WRAP_S,
			     GL_CLAMP_TO_BORDER);
	    glTexParameteri (cap->texture.target, GL_TEXTURE_WRAP_T,
			     GL_CLAMP_TO_BORDER);
	}

	if (s->hsize == 4)
	{
	    /* 4 viewports is pretty much straight forward ... */
	    glTexCoordPointer (2, GL_FLOAT, 0, cap->tc - (offset << 1));
	    glDrawArrays (GL_TRIANGLE_FAN, offset, cs->nVertices >> 1);
	}
	else if (s->hsize > 4)
	{
	    /* Paint image using custom vertexes */
	    int centerx = *cs->vertices;
	    int centery = *(cs->vertices + 1);
	    int centerz = *(cs->vertices + 2);
	    GLfloat x1, y1, x2, y2;
	    x1 = cap->tc[4];
	    x2 = cap->tc[2];
	    y1 = cap->tc[3];
	    y2 = cap->tc[9];

	    glBegin (GL_QUADS);

	    if (offset)
		centery -= 1;

	    if (offset)
	    {
		glTexCoord2f (x1, y1);
		glVertex3f (centerx - 0.5, centery + 0.5, centerz + 0.5);
		glTexCoord2f (x1, y2);
		glVertex3f (centerx - 0.5, centery + 0.5, centerz - 0.5);
		glTexCoord2f (x2, y2);
		glVertex3f (centerx + 0.5, centery + 0.5, centerz - 0.5);
		glTexCoord2f (x2, y1);
		glVertex3f (centerx + 0.5, centery + 0.5, centerz + 0.5);
	    }
	    else
	    {
		glTexCoord2f (x2,y2);
		glVertex3f (centerx + 0.5, centery + 0.5, centerz + 0.5);
		glTexCoord2f (x2, y1);
		glVertex3f (centerx + 0.5, centery + 0.5, centerz - 0.5);
		glTexCoord2f (x1, y1);
		glVertex3f (centerx - 0.5, centery + 0.5, centerz - 0.5);
		glTexCoord2f (x1, y2);
		glVertex3f (centerx - 0.5, centery + 0.5, centerz + 0.5);
	    }

	    glEnd ();
	}

	disableTexture (s, &cap->texture);

	if (opacity != OPAQUE)
	    screenTexEnvMode (s, GL_REPLACE);

	glDisable (GL_BLEND);
	glBlendFunc (GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    }
    else
    {
	glDisableClientState (GL_TEXTURE_COORD_ARRAY);
	glDrawArrays (GL_TRIANGLE_FAN, offset, cs->nVertices >> 1);
	glEnableClientState (GL_TEXTURE_COORD_ARRAY);
    }
}

/* Cube hooks --------------------------------------------------------------- */

/*
 * Paint top cube face
 */
static void
cubecapsPaintTop (CompScreen		  *s,
		  const ScreenPaintAttrib *sAttrib,
		  const CompTransform     *transform,
		  CompOutput		  *output,
		  int			  size)
{
    ScreenPaintAttrib sa = *sAttrib;
    CompTransform     sTransform = *transform;

    CUBE_SCREEN (s);
    CUBECAPS_SCREEN (s);

    /* Only paint if required */
    if (!cubecapsGetDrawTop (s))
	return;

    screenLighting (s, TRUE);

    glPushMatrix ();

    /* Readjust cap orientation ... */
    if (cs->invert == 1)
    {
	sa.yRotate += (360.0f / size) * (cs->xRotations + 1);
	if (!cubecapsGetAdjustTop (s)) /* ... Or not */
	    sa.yRotate -= (360.0f / size) * s->x;
    }
    else
    {
	sa.yRotate -= (360.0f / size) * (cs->xRotations - 1);
	if (!cubecapsGetAdjustTop (s)) /* ... Or not */
	    sa.yRotate += (360.0f / size) * s->x;
    }

    (*s->applyScreenTransform) (s, &sa, output, &sTransform);

    glLoadMatrixf (sTransform.m);

    /* Actually paint the cap */
    cubecapsPaintCap (s, 0, &ccs->topCap, &ccs->bottomCap,
		      cubecapsGetTopColor (s), cubecapsGetBottomColor (s));

    glPopMatrix ();

    glColor4usv (defaultColor);
}

/*
 * Paint bottom cube face
 */
static void
cubecapsPaintBottom (CompScreen		     *s,
		     const ScreenPaintAttrib *sAttrib,
		     const CompTransform     *transform,
		     CompOutput		     *output,
		     int		     size)
{
    ScreenPaintAttrib sa = *sAttrib;
    CompTransform     sTransform = *transform;

    CUBE_SCREEN (s);
    CUBECAPS_SCREEN (s);

    /* Only paint if required */
    if (!cubecapsGetDrawBottom (s))
	return;

    screenLighting (s, TRUE);

    glPushMatrix ();

    /* Readjust cap orientation ... */
    if (cs->invert == 1)
    {
	sa.yRotate += (360.0f / size) * cs->xRotations;
	if (!cubecapsGetAdjustBottom (s)) /* ... Or not */
	    sa.yRotate -= (360.0f / size) * s->x;
    }
    else
    {
	sa.yRotate -= (360.0f / size) * cs->xRotations;
	if (!cubecapsGetAdjustBottom (s)) /* ... Or not */
	    sa.yRotate += (360.0f / size) * s->x;
    }

    (*s->applyScreenTransform) (s, &sa, output, &sTransform);

    glLoadMatrixf (sTransform.m);

    /* Actually paint the cap */
    cubecapsPaintCap (s, cs->nVertices >> 1, &ccs->bottomCap, &ccs->topCap,
		      cubecapsGetBottomColor (s), cubecapsGetTopColor (s));

    glPopMatrix ();

    glColor4usv (defaultColor);
}

/* Settings handling -------------------------------------------------------- */

/*
 * Switch cap, load it and damage screen if possible
 */
static void
cubecapsChangeCap (CompScreen *s,
		   CubeCap    *cap,
		   int	      change)
{
    if (cap->files && cap->files->nValue)
    {
	int count = cap->files->nValue;
	cap->current = (cap->current + change + count) % count;
	cubecapsLoadCap (s, cap);
	damageScreen (s);
    }
}

/*
 * Top images list changed, reload top cap if any
 */
static void
cubecapsTopImagesChanged (CompScreen		*s,
			  CompOption		*opt,
			  CubecapsScreenOptions num)
{
    CUBECAPS_SCREEN (s);

    ccs->topCap.files = cubecapsGetTopImages (s);
    cubecapsChangeCap (s, &ccs->topCap, 0);
}

/*
 * Bottom images list changed, reload bottom cap if any
 */
static void
cubecapsBottomImagesChanged (CompScreen		   *s,
			     CompOption		   *opt,
			     CubecapsScreenOptions num)
{
    CUBECAPS_SCREEN (s);

    ccs->bottomCap.files = cubecapsGetBottomImages (s);
    cubecapsChangeCap (s, &ccs->bottomCap, 0);
}

/*
 * scale_top_image setting changed, reload top cap if any to update texture
 * coordinates
 */
static void
cubecapsScaleTopImageChanged (CompScreen	    *s,
			      CompOption	    *opt,
			      CubecapsScreenOptions num)
{
    CUBECAPS_SCREEN (s);

    ccs->topCap.scale = cubecapsGetScaleTopImage (s);
    cubecapsChangeCap (s, &ccs->topCap, 0);
}

/*
 * scale_bottom_image setting changed, reload bottom cap if any to update
 * texture coordinates
 */
static void
cubecapsScaleBottomImageChanged (CompScreen		*s,
				 CompOption		*opt,
				 CubecapsScreenOptions	num)
{
    CUBECAPS_SCREEN (s);

    ccs->bottomCap.scale = cubecapsGetScaleBottomImage (s);
    cubecapsChangeCap (s, &ccs->bottomCap, 0);
}

/* Actions handling --------------------------------------------------------- */

/*
 * Switch to next top image
 */
static Bool
cubecapsTopNext (CompDisplay     *d,
		 CompAction      *action,
		 CompActionState state,
		 CompOption      *option,
		 int		 nOption)
{
    CompScreen *s;
    Window     xid;

    xid = getIntOptionNamed (option, nOption, "root", 0);

    s = findScreenAtDisplay (d, xid);

    if (s)
    {
	CUBECAPS_SCREEN (s);
	cubecapsChangeCap (s, &ccs->topCap, 1);
    }

    return FALSE;
}

/*
 * Switch to previous top image
 */
static Bool
cubecapsTopPrev (CompDisplay     *d,
		 CompAction      *action,
		 CompActionState state,
		 CompOption      *option,
		 int		 nOption)
{
    CompScreen *s;
    Window     xid;

    xid = getIntOptionNamed (option, nOption, "root", 0);

    s = findScreenAtDisplay (d, xid);
    if (s)
    {
	CUBECAPS_SCREEN (s);
	cubecapsChangeCap (s, &ccs->topCap, -1);
    }

    return FALSE;
}

/*
 * Switch to next bottom image
 */
static Bool
cubecapsBottomNext (CompDisplay     *d,
		    CompAction      *action,
		    CompActionState state,
		    CompOption      *option,
		    int		    nOption)
{
    CompScreen *s;
    Window     xid;

    xid = getIntOptionNamed (option, nOption, "root", 0);

    s = findScreenAtDisplay (d, xid);

    if (s)
    {
	CUBECAPS_SCREEN (s);
	cubecapsChangeCap (s, &ccs->bottomCap, 1);
    }

    return FALSE;
}

/*
 * Switch to previous bottom image
 */
static Bool
cubecapsBottomPrev (CompDisplay     *d,
		    CompAction      *action,
		    CompActionState state,
		    CompOption      *option,
		    int		    nOption)
{
    CompScreen *s;
    Window     xid;

    xid = getIntOptionNamed (option, nOption, "root", 0);

    s = findScreenAtDisplay (d, xid);
    if (s)
    {
	CUBECAPS_SCREEN (s);
	cubecapsChangeCap (s, &ccs->bottomCap, -1);
    }

    return FALSE;
}

/* Internal stuff ----------------------------------------------------------- */

static Bool
cubecapsInitDisplay (CompPlugin  *p,
		     CompDisplay *d)
{
    CubeCapsDisplay *ccd;
    CompPlugin	 *cube = findActivePlugin ("cube");

    CompOption	*option;
    int		nOption;

    if (!cube || !cube->vTable->getDisplayOptions)
	return FALSE;

    option = (*cube->vTable->getDisplayOptions) (cube, d, &nOption);

    if (getIntOptionNamed (option, nOption, "abi", 0) != CUBE_ABIVERSION)
    {
	compLogMessage (d, "cubecaps", CompLogLevelError,
			"cube ABI version mismatch");
	return FALSE;
    }

    cubeDisplayPrivateIndex = getIntOptionNamed (option, nOption, "index", -1);

    if (cubeDisplayPrivateIndex < 0)
	return FALSE;

    ccd = malloc (sizeof (CubeCapsDisplay));

    if (!ccd)
	return FALSE;

    ccd->screenPrivateIndex = allocateScreenPrivateIndex (d);

    if (ccd->screenPrivateIndex < 0)
    {
	free (ccd);
	return FALSE;
    }

    cubecapsSetTopNextInitiate (d, cubecapsTopNext);
    cubecapsSetTopPrevInitiate (d, cubecapsTopPrev);
    cubecapsSetBottomNextInitiate (d, cubecapsBottomNext);
    cubecapsSetBottomPrevInitiate (d, cubecapsBottomPrev);

    d->privates[displayPrivateIndex].ptr = ccd;

    return TRUE;
}

static void
cubecapsFiniDisplay (CompPlugin  *p,
		     CompDisplay *d)
{
    CUBECAPS_DISPLAY (d);

    freeScreenPrivateIndex (d, ccd->screenPrivateIndex);
    free (ccd);
}

static Bool
cubecapsInitScreen (CompPlugin *p,
		    CompScreen *s)
{
    CubeCapsScreen *ccs;
    CUBECAPS_DISPLAY (s->display);
    CUBE_SCREEN (s);

    ccs = malloc (sizeof (CubeCapsScreen));

    if (!ccs)
	return FALSE;

    cubecapsInitCap (s, &ccs->topCap);
    cubecapsInitCap (s, &ccs->bottomCap);

    ccs->topCap.files = cubecapsGetTopImages (s);
    ccs->bottomCap.files = cubecapsGetBottomImages (s);

    cubecapsSetTopImagesNotify (s, cubecapsTopImagesChanged);
    cubecapsSetBottomImagesNotify (s, cubecapsBottomImagesChanged);

    cubecapsSetScaleTopImageNotify (s, cubecapsScaleTopImageChanged);
    cubecapsSetScaleBottomImageNotify (s, cubecapsScaleBottomImageChanged);

    WRAP (ccs, cs, paintTop, cubecapsPaintTop);
    WRAP (ccs, cs, paintBottom, cubecapsPaintBottom);

    s->privates[ccd->screenPrivateIndex].ptr = ccs;

    cubecapsChangeCap (s, &ccs->topCap, 0);
    cubecapsChangeCap (s, &ccs->bottomCap, 0);

    return TRUE;
}

static void
cubecapsFiniScreen (CompPlugin *p,
		    CompScreen *s)
{
    CUBECAPS_SCREEN (s);
    CUBE_SCREEN (s);

    UNWRAP (ccs, cs, paintTop);
    UNWRAP (ccs, cs, paintBottom);

    free (ccs);
}

static Bool
cubecapsInit (CompPlugin * p)
{
    displayPrivateIndex = allocateDisplayPrivateIndex();

    if (displayPrivateIndex < 0)
	return FALSE;

    return TRUE;
}

static void
cubecapsFini (CompPlugin * p)
{
    if (displayPrivateIndex >= 0)
	freeDisplayPrivateIndex (displayPrivateIndex);
}

static int
cubecapsGetVersion (CompPlugin *plugin,
		    int        version)
{
    return ABIVERSION;
}

CompPluginVTable cubecapsVTable =
{
    "cubecaps",
    cubecapsGetVersion,
    0,
    cubecapsInit,
    cubecapsFini,
    cubecapsInitDisplay,
    cubecapsFiniDisplay,
    cubecapsInitScreen,
    cubecapsFiniScreen,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL
};

CompPluginVTable *
getCompPluginInfo (void)
{
    return &cubecapsVTable;
}
