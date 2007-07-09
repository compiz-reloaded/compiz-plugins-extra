/*
 * Compiz cube gears plugin
 *
 * gears.c
 *
 * This is an example plugin to show how to render something inside
 * of the transparent cube
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
 * Based on glxgears.c:
 *    http://cvsweb.xfree86.org/cvsweb/xc/programs/glxgears/glxgears.c
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <math.h>

#include <compiz.h>
#include <cube.h>


#define DEG2RAD (M_PI / 180.0f)

static const CompTransform identity =
{
   {
	1.0, 0.0, 0.0, 0.0,
	0.0, 1.0, 0.0, 0.0,
	0.0, 0.0, 1.0, 0.0,
	0.0, 0.0, 0.0, 1.0
    }
};

#define MULTMV(m, v) { \
	float v0 = m[0]*v[0] + m[4]*v[1] + m[8]*v[2] + m[12]*v[3]; \
	float v1 = m[1]*v[0] + m[5]*v[1] + m[9]*v[2] + m[13]*v[3]; \
	float v2 = m[2]*v[0] + m[6]*v[1] + m[10]*v[2] + m[14]*v[3]; \
	float v3 = m[3]*v[0] + m[7]*v[1] + m[11]*v[2] + m[15]*v[3]; \
	v[0] = v0; v[1] = v1; v[2] = v2; v[3] = v3; }

static int displayPrivateIndex;

static int cubeDisplayPrivateIndex;

typedef struct _GearsDisplay
{
    int screenPrivateIndex;

}
GearsDisplay;

typedef struct _GearsScreen
{
    DonePaintScreenProc    donePaintScreen;
    PreparePaintScreenProc preparePaintScreen;

    CubeClearTargetOutputProc clearTargetOutput;
    CubePaintInsideProc       paintInside;

    Bool damage;

    float contentRotation;
    GLuint gear1, gear2, gear3;
    float angle;
    float a1, a2, a3;
}
GearsScreen;

#define GET_GEARS_DISPLAY(d) \
    ((GearsDisplay *) (d)->privates[displayPrivateIndex].ptr)
#define GEARS_DISPLAY(d) \
    GearsDisplay *gd = GET_GEARS_DISPLAY(d);

#define GET_GEARS_SCREEN(s, gd) \
    ((GearsScreen *) (s)->privates[(gd)->screenPrivateIndex].ptr)
#define GEARS_SCREEN(s) \
    GearsScreen *gs = GET_GEARS_SCREEN(s, GET_GEARS_DISPLAY(s->display))


static void
gear (GLfloat inner_radius,
      GLfloat outer_radius,
      GLfloat width,
      GLint   teeth,
      GLfloat tooth_depth)
{
    GLint i;
    GLfloat r0, r1, r2, maxr2, minr2;
    GLfloat angle, da;
    GLfloat u, v, len;

    r0 = inner_radius;
    r1 = outer_radius - tooth_depth / 2.0;
    maxr2 = r2 = outer_radius + tooth_depth / 2.0;
    minr2 = r2;

    da = 2.0 * M_PI / teeth / 4.0;

    glShadeModel (GL_FLAT);

    glNormal3f (0.0, 0.0, 1.0);

    /* draw front face */
    glBegin (GL_QUAD_STRIP);

    for (i = 0; i <= teeth; i++)
    {
	angle = i * 2.0 * M_PI / teeth;
	glVertex3f (r0 * cos (angle), r0 * sin (angle), width * 0.5);
	glVertex3f (r1 * cos (angle), r1 * sin (angle), width * 0.5);

	if (i < teeth)
	{
	    glVertex3f (r0 * cos (angle), r0 * sin (angle), width * 0.5);
	    glVertex3f (r1 * cos (angle + 3 * da), r1 * sin (angle + 3 * da),
			width * 0.5);
	}
    }

    glEnd();

    /* draw front sides of teeth */
    glBegin (GL_QUADS);

    for (i = 0; i < teeth; i++)
    {
	angle = i * 2.0 * M_PI / teeth;

	glVertex3f (r1 * cos (angle), r1 * sin (angle), width * 0.5);
	glVertex3f (r2 * cos (angle + da), r2 * sin (angle + da), width * 0.5);
	glVertex3f (r2 * cos (angle + 2 * da), r2 * sin (angle + 2 * da),
		    width * 0.5);
	glVertex3f (r1 * cos (angle + 3 * da), r1 * sin (angle + 3 * da),
		    width * 0.5);
	r2 = minr2;
    }

    r2 = maxr2;

    glEnd();

    glNormal3f (0.0, 0.0, -1.0);

    /* draw back face */
    glBegin (GL_QUAD_STRIP);

    for (i = 0; i <= teeth; i++)
    {
	angle = i * 2.0 * M_PI / teeth;
	glVertex3f (r1 * cos (angle), r1 * sin (angle), -width * 0.5);
	glVertex3f (r0 * cos (angle), r0 * sin (angle), -width * 0.5);

	if (i < teeth)
	{
	    glVertex3f (r1 * cos (angle + 3 * da), r1 * sin (angle + 3 * da),
			-width * 0.5);
	    glVertex3f (r0 * cos (angle), r0 * sin (angle), -width * 0.5);
	}
    }

    glEnd();

    /* draw back sides of teeth */
    glBegin (GL_QUADS);
    da = 2.0 * M_PI / teeth / 4.0;

    for (i = 0; i < teeth; i++)
    {
	angle = i * 2.0 * M_PI / teeth;

	glVertex3f (r1 * cos (angle + 3 * da), r1 * sin (angle + 3 * da),
		    -width * 0.5);
	glVertex3f (r2 * cos (angle + 2 * da), r2 * sin (angle + 2 * da),
		    -width * 0.5);
	glVertex3f (r2 * cos (angle + da), r2 * sin (angle + da), -width * 0.5);
	glVertex3f (r1 * cos (angle), r1 * sin (angle), -width * 0.5);
	r2 = minr2;
    }

    r2 = maxr2;

    glEnd();

    /* draw outward faces of teeth */
    glBegin (GL_QUAD_STRIP);

    for (i = 0; i < teeth; i++)
    {
	angle = i * 2.0 * M_PI / teeth;

	glVertex3f (r1 * cos (angle), r1 * sin (angle), width * 0.5);
	glVertex3f (r1 * cos (angle), r1 * sin (angle), -width * 0.5);
	u = r2 * cos (angle + da) - r1 * cos (angle);
	v = r2 * sin (angle + da) - r1 * sin (angle);
	len = sqrt (u * u + v * v);
	u /= len;
	v /= len;
	glNormal3f (v, -u, 0.0);
	glVertex3f (r2 * cos (angle + da), r2 * sin (angle + da), width * 0.5);
	glVertex3f (r2 * cos (angle + da), r2 * sin (angle + da), -width * 0.5);
	glNormal3f (cos (angle + 1.5 * da), sin (angle + 1.5 * da), 0.0);
	glVertex3f (r2 * cos (angle + 2 * da), r2 * sin (angle + 2 * da),
		    width * 0.5);
	glVertex3f (r2 * cos (angle + 2 * da), r2 * sin (angle + 2 * da),
		    -width * 0.5);
	u = r1 * cos (angle + 3 * da) - r2 * cos (angle + 2 * da);
	v = r1 * sin (angle + 3 * da) - r2 * sin (angle + 2 * da);
	glNormal3f (v, -u, 0.0);
	glVertex3f (r1 * cos (angle + 3 * da), r1 * sin (angle + 3 * da),
		    width * 0.5);
	glVertex3f (r1 * cos (angle + 3 * da), r1 * sin (angle + 3 * da),
		    -width * 0.5);
	glNormal3f (cos (angle + 3.5 * da), sin (angle + 3.5 * da), 0.0);
	r2 = minr2;
    }

    r2 = maxr2;

    glVertex3f (r1 * cos (0), r1 * sin (0), width * 0.5);
    glVertex3f (r1 * cos (0), r1 * sin (0), -width * 0.5);

    glEnd();

    glShadeModel (GL_SMOOTH);

    /* draw inside radius cylinder */
    glBegin (GL_QUAD_STRIP);

    for (i = 0; i <= teeth; i++)
    {
	angle = i * 2.0 * M_PI / teeth;
	glNormal3f (-cos (angle), -sin (angle), 0.0);
	glVertex3f (r0 * cos (angle), r0 * sin (angle), -width * 0.5);
	glVertex3f (r0 * cos (angle), r0 * sin (angle), width * 0.5);
    }

    glEnd();
}

static void
gearsClearTargetOutput (CompScreen *s,
			float      xRotate,
			float      vRotate)
{
    GEARS_SCREEN (s);
    CUBE_SCREEN (s);

    UNWRAP (gs, cs, clearTargetOutput);
    (*cs->clearTargetOutput) (s, xRotate, vRotate);
    WRAP (gs, cs, clearTargetOutput, gearsClearTargetOutput);

    glClear (GL_DEPTH_BUFFER_BIT);
}

static void
gearsPaintInside (CompScreen              *s,
		  const ScreenPaintAttrib *sAttrib,
		  const CompTransform     *transform,
		  CompOutput              *output,
		  int                     size)
{
    GEARS_SCREEN (s);
    CUBE_SCREEN (s);

    static GLfloat white[4] = { 1.0, 1.0, 1.0, 1.0 };

    ScreenPaintAttrib sA = *sAttrib;

    sA.yRotate += (360.0f / size) * (cs->xRotations - (s->x * cs->nOutput));

    CompTransform mT = *transform;

    (*s->applyScreenTransform) (s, &sA, output, &mT);

    glPushMatrix();
    glLoadMatrixf (mT.m);
    glTranslatef (cs->outputXOffset, -cs->outputYOffset, 0.0f);
    glScalef (cs->outputXScale, cs->outputYScale, 1.0f);

    Bool enabledCull = FALSE;

    glPushAttrib (GL_COLOR_BUFFER_BIT | GL_TEXTURE_BIT);

    glDisable (GL_BLEND);

    if (!glIsEnabled (GL_CULL_FACE) )
    {
	enabledCull = TRUE;
	glEnable (GL_CULL_FACE);
    }

    glPushMatrix();

    glRotatef (gs->contentRotation, 0.0, 1.0, 0.0);

    glScalef (0.05, 0.05, 0.05);
    glColor4usv (defaultColor);

    glEnable (GL_NORMALIZE);
    glEnable (GL_LIGHTING);
    glEnable (GL_LIGHT1);
    glDisable (GL_COLOR_MATERIAL);

    glEnable (GL_DEPTH_TEST);
    glTexEnvi (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

    glPushMatrix();
    glTranslatef (-3.0, -2.0, 0.0);
    glRotatef (gs->angle, 0.0, 0.0, 1.0);
    glCallList (gs->gear1);
    glPopMatrix();

    glPushMatrix();
    glTranslatef (3.1, -2.0, 0.0);
    glRotatef (-2.0 * gs->angle - 9.0, 0.0, 0.0, 1.0);
    glCallList (gs->gear2);
    glPopMatrix();

    glPushMatrix();
    glTranslatef (-3.1, 4.2, 0.0);
    glRotatef (-2.0 * gs->angle - 25.0, 0.0, 0.0, 1.0);
    glCallList (gs->gear3);
    glPopMatrix();

    glMaterialfv (GL_FRONT, GL_AMBIENT_AND_DIFFUSE, white);

    glPopMatrix();

    glDisable (GL_LIGHT1);
    glDisable (GL_NORMALIZE);
    glEnable (GL_COLOR_MATERIAL);

    if (!s->lighting)
	glDisable (GL_LIGHTING);

    glDisable (GL_DEPTH_TEST);

    if (enabledCull)
	glDisable (GL_CULL_FACE);

    glPopMatrix();
    glPopAttrib();

    gs->damage = TRUE;

    UNWRAP (gs, cs, paintInside);
    (*cs->paintInside) (s, sAttrib, transform, output, size);
    WRAP (gs, cs, paintInside, gearsPaintInside);
}

static void
gearsPreparePaintScreen (CompScreen *s,
			 int        ms)
{
    GEARS_SCREEN (s);

    gs->contentRotation += ms * 360.0 / 20000.0;
    gs->contentRotation = fmod (gs->contentRotation, 360.0);
    gs->angle += ms * 360.0 / 8000.0;
    gs->angle = fmod (gs->angle, 360.0);
    gs->a1 += ms * 360.0 / 3000.0;
    gs->a1 = fmod (gs->a1, 360.0);
    gs->a2 += ms * 360.0 / 2000.0;
    gs->a2 = fmod (gs->a2, 360.0);
    gs->a3 += ms * 360.0 / 1000.0;
    gs->a3 = fmod (gs->a3, 360.0);

    UNWRAP (gs, s, preparePaintScreen);
    (*s->preparePaintScreen) (s, ms);
    WRAP (gs, s, preparePaintScreen, gearsPreparePaintScreen);
}

static void
gearsDonePaintScreen (CompScreen * s)
{
    GEARS_SCREEN (s);

    if (gs->damage)
    {
	damageScreen (s);
	gs->damage = FALSE;
    }

    UNWRAP (gs, s, donePaintScreen);
    (*s->donePaintScreen) (s);
    WRAP (gs, s, donePaintScreen, gearsDonePaintScreen);
}


static Bool
gearsInitDisplay (CompPlugin  *p,
		  CompDisplay *d)
{
    GearsDisplay *gd;
    CompPlugin	 *cube = findActivePlugin ("cube");

    CompOption	*option;
    int			nOption;

    if (!cube || !cube->vTable->getDisplayOptions)
	return FALSE;

    option = (*cube->vTable->getDisplayOptions) (cube, d, &nOption);

    if (getIntOptionNamed (option, nOption, "abi", 0) != CUBE_ABIVERSION)
    {
	compLogMessage (d, "gears", CompLogLevelError,
			"cube ABI version mismatch");
	return FALSE;
    }

    cubeDisplayPrivateIndex = getIntOptionNamed (option, nOption, "index", -1);

    if (cubeDisplayPrivateIndex < 0)
	return FALSE;

    gd = malloc (sizeof (GearsDisplay) );

    if (!gd)
	return FALSE;

    gd->screenPrivateIndex = allocateScreenPrivateIndex (d);

    if (gd->screenPrivateIndex < 0)
    {
	free (gd);
	return FALSE;
    }

    d->privates[displayPrivateIndex].ptr = gd;

    return TRUE;
}

static void
gearsFiniDisplay (CompPlugin  *p,
		  CompDisplay *d)
{
    GEARS_DISPLAY (d);

    freeScreenPrivateIndex (d, gd->screenPrivateIndex);
    free (gd);
}

static Bool
gearsInitScreen (CompPlugin *p,
		 CompScreen *s)
{
    GearsScreen *gs;

    static GLfloat pos[4]         = { 5.0, 5.0, 10.0, 0.0 };
    static GLfloat red[4]         = { 0.8, 0.1, 0.0, 1.0 };
    static GLfloat green[4]       = { 0.0, 0.8, 0.2, 1.0 };
    static GLfloat blue[4]        = { 0.2, 0.2, 1.0, 1.0 };
    static GLfloat ambientLight[] = { 0.3f, 0.3f, 0.3f, 0.3f };
    static GLfloat diffuseLight[] = { 0.5f, 0.5f, 0.5f, 0.5f };

    GEARS_DISPLAY (s->display);

    CUBE_SCREEN (s);

    gs = malloc (sizeof (GearsScreen) );

    if (!gs)
	return FALSE;

    s->privates[gd->screenPrivateIndex].ptr = gs;

    glLightfv (GL_LIGHT1, GL_AMBIENT, ambientLight);
    glLightfv (GL_LIGHT1, GL_DIFFUSE, diffuseLight);
    glLightfv (GL_LIGHT1, GL_POSITION, pos);

    gs->contentRotation = 0.0;

    gs->gear1 = glGenLists (1);
    glNewList (gs->gear1, GL_COMPILE);
    glMaterialfv (GL_FRONT, GL_AMBIENT_AND_DIFFUSE, red);
    gear (1.0, 4.0, 1.0, 20, 0.7);
    glEndList();

    gs->gear2 = glGenLists (1);
    glNewList (gs->gear2, GL_COMPILE);
    glMaterialfv (GL_FRONT, GL_AMBIENT_AND_DIFFUSE, green);
    gear (0.5, 2.0, 2.0, 10, 0.7);
    glEndList();

    gs->gear3 = glGenLists (1);
    glNewList (gs->gear3, GL_COMPILE);
    glMaterialfv (GL_FRONT, GL_AMBIENT_AND_DIFFUSE, blue);
    gear (1.3, 2.0, 0.5, 10, 0.7);
    glEndList();

    gs->angle = 0.0;
    gs->a1    = 0.0;
    gs->a2    = 0.0;
    gs->a3    = 0.0;

    WRAP (gs, s, donePaintScreen, gearsDonePaintScreen);
    WRAP (gs, s, preparePaintScreen, gearsPreparePaintScreen);
    WRAP (gs, cs, clearTargetOutput, gearsClearTargetOutput);
    WRAP (gs, cs, paintInside, gearsPaintInside);

    return TRUE;
}

static void
gearsFiniScreen (CompPlugin *p,
		 CompScreen *s)
{
    GEARS_SCREEN (s);
    CUBE_SCREEN (s);

    glDeleteLists (gs->gear1, 1);
    glDeleteLists (gs->gear2, 1);
    glDeleteLists (gs->gear3, 1);

    UNWRAP (gs, s, donePaintScreen);
    UNWRAP (gs, s, preparePaintScreen);

    UNWRAP (gs, cs, clearTargetOutput);
    UNWRAP (gs, cs, paintInside);

    free (gs);
}

static Bool
gearsInit (CompPlugin * p)
{
    displayPrivateIndex = allocateDisplayPrivateIndex();

    if (displayPrivateIndex < 0)
	return FALSE;

    return TRUE;
}

static void
gearsFini (CompPlugin * p)
{
    if (displayPrivateIndex >= 0)
	freeDisplayPrivateIndex (displayPrivateIndex);
}

static int
gearsGetVersion (CompPlugin *plugin,
		 int        version)
{
    return ABIVERSION;
}

CompPluginVTable gearsVTable = {

    "gears",
    gearsGetVersion,
    0,
    gearsInit,
    gearsFini,
    gearsInitDisplay,
    gearsFiniDisplay,
    gearsInitScreen,
    gearsFiniScreen,
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
    return &gearsVTable;
}
