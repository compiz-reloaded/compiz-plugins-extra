/*
 * Compiz cube reflection plugin
 *
 * cubereflex.c
 *
 * Copyright : (C) 2007 by Dennis Kasprzyk
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

#include <compiz.h>
#include <cube.h>

#include "cubereflex_options.h"

#define DEG2RAD (M_PI / 180.0f)

static const CompTransform identity = {
    {
		1.0, 0.0, 0.0, 0.0,
		0.0, 1.0, 0.0, 0.0,
		0.0, 0.0, 1.0, 0.0,
		0.0, 0.0, 0.0, 1.0
    }
};

#define MULTMV(m, v) { \
double v0 = m[0]*v[0] + m[4]*v[1] + m[8]*v[2] + m[12]*v[3]; \
double v1 = m[1]*v[0] + m[5]*v[1] + m[9]*v[2] + m[13]*v[3]; \
double v2 = m[2]*v[0] + m[6]*v[1] + m[10]*v[2] + m[14]*v[3]; \
double v3 = m[3]*v[0] + m[7]*v[1] + m[11]*v[2] + m[15]*v[3]; \
v[0] = v0; v[1] = v1; v[2] = v2; v[3] = v3; }

static int displayPrivateIndex;

static int cubeDisplayPrivateIndex;

typedef struct _CubereflexDisplay
{
	int screenPrivateIndex;
	
} CubereflexDisplay;

typedef struct _CubereflexScreen
{
	DonePaintScreenProc donePaintScreen;
	PaintOutputProc paintOutput;
	PaintScreenProc paintScreen;
	PreparePaintScreenProc preparePaintScreen;
	PaintTransformedOutputProc paintTransformedOutput;

	CubeClearTargetOutputProc clearTargetOutput;

	Bool reflection;
	Bool first;
	CompOutput *last;

} CubereflexScreen;

#define GET_CUBEREFLEX_DISPLAY(d) \
		((CubereflexDisplay *) (d)->privates[displayPrivateIndex].ptr)
#define CUBEREFLEX_DISPLAY(d) \
		CubereflexDisplay *rd = GET_CUBEREFLEX_DISPLAY(d);

#define GET_CUBEREFLEX_SCREEN(s, rd) \
		((CubereflexScreen *) (s)->privates[(rd)->screenPrivateIndex].ptr)
#define CUBEREFLEX_SCREEN(s) \
		CubereflexScreen *rs = GET_CUBEREFLEX_SCREEN(s, GET_CUBEREFLEX_DISPLAY(s->display))

static void
cubereflexClearTargetOutput (CompScreen *s, float xRotate, float vRotate)
{
	CUBEREFLEX_SCREEN(s);
	CUBE_SCREEN(s);

	if (rs->reflection)
		glCullFace(GL_BACK);
	
	UNWRAP(rs, cs, clearTargetOutput);
	(*cs->clearTargetOutput) (s, xRotate, 0.0);
	WRAP(rs, cs, clearTargetOutput, cubereflexClearTargetOutput);

	if (rs->reflection)
		glCullFace(GL_FRONT);
}

static void cubereflexPaintTransformedOutput(CompScreen * s,
											const ScreenPaintAttrib * sAttrib,
											const CompTransform    *transform,
											Region region, CompOutput *output,
											unsigned int mask)
{
	static GLfloat light0Position[] = { -0.5f, 0.5f, -9.0f, 1.0f };
	CUBEREFLEX_SCREEN(s);
	CUBE_SCREEN(s);
	CompTransform sTransform = *transform;

	if (cs->invert == 1 && rs->first)
	{
		rs->first = FALSE;
		rs->reflection = TRUE;
		
		if (cs->grabIndex)
		{
			CompTransform rTransform = *transform;
			matrixTranslate(&rTransform, 0.0, -1.0, 0.0);
			
			matrixScale(&rTransform, 1.0, -1.0, 1.0);

			glCullFace(GL_FRONT);
			UNWRAP(rs, s, paintTransformedOutput);
			(*s->paintTransformedOutput) (s, sAttrib, &rTransform, region, output, mask);
			WRAP(rs, s, paintTransformedOutput, cubereflexPaintTransformedOutput);
			glCullFace(GL_BACK);
		}
		else
		{
			CompTransform rTransform = *transform;
			CompTransform pTransform = identity;
			float angle = 360.0 / ((float)s->hsize * cs->nOutput);
			float xRotate,xRotate2,vRotate;
			double point[4] = {-0.5, -0.5, cs->distance, 1.0};
			double point2[4] = {-0.5, 0.5, cs->distance, 1.0};
			
			(*cs->getRotation) (s, &xRotate, &vRotate);

			xRotate2 = xRotate;
			
			if (vRotate < 0.0)
				xRotate += 180;
			
			vRotate = fmod(fabs(vRotate), 180.0);
			xRotate = fmod(fabs(xRotate), angle);
			xRotate2 = fmod(fabs(xRotate2), angle);
			if (vRotate >= 90.0)
				vRotate = 180.0 - vRotate;
			if (xRotate >= angle / 2.0)
				xRotate = angle - xRotate;
			if (xRotate2 >= angle / 2.0)
				xRotate2 = angle - xRotate2;

			matrixRotate (&pTransform, xRotate, 0.0f, 1.0f, 0.0f);
			matrixRotate (&pTransform, vRotate, cosf (xRotate * DEG2RAD),
						0.0f, sinf (xRotate * DEG2RAD));

			MULTMV(pTransform.m, point);

			pTransform = identity;
			matrixRotate (&pTransform, xRotate2, 0.0f, 1.0f, 0.0f);
			matrixRotate (&pTransform, vRotate, cosf (xRotate2 * DEG2RAD),
						0.0f, sinf (xRotate2 * DEG2RAD));
			MULTMV(pTransform.m, point2);
			
			matrixTranslate(&rTransform, 0.0, point[1] - 0.5,
					-point2[2] + cs->distance);
			
			matrixScale(&rTransform, 1.0, -1.0, 1.0);

			glPushMatrix();
			glLoadIdentity();
			glScalef(1.0, -1.0, 1.0);
			glLightfv (GL_LIGHT0, GL_POSITION, light0Position);
			glPopMatrix();
			
			glCullFace(GL_FRONT);
			UNWRAP(rs, s, paintTransformedOutput);
			(*s->paintTransformedOutput) (s, sAttrib, &rTransform, region, output, mask);
			WRAP(rs, s, paintTransformedOutput, cubereflexPaintTransformedOutput);
			glCullFace(GL_BACK);

			glPushMatrix();
			glLoadIdentity();
			glLightfv (GL_LIGHT0, GL_POSITION, light0Position);
			glPopMatrix();

			matrixTranslate(&sTransform, 0.0, -point[1] - 0.5,
					-point2[2] + cs->distance);
		}
		
		rs->reflection = FALSE;
		
		glPushMatrix();
		
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		glLoadIdentity();
		glTranslatef(0.0, 0.0, -DEFAULT_Z_CAMERA);
		
		glBegin(GL_QUADS);
			glColor4f(0.0, 0.0, 0.0, 1.0 - cubereflexGetIntensity(s));
			glVertex2f(0.5, 0.0);
			glVertex2f(-0.5, 0.0);
			glColor4f(0.0, 0.0, 0.0, 1.0);
			glVertex2f(-0.5, -0.5);
			glVertex2f(0.5, -0.5);
		glEnd();

		if (cubereflexGetGroundSize(s) > 0.0)
		{
			glBegin(GL_QUADS);
				glColor4usv(cubereflexGetGroundColor1(s));
				glVertex2f(-0.5, -0.5);
				glVertex2f(0.5, -0.5);
				glColor4usv(cubereflexGetGroundColor2(s));
				glVertex2f(0.5, -0.5 + cubereflexGetGroundSize(s));
				glVertex2f(-0.5,-0.5 + cubereflexGetGroundSize(s));
			glEnd();
		}

		glColor4usv(defaultColor);
		glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
		glDisable(GL_BLEND);
		glPopMatrix();
		
	}
	
	UNWRAP(rs, s, paintTransformedOutput);
	(*s->paintTransformedOutput) (s, sAttrib, &sTransform, region, output, mask);
	WRAP(rs, s, paintTransformedOutput, cubereflexPaintTransformedOutput);
}

static Bool cubereflexPaintOutput(CompScreen * s,
								  const ScreenPaintAttrib * sAttrib,
								  const CompTransform    *transform,
								  Region region, CompOutput *output,
								  unsigned int mask)
{
	Bool status;

	CUBEREFLEX_SCREEN(s);

	if (s->nOutputDev == 1 || rs->last != output)
		rs->first = TRUE;
	rs->last = output;

	UNWRAP(rs, s, paintOutput);
	status = (*s->paintOutput) (s, sAttrib, transform, region, output, mask);
	WRAP(rs, s, paintOutput, cubereflexPaintOutput);

	return status;
}

static void cubereflexDonePaintScreen(CompScreen * s)
{
	CUBEREFLEX_SCREEN(s);

	rs->first = FALSE;
	
	UNWRAP(rs, s, donePaintScreen);
	(*s->donePaintScreen) (s);
	WRAP(rs, s, donePaintScreen, cubereflexDonePaintScreen);
}


static Bool cubereflexInitDisplay(CompPlugin * p, CompDisplay * d)
{
	CubereflexDisplay *rd;
	CompPlugin	     *cube = findActivePlugin ("cube");

	CompOption	*option;
	int			nOption;

	if (!cube || !cube->vTable->getDisplayOptions)
		return FALSE;

	option = (*cube->vTable->getDisplayOptions) (cube, d, &nOption);

	if (getIntOptionNamed (option, nOption, "abi", 0) != CUBE_ABIVERSION)
	{
		compLogMessage (d, "cubereflex", CompLogLevelError,
						"cube ABI version mismatch");
		return FALSE;
	}

	cubeDisplayPrivateIndex = getIntOptionNamed (option, nOption, "index", -1);
	if (cubeDisplayPrivateIndex < 0)
		return FALSE;

	rd = malloc(sizeof(CubereflexDisplay));
	if (!rd)
		return FALSE;

	rd->screenPrivateIndex = allocateScreenPrivateIndex(d);

	if (rd->screenPrivateIndex < 0)
	{
		free(rd);
		return FALSE;
	}

	d->privates[displayPrivateIndex].ptr = rd;

	return TRUE;
}

static void cubereflexFiniDisplay(CompPlugin * p, CompDisplay * d)
{
	CUBEREFLEX_DISPLAY(d);

	freeScreenPrivateIndex(d, rd->screenPrivateIndex);
	free(rd);
}

static Bool cubereflexInitScreen(CompPlugin * p, CompScreen * s)
{
	CubereflexScreen *rs;

	CUBEREFLEX_DISPLAY(s->display);
	CUBE_SCREEN(s);

	rs = malloc(sizeof(CubereflexScreen));

	if (!rs)
		return FALSE;

	s->privates[rd->screenPrivateIndex].ptr = rs;

	rs->reflection = FALSE;
	rs->first      = TRUE;
	rs->last       = NULL;
	
	WRAP(rs, s, paintTransformedOutput, cubereflexPaintTransformedOutput);
	WRAP(rs, s, paintOutput, cubereflexPaintOutput);
	WRAP(rs, s, donePaintScreen, cubereflexDonePaintScreen);

	WRAP(rs, cs, clearTargetOutput, cubereflexClearTargetOutput);
	
	return TRUE;
}

static void cubereflexFiniScreen(CompPlugin * p, CompScreen * s)
{
	CUBEREFLEX_SCREEN(s);
	CUBE_SCREEN(s);
	
	UNWRAP(rs, s, paintTransformedOutput);
	UNWRAP(rs, s, paintOutput);
	UNWRAP(rs, s, donePaintScreen);

	UNWRAP(rs, cs, clearTargetOutput);
	
	free(rs);
}

static Bool cubereflexInit(CompPlugin * p)
{
	displayPrivateIndex = allocateDisplayPrivateIndex();

	if (displayPrivateIndex < 0)
		return FALSE;

	return TRUE;
}

static void cubereflexFini(CompPlugin * p)
{
	if (displayPrivateIndex >= 0)
		freeDisplayPrivateIndex(displayPrivateIndex);
}

static int
cubereflexGetVersion (CompPlugin * plugin, int version)
{
	return ABIVERSION;
}

CompPluginVTable cubereflexVTable = {
	"cubereflex",
	cubereflexGetVersion,
	0,
	cubereflexInit,
	cubereflexFini,
	cubereflexInitDisplay,
	cubereflexFiniDisplay,
	cubereflexInitScreen,
	cubereflexFiniScreen,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0
};

CompPluginVTable *
getCompPluginInfo (void)
{
	return &cubereflexVTable;
}
