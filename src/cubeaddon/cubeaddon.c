/*
 * Compiz cube reflection and cylinder deformation plugin
 *
 * cubeaddon.c
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
#define CUBEADDON_CAP_ELEMENTS 30

typedef struct _CubeaddonDisplay
{
    int screenPrivateIndex;
} CubeaddonDisplay;

typedef struct _CubeaddonScreen
{
    DonePaintScreenProc        donePaintScreen;
    PaintOutputProc            paintOutput;
    PaintScreenProc            paintScreen;
    PreparePaintScreenProc     preparePaintScreen;
    PaintTransformedOutputProc paintTransformedOutput;
    AddWindowGeometryProc      addWindowGeometry;

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

    Region tmpRegion;

    GLfloat capFill[CUBEADDON_CAP_ELEMENTS * 12];
} CubeaddonScreen;

#define CUBEADDON_DISPLAY(d) PLUGIN_DISPLAY(d, Cubeaddon, ca)
#define CUBEADDON_SCREEN(s) PLUGIN_SCREEN(s, Cubeaddon, ca)

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

    if (cas->deform > 0.0)
    {
	float z[3];
	Bool  ftb1, ftb2, ftb3;

	z[0] = cs->invert * cs->distance;
	z[1] = z[0] + (0.25 / cs->distance);
	z[2] = cs->invert * sqrt (0.25 + (cs->distance * cs->distance));

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

	return (order == FTB && (ftb1 || ftb2 || ftb3)) ||
	       (order == BTF && (!ftb1 || !ftb2 || !ftb3)) || rv;
    }

    return rv;
}

static void
cubeaddonPaintTop (CompScreen		  *s,
		  const ScreenPaintAttrib *sAttrib,
		  const CompTransform     *transform,
		  CompOutput		  *output,
		  int			  size)
{
    ScreenPaintAttrib sa;
    CompTransform     sTransform;
    int               i, opacity;
    Bool              wasCulled = glIsEnabled (GL_CULL_FACE);

    CUBE_SCREEN (s);
    CUBEADDON_SCREEN (s);

    UNWRAP (cas, cs, paintTop);
    (*cs->paintTop) (s, sAttrib, transform, output, size);
    WRAP (cas, cs, paintTop, cubeaddonPaintTop);

    if (cas->deform == 0.0 || !cubeaddonGetFillCaps (s))
	return;

    for (i = 0; i < CUBEADDON_CAP_ELEMENTS * 4; i++)
	cas->capFill[(i * 3) + 1] = 0.5;

    opacity = (cs->desktopOpacity * cubeaddonGetTopColor(s)[3]) / 0xffff;
    glColor4us (cubeaddonGetTopColor(s)[0],
		cubeaddonGetTopColor(s)[1],
		cubeaddonGetTopColor(s)[2],
		opacity);

    glPushMatrix ();

    if (cs->desktopOpacity != OPAQUE)
    {
	glEnable (GL_BLEND);
	glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }

    glDisableClientState (GL_TEXTURE_COORD_ARRAY);
    glVertexPointer (3, GL_FLOAT, 0, cas->capFill);

    glDisable (GL_CULL_FACE);

    for (i = 0; i < size; i++)
    {
	sa = *sAttrib;
	sTransform = *transform;
	sa.yRotate += (360.0f / size) * i;

	(*s->applyScreenTransform) (s, &sa, output, &sTransform);

        glLoadMatrixf (sTransform.m);

	glDrawArrays (GL_TRIANGLE_FAN, 0, CUBEADDON_CAP_ELEMENTS + 2);
    }
    glEnableClientState (GL_TEXTURE_COORD_ARRAY);
    glDisable (GL_BLEND);
    glBlendFunc (GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

    if (wasCulled)
	glEnable (GL_CULL_FACE);

    glPopMatrix ();

    glColor4usv (defaultColor);
}

/*
 * Paint bottom cube face
 */
static void
cubeaddonPaintBottom (CompScreen		     *s,
		      const ScreenPaintAttrib *sAttrib,
		      const CompTransform     *transform,
		      CompOutput		     *output,
		      int		     size)
{
    ScreenPaintAttrib sa;
    CompTransform     sTransform;
    int               i, opacity;
    Bool              wasCulled = glIsEnabled (GL_CULL_FACE);

    CUBE_SCREEN (s);
    CUBEADDON_SCREEN (s);

    UNWRAP (cas, cs, paintBottom);
    (*cs->paintBottom) (s, sAttrib, transform, output, size);
    WRAP (cas, cs, paintBottom, cubeaddonPaintBottom);

    if (cas->deform == 0.0 || !cubeaddonGetFillCaps (s))
	return;

    for (i = 0; i < CUBEADDON_CAP_ELEMENTS * 4; i++)
	cas->capFill[(i * 3) + 1] = -0.5;

    opacity = (cs->desktopOpacity * cubeaddonGetBottomColor(s)[3]) / 0xffff;
    glColor4us (cubeaddonGetBottomColor(s)[0],
		cubeaddonGetBottomColor(s)[1],
		cubeaddonGetBottomColor(s)[2],
		opacity);

    glPushMatrix ();

    if (cs->desktopOpacity != OPAQUE)
    {
	glEnable (GL_BLEND);
	glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }

    glDisableClientState (GL_TEXTURE_COORD_ARRAY);
    glVertexPointer (3, GL_FLOAT, 0, cas->capFill);

    glDisable (GL_CULL_FACE);

    for (i = 0; i < size; i++)
    {
	sa = *sAttrib;
	sTransform = *transform;
	sa.yRotate += (360.0f / size) * i;

	(*s->applyScreenTransform) (s, &sa, output, &sTransform);

        glLoadMatrixf (sTransform.m);

	glDrawArrays (GL_TRIANGLE_FAN, 0, CUBEADDON_CAP_ELEMENTS + 2);
    }
    glEnableClientState (GL_TEXTURE_COORD_ARRAY);
    glDisable (GL_BLEND);
    glBlendFunc (GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

    if (wasCulled)
	glEnable (GL_CULL_FACE);

    glPopMatrix ();

    glColor4usv (defaultColor);
}

static void
cubeaddonAddWindowGeometry (CompWindow *w,
			     CompMatrix *matrix,
			     int	nMatrix,
			     Region     region,
			     Region     clip)
{
    CompScreen *s = w->screen;

    CUBEADDON_SCREEN (s);
    CUBE_SCREEN (s);

    if (cas->deform > 0.0 && s->desktopWindowCount)
    {
	int         x1, x2, i, oldVCount = w->vCount;
	REGION      reg;
	GLfloat     *v;
	int         offX = 0, offY = 0;
	int         sx1, sx2, sw;

	const float radSquare = (cs->distance * cs->distance) + 0.25;
	float       ang;
			
	reg.numRects = 1;
	reg.rects = &reg.extents;

	reg.extents.y1 = region->extents.y1;
	reg.extents.y2 = region->extents.y2;

	x1 = region->extents.x1;
	x2 = MIN (x1 + CUBEADDON_GRID_SIZE, region->extents.x2);
	
	UNWRAP (cas, s, addWindowGeometry);
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
	}
	else if (cs->moMode == CUBE_MOMODE_MULTI)
	{
	    sx1 = cas->last->region.extents.x1;
	    sx2 = cas->last->region.extents.x2;
	    sw  = sx2 - sx1;
	}
	else
	{
	    sx1 = s->outputDev[cs->srcOutput].region.extents.x1;
	    sx2 = s->outputDev[cs->srcOutput].region.extents.x2;
	    sw  = sx2 - sx1;
	}
	
	for (i = oldVCount; i < w->vCount; i++)
	{
	    if (v[0] + offX >= sx1 - CUBEADDON_GRID_SIZE &&
		v[0] + offX < sx2 + CUBEADDON_GRID_SIZE)
	    {
		ang = (((v[0] + offX - sx1) / (float)sw) - 0.5);
		ang *= ang;
		if (ang < radSquare)
		{
		    v[2] = sqrt (radSquare - ang) - cs->distance;
		    v[2] *= cas->deform;
		}
	    }

	    v += w->vertexStride;
	}
    }
    else
    {
	UNWRAP (cas, s, addWindowGeometry);
	(*w->screen->addWindowGeometry) (w, matrix, nMatrix, region, clip);
	WRAP (cas, s, addWindowGeometry, cubeaddonAddWindowGeometry);
    }
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
    float          x, progress;

    CUBEADDON_SCREEN (s);
    CUBE_SCREEN (s);

    if (cubeaddonGetCylinder (s) && s->hsize * cs->nOutput > 2 &&
	(cs->rotationState == RotationManual ||
	(cs->rotationState == RotationChange &&
	!cubeaddonGetCylinderManualOnly (s)) || cas->deform > 0.0))
    {
	const float angle = atan (0.5 / cs->distance);
	const float rad = 0.5 / sin (angle);
	float       z, *quad;
	int         i;
	
	(*cs->getRotation) (s, &x, &x, &progress);
	cas->deform = progress;

	cas->capFill[0] = 0.0;
	cas->capFill[2] = cs->distance;

	for (i = 0; i <= CUBEADDON_CAP_ELEMENTS; i++)
	{
	    x = -0.5 + ((float)i / (float)CUBEADDON_CAP_ELEMENTS);
	    z = ((cos (asin (x / rad)) * rad) - cs->distance) * cas->deform;

	    quad = &cas->capFill[(i + 1) * 3];
	    quad[0] = x;
	    quad[2] = cs->distance + z;
	}
    }
    else
    {
	cas->deform = 0.0;
    }

    if (cs->invert == 1 && cas->first && cubeaddonGetReflection (s))
    {
	cas->first = FALSE;
	cas->reflection = TRUE;

	if (cs->grabIndex)
	{
	    CompTransform rTransform = *transform;

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
	    CompTransform rTransform = *transform;
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
		cas->yTrans     = 0.0;
		rYTrans        = point.y * 2.0;
		break;
	    case ModeDistance:
		cas->yTrans     = 0.0;
		rYTrans        = sqrt (0.5 + (cs->distance * cs->distance)) *
				 -2.0;
		break;
	    default:
		cas->yTrans     = -point.y - 0.5;
		rYTrans        = point.y - 0.5;
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

	    deform = (sqrt (0.25 + (cs->distance * cs->distance)) -
		     cs->distance) * -cas->deform;

	    if (cas->deform > 0.0)
	        cas->zTrans = deform;

	    if (cubeaddonGetMode (s) == ModeAbove && cas->vRot > 0.0)
	    {
		cas->backVRotate = cas->vRot;
		cas->yTrans      = 0.0;
		rYTrans         = 0.0;

		matrixGetIdentity (&pTransform);
		(*s->applyScreenTransform) (s, sAttrib, output, &pTransform);
		point.x = point.y = 0.0;
		point.z = -cs->distance;
		point.z += deform;
		point.w = 1.0;
		matrixMultiplyVector (&point, &point, &pTransform);
		
		matrixTranslate (&rTransform, 0.0, 0.0, point.z);
		matrixRotate (&rTransform, cas->vRot, 1.0, 0.0, 0.0);
		matrixScale (&rTransform, 1.0, -1.0, 1.0);
		matrixTranslate (&rTransform, 0.0, 1.0, 0.0);
		matrixTranslate (&rTransform, 0.0, 0.0, -point.z + cas->zTrans);
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

    CUBEADDON_DISPLAY (s->display);
    CUBE_SCREEN (s);

    cas = malloc (sizeof (CubeaddonScreen));

    if (!cas)
	return FALSE;

    s->base.privates[cad->screenPrivateIndex].ptr = cas;

    cas->reflection = FALSE;
    cas->first      = TRUE;
    cas->last       = NULL;
    cas->yTrans     = 0.0;
    cas->zTrans     = 0.0;
    cas->tmpRegion  = XCreateRegion ();
    cas->deform     = 0.0;

    WRAP (cas, s, paintTransformedOutput, cubeaddonPaintTransformedOutput);
    WRAP (cas, s, paintOutput, cubeaddonPaintOutput);
    WRAP (cas, s, donePaintScreen, cubeaddonDonePaintScreen);
    WRAP (cas, s, addWindowGeometry, cubeaddonAddWindowGeometry);

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
