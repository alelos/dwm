/* See LICENSE file for copyright and license details. */
#include <X11/XF86keysym.h>

/* appearance */
static const char *fonts[] = {
    "Terminess Powerline:size=16",
    "Siji:size=17",
};
static const char dmenufont[] = "-*-terminus-medium-r-*-*-16-*-*-*-*-*-*-*";
static const char rofifont[] = "Anonymous Pro 18";
static const char normbordercolor[] = "#444444";
static const char normbgcolor[]     = "#222222";
static const char normfgcolor[]     = "#bbbbbb";
static const char selbordercolor[]  = "#005577";
static const char selbgcolor[]      = "#005577";
static const char selfgcolor[]      = "#eeeeee";
static const unsigned int borderpx  = 3;        /* border pixel of windows */
static const unsigned int snap      = 32;       /* snap pixel */
static const Bool showbar           = True;     /* False means no bar */
static const Bool topbar            = True;     /* False means bottom bar */

/* tagging */
static const char *tags[] = { "\ue010", "\ue011", "\ue012", "\ue013", "\ue014", "6", "7", "8", "9" };

static const Rule rules[] = {
	/* xprop(1):
	 *	WM_CLASS(STRING) = instance, class
	 *	WM_NAME(STRING) = title
	 */
	/* class      instance    title       tags mask     isfloating   monitor */
        { "Termite", NULL, "weechat", 1 << 2, False, -1 },
        { "Firefox",     NULL, NULL,      1 << 1, False, -1 },
        { "qutebrowser", NULL, NULL,      1 << 1, False, -1 },
        { "mpv",         NULL, NULL,      1 << 4, False, -1 },
};

/* layout(s) */
static const float mfact      = 0.60; /* factor of master area size [0.05..0.95] */
static const int nmaster      = 1;    /* number of clients in master area */
static const Bool resizehints = False; /* True means respect size hints in tiled resizals */
static const float popuptermfact = 0.35;

static const Layout layouts[] = {
	/* symbol     arrange function */
	{ "[]=",      tile },    /* first entry is default */
	{ "><>",      NULL },    /* no layout function means floating behavior */
	{ "[M]",      monocle },
};

/* key definitions */
#define MODKEY Mod4Mask
#define TAGKEYS(KEY,TAG) \
	{ MODKEY,                       KEY,      toggleview,     {.ui = 1 << TAG} }, \
	{ MODKEY|ControlMask,           KEY,      view,           {.ui = 1 << TAG} }, \
	{ MODKEY|ShiftMask,             KEY,      tag,            {.ui = 1 << TAG} }, \
	{ MODKEY|ControlMask|ShiftMask, KEY,      toggletag,      {.ui = 1 << TAG} },

/* helper for spawning shell commands in the pre dwm-5.0 fashion */
#define SHCMD(cmd) { .v = (const char*[]){ "/bin/sh", "-c", cmd, NULL } }
#define NUMCOLORS 9
static const char colors[NUMCOLORS][MAXCOLORS][8] = {
    // border foreground background
    { "#484848", "#939393", "#000033" },  // normal
    { "#439dca", "#ee7600", "#000000" },  // selected
    { "#ff0000", "#f3f315", "#000000" },  // urgent/warning  (black on yellow)
    { "#ff0000", "#ffffff", "#ff0000" },  // error (white on red)
    { "#000000", "#006400", "#000033" },  // dark green
    { "#000000", "#990000", "#000033" },  // dark red
    { "#000000", "#ee7600", "#000033" },  // dark orange
    { "#000000", "#939393", "#000033" },  // gray
    { "#000000", "#ffffff", "#000033" },  // white
};

/* commands */
static char dmenumon[2] = "0"; /* component of dmenucmd, manipulated in spawn() */
static const char *dmenucmd[] = { "dmenu_run", "-m", dmenumon, "-fn", dmenufont, "-nb", normbgcolor, "-nf", normfgcolor, "-sb", selbgcolor, "-sf", selfgcolor, NULL };
static const char *termcmd[]  = { "termite", NULL };
static const char *weechatcmd[] = {"termite", "-t", "weechat", "-e", "weechat", NULL };
static const char popuptermname[] = "scratch";
static const char *popuptermcmd[] = {"termite", "-t", popuptermname, NULL };
static const char *volupcmd[] = { "amixer", "sset", "Master", "2+", NULL };
static const char *voldowncmd[] = { "amixer", "sset", "Master", "2-", NULL };
static const char *volmutecmd[] = { "amixer", "sset", "Master", "toggle", NULL };
static const char *brightupcmd[] = { "xbacklight", "-inc", "5", NULL };
static const char *brightdowncmd[] = { "xbacklight", "-dec", "5", NULL };
static const char *passcmd[] = {"passrofi", NULL};
static const char *roficmd[] = {"rofi", "-font", rofifont, "-show", "run", NULL};
static const char *lockcmd[] = {"lock", NULL};


static Key keys[] = {
	/* modifier                     key        function        argument */
	{ MODKEY,                       XK_o,      spawn,          {.v = roficmd } },
        { MODKEY,                       XK_p,      spawn,          {.v = passcmd} },
	{ MODKEY|ShiftMask,             XK_Return, spawn,          {.v = termcmd } },
	{ MODKEY,                       XK_b,      togglebar,      {0} },
	{ MODKEY,                       XK_j,      focusstack,     {.i = +1 } },
        { MODKEY|ShiftMask,             XK_j,      pushdown,       {0} },
        { MODKEY|ShiftMask,             XK_k,      pushup,         {0} },
	{ MODKEY,                       XK_k,      focusstack,     {.i = -1 } },
	{ MODKEY,                       XK_i,      incnmaster,     {.i = +1 } },
	{ MODKEY,                       XK_d,      incnmaster,     {.i = -1 } },
	{ MODKEY,                       XK_h,      setmfact,       {.f = -0.05} },
	{ MODKEY,                       XK_l,      setmfact,       {.f = +0.05} },
	{ MODKEY,                       XK_Return, zoom,           {0} },
	{ MODKEY,                       XK_Tab,    view,           {0} },
	{ MODKEY|ShiftMask,             XK_c,      killclient,     {0} },
	{ MODKEY,                       XK_t,      setlayout,      {.v = &layouts[0]} },
	{ MODKEY,                       XK_f,      setlayout,      {.v = &layouts[1]} },
	{ MODKEY,                       XK_m,      setlayout,      {.v = &layouts[2]} },
	{ MODKEY,                       XK_space,  setlayout,      {0} },
	{ MODKEY|ShiftMask,             XK_space,  togglefloating, {0} },
	{ MODKEY,                       XK_0,      view,           {.ui = ~0 } },
	{ MODKEY|ShiftMask,             XK_0,      tag,            {.ui = ~0 } },
	{ MODKEY,                       XK_comma,  focusmon,       {.i = -1 } },
	{ MODKEY,                       XK_period, focusmon,       {.i = +1 } },
	{ MODKEY|ShiftMask,             XK_comma,  tagmon,         {.i = -1 } },
	{ MODKEY|ShiftMask,             XK_period, tagmon,         {.i = +1 } },
        { MODKEY,                       XK_w,      spawn,          {.v = weechatcmd } },
        { MODKEY,                       XK_grave,  togglepopup,    {.v = popuptermcmd } },
        { MODKEY|ShiftMask,             XK_l,      spawn,          {.v = lockcmd} },
        { 0,                            XF86XK_AudioLowerVolume,   spawn, {.v = voldowncmd} },
        { 0,                            XF86XK_AudioRaiseVolume,   spawn, {.v = volupcmd} },
        { 0,                            XF86XK_AudioMute,          spawn, {.v = volmutecmd} },
        { 0,                            XF86XK_MonBrightnessUp,    spawn, {.v = brightupcmd} },
        { 0,                            XF86XK_MonBrightnessDown,  spawn, {.v = brightdowncmd} },
	TAGKEYS(                        XK_1,                      0)
	TAGKEYS(                        XK_2,                      1)
	TAGKEYS(                        XK_3,                      2)
	TAGKEYS(                        XK_4,                      3)
	TAGKEYS(                        XK_5,                      4)
	TAGKEYS(                        XK_6,                      5)
	TAGKEYS(                        XK_7,                      6)
	TAGKEYS(                        XK_8,                      7)
	TAGKEYS(                        XK_9,                      8)
	{ MODKEY|ShiftMask,             XK_q,      quit,           {0} },
};

/* button definitions */
/* click can be ClkLtSymbol, ClkStatusText, ClkWinTitle, ClkClientWin, or ClkRootWin */
static Button buttons[] = {
	/* click                event mask      button          function        argument */
	{ ClkLtSymbol,          0,              Button1,        setlayout,      {0} },
	{ ClkLtSymbol,          0,              Button3,        setlayout,      {.v = &layouts[2]} },
	{ ClkWinTitle,          0,              Button2,        zoom,           {0} },
	{ ClkStatusText,        0,              Button2,        spawn,          {.v = termcmd } },
	{ ClkClientWin,         MODKEY,         Button1,        movemouse,      {0} },
	{ ClkClientWin,         MODKEY,         Button2,        togglefloating, {0} },
	{ ClkClientWin,         MODKEY,         Button3,        resizemouse,    {0} },
	{ ClkTagBar,            0,              Button1,        toggleview,     {0} },
	{ ClkTagBar,            0,              Button3,        view,           {0} },
	{ ClkTagBar,            MODKEY,         Button1,        tag,            {0} },
	{ ClkTagBar,            MODKEY,         Button3,        toggletag,      {0} },
};

