/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Quake III Arena source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/
//
// ui_web.h -- [QL] the front end's pages, drawn with the renderer
//
// Retail draws everything outside the match in an embedded browser reading
// web.pak. The pages this build has are drawn here instead. What they offer and in
// what order is retail's; how they look is quakeclub.com's, in its dark theme,
// measured off the site: its colour tokens, its components (pill buttons and tabs,
// 10px cards, a 32x18 switch, a 6px slider track), and nothing moves.
//
// Lengths are CSS pixels of a 1920x1080 page; the page zooms by the window height
// / 1080. ui_startmatch.c was the first page and still names it all SM_.
//

#ifndef __UI_WEB_H
#define __UI_WEB_H

#include "ui_local.h"

#define SM_MAX_HITS 384
#define SM_MAX_LINES 8

#define SM_VIEW_H 1080.0f      // the page zooms by innerHeight / 1080
#define SM_MIN_VIEW_W 1500.0f  // narrower screens scale it down to keep it whole

// A CSS font-size is the em, and fontstash sizes a font by ascender minus
// descender. These are that height per em and, for the baseline,
// (ascender - (ascender - descender) / 2) per em: a line box lh high has its
// baseline lh / 2 + size * that below its top.
#define SM_SANS_EM (2789.0f / 2048.0f)     // pak00's Noto Sans
#define SM_SANS_BASE (794.5f / 2048.0f)
#define SM_HANDEL_EM (2289.0f / 2048.0f)   // pak00's Handel Gothic
#define SM_HANDEL_BASE (694.5f / 2048.0f)
#define SM_BARLOW_EM (1200.0f / 1000.0f)   // pak01's Barlow
#define SM_BARLOW_BASE (400.0f / 1000.0f)
#define SM_WEB_HANDEL_EM (2274.0f / 2048.0f)  // web.pak's Handel Gothic webfonts, medium
#define SM_WEB_HANDEL_BASE (704.0f / 2048.0f)
#define SM_WEB_HANDEL_BOLD_EM (2289.0f / 2048.0f)  // and bold
#define SM_WEB_HANDEL_BOLD_BASE (694.5f / 2048.0f)
#define SM_AWESOME_EM 1.0f                 // web.pak's FontAwesome
#define SM_AWESOME_BASE (640.0f / 1792.0f)

// FontAwesome's glyphs that retail's pages use
#define SM_FA_HOME 0xf015
#define SM_FA_GEAR 0xf013
#define SM_FA_POWER 0xf011
#define SM_FA_FILTER 0xf0b0
#define SM_FA_CARET_UP 0xf0d8
#define SM_FA_CARET_DOWN 0xf0d7
#define SM_FA_LOCK 0xf023
#define SM_FA_PLAY 0xf04b
#define SM_FA_STAR 0xf005
#define SM_FA_SIGNAL 0xf012

#define SM_RGB(r, g, b) {(r) / 255.0f, (g) / 255.0f, (b) / 255.0f, 1.0f}

/*
The colours are Quake Live's own, off the web UI's stylesheet (web.pak
css/main.css): its page is black, a panel rgba(0,0,0,0.8) over the background, a
field rgba(15,15,15) with a #333 border, text #DDD over #CCC, #999 and #666, and
#f7d100 -- the yellow of the QUAKE LIVE logo -- for whatever is chosen or under the
cursor. The names are the ones Quake Club's palette gave them.
*/
extern const vec4_t smPage;        // --background
extern const vec4_t smPanel;       // --qc-bg-dark, every card and panel
extern const vec4_t smRaised;      // --card / --qc-bg-medium: pills, fields, rows under the cursor
extern const vec4_t smRaisedHover; // --qc-bg-elevated
extern const vec4_t smBorder;      // --border
extern const vec4_t smField;       // --input
extern const vec4_t smText;        // --foreground
extern const vec4_t smTextSoft;    // --qc-text-secondary
extern const vec4_t smMuted;       // --muted-foreground
extern const vec4_t smSubtle;      // --qc-text-subtle
extern const vec4_t smPrimary;     // --primary, the cream of a chosen pill
extern const vec4_t smOnPrimary;   // --primary-foreground
extern const vec4_t smGold;        // --qc-chip-border, the accent
extern const vec4_t smRing;        // --ring, a focused field
extern const vec4_t smDanger;      // --destructive
extern const vec4_t smSuccess;     // --qc-success, the in-game banner
extern const vec4_t smSuccessHover;
extern const vec4_t smShadow;

// the names Start Match was written against, kept pointing at the same palette
#define smYellow smGold
#define smDim smPanel
#define smCard smRaised
#define smCardHover smRaisedHover
#define smRowOdd smPanel
#define smRowEven smPanel
#define smInput smField
#define smGrey smBorder
#define smFocus smRing
#define smLighter smRaisedHover
#define smSelectText smTextSoft
#define smOptionText smMuted
#define smDark smPage
#define smArrow smMuted
#define smLink smGold
#define smLinkHover smText
#define smButton smRaised
#define smButtonHover smRaisedHover
#define smButtonBorder smBorder
#define smDialog smRaised  // over a page: it has to hide what is under it
#define smOnAccent smOnPrimary

typedef struct {
    float x, y, w, h;
} smRect_t;

// the frame, set by SM_Frame once a frame
extern float smScale;     // screen pixels per CSS pixel
extern float smTop;       // the screen y of the frame's top
extern float smVW;        // the frame's width in CSS pixels
extern float smBottom;    // the screen's bottom edge in CSS pixels
extern float smMX, smMY;  // the cursor in the frame

void SM_Frame(void);

// [ioquakelive] a page looks like retail's web UI, with its fonts and square corners; the settings
// page keeps its own type and corners. Every page starts as retail's
typedef enum {
    SM_LOOK_RETAIL,
    SM_LOOK_OURS
} smLook_t;

extern smLook_t smLook;
void SM_SetLook(smLook_t look);

// a FontAwesome glyph out of web.pak, and its width; nothing when web.pak is not there
void SM_Icon(float x, float baseline, float size, const float* color, int codepoint);
float SM_IconWidth(float size, int codepoint);

smRect_t SM_R(float x, float y, float w, float h);
qboolean SM_In(const smRect_t* r, float x, float y);
qboolean SM_Clip(smRect_t* r, const smRect_t* clip);

// Quake Club's radii: a field, a card, a big panel
#define SM_RADIUS_SM 6.0f
#define SM_RADIUS_MD 10.0f
#define SM_RADIUS_LG 14.0f

void SM_Fill(float x, float y, float w, float h, const float* color, const smRect_t* clip);
void SM_Round(float x, float y, float w, float h, float r, const float* color, const smRect_t* clip);
void SM_Pill(float x, float y, float w, float h, const float* color, const smRect_t* clip);
void SM_Shadow(float x, float y, float w, float h, float r);
void SM_FillScreen(const float* color);
void SM_Pic(float x, float y, float w, float h, qhandle_t shader, const smRect_t* clip);
void SM_PicColor(float x, float y, float w, float h, qhandle_t shader, const float* color, const smRect_t* clip);
void SM_Arrow(float x, float y, qboolean up);
void SM_Chevron(float x, float y, qboolean up, const float* color);
void SM_Cross(float x, float y, float size, const float* color);
void SM_To640(float x, float y, float w, float h, float* out);

// player models drawn into a box of the page
void SM_DrawPlayerIn(playerInfo_t* pi, float x, float y, float w, float h);
void SM_TurnPlayer(playerInfo_t* pi, float yaw);
void SM_BuildPlayer(playerInfo_t* pi, const char* composed, float yaw, int weapon);

/*
Type. The web UI sets its headings in Handel Gothic and its body in Noto Sans
(web.pak css/main.css @font-face), and pak00 carries both. SM_FONT_TITLE is the
first, which every label and heading takes; SM_FONT_TEXT is the second, for
everything that is read rather than looked at.
*/
#define SM_FONT_TEXT FONT_SANS    // Noto Sans, the body
#define SM_FONT_TITLE FONT_DEFAULT  // Handel Gothic, the headings and labels
#define SM_FONT_STRONG FONT_DEFAULT

void SM_LoadFonts(void);
float SM_Em(int font);
float SM_Baseline(int font, float size, float lineTop, float lineHeight);
qboolean SM_TextInside(float baseline, float size, const smRect_t* clip);
float SM_Advance(int font, float size, const char* text, int len);
float SM_AdvanceBold(int font, float size, const char* text, int len, qboolean bold);
int SM_Fit(int font, float size, const char* text, float width);
void SM_Text(float x, float baseline, int font, float size, const float* color, const char* text, int limit,
             qboolean bold);
void SM_Centered(float middle, float baseline, float size, const float* color, const char* text, qboolean bold);
// [QL] the italic of web.pak's Noto Sans, for a page's em: pass it as the font
int SM_ItalicFace(void);
// [QL] text as a web page prints it: a ^ and what follows are characters, not a colour
float SM_AdvanceRaw(int font, float size, const char* text, int len, qboolean bold);
void SM_TextRaw(float x, float baseline, int font, float size, const float* color, const char* text, int limit,
                qboolean bold);
float SM_SpacedWidth(int font, float size, const char* text, float spacing);
void SM_SpacedText(float x, float baseline, int font, float size, const float* color, const char* text, float spacing,
                   qboolean bold);
int SM_Wrap(int font, float size, const char* text, float width, int* starts, int* lens);

// Quake Club's label: bold, upper case, letter-spaced. Returns its width.
float SM_LabelWidth(float size, const char* text, float spacing);
void SM_Label(float x, float baseline, float size, const float* color, const char* text, float spacing);

/*
===============================================================================

COMPONENTS

The few pieces every page is built out of, drawn the way Quake Club's are.

===============================================================================
*/

typedef enum {
    SM_BUTTON_PRIMARY,    // cream, dark text: the one thing the page wants you to do
    SM_BUTTON_SECONDARY,  // a raised pill, muted text
    SM_BUTTON_DANGER
} smButtonStyle_t;

// the height of a pill button; small ones are the filter pills of a panel header
#define SM_BUTTON_H 36.0f
#define SM_PILL_H 28.0f

float SM_ButtonWidth(const char* text, float height);
// [QL] skeleton's button, the one retail's pages use, 30 pixels high
#define SM_WEB_BUTTON_H 30.0f
float SM_WebButtonWidth(const char* label);
void SM_WebButton(float x, float y, float w, const char* label, qboolean disabled, qboolean over,
                  const smRect_t* clip);
void SM_Border(float x, float y, float w, float h, float t, const float* color, const smRect_t* clip);
void SM_Button(float x, float y, float w, float h, const char* text, int style, qboolean over);
void SM_Switch(float x, float y, qboolean on, qboolean over);
#define SM_SWITCH_W 34.0f
#define SM_SWITCH_H 20.0f
void SM_SliderTrack(float x, float y, float w, float frac, qboolean over);
#define SM_SLIDER_H 16.0f
void SM_FieldBox(float x, float y, float w, float h, qboolean focused, qboolean over, const smRect_t* clip);
float SM_Kbd(float x, float y, const char* text, qboolean waiting, const smRect_t* clip);

/*
===============================================================================

WHAT THE CURSOR IS OVER

Each frame lists what takes the mouse, in the order it is drawn; the last one
under the cursor is on top. Clicks and hovering use the list of the frame
before, which is what was on screen. The type is each page's own enum.

===============================================================================
*/

typedef struct {
    smRect_t r;
    int type;
    int index;
} smHit_t;

// Every page numbers its own hit types from 0. What more than one page draws -- the
// front end's bar, its cards and the in-game banner -- numbers them from here, so a
// page's type and the bar's can never be the same number. When they were, a click
// on the first demo of Replays was taken for a click on Play.
#define SM_HIT_SHARED 1000

extern smHit_t smHits[SM_MAX_HITS], smLastHits[SM_MAX_HITS];
extern int smNumHits, smNumLastHits;
extern smHit_t smUnder;
extern qboolean smHasUnder;

void SM_Hit(float x, float y, float w, float h, int type, int index, const smRect_t* clip);
const smHit_t* SM_HitAt(float x, float y);
qboolean SM_Over(int type, int index);
void SM_BeginFrame(void);
void SM_EndFrame(void);

/*
===============================================================================

THE FRONT END'S FRAME (ui_home.c)

===============================================================================
*/

typedef enum {
    HM_NAV_HOME,
    HM_NAV_PLAY,
    HM_NAV_BROWSER,
    HM_NAV_TRAINING,
    HM_NAV_REPLAYS,
    HM_NAV_STATS,
    HM_NAV_MATCHES,
    HM_NAV_SETTINGS,
    HM_NAV_QUIT,
    HM_NUM_NAV
} hmNav_t;

// [QL] the page's column: retail's .right.col, from 430px to 30px off the right edge, under the
// 75px bar and the 35px below it
#define HM_CONTAINER_W (UI_Home_ContentW())
#define HM_NAV_H 110.0f
#define HM_PANEL_GAP 17.0f

float UI_Home_Left(void);
float UI_Home_ContentW(void);
qboolean UI_Home_InMatch(void);
void UI_Home_Backdrop(void);
void UI_Home_PageTop(int active);
void UI_Home_Banner(void);
qboolean UI_Home_NavClick(void);
void UI_Home_Go(int nav);
float UI_Home_Header(float y, const char* eyebrow, const char* title);
float UI_Home_BlockTop(float blockHeight);
float UI_Home_FrameBottom(void);
qhandle_t UI_Home_Face(void);

// Career Stats (ui_stats.c), and the profile card's wins and KDR
void UI_Stats_Show(const char* steamId);
void UI_Stats_CardFor(int page);
void UI_Stats_DrawCardLine(float x, float lineTop, float width);

#endif  // __UI_WEB_H
