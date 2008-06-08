/*
 * Compiz cube reflection and cylinder deformation plugin
 *
 * cubeaddon.c
 *
 * Copyright : (C) 2008 by Dennis Kasprzyk
 * E-mail    : onestone@opencompositing.org
 *
 * includes code from cubecaps.c
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
 *
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <math.h>

#include <compiz-core.h>
#include <compiz-cube.h>

#include "cubeaddon_options.h"

static int CubeaddonDisplayPrivateIndex;
static int cubeDisplayPrivateIndex;

#define CUBEADDON_GRID_SIZE    100
#define CAP_ELEMENTS 15
#define CAP_NVERTEX (((CAP_ELEMENTS * (CAP_ELEMENTS + 1)) + 2) * 3)
#define CAP_NIDX (CAP_ELEMENTS * (CAP_ELEMENTS - 1) * 4)

#define CAP_NIMGVERTEX (((CAP_ELEMENTS + 1) * (CAP_ELEMENTS + 1)) * 5)
#define CAP_NIMGIDX (CAP_ELEMENTS * CAP_ELEMENTS * 4)

typedef struct _CubeaddonDisplay
{
    int screenPrivateIndex;
} CubeaddonDisplay;

typedef struct _CubeCap
{
    int		    current;
    CompListValue   *files;

    Bool            loaded;

    CompTexture	    texture;
    CompTransform   texMat;
} CubeCap;

typedef struct _CubeaddonScreen
{
    DonePaintScreenProc        donePaintScreen;
    PaintOutputProc            paintOutput;
    PaintTransformedOutputProc paintTransformedOutput;
    AddWindowGeometryProc      addWindowGeometry;
    DrawWindowProc	       drawWindow;
    DrawWindowTextureProc      drawWindowTexture;

    CubeClearTargetOutputProc   clearTargetOutput;
    CubeGetRotationProc	        getRotation;
    CubeCheckOrientationProc    checkOrientation;
    CubeShouldPaintViewportProc shouldPaintViewport;
    CubePaintTopProc	        paintTop;
    CubePaintBottomProc	        paintBottom;

    Bool reflection;
    Bool first;

    CompOutput *last;

    float yTrans;
    float zTrans;

    float backVRotate;
    float vRot;

    float deform;
    Bool  wasDeformed;

    Region tmpRegion;
    BoxPtr tmpBox;
    int    nTmpBox;

    GLfloat      *winNormals;
    unsigned int winNormSize;

    GLfloat  capFill[CAP_NVERTEX];
    GLfloat  capFillNorm[CAP_NVERTEX];
    GLushort capFillIdx[CAP_NIDX];
    float    capDeform;
    float    capDistance;
    int      capDeformType;

    CubeCap topCap;
    CubeCap bottomCap;
} CubeaddonScreen;

#define CUBEADDON_DISPLAY(d) PLUGIN_DISPLAY(d, Cubeaddon, ca)
#define CUBEADDON_SCREEN(s) PLUGIN_SCREEN(s, Cubeaddon, ca)

/*
 * Initiate a CubeCap
 */
static void
cubeaddonInitCap (CompScreen *s, CubeCap *cap)
{
    initTexture (s, &cap->texture);

    cap->current    = 0;
    cap->files	    = NULL;
    cap->loaded     = FALSE;
}

/*
 * Attempt to load current cap image (if any)
 */
static void
cubeaddonLoadCap (CompScreen *s,
		  CubeCap    *cap,
		  Bool       scale,
		  Bool       aspect,
		  Bool       clamp)
{
    unsigned int width, height;
    float        xScale, yScale;

    CUBE_SCREEN (s);

    finiTexture (s, &cap->texture);
    initTexture (s, &cap->texture);

    cap->loaded = FALSE;

    if (!cap->files || !cap->files->nValue)
	return;

    cap->current = cap->current % cap->files->nValue;

    if (!readImageToTexture (s, &cap->texture,
			     cap->files->value[cap->current].s,
			     &width, &height))
    {
	compLogMessage (s->display, "cubeaddon", CompLogLevelWarn,
			"Failed to load image: %s",
			cap->files->value[cap->current].s);

	finiTexture (s, &cap->texture);
	initTexture (s, &cap->texture);

	return;
    }

    cap->loaded = TRUE;
    matrixGetIdentity (&cap->texMat);

    cap->texMat.m[0] = cap->texture.matrix.xx;
    cap->texMat.m[1] = cap->texture.matrix.yx;
    cap->texMat.m[4] = cap->texture.matrix.xy;
    cap->texMat.m[5] = cap->texture.matrix.yy;
    cap->texMat.m[12] = cap->texture.matrix.x0;
    cap->texMat.m[13] = cap->texture.matrix.y0;

    if (aspect)
    {
	if (scale)
	    xScale = yScale = MIN (width, height);
	else
	    xScale = yScale = MAX (width, height);
    }
    else
    {
	xScale = width;
	yScale = height;
    }

    matrixTranslate (&cap->texMat, width / 2, height / 2.0, 0.0);
    matrixScale (&cap->texMat, xScale / 2.0, yScale / 2.0, 1.0);

    if (scale)
	xScale = 1.0 / sqrtf (((cs->distance * cs->distance) + 0.25));
    else
	xScale = 1.0 / sqrtf (((cs->distance * cs->distance) + 0.25) * 0.5);

    matrixScale (&cap->texMat, xScale, xScale, 1.0);

    enableTexture (s, &cap->texture, COMP_TEXTURE_FILTER_GOOD);
    if (clamp)
    {
	if (s->textureBorderClamp)
	{
	    glTexParameteri (cap->texture.target, GL_TEXTURE_WRAP_S,
			     GL_CLAMP_TO_BORDER);
	    glTexParameteri (cap->texture.target, GL_TEXTURE_WRAP_T,
			     GL_CLAMP_TO_BORDER);
	}
	else
	{
	    glTexParameteri (cap->texture.target, GL_TEXTURE_WRAP_S,
			     GL_CLAMP_TO_EDGE);
	    glTexParameteri (cap->texture.target, GL_TEXTURE_WRAP_T,
			     GL_CLAMP_TO_EDGE);
	}
    }
    else
    {
	glTexParameteri (cap->texture.target, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri (cap->texture.target, GL_TEXTURE_WRAP_T, GL_REPEAT);
    }
    disableTexture (s, &cap->texture);
}

/* Settings handling -------------------------------------------------------- */

/*
 * Switch cap, load it and damage screen if possible
 */
static void
cubeaddonChangeCap (CompScreen *s,
		    Bool       top,
		    int        change)
{
    CUBEADDON_SCREEN (s);
    CubeCap *cap = (top)? &cas->topCap : &cas->bottomCap;
    if (cap->files && cap->files->nValue)
    {
	int count = cap->files->nValue;
	cap->current = (cap->current + change + count) % count;
	if (top)
	{
	    cubeaddonLoadCap (s, cap, cubeaddonGetTopScale (s),
			      cubeaddonGetTopAspect (s),
			      cubeaddonGetTopClamp (s));
	}
	else
	{
	    cubeaddonLoadCap (s, cap, cubeaddonGetBottomScale (s),
			      cubeaddonGetBottomAspect (s),
			      cubeaddonGetBottomClamp (s));
	    matrixScale (&cap->texMat, 1.0, -1.0, 1.0);
	}
	damageScreen (s);
    }
}

/*
 * Top images list changed, reload top cap if any
 */
static void
cubeaddonTopImagesChanged (CompScreen             *s,
			   CompOption             *opt,
			   CubeaddonScreenOptions num)
{
    CUBEADDON_SCREEN (s);

    cas->topCap.files   = cubeaddonGetTopImages (s);
    cas->topCap.current = 0;
    cubeaddonChangeCap (s, TRUE, 0);
}

/*
 * Bottom images list changed, reload bottom cap if any
 */
static void
cubeaddonBottomImagesChanged (CompScreen             *s,
			      CompOption             *opt,
			      CubeaddonScreenOptions num)
{
    CUBEADDON_SCREEN (s);

    cas->bottomCap.files   = cubeaddonGetBottomImages (s);
    cas->bottomCap.current = 0;
    cubeaddonChangeCap (s, FALSE, 0);
}

/*
 * Top image attribute changed
 */
static void
cubeaddonTopImageChanged (CompScreen             *s,
			  CompOption             *opt,
			  CubeaddonScreenOptions num)
{
    cubeaddonChangeCap (s, TRUE, 0);
}

/*
 * Bottom images attribute changed
 */
static void
cubeaddonBottomImageChanged (CompScreen             *s,
			     CompOption             *opt,
			     CubeaddonScreenOptions num)
{
    cubeaddonChangeCap (s, FALSE, 0);
}

/* Actions handling --------------------------------------------------------- */

/*
 * Switch to next top image
 */
static Bool
cubeaddonTopNext (CompDisplay     *d,
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
	cubeaddonChangeCap (s, TRUE, 1);

    return FALSE;
}

/*
 * Switch to previous top image
 */
static Bool
cubeaddonTopPrev (CompDisplay     *d,
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
	cubeaddonChangeCap (s, TRUE, -1);

    return FALSE;
}

/*
 * Switch to next bottom image
 */
static Bool
cubeaddonBottomNext (CompDisplay     *d,
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
	cubeaddonChangeCap (s, FALSE, 1);

    return FALSE;
}

/*
 * Switch to previous bottom image
 */
static Bool
cubeaddonBottomPrev (CompDisplay     *d,
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
	cubeaddonChangeCap (s, FALSE, -1);

    return FALSE;
}

static void
drawBasicGround (CompScreen *s)
{
    float i;

    glPushMatrix ();

    glEnable (GL_BLEND);
    glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glLoadIdentity ();
    glTranslatef (0.0, 0.0, -DEFAULT_Z_CAMERA);

    i = cubeaddonGetIntensity (s) * 2;

    glBegin (GL_QUADS);
    glColor4f (0.0, 0.0, 0.0, MAX (0.0, 1.0 - i) );
    glVertex2f (0.5, 0.0);
    glVertex2f (-0.5, 0.0);
    glColor4f (0.0, 0.0, 0.0, MIN (1.0, 1.0 - (i - 1.0) ) );
    glVertex2f (-0.5, -0.5);
    glVertex2f (0.5, -0.5);
    glEnd ();

    if (cubeaddonGetGroundSize (s) > 0.0)
    {
	glBegin (GL_QUADS);
	glColor4usv (cubeaddonGetGroundColor1 (s) );
	glVertex2f (-0.5, -0.5);
	glVertex2f (0.5, -0.5);
	glColor4usv (cubeaddonGetGroundColor2 (s) );
	glVertex2f (0.5, -0.5 + cubeaddonGetGroundSize (s) );
	glVertex2f (-0.5, -0.5 + cubeaddonGetGroundSize (s) );
	glEnd ();
    }

    glColor4usv (defaultColor);

    glBlendFunc (GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    glDisable (GL_BLEND);
    glPopMatrix ();
}

static Bool
cubeaddonCheckOrientation (CompScreen              *s,
			   const ScreenPaintAttrib *sAttrib,
			   const CompTransform     *transform,
			   CompOutput              *outputPtr,
			   CompVector              *points)
{
    Bool status;

    CUBEADDON_SCREEN (s);
    CUBE_SCREEN (s);

    UNWRAP (cas, cs, checkOrientation);
    status = (*cs->checkOrientation) (s, sAttrib, transform,
				      outputPtr, points);
    WRAP (cas, cs, checkOrientation, cubeaddonCheckOrientation);

    if (cas->reflection)
	return !status;

    return status;
}

static void
cubeaddonGetRotation (CompScreen *s,
		      float      *x,
		      float      *v,
		      float      *progress)
{
    CUBE_SCREEN (s);
    CUBEADDON_SCREEN (s);

    UNWRAP (cas, cs, getRotation);
    (*cs->getRotation) (s, x, v, progress);
    WRAP (cas, cs, getRotation, cubeaddonGetRotation);

    if (cubeaddonGetMode (s) == ModeAbove && *v > 0.0 && cas->reflection)
    {
	cas->vRot = *v;
	*v = 0.0;
    }
    else
	cas->vRot = 0.0;
}

static void
cubeaddonClearTargetOutput (CompScreen *s,
			    float      xRotate,
			    float      vRotate)
{
    CUBEADDON_SCREEN (s);
    CUBE_SCREEN (s);

    if (cas->reflection)
	glCullFace (GL_BACK);

    UNWRAP (cas, cs, clearTargetOutput);
    (*cs->clearTargetOutput) (s, xRotate, cas->backVRotate);
    WRAP (cas, cs, clearTargetOutput, cubeaddonClearTargetOutput);

    if (cas->reflection)
	glCullFace (GL_FRONT);
}

static Bool
cubeaddonShouldPaintViewport (CompScreen              *s,
			      const ScreenPaintAttrib *sAttrib,
			      const CompTransform     *transform,
			      CompOutput              *outputPtr,
			      PaintOrder              order)
{
    Bool rv = FALSE;

    CUBEADDON_SCREEN (s);
    CUBE_SCREEN (s);

    UNWRAP (cas, cs, shouldPaintViewport);
    rv = (*cs->shouldPaintViewport) (s, sAttrib, transform,
				     outputPtr, order);
    WRAP (cas, cs, shouldPaintViewport, cubeaddonShouldPaintViewport);

    if (rv || cs->unfolded)
	return rv;

    if (cas->deform > 0.0 && cubeaddonGetDeformation (s) == DeformationCylinder)
    {
	float z[3];
	Bool  ftb1, ftb2, ftb3;

	z[0] = cs->invert * cs->distance;
	z[1] = z[0] + (0.25 / cs->distance);
	z[2] = cs->invert * sqrtf (0.25 + (cs->distance * cs->distance));

	CompVector vPoints[3][3] = { { {.v = { -0.5,  0.0, z[0], 1.0 } },
				       {.v = {  0.0,  0.5, z[1], 1.0 } },
				       {.v = {  0.0,  0.0, z[1], 1.0 } } },
				     { {.v = {  0.5,  0.0, z[0], 1.0 } },
				       {.v = {  0.0, -0.5, z[1], 1.0 } },
				       {.v = {  0.0,  0.0, z[1], 1.0 } } },
				     { {.v = { -0.5,  0.0, z[2], 1.0 } },
				       {.v = {  0.0,  0.5, z[2], 1.0 } },
				       {.v = {  0.0,  0.0, z[2], 1.0 } } } };

	ftb1 = (*cs->checkOrientation) (s, sAttrib, transform,
					outputPtr, vPoints[0]);
	ftb2 = (*cs->checkOrientation) (s, sAttrib, transform,
					outputPtr, vPoints[1]);
	ftb3 = (*cs->checkOrientation) (s, sAttrib, transform,
					outputPtr, vPoints[2]);

	rv = (order == FTB && (ftb1 || ftb2 || ftb3)) ||
	     (order == BTF && (!ftb1 || !ftb2 || !ftb3));
    }
    else if (cas->deform > 0.0 &&
	     cubeaddonGetDeformation (s) == DeformationSphere)
    {
	float z[4];
	Bool  ftb1, ftb2, ftb3, ftb4;

	z[0] = sqrtf (0.5 + (cs->distance * cs->distance));
	z[1] = z[0] + (0.25 / cs->distance);
	z[2] = sqrtf (0.25 + (cs->distance * cs->distance));
	z[3] = z[2] + 0.5;

	CompVector vPoints[4][3] = { { {.v = {  0.0,  0.0, z[3], 1.0 } },
				       {.v = { -0.5,  0.5, z[2], 1.0 } },
				       {.v = {  0.0,  0.5, z[2], 1.0 } } },
				     { {.v = {  0.0,  0.0, z[3], 1.0 } },
				       {.v = {  0.5, -0.5, z[2], 1.0 } },
				       {.v = {  0.0, -0.5, z[2], 1.0 } } },
	   			     { {.v = {  0.0,  0.0, z[1], 1.0 } },
				       {.v = { -0.5, -0.5, z[0], 1.0 } },
				       {.v = { -0.5,  0.0, z[0], 1.0 } } },
				     { {.v = {  0.0,  0.0, z[1], 1.0 } },
				       {.v = {  0.5,  0.5, z[0], 1.0 } },
				       {.v = {  0.5,  0.0, z[0], 1.0 } } } };

	ftb1 = (*cs->checkOrientation) (s, sAttrib, transform,
					outputPtr, vPoints[0]);
	ftb2 = (*cs->checkOrientation) (s, sAttrib, transform,
					outputPtr, vPoints[1]);
	ftb3 = (*cs->checkOrientation) (s, sAttrib, transform,
					outputPtr, vPoints[2]);
	ftb4 = (*cs->checkOrientation) (s, sAttrib, transform,
					outputPtr, vPoints[3]);

	rv = (order == FTB && (ftb1 || ftb2 || ftb3 || ftb4)) ||
	     (order == BTF && (!ftb1 || !ftb2 || !ftb3 || !ftb4));
    }

    return rv;
}

static void
cubeaddonPaintCap (CompScreen		   *s,
		   const ScreenPaintAttrib *sAttrib,
		   const CompTransform     *transform,
		   CompOutput		   *output,
		   int			   size,
		   Bool                    top,
		   Bool                    adjust,
		   unsigned short          *color)
{
    ScreenPaintAttrib sa;
    CompTransform     sTransform;
    int               i, l, opacity;
    int               cullNorm, cullInv;
    Bool              wasCulled = glIsEnabled (GL_CULL_FACE);
    float             cInv = (top) ? 1.0: -1.0;
    CubeCap           *cap;
    Bool              cScale, cAspect;

    CUBE_SCREEN (s);
    CUBEADDON_SCREEN (s);

    glGetIntegerv (GL_CULL_FACE_MODE, &cullNorm);
    cullInv   = (cullNorm == GL_BACK)? GL_FRONT : GL_BACK;

    opacity = cs->desktopOpacity * color[3] / 0xffff;

    glPushMatrix ();
    glEnable (GL_BLEND);

    if (top)
    {
	cap     = &cas->topCap;
	cScale  = cubeaddonGetTopScale (s);
	cAspect = cubeaddonGetTopAspect (s);
    }
    else
    {
	cap     = &cas->bottomCap;
	cScale  = cubeaddonGetBottomScale (s);
	cAspect = cubeaddonGetBottomAspect (s);
    }


    glDisableClientState (GL_TEXTURE_COORD_ARRAY);

    if (cubeaddonGetDeformation (s) == DeformationSphere &&
        cubeaddonGetDeformCaps (s))
	glEnableClientState (GL_NORMAL_ARRAY);

    glVertexPointer (3, GL_FLOAT, 0, cas->capFill);

    glEnable(GL_CULL_FACE);

    for (l = 0; l < ((cs->invert == 1) ? 2 : 1); l++)
    {
	if (cubeaddonGetDeformation (s) == DeformationSphere &&
	    cubeaddonGetDeformCaps (s))
	{
	    glNormalPointer (GL_FLOAT, 0, (l == 0) ? cas->capFill : cas->capFillNorm);
	}
	else
	    glNormal3f (0.0, (l == 0) ? 1.0 : -1.0, 0.0);

	glCullFace(((l == 1) ^ top) ? cullInv : cullNorm);

	for (i = 0; i < size; i++)
	{
	    sa = *sAttrib;
	    sTransform = *transform;
	    if (cs->invert == 1)
	    {
		sa.yRotate += (360.0f / size) * cs->xRotations;
		if (!adjust)
		    sa.yRotate -= (360.0f / size) * s->x;
	    }
	    else
	    {
		sa.yRotate += 180.0f;
		sa.yRotate -= (360.0f / size) * cs->xRotations;
		if (!adjust)
		    sa.yRotate += (360.0f / size) * s->x;
	    }
	    sa.yRotate += (360.0f / size) * i;

	    (*s->applyScreenTransform) (s, &sa, output, &sTransform);

	    glLoadMatrixf (sTransform.m);
	    glTranslatef (cs->outputXOffset, -cs->outputYOffset, 0.0f);
	    glScalef (cs->outputXScale, cs->outputYScale, 1.0f);

	    glScalef (1.0, cInv, 1.0);

	    glColor4us (color[0] * opacity / 0xffff,
		color[1] * opacity / 0xffff,
		color[2] * opacity / 0xffff,
		opacity);

	    glDrawArrays (GL_TRIANGLE_FAN, 0, CAP_ELEMENTS + 2);
	    if (cubeaddonGetDeformation (s) == DeformationSphere &&
	        cubeaddonGetDeformCaps (s))
		glDrawElements (GL_QUADS, CAP_NIDX, GL_UNSIGNED_SHORT,
				cas->capFillIdx);

	    if (cap->loaded)
	    {
		float s_gen[4], t_gen[4];
		CompTransform texMat = cap->texMat;

		if (cs->invert != 1)
		    matrixScale (&texMat, -1.0, 1.0, 1.0);

		glColor4us (cs->desktopOpacity, cs->desktopOpacity,
		    cs->desktopOpacity, cs->desktopOpacity);
	        enableTexture (s, &cap->texture, COMP_TEXTURE_FILTER_GOOD);

		if (cAspect)
		{
		    float scale, xScale = 1.0, yScale = 1.0;
		    scale = (float)output->width / (float)output->height;

		    if (output->width > output->height)
		    {
			xScale = 1.0;
			yScale = 1.0 / scale;
		    }
		    else
		    {
			xScale = scale;
			yScale = 1.0;
		    }

		    if (cubeaddonGetTopScale(s))
		    {
			scale = xScale;
			xScale = 1.0 / yScale;
			yScale = 1.0 / scale;
		    }

		    matrixScale (&texMat, xScale, yScale, 1.0);
		}
		
		matrixRotate (&texMat, -(360.0f / size) * i, 0.0, 0.0, 1.0);

		s_gen[0] = texMat.m[0];
		s_gen[1] = texMat.m[8];
		s_gen[2] = texMat.m[4];
		s_gen[3] = texMat.m[12];
		t_gen[0] = texMat.m[1];
		t_gen[1] = texMat.m[9];
		t_gen[2] = texMat.m[5];
		t_gen[3] = texMat.m[13];

		glTexGenfv(GL_T, GL_OBJECT_PLANE, t_gen);
		glTexGenfv(GL_S, GL_OBJECT_PLANE, s_gen);

		glTexGeni(GL_S, GL_TEXTURE_GEN_MODE, GL_OBJECT_LINEAR);
		glTexGeni(GL_T, GL_TEXTURE_GEN_MODE, GL_OBJECT_LINEAR);

		glEnable(GL_TEXTURE_GEN_S);
		glEnable(GL_TEXTURE_GEN_T);

		glDrawArrays (GL_TRIANGLE_FAN, 0, CAP_ELEMENTS + 2);
		if (cubeaddonGetDeformation (s) == DeformationSphere &&
	            cubeaddonGetDeformCaps (s))
		    glDrawElements (GL_QUADS, CAP_NIDX, GL_UNSIGNED_SHORT,
				    cas->capFillIdx);

		glDisable(GL_TEXTURE_GEN_S);
		glDisable(GL_TEXTURE_GEN_T);
		disableTexture (s, &cas->topCap.texture);
	    }
	}
    }

    glEnableClientState (GL_TEXTURE_COORD_ARRAY);
    glDisableClientState (GL_NORMAL_ARRAY);
    glDisable (GL_BLEND);
    glNormal3f (0.0, -1.0, 0.0);

    glCullFace (cullNorm);
    if (!wasCulled)
	glDisable (GL_CULL_FACE);

    glPopMatrix ();

    glColor4usv (defaultColor);
}

static void
cubeaddonPaintTop (CompScreen		   *s,
		   const ScreenPaintAttrib *sAttrib,
		   const CompTransform     *transform,
		   CompOutput		   *output,
		   int			   size)
{
    CUBE_SCREEN (s);
    CUBEADDON_SCREEN (s);

    if ((!cubeaddonGetDrawBottom (s) && cs->invert == -1) ||
        (!cubeaddonGetDrawTop (s) && cs->invert == 1))
    {
	UNWRAP (cas, cs, paintTop);
	(*cs->paintTop) (s, sAttrib, transform, output, size);
	WRAP (cas, cs, paintTop, cubeaddonPaintTop);
    }

    if (!cubeaddonGetDrawTop (s))
        return;

    cubeaddonPaintCap (s, sAttrib, transform, output, size, TRUE,
		       cubeaddonGetAdjustTop (s),
		       cubeaddonGetTopColor(s));
}

/*
 * Paint bottom cube face
 */
static void
cubeaddonPaintBottom (CompScreen	      *s,
		      const ScreenPaintAttrib *sAttrib,
		      const CompTransform     *transform,
		      CompOutput	      *output,
		      int		      size)
{
    CUBE_SCREEN (s);
    CUBEADDON_SCREEN (s);

    if ((!cubeaddonGetDrawBottom (s) && cs->invert == 1) ||
        (!cubeaddonGetDrawTop (s) && cs->invert == -1))
    {
	UNWRAP (cas, cs, paintBottom);
	(*cs->paintBottom) (s, sAttrib, transform, output, size);
	WRAP (cas, cs, paintBottom, cubeaddonPaintBottom);
    }

    if (!cubeaddonGetDrawBottom (s))
        return;

    cubeaddonPaintCap (s, sAttrib, transform, output, size, FALSE,
		       cubeaddonGetAdjustBottom (s),
		       cubeaddonGetBottomColor(s));
}
	
static void
cubeaddonAddWindowGeometry (CompWindow *w,
			    CompMatrix *matrix,
			    int        nMatrix,
			    Region     region,
			    Region     clip)
{
    CompScreen *s = w->screen;

    CUBEADDON_SCREEN (s);
    CUBE_SCREEN (s);

    if (cas->deform > 0.0)
    {
	int         x1, x2, y1, y2, yi, i, j, oldVCount = w->vCount;
	REGION      reg;
	GLfloat     *v;
	int         offX = 0, offY = 0;
	int         sx1, sx2, sw, sy1, sy2, sh, nBox, currBox, cLast;
	float       lastX, lastZ = 0.0, radSquare, last[2][4];
	Bool        found;
	float       inv = (cs->invert == 1) ? 1.0 : -1.0;

	float       a1, a2, ang;

	if (cubeaddonGetDeformation (s) == DeformationCylinder || cs->unfolded)
	{
	    yi = region->extents.y2 - region->extents.y1;
	    radSquare = (cs->distance * cs->distance) + 0.25;
	}
	else
	{
	    yi = CUBEADDON_GRID_SIZE;
	    radSquare = (cs->distance * cs->distance) + 0.5;
	}

	nBox = (((region->extents.y2 - region->extents.y1) / yi) + 1) *
	       (((region->extents.x2 - region->extents.x1) /
	       CUBEADDON_GRID_SIZE) + 1);

	reg.numRects = 1;
	reg.rects = &reg.extents;

	y1 = region->extents.y1;
	y2 = MIN (y1 + yi, region->extents.y2);
	
	UNWRAP (cas, s, addWindowGeometry);

	if (region->numRects > 1)
	{
	    while (y1 < region->extents.y2)
	    {
		reg.extents.y1 = y1;
		reg.extents.y2 = y2;

		x1 = region->extents.x1;
		x2 = MIN (x1 + CUBEADDON_GRID_SIZE, region->extents.x2);

		while (x1 < region->extents.x2)
		{
		    reg.extents.x1 = x1;
		    reg.extents.x2 = x2;

		    XIntersectRegion (region, &reg, cas->tmpRegion);

		    if (!XEmptyRegion (cas->tmpRegion))
		    {
			(*w->screen->addWindowGeometry) (w, matrix, nMatrix,
							 cas->tmpRegion, clip);
		    }

		    x1 = x2;
		    x2 = MIN (x2 + CUBEADDON_GRID_SIZE, region->extents.x2);
		}
		y1 = y2;
		y2 = MIN (y2 + yi, region->extents.y2);
	    }
	}
	else
	{
	    if (cas->nTmpBox < nBox)
	    {
		cas->tmpBox = realloc (cas->tmpBox, nBox * sizeof (BOX));
		if (!cas->tmpBox)
		    return;
		cas->nTmpBox = nBox;
	    }

	    reg.extents = region->extents;
	    reg.rects = cas->tmpBox;

	    currBox = 0;
	    while (y1 < region->extents.y2)
	    {
		x1 = region->extents.x1;
		x2 = MIN (x1 + CUBEADDON_GRID_SIZE, region->extents.x2);
	
		while (x1 < region->extents.x2)
		{
		    reg.rects[currBox].y1 = y1;
		    reg.rects[currBox].y2 = y2;
		    reg.rects[currBox].x1 = x1;
		    reg.rects[currBox].x2 = x2;

		    currBox++;

		    x1 = x2;
		    x2 = MIN (x2 + CUBEADDON_GRID_SIZE, region->extents.x2);
	        }
		y1 = y2;
		y2 = MIN (y2 + yi, region->extents.y2);
	    }
	    reg.numRects = currBox;
	    (*w->screen->addWindowGeometry) (w, matrix, nMatrix, &reg, clip);
	}
	WRAP (cas, s, addWindowGeometry, cubeaddonAddWindowGeometry);
	
	v  = w->vertices + (w->vertexStride - 3);
	v += w->vertexStride * oldVCount;

	if (!windowOnAllViewports (w))
	{
	    getWindowMovementForOffset (w, s->windowOffsetX,
                                        s->windowOffsetY, &offX, &offY);
	}

	if (cs->moMode == CUBE_MOMODE_ONE)
	{
	    sx1 = 0;
	    sx2 = s->width;
	    sw  = s->width;
	    sy1 = 0;
	    sy2 = s->height;
	    sh  = s->height;
	}
	else if (cs->moMode == CUBE_MOMODE_MULTI)
	{
	    sx1 = cas->last->region.extents.x1;
	    sx2 = cas->last->region.extents.x2;
	    sw  = sx2 - sx1;
	    sy1 = cas->last->region.extents.y1;
	    sy2 = cas->last->region.extents.y2;
	    sh  = sy2 - sy1;
	}
	else
	{
	    if (cs->nOutput != s->nOutputDev)
	    {
		sx1 = 0;
		sx2 = s->width;
		sw  = s->width;
		sy1 = 0;
		sy2 = s->height;
		sh  = s->height;
	    }
	    else
	    {
		sx1 = s->outputDev[cs->srcOutput].region.extents.x1;
		sx2 = s->outputDev[cs->srcOutput].region.extents.x2;
		sw  = sx2 - sx1;
		sy1 = s->outputDev[cs->srcOutput].region.extents.y1;
		sy2 = s->outputDev[cs->srcOutput].region.extents.y2;
		sh  = sy2 - sy1;
	    }
	}

	if (cubeaddonGetDeformation (s) == DeformationCylinder || cs->unfolded)
	{
	    lastX = -1000000000.0;
	
	    for (i = oldVCount; i < w->vCount; i++)
	    {
		if (v[0] == lastX)
		{
		    v[2] = lastZ;
		}
		else if (v[0] + offX >= sx1 - CUBEADDON_GRID_SIZE &&
			 v[0] + offX < sx2 + CUBEADDON_GRID_SIZE)
		{
		    ang = (((v[0] + offX - sx1) / (float)sw) - 0.5);
		    ang *= ang;
		    if (ang < radSquare)
		    {
			v[2] = sqrtf (radSquare - ang) - cs->distance;
			v[2] *= cas->deform * inv;
		    }
		}

		lastX = v[0];
		lastZ = v[2];

		v += w->vertexStride;
	    }
	}
	else
	{

	    last[0][0] = -1000000000.0;
	    last[1][0] = -1000000000.0;

	    cLast = 0;
	    for (i = oldVCount; i < w->vCount; i++)
	    {
		found = FALSE;

		for (j = 0; j < 2 && !found; j++)
		    if (last[j][0] == v[0] && last[j][1] == v[1])
		    {
			v[0] = last[j][2];
			v[2] = last[j][3];
			found = TRUE;
		    }

		if (!found && v[0] + offX >= sx1 - CUBEADDON_GRID_SIZE &&
		    v[0] + offX < sx2 + CUBEADDON_GRID_SIZE &&
		    v[1] + offY >= sy1 - CUBEADDON_GRID_SIZE &&
		    v[1] + offY < sy2 + CUBEADDON_GRID_SIZE)
		{
		    last[cLast][0] = v[0];
		    last[cLast][1] = v[1];
		    a1 = (((v[0] + offX - sx1) / (float)sw) - 0.5);
		    a2 = (((v[1] + offY - sy1) / (float)sh) - 0.5);
		    a2 *= a2;

		    ang = atanf (a1 / cs->distance);
		    a2 = sqrtf (radSquare - a2);

		    v[2] += ((cosf (ang) * a2) - cs->distance) *
			    cas->deform * inv;
		    v[0] += ((sinf (ang) * a2) - a1) * sw * cas->deform;
		    last[cLast][2] = v[0];
		    last[cLast][3] = v[2];
		    cLast = (cLast + 1) & 1;
		}
		v += w->vertexStride;
	    }
	}
    }
    else
    {
	UNWRAP (cas, s, addWindowGeometry);
	(*w->screen->addWindowGeometry) (w, matrix, nMatrix, region, clip);
	WRAP (cas, s, addWindowGeometry, cubeaddonAddWindowGeometry);
    }
}

static Bool
cubeaddonDrawWindow (CompWindow	          *w,
		     const CompTransform  *transform,
		     const FragmentAttrib *attrib,
		     Region		  region,
		     unsigned int	  mask)
{
    CompScreen *s = w->screen;
    Bool       status;

    CUBEADDON_SCREEN (s);

    if (!(mask & PAINT_WINDOW_TRANSFORMED_MASK) && cas->deform)
    {
	int offX = 0, offY = 0;
	int x1, x2;

	if (!windowOnAllViewports (w))
	{
	    getWindowMovementForOffset (w, s->windowOffsetX,
                                        s->windowOffsetY, &offX, &offY);
	}

	x1 = w->attrib.x - w->output.left + offX;
	x2 = w->attrib.x + w->width + w->output.right + offX;
	if (x1 < 0 && x2 < 0)
	    return FALSE;
	if (x1 > s->width && x2 > s->width)
	    return FALSE;
    }

    UNWRAP (cas, s, drawWindow);
    status = (*s->drawWindow) (w, transform, attrib, region, mask);
    WRAP (cas, s, drawWindow, cubeaddonDrawWindow);

    return status;
}

static void
cubeaddonDrawWindowTexture (CompWindow	         *w,
			    CompTexture	         *texture,
			    const FragmentAttrib *attrib,
			    unsigned int	 mask)
{
    CompScreen *s = w->screen;

    CUBEADDON_SCREEN (s);

    if (cas->deform > 0.0 && s->lighting)
    {
	int     i;
	int     sx1, sx2, sw, sy1, sy2, sh;
	int     offX = 0, offY = 0;
	float   x, y, ym;
	GLfloat *v;
	float   inv;
	
	CUBE_SCREEN (s);

	inv = (cs->invert == 1) ? 1.0: -1.0;
	ym  = (cubeaddonGetDeformation (s) == DeformationCylinder) ? 0.0 : 1.0;
	
	if (cas->winNormSize < w->vCount * 3)
	{
	    cas->winNormals = realloc (cas->winNormals, 
				       w->vCount * 3 * sizeof (GLfloat));
	    if (!cas->winNormals)
	    {
		cas->winNormSize = 0;
		return;
	    }
	    cas->winNormSize = w->vCount * 3;
	}
	
	if (!windowOnAllViewports (w))
	{
	    getWindowMovementForOffset (w, s->windowOffsetX,
                                        s->windowOffsetY, &offX, &offY);
	}
	
	if (cs->moMode == CUBE_MOMODE_ONE)
	{
	    sx1 = 0;
	    sx2 = s->width;
	    sw  = s->width;
	    sy1 = 0;
	    sy2 = s->height;
	    sh  = s->height;
	}
	else if (cs->moMode == CUBE_MOMODE_MULTI)
	{
	    sx1 = cas->last->region.extents.x1;
	    sx2 = cas->last->region.extents.x2;
	    sw  = sx2 - sx1;
	    sy1 = cas->last->region.extents.y1;
	    sy2 = cas->last->region.extents.y2;
	    sh  = sy2 - sy1;
	}
	else
	{
	    if (cs->nOutput != s->nOutputDev)
	    {
		sx1 = 0;
		sx2 = s->width;
		sw  = s->width;
		sy1 = 0;
		sy2 = s->height;
		sh  = s->height;
	    }
	    else
	    {
		sx1 = s->outputDev[cs->srcOutput].region.extents.x1;
		sx2 = s->outputDev[cs->srcOutput].region.extents.x2;
		sw  = sx2 - sx1;
		sy1 = s->outputDev[cs->srcOutput].region.extents.y1;
		sy2 = s->outputDev[cs->srcOutput].region.extents.y2;
		sh  = sy2 - sy1;
	    }
	}
	
	v = w->vertices + (w->vertexStride - 3);
	
	for (i = 0; i < w->vCount; i++)
	{
	    x = (((v[0] + offX - sx1) / (float)sw) - 0.5);
	    y = (((v[1] + offY - sy1) / (float)sh) - 0.5);

	    if (cs->paintOrder == FTB)
	    {
		cas->winNormals[i * 3] = x / sw * cas->deform;
		cas->winNormals[(i * 3) + 1] = y / sh * cas->deform * ym;
		cas->winNormals[(i * 3) + 2] = v[2] + cs->distance;
	    }
	    else
	    {
		cas->winNormals[i * 3] = -x / sw * cas->deform * inv;
		cas->winNormals[(i * 3) + 1] = -y / sh * cas->deform * ym * inv;
		cas->winNormals[(i * 3) + 2] = -(v[2] + cs->distance);
	    }

	    v += w->vertexStride;
	}
	
	glEnable (GL_NORMALIZE);
	glNormalPointer (GL_FLOAT,0, cas->winNormals);
	
	glEnableClientState (GL_NORMAL_ARRAY);
	
	UNWRAP (cas, s, drawWindowTexture);
	(*s->drawWindowTexture) (w, texture, attrib, mask);
	WRAP (cas, s, drawWindowTexture, cubeaddonDrawWindowTexture);

	glDisable (GL_NORMALIZE);
	glDisableClientState (GL_NORMAL_ARRAY);
	glNormal3f (0.0, 0.0, -1.0);
	return;
    }

    UNWRAP (cas, s, drawWindowTexture);
    (*s->drawWindowTexture) (w, texture, attrib, mask);
    WRAP (cas, s, drawWindowTexture, cubeaddonDrawWindowTexture);
}

static void
cubeaddonPaintTransformedOutput (CompScreen              *s,
				 const ScreenPaintAttrib *sAttrib,
				 const CompTransform     *transform,
				 Region                  region,
				 CompOutput              *output,
				 unsigned int            mask)
{
    static GLfloat light0Position[] = { -0.5f, 0.5f, -9.0f, 1.0f };
    CompTransform  sTransform = *transform;

    CUBEADDON_SCREEN (s);
    CUBE_SCREEN (s);

    if (cubeaddonGetDeformation (s) != DeformationNone
	&& s->hsize * cs->nOutput > 2 && s->desktopWindowCount &&
	(cs->rotationState == RotationManual ||
	(cs->rotationState == RotationChange &&
	!cubeaddonGetCylinderManualOnly (s)) || cas->wasDeformed) &&
        (!cs->unfolded || cubeaddonGetUnfoldDeformation (s)))
    {
	float x, progress;
	
	(*cs->getRotation) (s, &x, &x, &progress);
	cas->deform = progress;

	if (cubeaddonGetSphereAspect (s) > 0.0 && cs->invert == 1 &&
	    cubeaddonGetDeformation (s) == DeformationSphere)
	{
	    float scale, val = cubeaddonGetSphereAspect (s) * cas->deform;

	    if (output->width > output->height)
	    {
		scale = (float)output->height / (float)output->width;
		scale = (scale * val) + 1.0 - val;
		matrixScale (&sTransform, scale, 1.0, 1.0);
	    }
	    else
	    {
		scale = (float)output->width / (float)output->height;
		scale = (scale * val) + 1.0 - val;
		matrixScale (&sTransform, 1.0, scale, 1.0);
	    }
	}
    }
    else
    {
	cas->deform = 0.0;
    }

    cs->paintAllViewports |= !cubeaddonGetDrawTop (s)			  ||
			     !cubeaddonGetDrawBottom (s)		  ||
			     (cubeaddonGetTopColorAlpha (s) != OPAQUE)	  ||
			     (cubeaddonGetBottomColorAlpha (s) != OPAQUE) ||
		             (cas->deform > 0.0);

    if (cas->capDistance != cs->distance)
    {
	cubeaddonChangeCap (s, TRUE, 0);
	cubeaddonChangeCap (s, FALSE, 0);
    }

    if (cas->deform != cas->capDeform || cas->capDistance != cs->distance ||
        cas->capDeformType != cubeaddonGetDeformation (s))
    {
	float       *quad;
	int         i, j;
	float       rS, r, x, y, z, w;
	if (cubeaddonGetDeformation (s) != DeformationSphere ||
	    !cubeaddonGetDeformCaps (s))
	{
	    rS = (cs->distance * cs->distance) + 0.5;

	    cas->capFill[0] = 0.0;
	    cas->capFill[1] = 0.5;
	    cas->capFill[2] = 0.0;
	    cas->capFillNorm[0] = 0.0;
	    cas->capFillNorm[1] = -1.0;
	    cas->capFillNorm[2] = 0.0;

	    z = cs->distance;
	    r = 0.25 + (cs->distance * cs->distance);

	    for (j = 0; j <= CAP_ELEMENTS; j++)
	    {
		x = -0.5 + ((float)j / (float)CAP_ELEMENTS);
		z = ((sqrtf(r - (x * x)) - cs->distance) * cas->deform) +
		    cs->distance;
		y = 0.5;

		quad = &cas->capFill[(1 + j) * 3];

		quad[0] = x;
		quad[1] = y;
		quad[2] = z;

		quad = &cas->capFillNorm[(1 + j) * 3];

		quad[0] = -x;
		quad[1] = -y;
		quad[2] = -z;
	    }
	}
	else
	{
	    rS = (cs->distance * cs->distance) + 0.5;

	    cas->capFill[0] = 0.0;
	    cas->capFill[1] = ((sqrtf (rS) - 0.5) * cas->deform) + 0.5;
	    cas->capFill[2] = 0.0;
	    cas->capFillNorm[0] = 0.0;
	    cas->capFillNorm[1] = -1.0;
	    cas->capFillNorm[2] = 0.0;

	    for (i = 0; i < CAP_ELEMENTS; i++)
	    {
		w = (float)(i + 1) / (float)CAP_ELEMENTS;

		r = (((w / 2.0) * (w / 2.0)) +
		    (cs->distance * cs->distance * w * w));

		for (j = 0; j <= CAP_ELEMENTS; j++)
		{
		    x = - (w / 2.0) + ((float)j * w / (float)CAP_ELEMENTS);
		    z = ((sqrtf(r - (x * x)) - (cs->distance * w)) *
			cas->deform) + (cs->distance * w);
		    y = ((sqrtf(rS - (x * x) - (r - (x * x))) - 0.5) *
			cas->deform) + 0.5;

		    quad = &cas->capFill[(1 + (i * (CAP_ELEMENTS + 1)) +
				         j) * 3];

		    quad[0] = x;
		    quad[1] = y;
		    quad[2] = z;

		    quad = &cas->capFillNorm[(1 + (i * (CAP_ELEMENTS + 1)) +
					     j) * 3];

		    quad[0] = -x;
		    quad[1] = -y;
		    quad[2] = -z;
		}
	    }
	}

	cas->capDeform     = cas->deform;
	cas->capDistance   = cs->distance;
	cas->capDeformType = cubeaddonGetDeformation (s);
    }

    if (cs->invert == 1 && cas->first && cubeaddonGetReflection (s))
    {
	cas->first = FALSE;
	cas->reflection = TRUE;

	if (cs->grabIndex)
	{
	    CompTransform rTransform = sTransform;

	    matrixTranslate (&rTransform, 0.0, -1.0, 0.0);
	    matrixScale (&rTransform, 1.0, -1.0, 1.0);
	    glCullFace (GL_FRONT);

	    UNWRAP (cas, s, paintTransformedOutput);
	    (*s->paintTransformedOutput) (s, sAttrib, &rTransform,
					  region, output, mask);
	    WRAP (cas, s, paintTransformedOutput,
		  cubeaddonPaintTransformedOutput);

	    glCullFace (GL_BACK);
	    drawBasicGround (s);
	}
	else
	{
	    CompTransform rTransform = sTransform;
	    CompTransform pTransform;
	    float         angle = 360.0 / ((float) s->hsize * cs->nOutput);
	    float         xRot, vRot, xRotate, xRotate2, vRotate, p;
	    float         rYTrans;
	    CompVector    point  = { .v = { -0.5, -0.5, cs->distance, 1.0 } };
	    CompVector    point2 = { .v = { -0.5,  0.5, cs->distance, 1.0 } };
	    float         deform;

	    (*cs->getRotation) (s, &xRot, &vRot, &p);

	    cas->backVRotate = 0.0;

	    xRotate  = xRot;
	    xRotate2 = xRot;
	    vRotate  = vRot;

	    if (vRotate < 0.0)
		xRotate += 180;

	    vRotate = fmod (fabs (vRotate), 180.0);
	    xRotate = fmod (fabs (xRotate), angle);
	    xRotate2 = fmod (fabs (xRotate2), angle);

	    if (vRotate >= 90.0)
		vRotate = 180.0 - vRotate;

	    if (xRotate >= angle / 2.0)
		xRotate = angle - xRotate;

	    if (xRotate2 >= angle / 2.0)
		xRotate2 = angle - xRotate2;

	    xRotate = (cas->deform * angle * 0.5) +
		      ((1.0 - cas->deform) * xRotate);
	    xRotate2 = (cas->deform * angle * 0.5) +
		       ((1.0 - cas->deform) * xRotate2);

	    matrixGetIdentity (&pTransform);
	    matrixRotate (&pTransform, xRotate, 0.0f, 1.0f, 0.0f);
	    matrixRotate (&pTransform,
			  vRotate, cosf (xRotate * DEG2RAD),
			  0.0f, sinf (xRotate * DEG2RAD));

	    matrixMultiplyVector (&point, &point, &pTransform);

	    matrixGetIdentity (&pTransform);
	    matrixRotate (&pTransform, xRotate2, 0.0f, 1.0f, 0.0f);
	    matrixRotate (&pTransform,
			  vRotate, cosf (xRotate2 * DEG2RAD),
			  0.0f, sinf (xRotate2 * DEG2RAD));

	    matrixMultiplyVector (&point2, &point2, &pTransform);

	    switch (cubeaddonGetMode (s)) {
	    case ModeJumpyReflection:
		cas->yTrans    = 0.0;
		if (cubeaddonGetDeformation (s) == DeformationSphere &&
		    cubeaddonGetDeformCaps (s) && cubeaddonGetDrawBottom (s))
		{
		    rYTrans = sqrt (0.5 + (cs->distance * cs->distance)) *
			      -2.0;
		}
		else
		{
		    rYTrans = point.y * 2.0;
		}

		break;
	    case ModeDistance:
		cas->yTrans = 0.0;
		rYTrans     = sqrt (0.5 + (cs->distance * cs->distance)) * -2.0;
		break;
	    default:

		if (cubeaddonGetDeformation (s) == DeformationSphere &&
		    cubeaddonGetDeformCaps (s) && cubeaddonGetDrawBottom (s))
		{
		    cas->yTrans =  cas->capFill[1] - 0.5;
		    rYTrans     = -cas->capFill[1] - 0.5;
		}
		else if (cubeaddonGetDeformation (s) == DeformationSphere &&
		         vRotate > atan (cs->distance * 2) / DEG2RAD)
		{
		    cas->yTrans = sqrt (0.5 + (cs->distance * cs->distance)) - 0.5;
		    rYTrans     = -sqrt (0.5 + (cs->distance * cs->distance)) - 0.5;
		}
		else
		{
		    cas->yTrans = -point.y - 0.5;
		    rYTrans     =  point.y - 0.5;
		}
		break;
	    }

	    if (!cubeaddonGetAutoZoom (s) ||
		((cs->rotationState != RotationManual) &&
		 cubeaddonGetZoomManualOnly (s)))
	    {
		cas->zTrans = 0.0;
	    }
	    else
		cas->zTrans = -point2.z + cs->distance;

	    if (cubeaddonGetMode (s) == ModeAbove)
		cas->zTrans = 0.0;

	    if (cubeaddonGetDeformation (s) == DeformationCylinder) 
		deform = (sqrt (0.25 + (cs->distance * cs->distance)) -
			  cs->distance) * -cas->deform;
	    else if (cubeaddonGetDeformation (s) == DeformationSphere) 
		deform = (sqrt (0.5 + (cs->distance * cs->distance)) -
			  cs->distance) * -cas->deform;

	    if (cas->deform > 0.0)
	        cas->zTrans = deform;

	    if (cubeaddonGetMode (s) == ModeAbove && cas->vRot > 0.0)
	    {
		cas->backVRotate = cas->vRot;
		if (cubeaddonGetDeformation (s) == DeformationSphere &&
		    cubeaddonGetDeformCaps (s) && cubeaddonGetDrawBottom (s))
		{
		    cas->yTrans =  cas->capFill[1] - 0.5;
		    rYTrans     = -cas->capFill[1] - 0.5;
		}
		else
		{
		    cas->yTrans = 0.0;
		    rYTrans     = -1.0;
		}

		matrixGetIdentity (&pTransform);
		applyScreenTransform (s, sAttrib, output, &pTransform);
		point.x = point.y = 0.0;
		point.z = -cs->distance;
		point.z += deform;
		point.w = 1.0;
		matrixMultiplyVector (&point, &point, &pTransform);
		
		matrixTranslate (&rTransform, 0.0, 0.0, point.z);
		matrixRotate (&rTransform, cas->vRot, 1.0, 0.0, 0.0);
		matrixScale (&rTransform, 1.0, -1.0, 1.0);
		matrixTranslate (&rTransform, 0.0, -rYTrans,
				 -point.z + cas->zTrans);
	    }
	    else
	    {
		matrixTranslate (&rTransform, 0.0, rYTrans, cas->zTrans);
		matrixScale (&rTransform, 1.0, -1.0, 1.0);
	    }

	    glPushMatrix ();
	    glLoadIdentity ();
	    glScalef (1.0, -1.0, 1.0);
	    glLightfv (GL_LIGHT0, GL_POSITION, light0Position);
	    glPopMatrix ();
	    glCullFace (GL_FRONT);

	    UNWRAP (cas, s, paintTransformedOutput);
	    (*s->paintTransformedOutput) (s, sAttrib, &rTransform,
					  region, output, mask);
	    WRAP (cas, s, paintTransformedOutput,
		  cubeaddonPaintTransformedOutput);

	    glCullFace (GL_BACK);
	    glPushMatrix ();
	    glLoadIdentity ();
	    glLightfv (GL_LIGHT0, GL_POSITION, light0Position);
	    glPopMatrix ();

	    if (cubeaddonGetMode (s) == ModeAbove && cas->vRot > 0.0)
	    {
		int   j;
		float i, c;
		float v = MIN (1.0, cas->vRot / 30.0);
		float col1[4], col2[4];

		glPushMatrix ();

		glEnable (GL_BLEND);
		glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		glLoadIdentity ();
		glTranslatef (0.0, 0.0, -DEFAULT_Z_CAMERA);

		i = cubeaddonGetIntensity (s) * 2;
		c = cubeaddonGetIntensity (s);

		glBegin (GL_QUADS);
		glColor4f (0.0, 0.0, 0.0,
			   ((1 - v) * MAX (0.0, 1.0 - i)) + (v * c));
		glVertex2f (0.5, v / 2.0);
		glVertex2f (-0.5, v / 2.0);
		glColor4f (0.0, 0.0, 0.0,
			   ((1 - v) * MIN (1.0, 1.0 - (i - 1.0))) + (v * c));
		glVertex2f (-0.5, -0.5);
		glVertex2f (0.5, -0.5);
		glEnd ();

		for (j = 0; j < 4; j++)
		{
		    col1[j] = (1.0 - v) * cubeaddonGetGroundColor1 (s) [j] +
			      (v * (cubeaddonGetGroundColor1 (s) [j] +
				    cubeaddonGetGroundColor2 (s) [j]) * 0.5);
		    col1[j] /= 0xffff;
		    col2[j] = (1.0 - v) * cubeaddonGetGroundColor2 (s) [j] +
			      (v * (cubeaddonGetGroundColor1 (s) [j] +
				    cubeaddonGetGroundColor2 (s) [j]) * 0.5);
		    col2[j] /= 0xffff;
		}

		if (cubeaddonGetGroundSize (s) > 0.0)
		{
		    glBegin (GL_QUADS);
		    glColor4fv (col1);
		    glVertex2f (-0.5, -0.5);
		    glVertex2f (0.5, -0.5);
		    glColor4fv (col2);
		    glVertex2f (0.5, -0.5 +
				((1 - v) * cubeaddonGetGroundSize (s)) + v);
		    glVertex2f (-0.5, -0.5 +
				((1 - v) * cubeaddonGetGroundSize (s)) + v);
		    glEnd ();
		}

		glColor4usv (defaultColor);

		glBlendFunc (GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
		glDisable (GL_BLEND);
		glPopMatrix ();
	    }
	    else
		drawBasicGround (s);
	}
	
	memset (cs->capsPainted, 0, sizeof (Bool) * s->nOutputDev);
	cas->reflection = FALSE;
    }

    if (!cubeaddonGetReflection (s))
    {
	cas->yTrans = 0.0;
	cas->zTrans = (sqrt (0.25 + (cs->distance * cs->distance)) -
		     cs->distance) * -cas->deform;
    }

    matrixTranslate (&sTransform, 0.0, cas->yTrans, cas->zTrans);

    UNWRAP (cas, s, paintTransformedOutput);
    (*s->paintTransformedOutput) (s, sAttrib, &sTransform,
				  region, output, mask);
    WRAP (cas, s, paintTransformedOutput, cubeaddonPaintTransformedOutput);
}

static Bool
cubeaddonPaintOutput (CompScreen              *s,
		      const ScreenPaintAttrib *sAttrib,
		      const CompTransform     *transform,
		      Region                  region,
		      CompOutput              *output,
		      unsigned int            mask)
{
    Bool status;

    CUBEADDON_SCREEN (s);

    if (cas->last != output)
	cas->first = TRUE;

    cas->last = output;

    UNWRAP (cas, s, paintOutput);
    status = (*s->paintOutput) (s, sAttrib, transform, region, output, mask);
    WRAP (cas, s, paintOutput, cubeaddonPaintOutput);

    return status;
}

static void
cubeaddonDonePaintScreen (CompScreen * s)
{
    CUBEADDON_SCREEN (s);

    cas->first      = TRUE;
    cas->yTrans     = 0.0;
    cas->zTrans     = 0.0;

    cas->wasDeformed = (cas->deform > 0.0);

    if (cas->deform > 0.0 && cas->deform < 1.0)
    {
	damageScreen (s);
	cas->deform = 0.0;
    }

    UNWRAP (cas, s, donePaintScreen);
    (*s->donePaintScreen) (s);
    WRAP (cas, s, donePaintScreen, cubeaddonDonePaintScreen);
}


static Bool
cubeaddonInitDisplay (CompPlugin  *p,
		      CompDisplay *d)
{
    CubeaddonDisplay *cad;

    if (!checkPluginABI ("core", CORE_ABIVERSION) ||
	!checkPluginABI ("cube", CUBE_ABIVERSION))
	return FALSE;

    if (!getPluginDisplayIndex (d, "cube", &cubeDisplayPrivateIndex))
	return FALSE;

    cad = malloc (sizeof (CubeaddonDisplay));

    if (!cad)
	return FALSE;

    cad->screenPrivateIndex = allocateScreenPrivateIndex (d);

    if (cad->screenPrivateIndex < 0)
    {
	free (cad);
	return FALSE;
    }

    d->base.privates[CubeaddonDisplayPrivateIndex].ptr = cad;

    cubeaddonSetTopNextKeyInitiate (d, cubeaddonTopNext);
    cubeaddonSetTopPrevKeyInitiate (d, cubeaddonTopPrev);
    cubeaddonSetBottomNextKeyInitiate (d, cubeaddonBottomNext);
    cubeaddonSetBottomPrevKeyInitiate (d, cubeaddonBottomPrev);

    cubeaddonSetTopNextButtonInitiate (d, cubeaddonTopNext);
    cubeaddonSetTopPrevButtonInitiate (d, cubeaddonTopPrev);
    cubeaddonSetBottomNextButtonInitiate (d, cubeaddonBottomNext);
    cubeaddonSetBottomPrevButtonInitiate (d, cubeaddonBottomPrev);
    
    return TRUE;
}

static void
cubeaddonFiniDisplay (CompPlugin  *p,
		      CompDisplay *d)
{
    CUBEADDON_DISPLAY (d);

    freeScreenPrivateIndex (d, cad->screenPrivateIndex);
    free (cad);
}

static Bool
cubeaddonInitScreen (CompPlugin *p,
		     CompScreen *s)
{
    CubeaddonScreen *cas;
    GLushort        *idx;
    int             i, j;

    CUBEADDON_DISPLAY (s->display);
    CUBE_SCREEN (s);

    cas = malloc (sizeof (CubeaddonScreen));

    if (!cas)
	return FALSE;

    s->base.privates[cad->screenPrivateIndex].ptr = cas;

    cas->reflection  = FALSE;
    cas->first       = TRUE;
    cas->last        = NULL;
    cas->yTrans      = 0.0;
    cas->zTrans      = 0.0;
    cas->tmpRegion   = XCreateRegion ();
    cas->deform      = 0.0;
    cas->capDeform   = -1.0;
    cas->capDistance = cs->distance;

    cas->winNormals  = NULL;
    cas->winNormSize = 0;

    cas->tmpBox  = NULL;
    cas->nTmpBox = 0;

    idx = cas->capFillIdx;
    for (i = 0; i < CAP_ELEMENTS - 1; i++)
    {
	for (j = 0; j < CAP_ELEMENTS; j++)
	{
	    idx[0] = 1 + (i * (CAP_ELEMENTS + 1)) + j;
	    idx[1] = 1 + ((i + 1) * (CAP_ELEMENTS + 1)) + j;
	    idx[2] = 2 + ((i + 1) * (CAP_ELEMENTS + 1)) + j;
	    idx[3] = 2 + (i * (CAP_ELEMENTS + 1)) + j;
	    idx += 4;
	}
    }

    cubeaddonInitCap (s, &cas->topCap);
    cubeaddonInitCap (s, &cas->bottomCap);

    cas->topCap.files = cubeaddonGetTopImages (s);
    cas->bottomCap.files = cubeaddonGetBottomImages (s);

    cubeaddonSetTopImagesNotify (s, cubeaddonTopImagesChanged);
    cubeaddonSetBottomImagesNotify (s, cubeaddonBottomImagesChanged);

    cubeaddonSetTopScaleNotify (s, cubeaddonTopImageChanged);
    cubeaddonSetTopAspectNotify (s, cubeaddonTopImageChanged);
    cubeaddonSetTopClampNotify (s, cubeaddonTopImageChanged);
    cubeaddonSetBottomScaleNotify (s, cubeaddonBottomImageChanged);
    cubeaddonSetBottomAspectNotify (s, cubeaddonBottomImageChanged);
    cubeaddonSetBottomClampNotify (s, cubeaddonTopImageChanged);

    cubeaddonChangeCap (s, TRUE, 0);
    cubeaddonChangeCap (s, FALSE, 0);

    WRAP (cas, s, paintTransformedOutput, cubeaddonPaintTransformedOutput);
    WRAP (cas, s, paintOutput, cubeaddonPaintOutput);
    WRAP (cas, s, donePaintScreen, cubeaddonDonePaintScreen);
    WRAP (cas, s, addWindowGeometry, cubeaddonAddWindowGeometry);
    WRAP (cas, s, drawWindow, cubeaddonDrawWindow);
    WRAP (cas, s, drawWindowTexture, cubeaddonDrawWindowTexture);


    WRAP (cas, cs, clearTargetOutput, cubeaddonClearTargetOutput);
    WRAP (cas, cs, getRotation, cubeaddonGetRotation);
    WRAP (cas, cs, checkOrientation, cubeaddonCheckOrientation);
    WRAP (cas, cs, shouldPaintViewport, cubeaddonShouldPaintViewport);
    WRAP (cas, cs, paintTop, cubeaddonPaintTop);
    WRAP (cas, cs, paintBottom, cubeaddonPaintBottom);

    return TRUE;
}

static void
cubeaddonFiniScreen (CompPlugin *p,
		     CompScreen *s)
{
    CUBEADDON_SCREEN (s);
    CUBE_SCREEN (s);

    XDestroyRegion (cas->tmpRegion);

    UNWRAP (cas, s, paintTransformedOutput);
    UNWRAP (cas, s, paintOutput);
    UNWRAP (cas, s, donePaintScreen);
    UNWRAP (cas, s, addWindowGeometry);
    UNWRAP (cas, s, drawWindow);
    UNWRAP (cas, s, drawWindowTexture);

    UNWRAP (cas, cs, clearTargetOutput);
    UNWRAP (cas, cs, getRotation);
    UNWRAP (cas, cs, checkOrientation);
    UNWRAP (cas, cs, shouldPaintViewport);
    UNWRAP (cas, cs, paintTop);
    UNWRAP (cas, cs, paintBottom);

    free (cas);
}

static Bool
cubeaddonInit (CompPlugin *p)
{
    CubeaddonDisplayPrivateIndex = allocateDisplayPrivateIndex ();

    if (CubeaddonDisplayPrivateIndex < 0)
	return FALSE;

    return TRUE;
}

static void
cubeaddonFini (CompPlugin *p)
{
    freeDisplayPrivateIndex (CubeaddonDisplayPrivateIndex);
}

static CompBool
cubeaddonInitObject (CompPlugin *p,
		     CompObject *o)
{
    static InitPluginObjectProc dispTab[] = {
	(InitPluginObjectProc) 0, /* InitCore */
	(InitPluginObjectProc) cubeaddonInitDisplay,
	(InitPluginObjectProc) cubeaddonInitScreen
    };

    RETURN_DISPATCH (o, dispTab, ARRAY_SIZE (dispTab), TRUE, (p, o));
}

static void
cubeaddonFiniObject (CompPlugin *p,
		     CompObject *o)
{
    static FiniPluginObjectProc dispTab[] = {
	(FiniPluginObjectProc) 0, /* FiniCore */
	(FiniPluginObjectProc) cubeaddonFiniDisplay,
	(FiniPluginObjectProc) cubeaddonFiniScreen
    };

    DISPATCH (o, dispTab, ARRAY_SIZE (dispTab), (p, o));
}

CompPluginVTable cubeaddonVTable = {
    "cubeaddon",
    0,
    cubeaddonInit,
    cubeaddonFini,
    cubeaddonInitObject,
    cubeaddonFiniObject,
    0,
    0
};

CompPluginVTable *
getCompPluginInfo (void)
{
    return &cubeaddonVTable;
}
