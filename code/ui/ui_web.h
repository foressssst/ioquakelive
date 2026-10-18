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
#ifndef __UI_WEB_H
#define __UI_WEB_H

#include "ui_local.h"

#define SM_MAX_HITS 384
#define SM_MAX_LINES 8

#define SM_VIEW_H 1080.0f

#define SM_SANS_EM (2789.0f / 2048.0f)
#define SM_SANS_BASE (794.5f / 2048.0f)
#define SM_HANDEL_EM (2289.0f / 2048.0f)
#define SM_HANDEL_BASE (694.5f / 2048.0f)
#define SM_BARLOW_EM (1200.0f / 1000.0f)
#define SM_BARLOW_BASE (400.0f / 1000.0f)
#define SM_WEB_HANDEL_EM (2274.0f / 2048.0f)
#define SM_WEB_HANDEL_BASE (704.0f / 2048.0f)
#define SM_WEB_HANDEL_BOLD_EM (2289.0f / 2048.0f)
#define SM_WEB_HANDEL_BOLD_BASE (694.5f / 2048.0f)
#define SM_ARIAL_EM (2288.0f / 2048.0f)
#define SM_ARIAL_BASE (710.0f / 2048.0f)
#define SM_AWESOME_EM 1.0f
#define SM_AWESOME_BASE (640.0f / 1792.0f)

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

extern const vec4_t smPage;
extern const vec4_t smPanel;
extern const vec4_t smRaised;
extern const vec4_t smRaisedHover;
extern const vec4_t smBorder;
extern const vec4_t smField;
extern const vec4_t smText;
extern const vec4_t smTextSoft;
extern const vec4_t smMuted;
extern const vec4_t smSubtle;
extern const vec4_t smPrimary;
extern const vec4_t smOnPrimary;
extern const vec4_t smGold;
extern const vec4_t smRing;
extern const vec4_t smDanger;
extern const vec4_t smSuccess;
extern const vec4_t smShadow;

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
#define smDialog smRaised
#define smOnAccent smOnPrimary

typedef struct {
    float x, y, w, h;
} smRect_t;

extern float smScale;
extern float smTop;
extern float smVW;
extern float smBottom;
extern float smMX, smMY;

void SM_Frame(void);

typedef enum {
    SM_LOOK_RETAIL,
    SM_LOOK_OURS
} smLook_t;

extern smLook_t smLook;
void SM_SetLook(smLook_t look);

void SM_Icon(float x, float baseline, float size, const float* color, int codepoint);
float SM_IconWidth(float size, int codepoint);

smRect_t SM_R(float x, float y, float w, float h);
qboolean SM_In(const smRect_t* r, float x, float y);
qboolean SM_Clip(smRect_t* r, const smRect_t* clip);

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

void SM_DrawPlayerIn(playerInfo_t* pi, float x, float y, float w, float h);
void SM_TurnPlayer(playerInfo_t* pi, float yaw);
void SM_BuildPlayer(playerInfo_t* pi, const char* composed, float yaw, int weapon);

#define SM_FONT_TEXT FONT_SANS
#define SM_FONT_TITLE FONT_DEFAULT
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
int SM_ItalicFace(void);
float SM_AdvanceRaw(int font, float size, const char* text, int len, qboolean bold);
void SM_TextRaw(float x, float baseline, int font, float size, const float* color, const char* text, int limit,
                qboolean bold);
float SM_SpacedWidth(int font, float size, const char* text, float spacing);
void SM_SpacedText(float x, float baseline, int font, float size, const float* color, const char* text, float spacing,
                   qboolean bold);
int SM_Wrap(int font, float size, const char* text, float width, int* starts, int* lens);

float SM_LabelWidth(float size, const char* text, float spacing);
void SM_Label(float x, float baseline, float size, const float* color, const char* text, float spacing);

typedef enum {
    SM_BUTTON_PRIMARY,
    SM_BUTTON_SECONDARY,
    SM_BUTTON_DANGER
} smButtonStyle_t;

#define SM_BUTTON_H 36.0f
#define SM_PILL_H 28.0f

float SM_ButtonWidth(const char* text, float height);
#define SM_WEB_BUTTON_H 30.0f
float SM_WebButtonWidth(const char* label);
void SM_WebButton(float x, float y, float w, const char* label, qboolean disabled, qboolean over,
                  const smRect_t* clip);
void SM_WebButtonPadded(float x, float y, float w, float padLeft, float padRight, const char* label,
                        qboolean disabled, qboolean over, const smRect_t* clip);
extern const vec4_t smWebButton;
extern const vec4_t smWebButtonHover;
extern const vec4_t smWebYellow;
qboolean SM_Narrow(void);
int SM_SansSerifFace(void);
#define SM_SPACING_H1 -1.0f
#define SM_SPACING_H4 -0.8f
#define SM_SPACING_H5 -0.5f
void SM_SetSpacing(float spacing);
void SM_Border(float x, float y, float w, float h, float t, const float* color, const smRect_t* clip);
void SM_Button(float x, float y, float w, float h, const char* text, int style, qboolean over);
void SM_Switch(float x, float y, qboolean on, qboolean over);
#define SM_SWITCH_W 34.0f
#define SM_SWITCH_H 20.0f
void SM_SliderTrack(float x, float y, float w, float frac, qboolean over);
#define SM_SLIDER_H 16.0f
void SM_FieldBox(float x, float y, float w, float h, qboolean focused, qboolean over, const smRect_t* clip);
float SM_Kbd(float x, float y, const char* text, qboolean waiting, const smRect_t* clip);

typedef struct {
    smRect_t r;
    int type;
    int index;
} smHit_t;

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
qhandle_t SM_Avatar(const char* steamId);

void UI_Stats_Show(const char* steamId);
void UI_Matches_ShowMatch(const char* guid);
void UI_Stats_CardFor(int page);
void UI_Stats_DrawCardLine(float x, float lineTop, float width);

#endif
