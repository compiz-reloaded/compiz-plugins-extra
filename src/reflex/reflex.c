/*
 * Compiz reflection effect plugin
 *
 * mblur.c
 *
 * Copyright : (C) 2007 by Dennis Kasprzyk
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
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <X11/Xatom.h>
#include <X11/extensions/Xrender.h>

#include <compiz.h>

#include "reflex_options.h"


static int displayPrivateIndex = 0;

typedef struct _ReflexDisplay
{
	int screenPrivateIndex;
} ReflexDisplay;


typedef struct _ReflexScreen
{
	int windowPrivateIndex;

	DrawWindowTextureProc drawWindowTexture;

	Bool imageLoaded;
	CompTexture image;
	unsigned int width;
	unsigned int height;

	int function;
} ReflexScreen;

#define GET_REFLEX_DISPLAY(d)                                  \
    ((ReflexDisplay *) (d)->privates[displayPrivateIndex].ptr)

#define REFLEX_DISPLAY(d)                                      \
    ReflexDisplay *rd = GET_REFLEX_DISPLAY (d)

#define GET_REFLEX_SCREEN(s, rd)                               \
    ((ReflexScreen *) (s)->privates[(rd)->screenPrivateIndex].ptr)

#define REFLEX_SCREEN(s)                                       \
    ReflexScreen *rs = GET_REFLEX_SCREEN (s, GET_REFLEX_DISPLAY (s->display))

static int
getReflexFragmentFunction (CompScreen * s, CompTexture * texture,
						   int param, int unit)
{
	CompFunctionData *data;

	REFLEX_SCREEN (s);

	int target;
	char *targetString;

	if (texture->target == GL_TEXTURE_2D)
	{
		target = COMP_FETCH_TARGET_2D;
	}
	else
	{
		target = COMP_FETCH_TARGET_RECT;
	}

	if (rs->image.target == GL_TEXTURE_2D)
	{
		targetString = "2D";
	}
	else
	{
		targetString = "RECT";
	}


	if (rs->function)
		return rs->function;

	data = createFunctionData ();
	if (data)
	{
		Bool ok = TRUE;
		int handle = 0;
		char str[1024];

		ok &= addTempHeaderOpToFunctionData (data, "image");
		ok &= addTempHeaderOpToFunctionData (data, "coord");
		ok &= addTempHeaderOpToFunctionData (data, "mask");

		ok &= addFetchOpToFunctionData (data, "output", NULL, target);
		ok &= addColorOpToFunctionData (data, "output", "output");

		// coord <-- (pos + d) * t = pos * t + d * t
		snprintf (str, 1024,
				  "MAD coord, fragment.position, program.env[%d],"
				  " program.env[%d];", param, param + 1);
		ok &= addDataOpToFunctionData (data, str);

		snprintf (str, 1024,
				  "TEX image, coord, texture[%d], %s;", unit, targetString);
		ok &= addDataOpToFunctionData (data, str);

		snprintf (str, 1024,
				  "MUL_SAT mask, output.a, program.env[%d].r;"
				  "MAD image, -output.a, image, image;"
				  "MAD output, image, mask.a, output;", param + 2);
		ok &= addDataOpToFunctionData (data, str);

		if (!ok)
		{
			destroyFunctionData (data);
			return 0;
		}

		handle = createFragmentFunction (s, "reflex", data);

		rs->function = handle;

		destroyFunctionData (data);

		return handle;
	}

	return 0;
}

static void
reflexScreenOptionChanged (CompScreen * s,
						   CompOption * opt, ReflexScreenOptions num)
{
	REFLEX_SCREEN (s);
	switch (num)
	{
	case ReflexScreenOptionFile:
		if (rs->imageLoaded)
		{
			finiTexture (s, &rs->image);
			initTexture (s, &rs->image);
		}
		rs->imageLoaded =
			readImageToTexture (s, &rs->image, reflexGetFile (s), &rs->width,
								&rs->height);
		damageScreen (s);
		break;
	default:
		damageScreen (s);
		break;
	}
}

static void
reflexDrawWindowTexture (CompWindow * w,
						 CompTexture * texture,
						 const FragmentAttrib * attrib, unsigned int mask)
{
	CompScreen *s = w->screen;
	REFLEX_SCREEN (s);

	Bool enabled = (texture == w->texture) ?
		reflexGetWindow (s) : reflexGetDecoration (s);

	if (enabled && matchEval (reflexGetMatch (s), w) && rs->imageLoaded &&
		w->screen->fragmentProgram)
	{
		FragmentAttrib fa = *attrib;
		int function;
		int unit = 0;
		int param;
		float tx, ty, dx, mx;

		mx = w->attrib.x + (w->width / 2);
		mx /= s->width / 2.0;
		mx -= 1.0;
		mx *= -0.065;

		if (rs->image.target == GL_TEXTURE_2D)
		{
			tx = 1.0 / s->width;
			ty = 1.0 / s->height;
			dx = mx;
		}
		else
		{
			tx = 1.0 / s->width * rs->width;
			ty = 1.0 / s->height * rs->height;
			dx = mx * rs->width;
		}



		unit = allocFragmentTextureUnits (&fa, 1);
		param = allocFragmentParameters (&fa, 3);

		function =
			getReflexFragmentFunction (w->screen, texture, param, unit);
		if (function)
		{
			addFragmentFunction (&fa, function);
			(*s->activeTexture) (GL_TEXTURE0_ARB + unit);
			enableTexture (s, &rs->image, COMP_TEXTURE_FILTER_GOOD);
			(*s->activeTexture) (GL_TEXTURE0_ARB);
			(*s->programEnvParameter4f) (GL_FRAGMENT_PROGRAM_ARB, param,
										 tx, ty, 0.0f, 0.0f);
			(*s->programEnvParameter4f) (GL_FRAGMENT_PROGRAM_ARB, param + 1,
										 dx * tx, 0.0f, 0.0f, 0.0f);
			(*s->programEnvParameter4f) (GL_FRAGMENT_PROGRAM_ARB, param + 2,
										 reflexGetThreshold (s), 0.0f, 0.0f,
										 0.0f);
		}
		UNWRAP (rs, w->screen, drawWindowTexture);
		(*w->screen->drawWindowTexture) (w, texture, &fa, mask);
		WRAP (rs, w->screen, drawWindowTexture, reflexDrawWindowTexture);


		if (unit)
		{
			(*s->activeTexture) (GL_TEXTURE0_ARB + unit);
			disableTexture (s, &rs->image);
			(*s->activeTexture) (GL_TEXTURE0_ARB);
		}
	}
	else
	{
		UNWRAP (rs, w->screen, drawWindowTexture);
		(*w->screen->drawWindowTexture) (w, texture, attrib, mask);
		WRAP (rs, w->screen, drawWindowTexture, reflexDrawWindowTexture);
	}
}



static Bool
reflexInitDisplay (CompPlugin * p, CompDisplay * d)
{
	ReflexDisplay *rd;

	rd = malloc (sizeof (ReflexDisplay));
	if (!rd)
		return FALSE;

	rd->screenPrivateIndex = allocateScreenPrivateIndex (d);
	if (rd->screenPrivateIndex < 0)
	{
		free (rd);
		return FALSE;
	}

	d->privates[displayPrivateIndex].ptr = rd;

	return TRUE;
}

static void
reflexFiniDisplay (CompPlugin * p, CompDisplay * d)
{
	REFLEX_DISPLAY (d);
	freeScreenPrivateIndex (d, rd->screenPrivateIndex);

	free (rd);
}


static Bool
reflexInitScreen (CompPlugin * p, CompScreen * s)
{
	ReflexScreen *rs;

	REFLEX_DISPLAY (s->display);

	rs = malloc (sizeof (ReflexScreen));
	if (!rs)
		return FALSE;

	rs->windowPrivateIndex = allocateWindowPrivateIndex (s);
	if (rs->windowPrivateIndex < 0)
	{
		free (rs);
		return FALSE;
	}

	initTexture (s, &rs->image);

	rs->imageLoaded = readImageToTexture (s, &rs->image, reflexGetFile (s),
										  &rs->width, &rs->height);
	reflexSetFileNotify (s, reflexScreenOptionChanged);

	s->privates[rd->screenPrivateIndex].ptr = rs;

	rs->function = 0;

	WRAP (rs, s, drawWindowTexture, reflexDrawWindowTexture);

	return TRUE;
}


static void
reflexFiniScreen (CompPlugin * p, CompScreen * s)
{
	REFLEX_SCREEN (s);

	freeWindowPrivateIndex (s, rs->windowPrivateIndex);

	UNWRAP (rs, s, drawWindowTexture);

	if (rs->function)
		destroyFragmentFunction (s, rs->function);

	free (rs);
}

static Bool
reflexInit (CompPlugin * p)
{
	displayPrivateIndex = allocateDisplayPrivateIndex ();
	if (displayPrivateIndex < 0)
		return FALSE;

	return TRUE;
}

static void
reflexFini (CompPlugin * p)
{
	if (displayPrivateIndex >= 0)
		freeDisplayPrivateIndex (displayPrivateIndex);
}

static int
reflexGetVersion (CompPlugin * plugin, int version)
{
	return ABIVERSION;
}

CompPluginVTable reflexVTable = {
	"reflex",
	reflexGetVersion,
	0,
	reflexInit,
	reflexFini,
	reflexInitDisplay,
	reflexFiniDisplay,
	reflexInitScreen,
	reflexFiniScreen,
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
	return &reflexVTable;
}
