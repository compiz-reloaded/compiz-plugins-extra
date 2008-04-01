/*
 *
 * Compiz bicubic filter plugin
 *
 * bicubic.c
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

#include <compiz-core.h>

static CompMetadata bicubicMetadata;

static int BicubicDisplayPrivateIndex;

typedef struct _BicubicFunction {
    struct _BicubicFunction *next;

    int handle;
    int target;
    int param;
    int unit;
} BicubicFunction;

typedef struct _BicubicDisplay
{
    int screenPrivateIndex;
} BicubicDisplay;


typedef struct _BicubicScreen
{
    DrawWindowTextureProc drawWindowTexture;

    BicubicFunction *func;

    GLenum lTexture;

} BicubicScreen;

#define GET_PLUGIN_CORE(object, plugin) \
    ((plugin##Core *) (object)->base.privates[plugin##CorePrivateIndex].ptr)
#define PLUGIN_CORE(object, plugin, prefix) \
    plugin##Core * prefix##c = GET_PLUGIN_CORE (object, plugin)

#define GET_PLUGIN_DISPLAY(object, plugin) \
    ((plugin##Display *) \
	(object)->base.privates[plugin##DisplayPrivateIndex].ptr)
#define PLUGIN_DISPLAY(object, plugin, prefix) \
    plugin##Display * prefix##d = GET_PLUGIN_DISPLAY (object, plugin)

#define GET_PLUGIN_SCREEN(object, parent, plugin) \
    ((plugin##Screen *) \
	(object)->base.privates[(parent)->screenPrivateIndex].ptr)
#define PLUGIN_SCREEN(object, plugin, prefix) \
    plugin##Screen * prefix##s = \
	GET_PLUGIN_SCREEN (object, \
	GET_PLUGIN_DISPLAY ((object)->display, plugin), plugin)

#define GET_PLUGIN_WINDOW(object, parent, plugin) \
    ((plugin##Window *) \
	(object)->base.privates[(parent)->windowPrivateIndex].ptr)
#define PLUGIN_WINDOW(object, plugin, prefix) \
    plugin##Window * prefix##w = \
	GET_PLUGIN_WINDOW  (object, \
	GET_PLUGIN_SCREEN  ((object)->screen, \
	GET_PLUGIN_DISPLAY ((object)->screen->display, plugin), plugin), plugin)

#define BICUBIC_DISPLAY(d) PLUGIN_DISPLAY(d, Bicubic, b)
#define BICUBIC_SCREEN(s) PLUGIN_SCREEN(s, Bicubic, b)

static int
getBicubicFragmentFunction (CompScreen  *s,
			    CompTexture *texture,
			    int         param,
			    int         unit)
{
    BicubicFunction  *function;
    CompFunctionData *data;
    int		     target;
    char	     *targetString;

    BICUBIC_SCREEN (s);

    if (texture->target == GL_TEXTURE_2D)
    {
	target	     = COMP_FETCH_TARGET_2D;
	targetString = "2D";
    }
    else
    {
	target	     = COMP_FETCH_TARGET_RECT;
	targetString = "RECT";
    }

    for (function = bs->func; function; function = function->next)
	if (function->param  == param  &&
	    function->target == target &&
	    function->unit   == unit)
	    return function->handle;

    data = createFunctionData ();
    if (data)
    {
	Bool ok = TRUE;
	int  handle = 0, i;
	
	static char *filterTemp[] = {
	    "hgX", "hgY", "cs00", "cs01", "cs10", "cs11"
	};

	for (i = 0; i < sizeof (filterTemp) / sizeof (filterTemp[0]); i++)
	    ok &= addTempHeaderOpToFunctionData (data, filterTemp[i]);


	ok &= addDataOpToFunctionData (data,
		"MAD cs00, fragment.texcoord[0], program.env[%d],"
		"{-0.5, -0.5, 0.0, 0.0};", param + 2);
	
	ok &= addDataOpToFunctionData (data, 
		"TEX hgX, cs00.x, texture[%d], 1D;", unit);
	ok &= addDataOpToFunctionData (data, 
		"TEX hgY, cs00.y, texture[%d], 1D;", unit);
	
	ok &= addDataOpToFunctionData (data,
		"MUL cs10, program.env[%d], hgX.y;", param);
	ok &= addDataOpToFunctionData (data,
		"MUL cs00, program.env[%d], -hgX.x;", param);
	ok &= addDataOpToFunctionData (data,
		"MAD cs11, program.env[%d], hgY.y, cs10;", param + 1);
	ok &= addDataOpToFunctionData (data,
		"MAD cs01, program.env[%d], hgY.y, cs00;", param + 1);
	ok &= addDataOpToFunctionData (data,
		"MAD cs10, program.env[%d], -hgY.x, cs10;", param + 1);
	ok &= addDataOpToFunctionData (data,
		"MAD cs00, program.env[%d], -hgY.x, cs00;", param + 1);

	ok &= addFetchOpToFunctionData (data, "cs00", "cs00", target);
	ok &= addFetchOpToFunctionData (data, "cs01", "cs01", target);
	ok &= addFetchOpToFunctionData (data, "cs10", "cs10", target);
	ok &= addFetchOpToFunctionData (data, "cs11", "cs11", target);

	ok &= addDataOpToFunctionData (data, "LRP cs00, hgY.z, cs00, cs01;");
	ok &= addDataOpToFunctionData (data, "LRP cs10, hgY.z, cs10, cs11;");
 
	ok &= addDataOpToFunctionData (data, "LRP output, hgX.z, cs00, cs10;");
	
	ok &= addColorOpToFunctionData (data, "output", "output");
	if (!ok)
	{
	    destroyFunctionData (data);
	    return 0;
	}
	
	function = malloc (sizeof (BicubicFunction));
	if (function)
	{
	    handle = createFragmentFunction (s, "bicubic", data);

	    function->handle = handle;
	    function->target = target;
	    function->param  = param;
	    function->unit   = unit;

	    function->next = bs->func;
	    bs->func = function;
	}

	destroyFunctionData (data);

	return handle;
    }

    return 0;
}

static void
BicubicDrawWindowTexture (CompWindow           *w,
			  CompTexture          *texture,
			  const FragmentAttrib *attrib,
			  unsigned int         mask)
{
    CompScreen *s = w->screen;

    BICUBIC_SCREEN (s);

    if ((mask & (PAINT_WINDOW_TRANSFORMED_MASK |
	         PAINT_WINDOW_ON_TRANSFORMED_SCREEN_MASK)) &&
        s->filter[SCREEN_TRANS_FILTER] == COMP_TEXTURE_FILTER_GOOD)
    {
	FragmentAttrib fa = *attrib;
	int            function, param;
	int            unit = 0;
	
	param = allocFragmentParameters (&fa, 3);
	unit  = allocFragmentTextureUnits (&fa, 1);

	function = getBicubicFragmentFunction (s, texture, param, unit);
	
	if (function)
	{
	    addFragmentFunction (&fa, function);

	    (*s->activeTexture) (GL_TEXTURE0_ARB + unit);
	    glBindTexture (GL_TEXTURE_1D, bs->lTexture);
	    (*s->activeTexture) (GL_TEXTURE0_ARB);


	    (*s->programEnvParameter4f) (GL_FRAGMENT_PROGRAM_ARB, param,
					 texture->matrix.xx, 0.0f,
					 0.0f, 0.0f);
	    (*s->programEnvParameter4f) (GL_FRAGMENT_PROGRAM_ARB,
					 param + 1,
					 0.0f, -texture->matrix.yy,
					 0.0f, 0.0f);
	    (*s->programEnvParameter4f) (GL_FRAGMENT_PROGRAM_ARB,
					 param + 2,
					 1.0 / texture->matrix.xx, 
					 1.0 / -texture->matrix.yy,
					 0.0f, 0.0f);
	}

	UNWRAP (bs, s, drawWindowTexture);
	(*s->drawWindowTexture) (w, texture, &fa, mask);
	WRAP (bs, s, drawWindowTexture, BicubicDrawWindowTexture);
	
	if (unit)
	{
	    (*s->activeTexture) (GL_TEXTURE0_ARB + unit);
	    glBindTexture (GL_TEXTURE_1D, 0);
	    (*s->activeTexture) (GL_TEXTURE0_ARB);
	}
    }
    else
    {
	UNWRAP (bs, s, drawWindowTexture);
	(*s->drawWindowTexture) (w, texture, attrib, mask);
	WRAP (bs, s, drawWindowTexture, BicubicDrawWindowTexture);
    }
}

static void
generateLookupTexture (CompScreen *s, GLenum format)
{
    GLfloat values[512];
    int     i;
    float   a, a2, a3, w0, w1, w2, w3;

    BICUBIC_SCREEN (s);

    for (i = 0; i < 512; i += 4)
    {
	a  = (float)i / 512.0;
	a2 = a * a;
	a3 = a2 * a;
	
	w0 = (1.0 / 6.0) * ((-a3) + (3.0 * a2) + (-3.0 * a) + 1.0);
	w1 = (1.0 / 6.0) * ((3.0 * a3) + (-6.0 * a2) + 4.0);
	w2 = (1.0 / 6.0) * ((-3.0 * a3) + (3.0 * a2) + (3.0 * a) + 1.0);
	w3 = (1.0 / 6.0) * a3;
	
	values[i]     = 1.0 - (w1 / (w0 + w1)) + a;
	values[i + 1] = 1.0 + (w3 / (w2 + w3)) - a;
	values[i + 2] = w0 + w1;
	values[i + 3] = w2 + w3;
    }

    glGenTextures (1, &bs->lTexture);

    glBindTexture (GL_TEXTURE_1D, bs->lTexture);

    glTexImage1D (GL_TEXTURE_1D, 0, format, 128, 0, GL_RGBA,
		  GL_FLOAT, values);

    glTexParameteri (GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri (GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glTexParameteri (GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri (GL_TEXTURE_1D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    glBindTexture (GL_TEXTURE_1D, 0);
}

static Bool
BicubicInitDisplay (CompPlugin  *p,
		    CompDisplay *d)
{
    BicubicDisplay *bd;

    if (!checkPluginABI ("core", CORE_ABIVERSION))
        return FALSE;

    bd = malloc (sizeof (BicubicDisplay));
    if (!bd)
	return FALSE;

    bd->screenPrivateIndex = allocateScreenPrivateIndex(d);
    if (bd->screenPrivateIndex < 0)
    {
	free (bd);
	return FALSE;
    }

    d->base.privates[BicubicDisplayPrivateIndex].ptr = bd;

    return TRUE;
}

static void
BicubicFiniDisplay (CompPlugin  *p,
		CompDisplay *d)
{
    BICUBIC_DISPLAY (d);

    freeScreenPrivateIndex (d, bd->screenPrivateIndex);

    free (bd);
}

static Bool
BicubicInitScreen (CompPlugin *p,
		   CompScreen *s)
{
    BicubicScreen  *bs;
    const char     *glExtensions;
    GLenum         format = GL_RGBA16F_ARB;

    BICUBIC_DISPLAY (s->display);

    if (!s->fragmentProgram)
    {
	compLogMessage (s->display, "bicube", CompLogLevelFatal,
			"GL_ARB_fragment_program not supported.");
	return FALSE;
    }

    glExtensions = (const char *) glGetString (GL_EXTENSIONS);
    if (!glExtensions)
    {
	compLogMessage (s->display, "bicubic", CompLogLevelFatal,
			"No valid GL extensions string found.");
	return FALSE;
    }

    if (!strstr (glExtensions, "GL_ARB_texture_float"))
    {
	compLogMessage (s->display, "bicubic", CompLogLevelFatal,
			"GL_ARB_texture_float not supported. "
		        "This can lead to visual artifacts.");
	format = GL_RGBA;
    }

    bs = malloc (sizeof (BicubicScreen));
    if (!bs)
	return FALSE;

    /* wrap overloaded functions */
    WRAP (bs, s, drawWindowTexture, BicubicDrawWindowTexture);

    s->base.privates[bd->screenPrivateIndex].ptr = bs;

    generateLookupTexture (s, format);

    bs->func = NULL;

    return TRUE;
}

static void
BicubicFiniScreen (CompPlugin *p,
		   CompScreen *s)
{
    BicubicFunction *f, *f2;

    BICUBIC_SCREEN (s);

    UNWRAP (bs, s, drawWindowTexture);

    for (f = bs->func; f;)
    {
	destroyFragmentFunction (s, f->handle);
	f2 = f;
	f = f->next;
	free (f2);
    }

    glDeleteTextures (1, &bs->lTexture);

    free (bs);
}

static Bool
BicubicInit (CompPlugin * p)
{

    if (!compInitPluginMetadataFromInfo (&bicubicMetadata, p->vTable->name,
					 0, 0, 0, 0))
	return FALSE;
    
    BicubicDisplayPrivateIndex = allocateDisplayPrivateIndex ();
    if (BicubicDisplayPrivateIndex < 0)
    {
	compFiniMetadata (&bicubicMetadata);
	return FALSE;
    }

    compAddMetadataFromFile (&bicubicMetadata, p->vTable->name);

    return TRUE;
}

static void
BicubicFini (CompPlugin * p)
{
    freeDisplayPrivateIndex (BicubicDisplayPrivateIndex);
    compFiniMetadata (&bicubicMetadata);
}

static CompBool
BicubicInitObject (CompPlugin *p,
		   CompObject *o)
{
    static InitPluginObjectProc dispTab[] = {
	(InitPluginObjectProc) 0,
	(InitPluginObjectProc) BicubicInitDisplay,
	(InitPluginObjectProc) BicubicInitScreen
    };

    RETURN_DISPATCH (o, dispTab, ARRAY_SIZE (dispTab), TRUE, (p, o));
}

static void
BicubicFiniObject (CompPlugin *p,
		   CompObject *o)
{
    static FiniPluginObjectProc dispTab[] = {
	(FiniPluginObjectProc) 0,
	(FiniPluginObjectProc) BicubicFiniDisplay,
	(FiniPluginObjectProc) BicubicFiniScreen
    };

    DISPATCH (o, dispTab, ARRAY_SIZE (dispTab), (p, o));
}

CompPluginVTable BicubicVTable = {
    "bicubic",
    0,
    BicubicInit,
    BicubicFini,
    BicubicInitObject,
    BicubicFiniObject,
    0,
    0,
};

CompPluginVTable*
getCompPluginInfo20070830 (void)
{
    return &BicubicVTable;
}


