/*
 * Copyright © 2012 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial
 * portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT.  IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 * 
 * Copyright © 2017 Scott Moreau
 */

#define _GNU_SOURCE /* For asprintf */
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <dirent.h>
#include <sys/stat.h>
#include <math.h>

#include <compiz-core.h>

#include "vidcap_options.h"
#include "wcap-decode.h"

#define WCAPFILE "/tmp/vidcap.wcap"
#define RAWFILE "/tmp/vidcap.raw"

#define DONE_MS 1500
#define SPIN_MS 1000
#define BLINK_MS 500

static int VidcapDisplayPrivateIndex;

typedef struct _VidcapDisplay
{
    int screenPrivateIndex;
    int fd;
    uint32_t ms;
    uint32_t *frame;

	int dot_timer;
    pthread_t thread;
    Bool thread_running, recording, show_dot, done;
} VidcapDisplay;

typedef struct _VidcapScreen
{
    PaintScreenProc	paintScreen;
    PreparePaintScreenProc	preparePaintScreen;
    DonePaintScreenProc	donePaintScreen;
} VidcapScreen;

#define VIDCAP_DISPLAY(d) PLUGIN_DISPLAY(d, Vidcap, v)
#define VIDCAP_SCREEN(s) PLUGIN_SCREEN(s, Vidcap, v)

static void
wcap_decoder_decode_rectangle(struct wcap_decoder *decoder,
			      struct wcap_rectangle *rect)
{
	uint32_t v, *p = decoder->p, *d;
	int width = rect->x2 - rect->x1, height = rect->y2 - rect->y1;
	int x, i, j, k, l, count = width * height;
	unsigned char r, g, b, dr, dg, db;

	d = decoder->frame + (rect->y2 - 1) * decoder->width;
	x = rect->x1;
	i = 0;
	while (i < count) {
		v = *p++;
		l = v >> 24;
		if (l < 0xe0) {
			j = l + 1;
		} else {
			j = 1 << (l - 0xe0 + 7);
		}

		dr = (v >> 16);
		dg = (v >>  8);
		db = (v >>  0);
		for (k = 0; k < j; k++) {
			r = (d[x] >> 16) + dr;
			g = (d[x] >>  8) + dg;
			b = (d[x] >>  0) + db;
			d[x] = 0xff000000 | (r << 16) | (g << 8) | b;
			x++;
			if (x == rect->x2) {
				x = rect->x1;
				d -= decoder->width;
			}
		}
		i += j;
	}

	if (i != count)
		printf("rle encoding longer than expected (%d expected %d)\n",
		       i, count);

	decoder->p = p;
}

int
wcap_decoder_get_frame(struct wcap_decoder *decoder)
{
	struct wcap_rectangle *rects;
	struct wcap_frame_header *header;
	uint32_t i;

	if (decoder->p == decoder->end)
		return 0;

	header = decoder->p;
	decoder->msecs = header->msecs;
	decoder->count++;

	rects = (void *) (header + 1);
	decoder->p = (uint32_t *) (rects + header->nrects);
	for (i = 0; i < header->nrects; i++)
		wcap_decoder_decode_rectangle(decoder, &rects[i]);

	return 1;
}

struct wcap_decoder *
wcap_decoder_create(const char *filename)
{
	struct wcap_decoder *decoder;
	struct wcap_header *header;
	int frame_size;
	struct stat buf;

	decoder = malloc(sizeof *decoder);
	if (decoder == NULL)
		return NULL;

	decoder->fd = open(filename, O_RDONLY);
	if (decoder->fd == -1) {
		free(decoder);
		return NULL;
	}

	fstat(decoder->fd, &buf);
	decoder->size = buf.st_size;
	decoder->map = mmap(NULL, decoder->size,
			    PROT_READ, MAP_PRIVATE, decoder->fd, 0);
	if (decoder->map == MAP_FAILED) {
		fprintf(stderr, "mmap failed\n");
		close(decoder->fd);
		free(decoder);
		return NULL;
	}

	header = decoder->map;
	decoder->format = header->format;
	decoder->count = 0;
	decoder->width = header->width;
	decoder->height = header->height;
	decoder->p = header + 1;
	decoder->end = (char *) decoder->map + decoder->size;

	frame_size = header->width * header->height * 4;
	decoder->frame = malloc(frame_size);
	if (decoder->frame == NULL) {
		close(decoder->fd);
		free(decoder);
		return NULL;
	}
	memset(decoder->frame, 0, frame_size);

	return decoder;
}

void
wcap_decoder_destroy(struct wcap_decoder *decoder)
{
	munmap(decoder->map, decoder->size);
	close(decoder->fd);
	free(decoder->frame);
	free(decoder);
}

static uint32_t *
output_run(uint32_t *p, uint32_t delta, int run)
{
	int i;

	while (run > 0) {
		if (run <= 0xe0) {
			*p++ = delta | ((run - 1) << 24);
			break;
		}

		i = 24 - __builtin_clz(run);
		*p++ = delta | ((i + 0xe0) << 24);
		run -= 1 << (7 + i);
	}

	return p;
}

static uint32_t
component_delta(uint32_t next, uint32_t prev)
{
	unsigned char dr, dg, db;

	dr = (next >> 16) - (prev >> 16);
	dg = (next >>  8) - (prev >>  8);
	db = (next >>  0) - (prev >>  0);

	return (dr << 16) | (dg << 8) | (db << 0);
}

static void
vidcapPreparePaintScreen (CompScreen *s, int ms)
{
	int delay_ms;
	VIDCAP_SCREEN (s);
	VIDCAP_DISPLAY (s->display);

	if (vd->recording)
		vd->ms += ms;

	if (vidcapGetDrawIndicator (s->display) &&
		(vd->recording || vd->thread_running || vd->done)) {
		vd->dot_timer += ms;
		delay_ms = (vd->thread_running ? SPIN_MS : BLINK_MS);
		if (!vd->done && vd->dot_timer > delay_ms)	{
			if (vd->thread_running)
				vd->dot_timer -= SPIN_MS;
			else
				vd->dot_timer -= BLINK_MS;
			vd->show_dot = !vd->show_dot;
		}
		if (vd->done && vd->dot_timer > DONE_MS)
			vd->done = FALSE;
	}

	UNWRAP (vs, s, preparePaintScreen);
	(*s->preparePaintScreen) (s, ms); 
	WRAP (vs, s, preparePaintScreen, vidcapPreparePaintScreen);
}

static void
vidcapDonePaintScreen (CompScreen *s)
{
	VIDCAP_SCREEN (s);
	VIDCAP_DISPLAY (s->display);

	if (vidcapGetDrawIndicator (s->display) &&
		(vd->recording || vd->thread_running || vd->done))
		damageScreen (s);

	UNWRAP (vs, s, donePaintScreen);
	(*s->donePaintScreen) (s); 
	WRAP (vs, s, donePaintScreen, vidcapDonePaintScreen);
		
}

static void
vidcapPaintScreen (CompScreen   *screen,
					CompOutput   *outputs,
					int          numOutput,
					unsigned int mask)
{
	struct {
		uint32_t msecs;
		uint32_t nrects;
	} header;
	struct box {
		uint32_t x1;
		uint32_t y1;
		uint32_t x2;
		uint32_t y2;
	};
	uint32_t delta, prev, *d, *s, *p, next, *outbuf, *pixel_data;
	int i, j, k, width, height, run, stride, y_orig, size;
	struct iovec v[2];

    VIDCAP_SCREEN (screen);
    VIDCAP_DISPLAY (screen->display);

	UNWRAP (vs, screen, paintScreen);
	(*screen->paintScreen) (screen, outputs, numOutput, mask); 
	WRAP (vs, screen, paintScreen, vidcapPaintScreen);

	if (vd->recording) {
		struct box *b = malloc (screen->nOutputDev * sizeof (struct box));
		for (i = 0; i < screen->nOutputDev; i++) {
			b[i].x1 = outputs[i].region.extents.x1;
			b[i].y1 = outputs[i].region.extents.y1;
			b[i].x2 = outputs[i].region.extents.x2;
			b[i].y2 = outputs[i].region.extents.y2;
		}

		header.msecs = vd->ms;
		header.nrects = screen->nOutputDev;

		v[0].iov_base = &header;
		v[0].iov_len = sizeof header;
		v[1].iov_base = b;
		v[1].iov_len = screen->nOutputDev * sizeof (struct box);

		writev (vd->fd, v, 2);

		stride = screen->width;

		for (i = 0; i < screen->nOutputDev; i++) {
			width = outputs[i].width;
			height = outputs[i].height;

			size = width * height * 4;

			pixel_data = malloc(size);
			if (!pixel_data)
				return;

			outbuf = pixel_data;

			y_orig = screen->height - b[i].y2;

			glReadPixels(b[i].x1, y_orig, width, height,
					GL_RGBA, GL_UNSIGNED_BYTE, (GLvoid *) pixel_data);

			p = outbuf;
			run = prev = 0;
			for (j = 0; j < height; j++) {
				s = pixel_data + width * j;
				y_orig = b[i].y2 - j - 1;
				d = vd->frame + stride * y_orig + b[i].x1;

				for (k = 0; k < width; k++) {
					next = *s++;
					delta = component_delta(next, *d);
					*d++ = next;
					if (run == 0 || delta == prev) {
						run++;
					} else {
						p = output_run(p, prev, run);
						run = 1;
					}
					prev = delta;
				}
			}

			p = output_run(p, prev, run);

			write(vd->fd, outbuf, (p - outbuf) * 4);

			free(pixel_data);
		}
	}

	if (vidcapGetDrawIndicator (screen->display) &&
		((vd->recording && vd->show_dot) ||
			vd->thread_running || vd->done)) {
		glViewport(0, 0, screen->width, screen->height);

		glPushMatrix();

		glTranslatef(-0.5f, -0.5f, -DEFAULT_Z_CAMERA);
		glScalef(1.0f  / screen->width, -1.0f / screen->height, 1.0f);
		glTranslatef(0, -screen->height, 0.0f);

		for (i = 0; i < screen->nOutputDev; i++) {
			int angle;
			double vectorX, vectorY;
			int centerX = outputs[i].region.extents.x2 - 50;
			int centerY = outputs[i].region.extents.y2 - 50;

			if (vd->recording)
				glColor4f(1.0, 0.0, 0.0, 0.5);
			else if (vd->thread_running)
				glColor4f(0.0, 0.5, 0.8, 0.5);
			else if (vd->done)
				glColor4f(0.0, 1.0, 0.0,
					cosf((vd->dot_timer / (float) DONE_MS) * M_PI * 0.5));

			glEnable(GL_BLEND);

			glBegin(GL_TRIANGLE_FAN);
			glVertex2d(centerX, centerY);
			if ((vd->recording && vd->show_dot) || vd->done) {
				for (angle = 0; angle <= 360; angle++)
				{
					vectorX = centerX +
							 (25 * sinf(angle * DEG2RAD));
					vectorY = centerY +
							 (25 * cosf(angle * DEG2RAD));
					glVertex2d (vectorX, vectorY);
				}
			}
			else
			{
				int target_angle = vd->dot_timer / (SPIN_MS / 360.0f);
				if (!target_angle)
					target_angle++;
				if (vd->show_dot) {
					for (angle = target_angle; angle >= 0; angle--) {
						vectorX = centerX +
								 (25 * sinf(angle * DEG2RAD));
						vectorY = centerY -
								 (25 * cosf(angle * DEG2RAD));
						glVertex2d (vectorX, vectorY);
					}
				} else {
					for (angle = 360; angle >= target_angle; angle--) {
						vectorX = centerX +
								 (25 * sinf(angle * DEG2RAD));
						vectorY = centerY -
								 (25 * cosf(angle * DEG2RAD));
						glVertex2d (vectorX, vectorY);
					}
				}
			}
			glEnd();

			glDisable(GL_BLEND);

			glColor4usv(defaultColor);
		}

		glPopMatrix ();
	}
}

static inline int
rgb_to_yuv(uint32_t format, uint32_t p, int *u, int *v)
{
	int r, g, b, y;

	r = (p >> 0) & 0xff;
	g = (p >> 8) & 0xff;
	b = (p >> 16) & 0xff;

	y = (19595 * r + 38469 * g + 7472 * b) >> 16;
	if (y > 255)
		y = 255;

	*u += 46727 * (r - y);
	*v += 36962 * (b - y);

	return y;
}

static inline
int clamp_uv(int u)
{
	int clamp = (u >> 18) + 128;

	if (clamp < 0)
		return 0;
	else if (clamp > 255)
		return 255;
	else
		return clamp;
}

static void
convert_to_yv12(struct wcap_decoder *decoder, unsigned char *out)
{
	unsigned char *y1, *y2, *u, *v;
	uint32_t *p1, *p2, *end;
	int i, u_accum, v_accum, stride0, stride1;
	uint32_t format = decoder->format;

	stride0 = decoder->width;
	stride1 = decoder->width / 2;
	for (i = 0; i < decoder->height; i += 2) {
		y1 = out + stride0 * i;
		y2 = y1 + stride0;
		v = out + stride0 * decoder->height + stride1 * i / 2;
		u = v + stride1 * decoder->height / 2;
		p1 = decoder->frame + decoder->width * i;
		p2 = p1 + decoder->width;
		end = p1 + decoder->width;

		while (p1 < end) {
			u_accum = 0;
			v_accum = 0;
			y1[0] = rgb_to_yuv(format, p1[0], &u_accum, &v_accum);
			y1[1] = rgb_to_yuv(format, p1[1], &u_accum, &v_accum);
			y2[0] = rgb_to_yuv(format, p2[0], &u_accum, &v_accum);
			y2[1] = rgb_to_yuv(format, p2[1], &u_accum, &v_accum);
			u[0] = clamp_uv(u_accum);
			v[0] = clamp_uv(v_accum);

			y1 += 2;
			p1 += 2;
			y2 += 2;
			p2 += 2;
			u++;
			v++;
		}
	}
}

static void
output_yuv_frame(struct wcap_decoder *decoder, FILE *f)
{
	unsigned char *out;
	int size;

	size = decoder->width * decoder->height * 3 / 2;

	out = malloc(size);

	convert_to_yv12(decoder, out);

	fwrite("FRAME\n", 1, 6, f);
	fwrite(out, 1, size, f);
	free (out);
}

static void
write_file(int fd)
{
	struct wcap_decoder *decoder = wcap_decoder_create(WCAPFILE);
	int i, has_frame;
	int num = 30, denom = 1;
	uint32_t msecs, frame_time;
	FILE *f;
	char *header;
	int size;

	compLogMessage("vidcap", CompLogLevelInfo, "Decoding");

	size = asprintf (&header, "YUV4MPEG2 C420jpeg W%d H%d F%d:%d Ip A0:0\n",
							decoder->width, decoder->height, num, denom);
	f = fdopen(fd, "w");
	fwrite(header, 1, size, f);
	free (header);

	i = 0;
	has_frame = wcap_decoder_get_frame(decoder);
	msecs = decoder->msecs;
	frame_time = 1000 * denom / num;
	while (has_frame) {
		output_yuv_frame(decoder, f);
		if ((i % 5) == 0)
		{
			printf(" .");
			fflush (stdout);
		}
		i++;
		msecs += frame_time;
		while (decoder->msecs < msecs && has_frame)
			has_frame = wcap_decoder_get_frame(decoder);
	}
	printf("\n");

	fclose(f);

	compLogMessage("vidcap", CompLogLevelInfo,
		"wcap file: size %dx%d, %d frames\n",
		decoder->width, decoder->height, i);

	wcap_decoder_destroy(decoder);
}

static void *
thread_func(void *data)
{
	CompDisplay *d = (CompDisplay *) data;
	int fd;
	DIR *dir;
	struct dirent *file;
	struct stat st;
	char *directory, *command, *tmpcmd, *fullpath;
	char filename[256], ext[32];
	int i, j, ret, found;

	fd = open(RAWFILE, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);

	write_file(fd);

	close(fd);

	if (stat(vidcapGetDirectory (d), &st) == 0 && S_ISDIR(st.st_mode) &&
			access(vidcapGetDirectory (d), W_OK) == 0) {
		directory = strdup (vidcapGetDirectory (d));
	} else {
		compLogMessage("vidcap", CompLogLevelWarn,
			"Could not stat '%s' or not a writable directory, "
										"defaulting to /tmp\n",
			vidcapGetDirectory (d));
		directory = strdup ("/tmp");
	}

	tmpcmd = strdup (vidcapGetCommand (d));
	found = 0;

	for (i = 0; i < strlen(tmpcmd); i++) {
		if (!strncmp(&tmpcmd[i], "%f.", 3)) {
			found = 1;
			for (j = i + 3; strncmp(&tmpcmd[j], " ", 1) &&
							strncmp(&tmpcmd[j], "\0", 1); j++);
			j = j - (i + 3);
			strncpy(ext, &tmpcmd[i + 3], j);
			strncpy(&ext[j+1], "\0", 1);
			break;
		}
	}
	free(tmpcmd);

	if (!found)
		strcpy (ext, "mp4");

	i = 0;
	sprintf(filename, "vidcap-%02d.%s", i, ext);
	while ((dir = opendir(directory)) != NULL) {
		found = 0;
		while ((file = readdir(dir)) != 0)
		{
			if (!strcmp(file->d_name, filename))
			{
				found = 1;
				break;
			}
		}
		if (found) {
			i++;
			sprintf(filename, "vidcap-%02d.%s", i, ext);
		} else {
			closedir (dir);
			break;
		}
		closedir (dir);
	}
	if (asprintf (&fullpath, "%s/%s", directory, filename) <= 0)
		fullpath = strdup ("/tmp/vidcap.mp4");

	tmpcmd = strdup(vidcapGetCommand (d));
	ret = found = 0;

	for (i = 0; i < strlen (tmpcmd); i++) {
		if (!strncmp (&tmpcmd[i], "%f.", 3)) {
			found = 1;
			for (j = i + 3; strncmp(&tmpcmd[j], " ", 1) &&
							strncmp(&tmpcmd[j], "\0", 1); j++);
			j = j - (i + 3);
			tmpcmd[i] = '\0';
			ret = asprintf(&command, "cat %s | %s%s%s",
						RAWFILE, tmpcmd, fullpath, &tmpcmd[i+3+j]);
			break;
		}
	}

	if (!found)
		ret = asprintf(&command, "cat %s | avconv -i - %s", RAWFILE, fullpath);

	if (ret > 0)
		system(command);

	compLogMessage("vidcap", CompLogLevelInfo, "Created: %s\n", fullpath);

	remove(RAWFILE);
	remove(WCAPFILE);

	free (tmpcmd);
	free (command);
	free (fullpath);
	free (directory);

	VIDCAP_DISPLAY (d);

	vd->thread_running = FALSE;
	vd->done = TRUE;
	vd->dot_timer = 0;

	return NULL;
}

static Bool
vidcapToggle(CompDisplay     *d,
				CompAction      *action,
				CompActionState state,
				CompOption      *option,
				int             nOption)
{
	VIDCAP_DISPLAY (d);
	struct wcap_header header;

	if (vd->thread_running)
	{
		vd->recording = FALSE;
		compLogMessage("vidcap", CompLogLevelInfo, "Processing, please wait");
		return TRUE;
	}

	vd->recording = !vd->recording;

	if (vd->recording)
	{
		compLogMessage("vidcap", CompLogLevelInfo, "Recording started");
		vd->frame = malloc (d->screens->width * d->screens->height * 4);
		if (!vd->frame)
		{
			vd->recording = FALSE;
			return TRUE;
		}
		memset(vd->frame, 0, d->screens->width * d->screens->height * 4);
		vd->ms = 0;

		header.magic = WCAP_HEADER_MAGIC;
		header.format = WCAP_FORMAT_XBGR8888;
		header.width = d->screens->width;
		header.height = d->screens->height;

		vd->fd = open(WCAPFILE, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);

		write(vd->fd, &header, sizeof header);

		vd->dot_timer = 0;
		vd->done = FALSE;
	}
	else
	{
		free (vd->frame);
		close (vd->fd);
		vd->dot_timer = 0;
		vd->thread_running = TRUE;
		pthread_create(&vd->thread, NULL, thread_func, d);
		compLogMessage("vidcap", CompLogLevelInfo, "Recording stopped");
	}

	return TRUE;
}

static Bool
vidcapInitDisplay(CompPlugin *p,
					CompDisplay *d)
{
	VidcapDisplay * vd;

	if (!checkPluginABI ("core", CORE_ABIVERSION))
		return FALSE;

	vd = malloc(sizeof (VidcapDisplay));
	if (!vd)
		return FALSE;

	vd->screenPrivateIndex = allocateScreenPrivateIndex (d);
	if (vd->screenPrivateIndex < 0)
	{
		free(vd);
		return FALSE;
	}

	vd->done = FALSE;
	vd->recording = FALSE;
	vd->thread_running = FALSE;

    vidcapSetToggleRecordInitiate(d, vidcapToggle);

    d->base.privates[VidcapDisplayPrivateIndex].ptr = vd;

    return TRUE;

}

static void
vidcapFiniDisplay(CompPlugin *p,
					CompDisplay *d)
{
	VIDCAP_DISPLAY (d);

	freeScreenPrivateIndex(d, vd->screenPrivateIndex);

	free (vd);
}

static Bool
vidcapInitScreen(CompPlugin *p,
				 CompScreen *s)
{
	VidcapScreen *vs;

    VIDCAP_DISPLAY (s->display);

	vs = malloc(sizeof (VidcapScreen));
	if (!vs)
		return FALSE;

    s->base.privates[vd->screenPrivateIndex].ptr = vs;

	WRAP (vs, s, preparePaintScreen, vidcapPreparePaintScreen);
	WRAP (vs, s, donePaintScreen, vidcapDonePaintScreen);
	WRAP (vs, s, paintScreen, vidcapPaintScreen);

	return TRUE;

}

static void
vidcapFiniScreen(CompPlugin *p,
					CompScreen *s)
{
    VIDCAP_SCREEN (s);

    UNWRAP (vs, s, preparePaintScreen);
    UNWRAP (vs, s, donePaintScreen);
    UNWRAP (vs, s, paintScreen);

    free (vs);
}


static CompBool
vidcapInitObject(CompPlugin *p,
					CompObject *o)
{
	static InitPluginObjectProc dispTab[] = {
		(InitPluginObjectProc) 0, /* InitCore */
		(InitPluginObjectProc) vidcapInitDisplay,
		(InitPluginObjectProc) vidcapInitScreen
	};

	RETURN_DISPATCH (o, dispTab, ARRAY_SIZE (dispTab), TRUE, (p, o));
}

static void
vidcapFiniObject(CompPlugin *p,
					CompObject *o)
{
	static FiniPluginObjectProc dispTab[] = {
		(FiniPluginObjectProc) 0, /* FiniCore */
		(FiniPluginObjectProc) vidcapFiniDisplay,
		(FiniPluginObjectProc) vidcapFiniScreen
	};

	DISPATCH (o, dispTab, ARRAY_SIZE (dispTab), (p, o));
}

static Bool
vidcapInit(CompPlugin *p)
{
	VidcapDisplayPrivateIndex = allocateDisplayPrivateIndex ();

	if (VidcapDisplayPrivateIndex < 0)
		return FALSE;

	return TRUE;
}

static void
vidcapFini(CompPlugin *p)
{
	freeDisplayPrivateIndex (VidcapDisplayPrivateIndex);
}

static CompPluginVTable vidcapVTable=
{
    "vidcap",
    0,
    vidcapInit,
    vidcapFini,
    vidcapInitObject,
    vidcapFiniObject,
    0,
    0
};

CompPluginVTable* getCompPluginInfo (void)
{
    return &vidcapVTable;
}
