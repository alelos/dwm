/* See LICENSE file for copyright and license details. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <X11/Xlib.h>
#include <X11/Xft/Xft.h>

#include "drw.h"
#include "util.h"

#define UTF_INVALID 0xFFFD
#define UTF_SIZ 4

static const unsigned char utfbyte[UTF_SIZ + 1] = {0x80,    0, 0xC0, 0xE0, 0xF0};
static const unsigned char utfmask[UTF_SIZ + 1] = {0xC0, 0x80, 0xE0, 0xF0, 0xF8};
static const long utfmin[UTF_SIZ + 1] = {       0,    0,  0x80,  0x800,  0x10000};
static const long utfmax[UTF_SIZ + 1] = {0x10FFFF, 0x7F, 0x7FF, 0xFFFF, 0x10FFFF};

static long
utf8decodebyte(const char c, size_t *i) {
	for(*i = 0; *i < (UTF_SIZ + 1); ++(*i))
		if(((unsigned char)c & utfmask[*i]) == utfbyte[*i])
			return (unsigned char)c & ~utfmask[*i];
	return 0;
}

static size_t
utf8validate(long *u, size_t i) {
	if(!BETWEEN(*u, utfmin[i], utfmax[i]) || BETWEEN(*u, 0xD800, 0xDFFF))
		*u = UTF_INVALID;
	for(i = 1; *u > utfmax[i]; ++i)
		;
	return i;
}

static size_t
utf8decode(const char *c, long *u, size_t clen) {
	size_t i, j, len, type;
	long udecoded;

	*u = UTF_INVALID;
	if(!clen)
		return 0;
	udecoded = utf8decodebyte(c[0], &len);
	if(!BETWEEN(len, 1, UTF_SIZ))
		return 1;
	for(i = 1, j = 1; i < clen && j < len; ++i, ++j) {
		udecoded = (udecoded << 6) | utf8decodebyte(c[i], &type);
		if(type != 0)
			return j;
	}
	if(j < len)
		return 0;
	*u = udecoded;
	utf8validate(u, len);
	return len;
}

Drw *
drw_create(Display *dpy, int screen, Window root, unsigned int w, unsigned int h) {
	Drw *drw = (Drw *)calloc(1, sizeof(Drw));
	if(!drw)
		return NULL;
	drw->dpy = dpy;
	drw->screen = screen;
	drw->root = root;
	drw->w = w;
	drw->h = h;
	drw->drawable = XCreatePixmap(dpy, root, w, h, DefaultDepth(dpy, screen));
	drw->gc = XCreateGC(dpy, root, 0, NULL);
	drw->fontcount = 0;
	XSetLineAttributes(dpy, drw->gc, 1, LineSolid, CapButt, JoinMiter);
	return drw;
}

void
drw_resize(Drw *drw, unsigned int w, unsigned int h) {
	if(!drw)
		return;
	drw->w = w;
	drw->h = h;
	if(drw->drawable != 0)
		XFreePixmap(drw->dpy, drw->drawable);
	drw->drawable = XCreatePixmap(drw->dpy, drw->root, w, h, DefaultDepth(drw->dpy, drw->screen));
}

void
drw_free(Drw *drw) {
	size_t i;
	for (i = 0; i < drw->fontcount; i++) {
		drw_font_free(drw->fonts[i]);
	}
	XFreePixmap(drw->dpy, drw->drawable);
	XFreeGC(drw->dpy, drw->gc);
	free(drw);
}

/* This function is an implementation detail. Library users should use
 * drw_font_create instead.
 */
static Fnt *
drw_font_xcreate(Drw *drw, const char *fontname, FcPattern *fontpattern) {
	Fnt *font;

	if (!(fontname || fontpattern))
		die("No font specified.\n");

	if (!(font = (Fnt *)calloc(1, sizeof(Fnt))))
		return NULL;

	if (fontname) {
		/* Using the pattern found at font->xfont->pattern does not yield same
		 * the same substitution results as using the pattern returned by
		 * FcNameParse; using the latter results in the desired fallback
		 * behaviour whereas the former just results in
		 * missing-character-rectangles being drawn, at least with some fonts.
		 */
		if (!(font->xfont = XftFontOpenName(drw->dpy, drw->screen, fontname)) ||
		    !(font->pattern = FcNameParse((FcChar8 *) fontname))) {
			if (font->xfont) {
				XftFontClose(drw->dpy, font->xfont);
				font->xfont = NULL;
			}
			fprintf(stderr, "error, cannot load font: '%s'\n", fontname);
		}
	} else if (fontpattern) {
		if (!(font->xfont = XftFontOpenPattern(drw->dpy, fontpattern))) {
			fprintf(stderr, "error, cannot load font pattern.\n");
		} else {
			font->pattern = NULL;
		}
	}

	if (!font->xfont) {
		free(font);
		return NULL;
	}

	font->ascent = font->xfont->ascent;
	font->descent = font->xfont->descent;
	font->h = font->ascent + font->descent;
	font->dpy = drw->dpy;
	return font;
}

Fnt*
drw_font_create(Drw *drw, const char *fontname) {
	return drw_font_xcreate(drw, fontname, NULL);
}

void
drw_load_fonts(Drw* drw, const char *fonts[], size_t fontcount) {
	size_t i;
	Fnt *font;
	for (i = 0; i < fontcount; i++) {
		if (drw->fontcount >= DRW_FONT_CACHE_SIZE) {
			die("Font cache exhausted.\n");
		} else if ((font = drw_font_xcreate(drw, fonts[i], NULL))) {
			drw->fonts[drw->fontcount++] = font;
		}
	}
}

void
drw_font_free(Fnt *font) {
	if(!font)
		return;
	if(font->pattern)
		FcPatternDestroy(font->pattern);
	XftFontClose(font->dpy, font->xfont);
	free(font);
}

Clr *
drw_clr_create(Drw *drw, const char *clrname) {
	Clr *clr;
	Colormap cmap;
	Visual *vis;

	if(!drw)
		return NULL;
	clr = (Clr *)calloc(1, sizeof(Clr));
	if(!clr)
		return NULL;
	cmap = DefaultColormap(drw->dpy, drw->screen);
	vis = DefaultVisual(drw->dpy, drw->screen);
	if(!XftColorAllocName(drw->dpy, vis, cmap, clrname, &clr->rgb))
		die("error, cannot allocate color '%s'\n", clrname);
	clr->pix = clr->rgb.pixel;
	return clr;
}

void
drw_clr_free(Clr *clr) {
	if(clr)
		free(clr);
}

void
drw_setscheme(Drw *drw, ClrScheme *scheme) {
	if(drw && scheme)
		drw->scheme = scheme;
}

int
drw_colored_text(Drw *drw, ClrScheme *scheme, int numcolors, int x, int y, unsigned int w, unsigned int h, char *text) {
	if(!drw || !drw->fontcount || !drw->scheme)
		return 0;

	char *buf = text, *ptr = buf, c =1;
	int i;

	while(*ptr) {
		for(i = 0; *ptr < 0 || *ptr > numcolors; i++, ptr++);
		if(!*ptr)
			break;
		c = *ptr;
		*ptr = 0;
		if(i) {
			x = drw_text(drw, x, y, w, h, buf, 0);
		}
		*ptr = c;
		drw_setscheme(drw, &scheme[c-1]);
		buf = ++ptr;
	}
	drw_text(drw, x, y, w, h, buf, 0);
	return x;
}

void
drw_rect(Drw *drw, int x, int y, unsigned int w, unsigned int h, int filled, int empty) {
	int dx;

	if(!drw || !drw->fontcount || !drw->scheme)
		return;
	XSetForeground(drw->dpy, drw->gc, drw->scheme->fg->pix);
	dx = (drw->fonts[0]->ascent + drw->fonts[0]->descent + 2) / 4;
	if(filled)
		XFillRectangle(drw->dpy, drw->drawable, drw->gc, x+1, y+1, dx+1, dx+1);
	else if(empty)
		XDrawRectangle(drw->dpy, drw->drawable, drw->gc, x+1, y+1, dx, dx);
}

int
drw_text(Drw *drw, int x, int y, unsigned int w, unsigned int h, const char *text, int pad) {
	char buf[1024];
	int tx, ty, th;
	Extnts tex;
	Colormap cmap;
	Visual *vis;
	XftDraw *d;
	Fnt *curfont, *nextfont;
	size_t i, len;
	int utf8strlen, utf8charlen, render;
	long utf8codepoint = 0;
	const char *utf8str;
	FcCharSet *fccharset;
	FcPattern *fcpattern;
	FcPattern *match;
	XftResult result;
	int charexists = 0;

	if (!(render = x || y || w || h)) {
		w = ~w;
	}

	if (!drw || !drw->scheme) {
		return 0;
	} else if (render) {
		XSetForeground(drw->dpy, drw->gc, drw->scheme->bg->pix);
		XFillRectangle(drw->dpy, drw->drawable, drw->gc, x, y, w, h);
	}

	if (!text || !drw->fontcount) {
		return 0;
	} else if (render) {
		cmap = DefaultColormap(drw->dpy, drw->screen);
		vis = DefaultVisual(drw->dpy, drw->screen);
		d = XftDrawCreate(drw->dpy, drw->drawable, vis, cmap);
	}

	curfont = drw->fonts[0];
	while (1) {
		utf8strlen = 0;
		utf8str = text;
		nextfont = NULL;
		while (*text) {
			utf8charlen = utf8decode(text, &utf8codepoint, UTF_SIZ);
			for (i = 0; i < drw->fontcount; i++) {
				charexists = charexists || XftCharExists(drw->dpy, drw->fonts[i]->xfont, utf8codepoint);
				if (charexists) {
					if (drw->fonts[i] == curfont) {
						utf8strlen += utf8charlen;
						text += utf8charlen;
					} else {
						nextfont = drw->fonts[i];
					}
					break;
				}
			}

			if (!charexists || (nextfont && nextfont != curfont)) {
				break;
			} else {
				charexists = 0;
			}
		}

		if (utf8strlen) {
			drw_font_getexts(curfont, utf8str, utf8strlen, &tex);
			/* shorten text if necessary */
			for(len = MIN(utf8strlen, (sizeof buf) - 1); len && (tex.w > w - drw->fonts[0]->h || w < drw->fonts[0]->h); len--)
				drw_font_getexts(curfont, utf8str, len, &tex);

			if (len) {
				memcpy(buf, utf8str, len);
				buf[len] = '\0';
				if(len < utf8strlen)
					for(i = len; i && i > len - 3; buf[--i] = '.');

				if (render) {
					th = pad ? (curfont->ascent + curfont->descent) : 0;
					ty = y + ((h + curfont->ascent - curfont->descent) / 2);
					tx = x + (h / 2);
					XftDrawStringUtf8(d, &drw->scheme->fg->rgb, curfont->xfont, tx, ty, (XftChar8 *)buf, len);
				}

				x += tex.w;
				w -= tex.w;
			}
		}

		if (!*text) {
			break;
		} else if (nextfont) {
			charexists = 0;
			curfont = nextfont;
		} else {
			/* Regardless of whether or not a fallback font is found, the
			 * character must be drawn.
			 */
			charexists = 1;

			if (drw->fontcount >= DRW_FONT_CACHE_SIZE) {
				continue;
			}

			fccharset = FcCharSetCreate();
			FcCharSetAddChar(fccharset, utf8codepoint);

			if (!drw->fonts[0]->pattern) {
				/* Refer to the comment in drw_font_xcreate for more
				 * information.
				 */
				die("The first font in the cache must be loaded from a font string.\n");
			}

			fcpattern = FcPatternDuplicate(drw->fonts[0]->pattern);
			FcPatternAddCharSet(fcpattern, FC_CHARSET, fccharset);
			FcPatternAddBool(fcpattern, FC_SCALABLE, FcTrue);

			FcConfigSubstitute(NULL, fcpattern, FcMatchPattern);
			FcDefaultSubstitute(fcpattern);
			match = XftFontMatch(drw->dpy, drw->screen, fcpattern, &result);

			FcCharSetDestroy(fccharset);
			FcPatternDestroy(fcpattern);

			if (match) {
				curfont = drw_font_xcreate(drw, NULL, match);
				if (curfont && XftCharExists(drw->dpy, curfont->xfont, utf8codepoint)) {
					drw->fonts[drw->fontcount++] = curfont;
				} else {
					if (curfont) {
						drw_font_free(curfont);
					}
					curfont = drw->fonts[0];
				}
			}
		}
	}

	if (render) {
		XftDrawDestroy(d);
	}

	return x;
}

void
drw_map(Drw *drw, Window win, int x, int y, unsigned int w, unsigned int h) {
	if(!drw)
		return;
	XCopyArea(drw->dpy, drw->drawable, win, drw->gc, x, y, w, h, x, y);
	XSync(drw->dpy, False);
}


void
drw_font_getexts(Fnt *font, const char *text, unsigned int len, Extnts *tex) {
	XGlyphInfo ext;

	if(!font || !text)
		return;
	XftTextExtentsUtf8(font->dpy, font->xfont, (XftChar8 *)text, len, &ext);
	tex->h = font->h;
	tex->w = ext.xOff;
}

unsigned int
drw_font_getexts_width(Fnt *font, const char *text, unsigned int len) {
	Extnts tex;

	if(!font)
		return -1;
	drw_font_getexts(font, text, len, &tex);
	return tex.w;
}

Cur *
drw_cur_create(Drw *drw, int shape) {
	Cur *cur = (Cur *)calloc(1, sizeof(Cur));

	if(!drw || !cur)
		return NULL;
	cur->cursor = XCreateFontCursor(drw->dpy, shape);
	return cur;
}

void
drw_cur_free(Drw *drw, Cur *cursor) {
	if(!drw || !cursor)
		return;
	XFreeCursor(drw->dpy, cursor->cursor);
	free(cursor);
}
