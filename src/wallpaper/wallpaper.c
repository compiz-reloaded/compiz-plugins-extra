/*
 * Compiz wallpaper plugin
 *
 * wallpaper.c
 *
 * Copyright (c) 2017 Scott Moreau <oreaus@gmail.com>
 *
 * Copyright (c) 2008 Dennis Kasprzyk <onestone@opencompositing.org>
 *
 * Rewrite of wallpaper.c
 * Copyright (c) 2007 Robert Carr <racarr@opencompositing.org>
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

#include <stdarg.h>
#include <string.h>
#include <math.h>
#include <dirent.h>
#include <sys/stat.h>

#include <X11/Xatom.h>
#include <X11/extensions/Xrender.h>
#include <X11/extensions/shape.h>

#include <compiz-core.h>

#include "wallpaper_options.h"

static int WallpaperDisplayPrivateIndex;

typedef struct _WallpaperBackground
{
	char           *image;
	int            imagePos;
	int            fillType;
	unsigned short color1[4];
	unsigned short color2[4];

	CompTexture    imgTex;
	unsigned int            width;
	unsigned int            height;
	void           *data;
	Bool           changed;
	Bool           loaded;

	CompTexture    fillTex;
} WallpaperBackground;

typedef struct _WallpaperDisplay
{
	HandleEventProc handleEvent;

	int screenPrivateIndex;

	/* _COMPIZ_WALLPAPER_SUPPORTED atom is used to indicate that
	 * the wallpaper plugin or a plugin providing similar functionality is
	 * active so that desktop managers can respond appropriately */
	Atom compizWallpaperAtom;
} WallpaperDisplay;

typedef struct _WallpaperScreen
{
	PaintOutputProc      paintOutput;
	DrawWindowProc       drawWindow;
	PaintWindowProc      paintWindow;
	DamageWindowRectProc damageWindowRect;
	PreparePaintScreenProc preparePaintScreen;

	WallpaperBackground  *backgrounds;
	unsigned int         nBackgrounds;
	int                  bgOffset;
	Bool                 fading;
	float                fade_progress;
	int                  fade_time;
	int                  fade_remaining;
	CompTimeoutHandle    cycle_timeout;

	Bool                 propSet;
	Window               fakeDesktop;

	CompWindow           *desktop;
} WallpaperScreen;

#define WALLPAPER_DISPLAY(d) PLUGIN_DISPLAY(d, Wallpaper, w)
#define WALLPAPER_SCREEN(s) PLUGIN_SCREEN(s, Wallpaper, w)

#define NUM_LIST_OPTIONS 5

static Bool
initBackground (void *object,
		void *closure)
{
	CompScreen          *s = (CompScreen *) closure;
	WallpaperBackground *back = (WallpaperBackground *) object;
	unsigned int        c[2];
	unsigned short      *color;

	initTexture (s, &back->imgTex);
	initTexture (s, &back->fillTex);

	color = back->color1;
	c[0] = ((color[3] << 16) & 0xff000000) |
		((color[0] * color[3] >> 8) & 0xff0000) |
		((color[1] * color[3] >> 16) & 0xff00) |
		((color[2] * color[3] >> 24) & 0xff);

	color = back->color2;
	c[1] = ((color[3] << 16) & 0xff000000) |
		((color[0] * color[3] >> 8) & 0xff0000) |
		((color[1] * color[3] >> 16) & 0xff00) |
		((color[2] * color[3] >> 24) & 0xff);

	if (back->fillType == BgFillTypeVerticalGradient)
	{
		imageBufferToTexture (s, &back->fillTex, (char *) &c, 1, 2);
		back->fillTex.matrix.xx = 0.0;
	}
	else if (back->fillType == BgFillTypeHorizontalGradient)
	{
		imageBufferToTexture (s, &back->fillTex, (char *) &c, 2, 1);
		back->fillTex.matrix.yy = 0.0;
	}
	else
	{
		imageBufferToTexture (s, &back->fillTex, (char *) &c, 1, 1);
		back->fillTex.matrix.xx = 0.0;
		back->fillTex.matrix.yy = 0.0;
	}

	if (back->image && strlen (back->image))
	{
		if (!readImageToTexture (s, &back->imgTex, back->image,
									&back->width, &back->height))
		{
			compLogMessage ("wallpaper", CompLogLevelWarn,
					"Failed to load image: %s", back->image);

			back->width  = 0;
			back->height = 0;

			finiTexture (s, &back->imgTex);
			initTexture (s, &back->imgTex);

			return FALSE;
		}
		else
		{
			back->loaded = TRUE;
			return TRUE;
		}
	}

	return FALSE;
}

static void
finiBackground (void *object,
				void *closure)
{
	CompScreen          *s = (CompScreen *) closure;
	WallpaperBackground *back = (WallpaperBackground *) object;
	
	finiTexture (s, &back->imgTex);
	finiTexture (s, &back->fillTex);

	back->loaded = FALSE;
}

static void
wallpaperAddToList (char *fullpath, CompListValue **options,
				CompListValue ***new_options, int j, int n, int nElements)
{
	int i;

	CompListValue **opts = *new_options;

	for (i = 0; i < NUM_LIST_OPTIONS; i++)
	{
		switch (options[i]->type)
		{
			case CompOptionTypeString:
				opts[i]->value = realloc(opts[i]->value, sizeof (CompOption) * n);
				opts[i]->type = CompOptionTypeString;
				opts[i]->value[n-1].s = strdup(fullpath);
				opts[i]->nValue = n;
				break;
			case CompOptionTypeInt:
				opts[i]->value = realloc(opts[i]->value, sizeof (CompOption) * n);
				opts[i]->type = CompOptionTypeInt;
				opts[i]->value[n-1].i = options[i]->value[j].i;
				opts[i]->nValue = n;
				break;
			case CompOptionTypeColor:
				opts[i]->value = realloc(opts[i]->value, sizeof (CompOption) * n);
				opts[i]->type = CompOptionTypeColor;
				memcpy(&(opts[i]->value[n-1].c), &options[i]->value[j].c,
											sizeof (unsigned short) * 4);
				opts[i]->nValue = n;
				break;
			default:
				break;
		}
	}
}

static void
wallpaperSearchDirectory (CompScreen *s, char *path,
						CompListValue **options, CompListValue ***new_options,
						int j, int *n, int nElements, Bool recursive)
{
    DIR *dir;
    struct dirent *file;
	struct stat st, next;
	char fullpath[256];

	if (stat (path, &st) != 0)
		return;

	if (!S_ISDIR(st.st_mode))
		return;

	if ((dir = opendir(path)) == NULL)
		return;

	while ((file = readdir(dir)) != 0)
	{
		if (!strcmp(file->d_name, ".") || !strcmp(file->d_name, ".."))
			continue;
		strcpy (fullpath, path);
		strcat (fullpath, "/");
		strcat (fullpath, file->d_name);

		if (stat (fullpath, &next) == 0)
		{
			if (recursive && S_ISDIR(next.st_mode))
				wallpaperSearchDirectory(s, fullpath, options, new_options, j, n, nElements, TRUE);

			if (!S_ISDIR(next.st_mode))
				wallpaperAddToList(fullpath, options, new_options, j, ++(*n), nElements);
		}
	}
}

static void
wallpaperAddFilesFromDirectories (CompScreen *s, CompListValue ***new_options,
					CompListValue **options, unsigned int *nElements)
{
	struct stat st;
	int i, j, n;

	*new_options = malloc(sizeof (CompListValue *) * 5);
	if (!*new_options)
		return;

	for (i = 0; i < 5; i++)
		(*new_options)[i] = calloc(0, sizeof (CompListValue));

	for (i = 0, n = 0; i < *nElements; i++)
	{
		if (options[i]->type == CompOptionTypeString)
		{
			for (j = 0; j < options[i]->nValue; j++)
			{
				if (stat (options[i]->value[j].s, &st) == 0)
				{
					if (S_ISDIR(st.st_mode))
					{
						wallpaperSearchDirectory(s, options[i]->value[j].s,
												options, new_options,
												j, &n, *nElements,
												wallpaperGetRecursive(s->display));
					}
					else
					{
						wallpaperAddToList(options[i]->value[j].s,
											options, new_options, j,
											++n, *nElements);
					}
				}
			}
		}
	}

	*nElements = n;
}

static void
wallpaperFreeOptions (CompListValue  **options, int n)
{
	int i, j;

	for (i = 0; i < 5; i++)
	{
		switch(options[i]->type)
		{
			case CompOptionTypeString:
				for (j = 0; j < n; j++)
					free (options[i]->value[j].s);
				free (options[i]->value);
				free (options[i]);
				break;
			case CompOptionTypeInt:
			case CompOptionTypeColor:
				free (options[i]->value);
				free (options[i]);
				break;
			default:
				break;
		}
	}
}

static WallpaperBackground *
processMultiList (WallpaperBackground *currData,
                  unsigned int        *numReturn,
                  void                *closure,
                  unsigned int        numOptions,
                  ...)
{
	CompOption           *option;
	CompListValue        **options, **new_options;
	unsigned int         *offsets;
	unsigned int         i, j, nElements = 0;
	unsigned int         oldSize;
	WallpaperBackground  *rv;
	char                 *value, *newVal, *setVal;
	va_list              ap;
	Bool                 changed;

	CompOptionValue zeroVal, *optVal;

	char **stringValue, **stringValue2;

	if (!numReturn)
		return NULL;

	oldSize = *numReturn;

	options = malloc (sizeof (CompOption *) * numOptions);

	if (!options)
		return currData;

	offsets = malloc (sizeof (unsigned int) * numOptions);
	if (!offsets)
	{
		free (options);
		return currData;
	}

	newVal = malloc (sizeof (WallpaperBackground));
	if (!newVal)
	{
		free (options);
		free (offsets);
		return currData;
	}

	va_start (ap, numOptions);

	for (i = 0; i < numOptions; i++)
	{
		option = va_arg (ap, CompOption *);
		offsets[i] = va_arg (ap, unsigned int);

		if (option->type != CompOptionTypeList)
		{
			free (options);
			free (offsets);
			free (newVal);
			va_end (ap);
			return currData;
		}

		options[i] = &option->value.list;
		nElements = MAX (nElements, options[i]->nValue);
	}
	va_end (ap);

	if (options[0]->nValue != options[1]->nValue ||
		options[1]->nValue != options[2]->nValue ||
		options[2]->nValue != options[3]->nValue ||
		options[3]->nValue != options[4]->nValue ||
		options[4]->nValue != nElements)
	{
		free (options);
		free (offsets);
		free (newVal);
		return currData;
	}

	wallpaperAddFilesFromDirectories(closure, &new_options, options, &nElements);

	free (options);

	for (j = nElements; j < oldSize; j++)
	{
		finiBackground (&currData[j], closure);
		for (i = 0; i < numOptions; i++)
		{
			value = ((char *) (&currData[j])) + offsets[i];
			switch (new_options[i]->type)
			{
				case CompOptionTypeString:
					stringValue = (char **) value;
					free (*stringValue);
					break;
				default:
					break;
			}
		}
	}

	if (!nElements)
	{
		wallpaperFreeOptions(new_options, nElements);
		free (offsets);
		free (newVal);
		free (currData);
		*numReturn = 0;
		return NULL;
	}

	if (oldSize)
		rv = realloc (currData, nElements * sizeof (WallpaperBackground));
	else
		rv = malloc (nElements * sizeof (WallpaperBackground));

	if (!rv)
	{
		wallpaperFreeOptions(new_options, nElements);
		free (offsets);
		free (newVal);
		return currData;
	}

	if (nElements > oldSize)
	memset (&rv[oldSize], 0,
		(nElements - oldSize) * sizeof (WallpaperBackground));

	memset (&zeroVal, 0, sizeof (CompOptionValue));

	for (j = 0; j < nElements; j++)
	{
		changed = (j >= oldSize);
		memset (newVal, 0, sizeof (WallpaperBackground));
		for (i = 0; i < numOptions; i++)
		{
			value = ((char *) (&rv[j])) + offsets[i];
			setVal = newVal + offsets[i];

			if (j < new_options[i]->nValue)
			optVal = &new_options[i]->value[j];
			else
			optVal = &zeroVal;

			if (j < new_options[i]->nValue)
			{
				switch (new_options[i]->type)
				{
					case CompOptionTypeInt:
						memcpy (setVal, &optVal->i, sizeof (int));
						changed |= memcmp (value, setVal, sizeof (int));
						break;
					case CompOptionTypeString:
						stringValue = (char **) setVal;
						if (optVal->s)
							*stringValue = strdup (optVal->s);
						else
							*stringValue = strdup ("");
						stringValue2 = (char **) value;
						if (!*stringValue2 || strcmp (*stringValue, *stringValue2))
							changed = TRUE;
						break;
					case CompOptionTypeColor:
						memcpy (setVal, optVal->c, sizeof (unsigned short) * 4);
						changed |= memcmp (value, setVal,
								   sizeof (unsigned short) * 4);
						break;
					default:
						break;
				}
			}
		}

		if (changed)
		{
			setVal = (char *) &rv[j];
			finiBackground (setVal, closure);
		}
		else
			setVal = newVal;

		for (i = 0; i < numOptions; i++)
		{
			value = setVal + offsets[i];
			switch (new_options[i]->type)
			{
				case CompOptionTypeString:
					stringValue = (char **) value;
					free (*stringValue);
					break;
				default:
					break;
			}
		}

		if (changed)
		{
			((WallpaperBackground *) newVal)->changed = TRUE;
			memcpy (&rv[j], newVal, sizeof (WallpaperBackground));
		}
    }

	wallpaperFreeOptions (new_options, nElements);
	free (offsets);
	free (newVal);
	*numReturn = nElements;

	if (nElements > 0)
		compLogMessage ("wallpaper", CompLogLevelInfo,
				"Found %d files", nElements);

	return rv;
}

static Visual *
findArgbVisual (Display *dpy,
		int     screen)
{
	XVisualInfo         *xvi;
	XVisualInfo         template;
	int                 nvi;
	int                 i;
	XRenderPictFormat   *format;
	Visual              *visual;

	template.screen = screen;
	template.depth  = 32;
	template.class  = TrueColor;

	xvi = XGetVisualInfo (dpy,
			VisualScreenMask |
			VisualDepthMask  |
			VisualClassMask,
			&template,
			&nvi);
	if (!xvi)
		return 0;

	visual = 0;
	for (i = 0; i < nvi; i++)
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

static void
createFakeDesktopWindow (CompScreen *s)
{
	Display              *dpy = s->display->display;
	XSizeHints           xsh;
	XClassHint           xch;
	XWMHints             xwmh;
	XSetWindowAttributes attr;
	Visual               *visual;
	XserverRegion        region;

	WALLPAPER_SCREEN (s);

	visual = findArgbVisual (dpy, s->screenNum);
	if (!visual)
		return;

	xsh.flags       = PSize | PPosition | PWinGravity;
	xsh.width       = 1;
	xsh.height      = 1;
	xsh.win_gravity = StaticGravity;

	xwmh.flags = InputHint;
	xwmh.input = 0;

	attr.background_pixel = 0;
	attr.border_pixel     = 0;
	attr.colormap	      = XCreateColormap (dpy, s->root, visual, AllocNone);

	ws->fakeDesktop = XCreateWindow (dpy, s->root, -1, -1, 1, 1, 0, 32,
						InputOutput, visual,
						CWBackPixel | CWBorderPixel | CWColormap,
						&attr);

	XSetWMProperties (dpy, ws->fakeDesktop, NULL, NULL,
				programArgv, programArgc, &xsh, &xwmh, NULL);

	XChangeProperty (dpy, ws->fakeDesktop, s->display->winStateAtom,
			XA_ATOM, 32, PropModeReplace,
			(unsigned char *) &s->display->winStateSkipPagerAtom, 1);

	XChangeProperty (dpy, ws->fakeDesktop, s->display->winTypeAtom,
			XA_ATOM, 32, PropModeReplace,
			(unsigned char *) &s->display->winTypeDesktopAtom, 1);

	xch.res_name = xch.res_class = strdup("Compiz-Wallpaper");

	XSetClassHint (dpy, ws->fakeDesktop, &xch);

	free (xch.res_name);

	region = XFixesCreateRegion (dpy, NULL, 0);

	XFixesSetWindowShapeRegion (dpy, ws->fakeDesktop, ShapeInput, 0, 0, region);

	XFixesDestroyRegion (dpy, region);

	XMapWindow (dpy, ws->fakeDesktop);
	XLowerWindow (dpy, ws->fakeDesktop);
}

static void
destroyFakeDesktopWindow (CompScreen *s)
{
	WALLPAPER_SCREEN (s);

	if (ws->fakeDesktop != None)
		XDestroyWindow (s->display->display, ws->fakeDesktop);

	ws->fakeDesktop = None;
}

static void
updateProperty(CompScreen *s)
{
	WALLPAPER_SCREEN (s);

	if (!ws->nBackgrounds)
	{
		WALLPAPER_DISPLAY (s->display);

		if (ws->propSet)
			XDeleteProperty (s->display->display,
								s->root, wd->compizWallpaperAtom);
		ws->propSet = FALSE;
    }
    else if (!ws->propSet)
    {
		WALLPAPER_DISPLAY (s->display);
		unsigned char sd = 1;

		XChangeProperty (s->display->display, s->root,
							wd->compizWallpaperAtom, XA_CARDINAL,
							8, PropModeReplace, &sd, 1);
		ws->propSet = TRUE;
    }
}

static void
wallpaperShuffle (WallpaperBackground *bgs, int n)
{
	WallpaperBackground tmp;
	int i, j;

	if (n < 2)
		return;

	for (i = 0; i < n - 1; ++i)
	{
		j = i + rand() / (RAND_MAX / (n - i) + 1);
		tmp = bgs[j];
		bgs[j] = bgs[i];
		bgs[i] = tmp;
	}
}

static void
updateBackgrounds (CompScreen *s)
{
	WALLPAPER_SCREEN (s);

	ws->backgrounds =
	processMultiList (ws->backgrounds, &ws->nBackgrounds, s,
						NUM_LIST_OPTIONS, wallpaperGetBgImageOption (s),
						offsetof (WallpaperBackground, image),
						wallpaperGetBgImagePosOption (s),
						offsetof (WallpaperBackground, imagePos),
						wallpaperGetBgFillTypeOption (s),
						offsetof (WallpaperBackground, fillType),
						wallpaperGetBgColor1Option (s),
						offsetof (WallpaperBackground, color1),
						wallpaperGetBgColor2Option (s),
						offsetof (WallpaperBackground, color2));

	if (ws->nBackgrounds && wallpaperGetRandomize (s))
		wallpaperShuffle (ws->backgrounds, ws->nBackgrounds);
}

static Bool
wallpaperIncrementBackgrounds (void *closure)
{
    CompScreen  *s = (CompScreen *) closure;

	WALLPAPER_SCREEN (s);

    ws->cycle_timeout =
		compAddTimeout (wallpaperGetCycleTimeout (s) * 60000,
						wallpaperGetCycleTimeout (s) * 60000,
						wallpaperIncrementBackgrounds, s);

	ws->fade_time = ws->fade_remaining = wallpaperGetFadeTime (s) * 1000;

	if (!ws->nBackgrounds)
		return FALSE;

	if (++ws->bgOffset >= ws->nBackgrounds)
		ws->bgOffset = 0;

	ws->fading = TRUE;

	damageScreen (s);

	return FALSE;
}

static void
freeBackgrounds (CompScreen *s)
{
	WALLPAPER_SCREEN (s);

	unsigned int i;

	if (!ws->backgrounds || !ws->nBackgrounds)
		return;

	for (i = 0; i < ws->nBackgrounds; i++)
	{
		finiBackground (&ws->backgrounds[i], s);
		free (ws->backgrounds[i].image);
	}

	free (ws->backgrounds);

	ws->backgrounds  = NULL;
	ws->nBackgrounds = 0;
}

static void
wallpaperRecursiveNotify (CompDisplay *d, CompOption *o, WallpaperDisplayOptions num)
{
	CompScreen *s;

	for (s = d->screens; s; s = s->next)
	{
		updateBackgrounds (s);
		updateProperty (s);
		damageScreen (s);
	}
}

static void
wallpaperOptionChanged (CompScreen             *s,
                        CompOption             *o,
                        WallpaperScreenOptions num)
{
	WALLPAPER_SCREEN (s);

	switch (num)
	{
		case WallpaperScreenOptionCycleTimeout:
			if (ws->cycle_timeout)
			{
				compRemoveTimeout (ws->cycle_timeout);
				ws->cycle_timeout = 0;
			}
			if (wallpaperGetCycle (s) && !ws->cycle_timeout)
				ws->cycle_timeout =
				compAddTimeout (wallpaperGetCycleTimeout (s) * 60000,
								wallpaperGetCycleTimeout (s) * 60000,
								wallpaperIncrementBackgrounds, s);
			break;
		case WallpaperScreenOptionCycle:
			if (wallpaperGetCycle (s))
			{
				if (!ws->cycle_timeout)
				{
				ws->cycle_timeout =
				compAddTimeout (wallpaperGetCycleTimeout (s) * 60000,
								wallpaperGetCycleTimeout (s) * 60000,
								wallpaperIncrementBackgrounds, s);
				}
			}
			else
			{
				if (ws->cycle_timeout)
				{
					compRemoveTimeout (ws->cycle_timeout);
					ws->cycle_timeout = 0;
				}
			}
			break;
		case WallpaperScreenOptionHideOtherBackgrounds:
			damageScreen (s);
			if (wallpaperGetHideOtherBackgrounds (s))
			{
				CompWindow *w;
				for (w = s->windows; w; w = w->next)
				{
					if ((w->type & CompWindowTypeDesktopMask) &&
							w->id != ws->fakeDesktop)
						XLowerWindow (s->display->display, w->id);
				}
			}
			else
				XLowerWindow (s->display->display, ws->fakeDesktop);
			break;
		case WallpaperScreenOptionRandomize:
		case WallpaperScreenOptionBgImage:
		case WallpaperScreenOptionBgImagePos:
		case WallpaperScreenOptionBgFillType:
		case WallpaperScreenOptionBgColor1:
		case WallpaperScreenOptionBgColor2:
			updateBackgrounds (s);
			updateProperty (s);
			damageScreen (s);
			break;
		default:
			break;
	}
}

static int
getBackgroundForViewport (CompScreen *s)
{
	WALLPAPER_SCREEN(s);
	int x, y, bg_num;

	if (!ws->nBackgrounds)
		return -1;

	x = s->x - (s->windowOffsetX / s->width);
	x %= s->hsize;
	if (x < 0)
		x += s->hsize;

	y = s->y - (s->windowOffsetY / s->height);
	y %= s->vsize;
	if (y < 0)
		y += s->vsize;

	bg_num = ((x + (y * s->hsize)) % (s->hsize * s->vsize)) - ws->bgOffset;
	while (bg_num < 0)
		bg_num += ws->nBackgrounds;
	while (bg_num >= ws->nBackgrounds)
		bg_num -= ws->nBackgrounds;

    return bg_num;
}

static Bool
wallpaperPaintOutput (CompScreen              *s,
                      const ScreenPaintAttrib *sAttrib,
                      const CompTransform     *transform,
                      Region                  region,
                      CompOutput              *output,
                      unsigned int            mask)
{
	Bool status;

	WALLPAPER_SCREEN (s);

	ws->desktop = NULL;

	UNWRAP (ws, s, paintOutput);
	status = (*s->paintOutput) (s, sAttrib, transform, region, output, mask);
	WRAP (ws, s, paintOutput, wallpaperPaintOutput);

	return status;
}

static void
wallpaperPreparePaintScreen (CompScreen *s,
							int		ms)
{
	WALLPAPER_SCREEN (s);

	if (ws->fakeDesktop == None
		&& ws->nBackgrounds)
		createFakeDesktopWindow (s);

	if (!ws->nBackgrounds
		&& ws->fakeDesktop != None)
		destroyFakeDesktopWindow (s);

	if (!ws->fading)
		goto out;

	ws->fade_remaining -= ms;

	if (ws->fade_remaining <= 0)
	{
		ws->fade_remaining = 0;
		ws->fade_progress = 1.0f;
	}
	else
		ws->fade_progress = cosf(((float) ws->fade_remaining / ws->fade_time) * (M_PI * 0.5));

out:
	UNWRAP (ws, s, preparePaintScreen);
	(*s->preparePaintScreen) (s, ms);
	WRAP (ws, s, preparePaintScreen, wallpaperPreparePaintScreen);
}

static void
wallpaperPaintFill (CompWindow          *w,
                    WallpaperBackground *back,
                    Region              region,
                    FragmentAttrib      fA,
                    unsigned int        mask)
{
	CompScreen *s = w->screen;
	CompMatrix tmpMatrix;

	w->vCount = w->indexCount = 0;

	tmpMatrix = back->fillTex.matrix;

	if (back->fillType == BgFillTypeVerticalGradient)
	{
	    tmpMatrix.yy /= (float) s->height / 2.0;
	}
	else if (back->fillType == BgFillTypeHorizontalGradient)
	{
	    tmpMatrix.xx /= (float) s->width / 2.0;
	}

	(*s->addWindowGeometry) (w, &tmpMatrix, 1,
								&s->region, region);

	if (w->vCount)
	    (*s->drawWindowTexture) (w, &back->fillTex, &fA, mask);
}

static void
wallpaperPaintTex (CompWindow          *w,
                   WallpaperBackground *back,
                   Region              region,
                   FragmentAttrib      fA,
                   unsigned int        mask)
{
	CompScreen *s = w->screen;
	Region reg = &s->region;
	CompMatrix tmpMatrix;
	REGION tmpRegion;
	float  s1, s2;
	int    x, y, xi;

	tmpRegion.rects	 = &tmpRegion.extents;
	tmpRegion.numRects = 1;

	w->vCount = w->indexCount = 0;

	tmpMatrix = back->imgTex.matrix;

	if (back->imagePos == BgImagePosScaleAndCrop)
	{
		s1 = (float) s->width / back->width;
		s2 = (float) s->height / back->height;

		s1 = MAX (s1, s2);

		tmpMatrix.xx /= s1;
		tmpMatrix.yy /= s1;

		x = (s->width - ((int)back->width * s1)) / 2.0;
		tmpMatrix.x0 -= x * tmpMatrix.xx;
		y = (s->height - ((int)back->height * s1)) / 2.0;
		tmpMatrix.y0 -= y * tmpMatrix.yy;
	}
	else if (back->imagePos == BgImagePosScaled)
	{
		s1 = (float) s->width / back->width;
		s2 = (float) s->height / back->height;
		tmpMatrix.xx /= s1;
		tmpMatrix.yy /= s2;
	}
	else if (back->imagePos == BgImagePosCentered)
	{
		x = (s->width - (int)back->width) / 2;
		y = (s->height - (int)back->height) / 2;
		tmpMatrix.x0 -= x * tmpMatrix.xx;
		tmpMatrix.y0 -= y * tmpMatrix.yy;

		reg = &tmpRegion;

		tmpRegion.extents.x1 = MAX (0, x);
		tmpRegion.extents.y1 = MAX (0, y);
		tmpRegion.extents.x2 = MIN (s->width, x + back->width);
		tmpRegion.extents.y2 = MIN (s->height, y + back->height);
	}

	if (back->imagePos == BgImagePosTiled ||
		back->imagePos == BgImagePosCenterTiled)
	{
		if (back->imagePos == BgImagePosCenterTiled)
		{
			x = (s->width - (int)back->width) / 2;
			y = (s->height - (int)back->height) / 2;

			if (x > 0)
			x = (x % (int)back->width) - (int)back->width;
			if (y > 0)
			y = (y % (int)back->height) - (int)back->height;
		}
		else
		{
			x = 0;
			y = 0;
		}

		reg = &tmpRegion;

		while (y < s->height)
		{
			xi = x;
			while (xi < s->width)
			{
				tmpMatrix = back->imgTex.matrix;
				tmpMatrix.x0 -= xi * tmpMatrix.xx;
				tmpMatrix.y0 -= y * tmpMatrix.yy;

				tmpRegion.extents.x1 = MAX (0, xi);
				tmpRegion.extents.y1 = MAX (0, y);
				tmpRegion.extents.x2 = MIN (s->width, xi + back->width);
				tmpRegion.extents.y2 = MIN (s->height,
								y + back->height);

				(*s->addWindowGeometry) (w, &tmpMatrix, 1,
								 reg, region);

				xi += (int)back->width;
			}
			y += (int)back->height;
		}
	}
	else
	{
		(*s->addWindowGeometry) (w, &tmpMatrix, 1, reg, region);
	}

	if (w->vCount)
		(*s->drawWindowTexture) (w, &back->imgTex,
					 &fA, mask);
}

static Bool
wallpaperPaintWindow (CompWindow		     *w,
	     const WindowPaintAttrib *attrib,
	     const CompTransform     *transform,
	     Region		     region,
	     unsigned int	     mask)
{
    Bool           status;

    WALLPAPER_SCREEN (w->screen);

    if ((w->type & CompWindowTypeDesktopMask) &&
					w->id != ws->fakeDesktop &&
					ws->nBackgrounds &&
					wallpaperGetHideOtherBackgrounds (w->screen))
		return FALSE;


    UNWRAP (ws, w->screen, paintWindow);
    status = (*w->screen->paintWindow) (w, attrib, transform, region, mask);
    WRAP (ws, w->screen, paintWindow, wallpaperPaintWindow);

    return status;
}

static Bool
wallpaperDrawWindow (CompWindow           *w,
		     const CompTransform  *transform,
		     const FragmentAttrib *attrib,
		     Region               region,
		     unsigned int         mask)
{
    Bool           status;
    CompScreen     *s = w->screen;
    FragmentAttrib fA = *attrib;
    FragmentAttrib fA2 = *attrib;
	int bg1 = getBackgroundForViewport (s);

    WALLPAPER_SCREEN (w->screen);

    if ((ws->desktop && ws->desktop != w) || !ws->nBackgrounds || !w->alpha ||
	!(w->type & CompWindowTypeDesktopMask))
		goto out;

	int saveFilter, filterIdx;
	int bg2 = bg1 + 1; /* previous bg */
	if (bg2 >= ws->nBackgrounds)
		bg2 -= ws->nBackgrounds;

	WallpaperBackground *back = &ws->backgrounds[bg1];
	WallpaperBackground *back2 = &ws->backgrounds[bg2];

	if (!back->loaded)
	{
		while (!initBackground(back, s))
		{
			ws->nBackgrounds--;
			free (back->image);

			if (!ws->nBackgrounds)
				goto out;

			if (bg1 < ws->nBackgrounds)
				memcpy(back, back + 1, (ws->nBackgrounds - bg1) * sizeof (WallpaperBackground));

			ws->backgrounds = realloc(ws->backgrounds, ws->nBackgrounds * sizeof (WallpaperBackground));

			bg1 = getBackgroundForViewport (s);
			bg2 = bg1 + 1;
			if (bg2 >= ws->nBackgrounds)
				bg2 -= ws->nBackgrounds;
			back = &ws->backgrounds[bg1];
			back2 = &ws->backgrounds[bg2];
		}
	}

	if (ws->fading && ws->nBackgrounds > 1)
	{
		fA.opacity *= ws->fade_progress;
		fA2.opacity -= fA.opacity;
		if (!ws->fade_remaining)
		{
			ws->fading = FALSE;
			if (ws->nBackgrounds > 1 && (s->hsize * s->vsize) < ws->nBackgrounds)
			{
				int bg_tmp = bg2 + (s->hsize * s->vsize);

				if (bg_tmp >= ws->nBackgrounds)
					bg_tmp -= ws->nBackgrounds;

				if (ws->backgrounds[bg_tmp].loaded)
					finiBackground(&ws->backgrounds[bg_tmp], s);
			}
		}
		damageScreen(s);
	}
	
	if (mask & PAINT_WINDOW_TRANSFORMED_MASK)
	    region = &infiniteRegion;

	if (mask & PAINT_WINDOW_ON_TRANSFORMED_SCREEN_MASK)
	    filterIdx = SCREEN_TRANS_FILTER;
	else if (mask & PAINT_WINDOW_TRANSFORMED_MASK)
	    filterIdx = WINDOW_TRANS_FILTER;
	else
	    filterIdx = NOTHING_TRANS_FILTER;

	saveFilter = s->filter[filterIdx];

	s->filter[filterIdx] = COMP_TEXTURE_FILTER_GOOD;

	mask |= PAINT_WINDOW_BLEND_MASK;

	wallpaperPaintFill (w, back, region, fA, mask);

	if (ws->fading && ws->nBackgrounds > 1)
		wallpaperPaintFill (w, back2, region, fA2, mask);

	if (back->width && back->height)
		wallpaperPaintTex (w, back, region, fA, mask);

	if (ws->fading && ws->nBackgrounds > 1 && back2->width && back2->height)
		wallpaperPaintTex (w, back2, region, fA2, mask);

	s->filter[filterIdx] = saveFilter;

	ws->desktop = w;
	fA.opacity  = OPAQUE;

out:
    UNWRAP (ws, w->screen, drawWindow);
    status = (*w->screen->drawWindow) (w, transform, &fA, region, mask);
    WRAP (ws, w->screen, drawWindow, wallpaperDrawWindow);

    return status;
}

static Bool
wallpaperDamageWindowRect (CompWindow *w,
							Bool       initial,
							BoxPtr     rect)
{
	Bool status;

	WALLPAPER_SCREEN (w->screen);

	if (w->id == ws->fakeDesktop)
		damageScreen (w->screen);

	UNWRAP (ws, w->screen, damageWindowRect);
	status = (*w->screen->damageWindowRect) (w, initial, rect);
	WRAP (ws, w->screen, damageWindowRect, wallpaperDamageWindowRect);

	return status;
}

static Bool
wallpaperInitDisplay (CompPlugin  *p,
                      CompDisplay *d)
{
	WallpaperDisplay * wd;

	if (!checkPluginABI ("core", CORE_ABIVERSION))
		return FALSE;

	wd = malloc (sizeof (WallpaperDisplay));
	if (!wd)
		return FALSE;

	wd->screenPrivateIndex = allocateScreenPrivateIndex (d);
	if (wd->screenPrivateIndex < 0)
	{
		free (wd);
		return FALSE;
	}

	wd->compizWallpaperAtom = XInternAtom (d->display,
						"_COMPIZ_WALLPAPER_SUPPORTED", 0);

	d->base.privates[WallpaperDisplayPrivateIndex].ptr = wd;

	wallpaperSetRecursiveNotify (d, wallpaperRecursiveNotify);

	return TRUE;
}

static void wallpaperFiniDisplay (CompPlugin  *p,
                                  CompDisplay *d)
{
	WALLPAPER_DISPLAY (d);

	freeScreenPrivateIndex (d, wd->screenPrivateIndex);

	free (wd);
}

static Bool wallpaperInitScreen (CompPlugin *p,
                                 CompScreen *s)
{
	WallpaperScreen *ws;
	WALLPAPER_DISPLAY (s->display);

	ws = malloc (sizeof (WallpaperScreen));
	if (!ws)
		return FALSE;

	ws->backgrounds  = NULL;
	ws->nBackgrounds = 0;
	ws->bgOffset = 0;
	ws->fading = FALSE;
	ws->fade_progress = 1.0;
	ws->cycle_timeout = 0;

	ws->propSet = FALSE;

	ws->fakeDesktop = None;

	wallpaperSetBgImageNotify (s, wallpaperOptionChanged);
	wallpaperSetBgImagePosNotify (s, wallpaperOptionChanged);
	wallpaperSetBgFillTypeNotify (s, wallpaperOptionChanged);
	wallpaperSetBgColor1Notify (s, wallpaperOptionChanged);
	wallpaperSetBgColor2Notify (s, wallpaperOptionChanged);
	wallpaperSetCycleTimeoutNotify (s, wallpaperOptionChanged);
	wallpaperSetCycleNotify (s, wallpaperOptionChanged);
	wallpaperSetRandomizeNotify (s, wallpaperOptionChanged);
	wallpaperSetHideOtherBackgroundsNotify (s, wallpaperOptionChanged);

	s->base.privates[wd->screenPrivateIndex].ptr = ws;

	ws->fade_time = ws->fade_remaining = wallpaperGetFadeTime (s) * 1000;

	if (wallpaperGetCycle (s))
		ws->cycle_timeout =
			compAddTimeout (wallpaperGetCycleTimeout (s) * 60000,
							wallpaperGetCycleTimeout (s) * 60000,
							wallpaperIncrementBackgrounds, s);

	WRAP (ws, s, paintOutput, wallpaperPaintOutput);
	WRAP (ws, s, drawWindow, wallpaperDrawWindow);
	WRAP (ws, s, paintWindow, wallpaperPaintWindow);
	WRAP (ws, s, damageWindowRect, wallpaperDamageWindowRect);
	WRAP (ws, s, preparePaintScreen, wallpaperPreparePaintScreen);

	return TRUE;
}

static void wallpaperFiniScreen (CompPlugin *p,
                                 CompScreen *s)
{
	WALLPAPER_SCREEN (s);
	WALLPAPER_DISPLAY (s->display);

	if (ws->propSet)
		XDeleteProperty (s->display->display, s->root, wd->compizWallpaperAtom);

	if (ws->fakeDesktop != None)
		destroyFakeDesktopWindow (s);

	compRemoveTimeout (ws->cycle_timeout);

	freeBackgrounds (s);

	UNWRAP (ws, s, paintOutput);
	UNWRAP (ws, s, drawWindow);
	UNWRAP (ws, s, paintWindow);
	UNWRAP (ws, s, damageWindowRect);
	UNWRAP (ws, s, preparePaintScreen);

	free (ws);
}

static CompBool
wallpaperInitObject (CompPlugin *p,
                     CompObject *o)
{
	static InitPluginObjectProc dispTab[] = {
		(InitPluginObjectProc) 0, /* InitCore */
		(InitPluginObjectProc) wallpaperInitDisplay,
		(InitPluginObjectProc) wallpaperInitScreen
	};

	RETURN_DISPATCH (o, dispTab, ARRAY_SIZE (dispTab), TRUE, (p, o));
}

static void
wallpaperFiniObject (CompPlugin *p,
                     CompObject *o)
{
	static FiniPluginObjectProc dispTab[] = {
		(FiniPluginObjectProc) 0, /* FiniCore */
		(FiniPluginObjectProc) wallpaperFiniDisplay,
		(FiniPluginObjectProc) wallpaperFiniScreen
	};

	DISPATCH (o, dispTab, ARRAY_SIZE (dispTab), (p, o));
}

static Bool
wallpaperInit (CompPlugin *p)
{
	WallpaperDisplayPrivateIndex = allocateDisplayPrivateIndex ();
	if (WallpaperDisplayPrivateIndex < 0)
		return FALSE;
	return TRUE;
}

static void
wallpaperFini (CompPlugin *p)
{
	freeDisplayPrivateIndex (WallpaperDisplayPrivateIndex);
}

static CompPluginVTable wallpaperVTable=
{
	"wallpaper",
	0,
	wallpaperInit,
	wallpaperFini,
	wallpaperInitObject,
	wallpaperFiniObject,
	0,
	0
};

CompPluginVTable* getCompPluginInfo (void)
{
	return &wallpaperVTable;
}

