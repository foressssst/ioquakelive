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
// ui_web.h -- [QL] drawing a page of retail's web UI with the renderer
//
// Retail Quake Live draws everything outside the match in an embedded browser
// reading web.pak. The pages we need are drawn here instead, in the browser's
// own terms: lengths are the CSS pixels the pages were laid out for at 1920x1080
// (they zoom by the window height / 1080), and the colours are their stylesheet's.
//
// This is the part every page shares. ui_startmatch.c was the first of them and
// still names it all SM_; ui_settings.c is the second.
//

#ifndef __UI_WEB_H
#define __UI_WEB_H

#include "ui_local.h"

#define SM_MAX_HITS 256
#define SM_MAX_LINES 8

#define SM_VIEW_H 1080.0f      // the page zooms by innerHeight / 1080
#define SM_MIN_VIEW_W 1500.0f  // narrower screens scale it down to keep it whole

// A CSS font-size is the em, and fontstash sizes a font by ascender minus
// descender. For pak00's Noto Sans and Handel Gothic these are that height per
// em and, for the baseline, (ascender - (ascender - descender) / 2) per em: a
// line box lh high has its baseline lh / 2 + size * that below its top.
#define SM_SANS_EM (2789.0f / 2048.0f)
#define SM_SANS_BASE (794.5f / 2048.0f)
#define SM_HANDEL_EM (2289.0f / 2048.0f)
#define SM_HANDEL_BASE (694.5f / 2048.0f)

#define SM_RGB(r, g, b) {(r) / 255.0f, (g) / 255.0f, (b) / 255.0f, 1.0f}

extern const vec4_t smText;        // body
extern const vec4_t smYellow;      // h2, what is selected
extern const vec4_t smDim;         // .start-match, .widemodal-content, the overlay
extern const vec4_t smCard;        // button.card
extern const vec4_t smCardHover;
extern const vec4_t smRowOdd;      // .cvar:nth-child(odd)
extern const vec4_t smRowEven;     // .cvar:nth-child(even)
extern const vec4_t smInput;       // input
extern const vec4_t smGrey;        // borders, .Select-control, selections, scrollbar
extern const vec4_t smFocus;       // input:focus
extern const vec4_t smLighter;     // .Select-control:hover, input[type=range]:hover
extern const vec4_t smSelectText;  // .Select-placeholder
extern const vec4_t smOptionText;  // .Select-option
extern const vec4_t smDark;        // .Select-option.is-focused, input[type=range]
extern const vec4_t smArrow;       // .Select-arrow; the thumb and a button hovered
extern const vec4_t smLink;        // a
extern const vec4_t smLinkHover;   // a:hover
extern const vec4_t smButton;      // button
extern const vec4_t smButtonHover;
extern const vec4_t smButtonBorder;
extern const vec4_t smDialog;      // the modal
extern const vec4_t smShadow;      // .Select-menu's box-shadow
extern const vec4_t smOnAccent;    // text over a gold fill
extern const vec4_t smSubtle;      // --qc-text-subtle

typedef struct {
    float x, y, w, h;
} smRect_t;

// the frame, set by SM_Frame once a frame
extern float smScale;     // screen pixels per CSS pixel
extern float smTop;       // the screen y of the frame's top
extern float smVW;        // the frame's width in CSS pixels
extern float smMX, smMY;  // the cursor in the frame

void SM_Frame(void);

smRect_t SM_R(float x, float y, float w, float h);
qboolean SM_In(const smRect_t* r, float x, float y);
qboolean SM_Clip(smRect_t* r, const smRect_t* clip);

/*
Quake Club's motion vocabulary (app/qc-motion.css). Four curves, because a curve
says what kind of thing is moving and more than four stop meaning anything; three
durations, scaled off the 140ms the site already used most.
*/
typedef enum {
    SM_EASE_SUAVE,   // arrives and stays -- the default
    SM_EASE_FIRME,   // a whole journey
    SM_EASE_CORTE,   // leaves, and does not come back
    SM_EASE_RESORTE  // the accent: overshoots a little
} smEase_t;

#define SM_DUR_1 140  // colour, opacity, hover: must not be noticed
#define SM_DUR_2 260  // something moves, resizes, or arrives in place
#define SM_DUR_3 420  // panels and bars that enter the scene

#define SM_STAGGER 32      // between the items of a list
#define SM_STAGGER_LAST 6  // past the seventh the wait shows

// Quake Club's corner radii (--qc-radius-md and -lg)
#define SM_RADIUS_SM 6.0f
#define SM_RADIUS_MD 10.0f
#define SM_RADIUS_LG 14.0f

float SM_Curve(int ease, float t);
float SM_Progress(int startTime, int duration, int index);
void SM_EnterRise(int startTime, int index, float* alpha, float* dy);
void SM_EnterZoom(int startTime, float* alpha, float* scale);
void SM_Fade(const float* color, float alpha, vec4_t out);

void SM_Fill(float x, float y, float w, float h, const float* color, const smRect_t* clip);
void SM_Round(float x, float y, float w, float h, float r, const float* color, const smRect_t* clip);
void SM_Shadow(float x, float y, float w, float h, float r);

// what the cursor is on lifts a pixel or two, the way the site's rows and cards do
#define SM_LIFT 2.0f
void SM_FillScreen(const float* color);
void SM_Pic(float x, float y, float w, float h, qhandle_t shader, const smRect_t* clip);
void SM_Arrow(float x, float y, qboolean up);
void SM_To640(float x, float y, float w, float h, float* out);

int SM_BoldShift(void);
float SM_Em(int font);
float SM_Baseline(int font, float size, float lineTop, float lineHeight);
qboolean SM_TextInside(float baseline, float size, const smRect_t* clip);
float SM_Advance(int font, float size, const char* text, int len);
int SM_Fit(int font, float size, const char* text, float width);
void SM_Text(float x, float baseline, int font, float size, const float* color, const char* text, int limit,
             qboolean bold);
void SM_Centered(float middle, float baseline, float size, const float* color, const char* text, qboolean bold);
float SM_SpacedWidth(int font, float size, const char* text, float spacing);
void SM_SpacedText(float x, float baseline, int font, float size, const float* color, const char* text, float spacing,
                   qboolean bold);
int SM_Wrap(int font, float size, const char* text, float width, int* starts, int* lens);

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

extern smHit_t smHits[SM_MAX_HITS], smLastHits[SM_MAX_HITS];
extern int smNumHits, smNumLastHits;
extern smHit_t smUnder;
extern qboolean smHasUnder;

void SM_Hit(float x, float y, float w, float h, int type, int index, const smRect_t* clip);
const smHit_t* SM_HitAt(float x, float y);
qboolean SM_Over(int type, int index);

#endif  // __UI_WEB_H
