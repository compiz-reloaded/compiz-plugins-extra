/**
 *
 * Compiz benchmark plugin
 *
 * bench.c
 *
 * Copyright : (C) 2006 by Dennis Kasprzyk
 * E-mail    : onestone@beryl-project.org
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

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <X11/Xatom.h>
#include <X11/extensions/Xrender.h>
#include <compiz.h>

#include "bench_tex.h"
#include "bench_options.h"

#define GET_BENCH_DISPLAY(d)                                  \
    ((BenchDisplay *) (d)->privates[displayPrivateIndex].ptr)

#define BENCH_DISPLAY(d)                      \
    BenchDisplay *bd = GET_BENCH_DISPLAY (d)

#define GET_BENCH_SCREEN(s, bd)                                   \
    ((BenchScreen *) (s)->privates[(bd)->screenPrivateIndex].ptr)

#define BENCH_SCREEN(s)                                                      \
    BenchScreen *bs = GET_BENCH_SCREEN (s, GET_BENCH_DISPLAY (s->display))

#define TIMEVALDIFF(tv1, tv2)                                              \
    (((tv1)->tv_sec == (tv2)->tv_sec || (tv1)->tv_usec >= (tv2)->tv_usec) ? \
     ((((tv1)->tv_sec - (tv2)->tv_sec) * 1000000) +                         \
      ((tv1)->tv_usec - (tv2)->tv_usec)) / 1000 :                           \
     ((((tv1)->tv_sec - 1 - (tv2)->tv_sec) * 1000000) +                     \
      (1000000 + (tv1)->tv_usec - (tv2)->tv_usec)) / 1000)

#define TIMEVALDIFFU(tv1, tv2)                                              \
    (((tv1)->tv_sec == (tv2)->tv_sec || (tv1)->tv_usec >= (tv2)->tv_usec) ? \
     ((((tv1)->tv_sec - (tv2)->tv_sec) * 1000000) +                      \
      ((tv1)->tv_usec - (tv2)->tv_usec)):                                   \
     ((((tv1)->tv_sec - 1 - (tv2)->tv_sec) * 1000000) +                  \
      (1000000 + (tv1)->tv_usec - (tv2)->tv_usec)))

#ifdef GL_DEBUG
static GLenum gl_error;

#define GLERR  gl_error=glGetError(); if (gl_error !=  GL_NO_ERROR) { fprintf (stderr,"GL error 0x%X has occured at %s:%d\n",gl_error,__FILE__,__LINE__); }
#else
#define GLERR
#endif

static int displayPrivateIndex = 0;

typedef struct _BenchDisplay
{
	int screenPrivateIndex;
	Bool active;

} BenchDisplay;

typedef struct _BenchScreen
{
	GLuint dList;
	float rrVal;
	float fps;
	float alpha;
	struct timeval initTime;
	struct timeval lastRedraw;
	float ctime;
	float frames;

	GLuint numTex[10];
	GLuint backTex;

	PreparePaintScreenProc preparePaintScreen;
	DonePaintScreenProc donePaintScreen;
	PaintScreenProc paintScreen;

} BenchScreen;

static void benchPreparePaintScreen(CompScreen * s, int msSinceLastPaint)
{
	BENCH_SCREEN(s);
	BENCH_DISPLAY(s->display);

	float nRrVal;
	float ratio = 0.05;
	int timediff;
	struct timeval now;

	gettimeofday(&now, 0);

	timediff = TIMEVALDIFF(&now, &bs->lastRedraw);

	nRrVal = MIN(1.1, (float)s->optimalRedrawTime / (float)timediff);

	bs->rrVal = (bs->rrVal * (1.0 - ratio)) + (nRrVal * ratio);

	bs->fps =
			(bs->fps * (1.0 - ratio)) +
			(1000000.0 / TIMEVALDIFFU(&now, &bs->lastRedraw) * ratio);

	bs->lastRedraw = now;

	if (benchGetOutputConsole(s->display) && bd->active)
	{
		bs->frames++;
		bs->ctime += timediff;
		if (bs->ctime >
			benchGetConsoleUpdateTime(s->display) * 1000)
		{
			printf("[BENCH] : %.0f frames in %.1f seconds = %.3f FPS\n",
				   bs->frames, bs->ctime / 1000.0,
				   bs->frames / (bs->ctime / 1000.0));
			bs->frames = 0;
			bs->ctime = 0;
		}
	}

	UNWRAP(bs, s, preparePaintScreen);
	(*s->preparePaintScreen) (s,
							  (bs->alpha >
							   0.0) ? timediff : msSinceLastPaint);
	WRAP(bs, s, preparePaintScreen, benchPreparePaintScreen);

	if (bd->active)
		bs->alpha += timediff / 1000.0;
	else
		bs->alpha -= timediff / 1000.0;

	bs->alpha = MIN(1.0, MAX(0.0, bs->alpha));
}

static void benchDonePaintScreen(CompScreen * s)
{
	BENCH_SCREEN(s);
	BENCH_DISPLAY(s->display);

	if (bs->alpha > 0.0)
	{
		damageScreen(s);
		glFlush();
		XSync(s->display->display, FALSE);
		if (benchGetDisableLimiter(s->display))
		{
			s->lastRedraw = bs->initTime;
			s->timeMult = 0;
		}
		if (!bd->active)
			s->timeMult = 0;
	}
	UNWRAP(bs, s, donePaintScreen);
	(*s->donePaintScreen) (s);
	WRAP(bs, s, donePaintScreen, benchDonePaintScreen);
}


static Bool
benchPaintScreen(CompScreen * s, const ScreenPaintAttrib * sa,
				 const CompTransform    *transform,
				 Region region, int output, unsigned int mask)
{

	Bool status, isSet;
	unsigned int fps;

	BENCH_SCREEN(s);

	UNWRAP(bs, s, paintScreen);
	status = (*s->paintScreen) (s, sa, transform, region, output, mask);
	WRAP(bs, s, paintScreen, benchPaintScreen);

	if (bs->alpha <= 0.0
		|| !benchGetOutputScreen(s->display))
		return status;
	glGetError();
	glPushAttrib(GL_COLOR_BUFFER_BIT | GL_TEXTURE_BIT);
	GLERR;

	CompTransform     sTransform = *transform;

	transformToScreenSpace (s, output, -DEFAULT_Z_CAMERA, &sTransform);

	glPushMatrix ();
	glLoadMatrixf (sTransform.m);


	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glColor4f(1.0, 1.0, 1.0, bs->alpha);

	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

	glTranslatef(benchGetPositionX(s->display),
				 benchGetPositionY(s->display), 0);

	glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, bs->backTex);

	glBegin(GL_QUADS);
	glTexCoord2f(0, 0);
	glVertex2f(0, 0);
	glTexCoord2f(0, 1);
	glVertex2f(0, 256);
	glTexCoord2f(1, 1);
	glVertex2f(512, 256);
	glTexCoord2f(1, 0);
	glVertex2f(512, 0);
	glEnd();

	glBindTexture(GL_TEXTURE_2D, 0);
	glDisable(GL_TEXTURE_2D);


	glTranslatef(53, 83, 0);
	float rrVal = MIN(1.0, MAX(0.0, bs->rrVal));

	if (rrVal < 0.5)
	{

		glBegin(GL_QUADS);
		glColor4f(1.0, 0.0, 0.0, bs->alpha);
		glVertex2f(0.0, 0.0);
		glVertex2f(0.0, 25.0);
		glColor4f(1.0, rrVal * 2.0, 0.0, bs->alpha);
		glVertex2f(330.0 * rrVal, 25.0);
		glVertex2f(330.0 * rrVal, 0.0);
		glEnd();
	}
	else
	{

		glBegin(GL_QUADS);
		glColor4f(1.0, 0.0, 0.0, bs->alpha);
		glVertex2f(0.0, 0.0);
		glVertex2f(0.0, 25.0);
		glColor4f(1.0, 1.0, 0.0, bs->alpha);
		glVertex2f(165.0, 25.0);
		glVertex2f(165.0, 0.0);
		glEnd();

		glBegin(GL_QUADS);
		glColor4f(1.0, 1.0, 0.0, bs->alpha);
		glVertex2f(165.0, 0.0);
		glVertex2f(165.0, 25.0);
		glColor4f(1.0 - ((rrVal - 0.5) * 2.0), 1.0, 0.0, bs->alpha);
		glVertex2f(165.0 + 330.0 * (rrVal - 0.5), 25.0);
		glVertex2f(165.0 + 330.0 * (rrVal - 0.5), 0.0);
		glEnd();

	}

	glColor4f(0.0, 0.0, 0.0, bs->alpha);

	glCallList(bs->dList);

	glTranslatef(72, 45, 0);

	float red;

	if (bs->fps > 30.0)
		red = 0.0;
	else
		red = 1.0;
	if (bs->fps <= 30.0 && bs->fps > 20.0)
		red = 1.0 - ((bs->fps - 20.0) / 10.0);

	glColor4f(red, 0.0, 0.0, bs->alpha);

	glEnable(GL_TEXTURE_2D);
	isSet = FALSE;
	fps = (bs->fps * 100.0);
	fps = MIN(999999, fps);

	if (fps >= 100000)
	{
		glBindTexture(GL_TEXTURE_2D, bs->numTex[fps / 100000]);
		glCallList(bs->dList + 1);
		isSet = TRUE;
	}
	fps %= 100000;
	glTranslatef(12, 0, 0);
	if (fps >= 10000 || isSet)
	{
		glBindTexture(GL_TEXTURE_2D, bs->numTex[fps / 10000]);
		glCallList(bs->dList + 1);
		isSet = TRUE;
	}
	fps %= 10000;
	glTranslatef(12, 0, 0);
	if (fps >= 1000 || isSet)
	{
		glBindTexture(GL_TEXTURE_2D, bs->numTex[fps / 1000]);
		glCallList(bs->dList + 1);
	}
	fps %= 1000;
	glTranslatef(12, 0, 0);

	glBindTexture(GL_TEXTURE_2D, bs->numTex[fps / 100]);
	glCallList(bs->dList + 1);
	fps %= 100;

	glTranslatef(19, 0, 0);

	glBindTexture(GL_TEXTURE_2D, bs->numTex[fps / 10]);
	glCallList(bs->dList + 1);
	fps %= 10;

	glTranslatef(12, 0, 0);

	glBindTexture(GL_TEXTURE_2D, bs->numTex[fps]);
	glCallList(bs->dList + 1);

	glBindTexture(GL_TEXTURE_2D, 0);
	glDisable(GL_TEXTURE_2D);

	glPopMatrix();

	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

	glColor4f(1.0, 1.0, 1.0, 1.0);

	glPopAttrib();
	glGetError();

	return status;
}

static Bool benchInitScreen(CompPlugin * p, CompScreen * s)
{
	int i;

	BENCH_DISPLAY(s->display);

	BenchScreen *bs = (BenchScreen *) calloc(1, sizeof(BenchScreen));

	s->privates[bd->screenPrivateIndex].ptr = bs;

	WRAP(bs, s, paintScreen, benchPaintScreen);
	WRAP(bs, s, preparePaintScreen, benchPreparePaintScreen);
	WRAP(bs, s, donePaintScreen, benchDonePaintScreen);

	glGenTextures(10, bs->numTex);
	glGenTextures(1, &bs->backTex);

	glGetError();

	glEnable(GL_TEXTURE_2D);

	bs->alpha = 0;
	bs->ctime = 0;
	bs->frames = 0;

	for (i = 0; i < 10; i++)
	{
		//Bind the texture
		glBindTexture(GL_TEXTURE_2D, bs->numTex[i]);

		//Load the parameters
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

		glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, 16, 32, 0,
					 GL_RGBA, GL_UNSIGNED_BYTE, number_data[i]);
		GLERR;
	}

	glBindTexture(GL_TEXTURE_2D, bs->backTex);

	//Load the parameters
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

	glTexImage2D(GL_TEXTURE_2D, 0, 4, 512, 256, 0, GL_RGBA,
				 GL_UNSIGNED_BYTE, image_data);
	GLERR;

	glBindTexture(GL_TEXTURE_2D, 0);
	glDisable(GL_TEXTURE_2D);

	bs->dList = glGenLists(2);
	glNewList(bs->dList, GL_COMPILE);

	glLineWidth(2.0);

	glBegin(GL_LINE_LOOP);
	glVertex2f(0, 0);
	glVertex2f(0, 25);
	glVertex2f(330, 25);
	glVertex2f(330, 0);
	glEnd();

	glLineWidth(1.0);

	glBegin(GL_LINES);
	for (i = 33; i < 330; i += 33)
	{
		glVertex2f(i, 15);
		glVertex2f(i, 25);
	}
	for (i = 16; i < 330; i += 33)
	{
		glVertex2f(i, 20);
		glVertex2f(i, 25);
	}
	glEnd();

	glEndList();

	glNewList(bs->dList + 1, GL_COMPILE);
	glBegin(GL_QUADS);
	glTexCoord2f(0, 0);
	glVertex2f(0, 0);
	glTexCoord2f(0, 1);
	glVertex2f(0, 32);
	glTexCoord2f(1, 1);
	glVertex2f(16, 32);
	glTexCoord2f(1, 0);
	glVertex2f(16, 0);
	glEnd();
	glEndList();


	gettimeofday(&bs->initTime, 0);
	gettimeofday(&bs->lastRedraw, 0);

	return TRUE;
}


static void benchFiniScreen(CompPlugin * p, CompScreen * s)
{

	BENCH_SCREEN(s);
	glDeleteLists(bs->dList, 2);

	glDeleteTextures(10, bs->numTex);
	glDeleteTextures(1, &bs->backTex);

	//Restore the original function
	UNWRAP(bs, s, paintScreen);
	UNWRAP(bs, s, preparePaintScreen);
	UNWRAP(bs, s, donePaintScreen);

	//Free the pointer
	free(bs);

}

static Bool
benchInitiate(CompDisplay * d, CompAction * ac, CompActionState state,
			  CompOption * option, int nOption)
{
	CompScreen *s;

	BENCH_DISPLAY(d);
	bd->active = !bd->active;
	bd->active &= benchGetOutputScreen(d) || benchGetOutputConsole(d);
	s = findScreenAtDisplay(d, getIntOptionNamed(option, nOption, "root", 0));
	if (s)
	{
		BENCH_SCREEN(s);
		damageScreen(s);
		bs->ctime = 0;
		bs->frames = 0;
	}
	return FALSE;
}

static Bool benchInitDisplay(CompPlugin * p, CompDisplay * d)
{
	//Generate a bench display
	BenchDisplay *bd = (BenchDisplay *) malloc(sizeof(BenchDisplay));

	//Allocate a private index
	bd->screenPrivateIndex = allocateScreenPrivateIndex(d);
	//Check if its valid
	if (bd->screenPrivateIndex < 0)
	{
		//Its invalid so free memory and return
		free(bd);
		return FALSE;
	}

	benchSetInitiateInitiate(d, benchInitiate);
	
	bd->active = FALSE;
	//Record the display
	d->privates[displayPrivateIndex].ptr = bd;
	return TRUE;
}

static void benchFiniDisplay(CompPlugin * p, CompDisplay * d)
{
	BENCH_DISPLAY(d);
	//Free the private index
	freeScreenPrivateIndex(d, bd->screenPrivateIndex);
	//Free the pointer
	free(bd);
}



static Bool benchInit(CompPlugin * p)
{
	displayPrivateIndex = allocateDisplayPrivateIndex();

	if (displayPrivateIndex < 0)
		return FALSE;

	return TRUE;
}

static void benchFini(CompPlugin * p)
{
	if (displayPrivateIndex >= 0)
		freeDisplayPrivateIndex(displayPrivateIndex);
}

static int benchGetVersion (CompPlugin *plugin,                int        version)
{    
    return ABIVERSION;
}


CompPluginVTable benchVTable = {
	"bench",
	benchGetVersion,
	0,
	benchInit,
	benchFini,
	benchInitDisplay,
	benchFiniDisplay,
	benchInitScreen,
	benchFiniScreen,
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

CompPluginVTable *getCompPluginInfo(void)
{
	return &benchVTable;
}
