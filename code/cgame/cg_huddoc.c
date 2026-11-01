/*
===========================================================================
Copyright (C) 2026 ioquakelive contributors

This file is part of ioquakelive.

ioquakelive is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

ioquakelive is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with ioquakelive; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/
#include "cg_huddoc.h"

#define HDA_BLOCK -1
#define HDA_COLUMNS -2

typedef struct {
    const char* name;
    int arity;
} hdKeyword_t;

static const hdKeyword_t hd_assetKeywords[] = {
    {"font", 2}, {"smallFont", 2}, {"bigfont", 2}, {"gradientbar", 1}, {"menuEnterSound", 1},
    {"menuExitSound", 1}, {"itemFocusSound", 1}, {"menuBuzzSound", 1}, {"cursor", 1}, {"fadeClamp", 1},
    {"fadeCycle", 1}, {"fadeAmount", 1}, {"shadowX", 1}, {"shadowY", 1}, {"shadowColor", 4},
    {NULL, 0}};

static const hdKeyword_t hd_menuKeywords[] = {
    {"font", 1}, {"name", 1}, {"fullscreen", 1}, {"rect", 4}, {"style", 1}, {"visible", 1},
    {"onOpen", HDA_BLOCK}, {"onClose", HDA_BLOCK}, {"onESC", HDA_BLOCK}, {"border", 1}, {"borderSize", 1},
    {"backcolor", 4}, {"forecolor", 4}, {"bordercolor", 4}, {"focuscolor", 4}, {"disablecolor", 4},
    {"outlinecolor", 4}, {"background", 1}, {"ownerdraw", 1}, {"ownerdrawFlag", 1}, {"ownerdrawFlag2", 1},
    {"outOfBoundsClick", 0}, {"soundLoop", 1}, {"itemDef", 0}, {"cinematic", 1}, {"popup", 0},
    {"fadeClamp", 1}, {"fadeCycle", 1}, {"fadeAmount", 1}, {"widescreen", 1}, {"backgroundSize", 4},
    {NULL, 0}};

static const hdKeyword_t hd_itemKeywords[] = {
    {"name", 1}, {"text", 1}, {"group", 1}, {"asset_model", 1}, {"asset_shader", 1}, {"model_origin", 3},
    {"model_fovx", 1}, {"model_fovy", 1}, {"model_rotation", 1}, {"model_angle", 1}, {"rect", 4},
    {"style", 1}, {"decoration", 0}, {"notselectable", 0}, {"wrapped", 0}, {"autowrapped", 0},
    {"horizontalscroll", 0}, {"type", 1}, {"elementwidth", 1}, {"elementheight", 1}, {"feeder", 1},
    {"elementtype", 1}, {"columns", HDA_COLUMNS}, {"border", 1}, {"bordersize", 1}, {"visible", 1},
    {"ownerdraw", 1}, {"align", 1}, {"textalign", 1}, {"textalignx", 1}, {"textaligny", 1},
    {"textscale", 1}, {"textstyle", 1}, {"backcolor", 4}, {"forecolor", 4}, {"bordercolor", 4},
    {"outlinecolor", 4}, {"background", 1}, {"onFocus", HDA_BLOCK}, {"leaveFocus", HDA_BLOCK},
    {"mouseEnter", HDA_BLOCK}, {"mouseExit", HDA_BLOCK}, {"mouseEnterText", HDA_BLOCK},
    {"mouseExitText", HDA_BLOCK}, {"action", HDA_BLOCK}, {"special", 1}, {"cvar", 1}, {"maxChars", 1},
    {"maxPaintChars", 1}, {"focusSound", 1}, {"cvarFloat", 4}, {"cvarStrList", HDA_BLOCK},
    {"cvarFloatList", HDA_BLOCK}, {"addColorRange", 6}, {"ownerdrawFlag", 1}, {"enableCvar", HDA_BLOCK},
    {"cvarTest", 1}, {"disableCvar", HDA_BLOCK}, {"showCvar", HDA_BLOCK}, {"hideCvar", HDA_BLOCK},
    {"cinematic", 1}, {"doubleclick", HDA_BLOCK}, {"cvarInt", 4}, {"font", 1}, {"cvarPreset", HDA_BLOCK},
    {"cvarPresetList", HDA_BLOCK}, {"widescreen", 1}, {"ownerdrawFlag2", 1}, {"cvarTest2", 1},
    {"cvarTest3", 1}, {"cvarTest4", 1}, {"showCvar2", HDA_BLOCK}, {"showCvar3", HDA_BLOCK},
    {"showCvar4", HDA_BLOCK}, {"hideCvar2", HDA_BLOCK}, {"hideCvar3", HDA_BLOCK}, {"hideCvar4", HDA_BLOCK},
    {"cvara", 1}, {"selectedcolor", 4}, {"altrowcolor", 4}, {"outlineimage", 1}, {"elementimage", 1},
    {"elementcolor", 4}, {"cellId", 1}, {"defaultContent", 1}, {"precision", 1},
    {NULL, 0}};

static const hdKeyword_t* HD_Table(int kind) {
    if (kind == HDK_ASSET)
        return hd_assetKeywords;
    if (kind == HDK_MENU)
        return hd_menuKeywords;
    return hd_itemKeywords;
}

const char* HD_Keyword(int kind, int keyword) {
    if (keyword < 0)
        return "";
    return HD_Table(kind)[keyword].name;
}

static const char* HD_Text(const hdDoc_t* d, int tok) {
    return d->text + d->files[d->toks[tok].file].ofs + d->toks[tok].from;
}

static int HD_Len(const hdDoc_t* d, int tok) {
    return d->toks[tok].to - d->toks[tok].from;
}

void HD_TokenString(const hdDoc_t* d, int tok, char* out, int size) {
    int len = HD_Len(d, tok);
    const char* s = HD_Text(d, tok);

    if (d->toks[tok].type == HDT_STRING && len >= 2) {
        s++;
        len -= 2;
    }
    if (len > size - 1)
        len = size - 1;
    memcpy(out, s, len);
    out[len] = 0;
}

static qboolean HD_TokIs(const hdDoc_t* d, int tok, const char* s) {
    int len = HD_Len(d, tok);
    return (int)strlen(s) == len && !Q_stricmpn(HD_Text(d, tok), s, len);
}

static qboolean HD_XIs(const hdDoc_t* d, int x, const char* s) {
    return x < d->numX && HD_TokIs(d, d->xtoks[x].tok, s);
}

static int HD_XType(const hdDoc_t* d, int x) {
    return x < d->numX ? d->toks[d->xtoks[x].tok].type : -1;
}

static void HD_Fail(hdDoc_t* d, const char* msg) {
    if (!d->error[0])
        Q_strncpyz(d->error, msg, sizeof(d->error));
}

static int HD_AddFile(hdDoc_t* d, const char* name, const char* text, int len) {
    hdFile_t* f;

    if (d->numFiles >= HD_MAX_FILES || d->textUsed + len + 1 > HD_MAX_TEXT) {
        HD_Fail(d, "file too large");
        return -1;
    }
    f = &d->files[d->numFiles];
    Q_strncpyz(f->name, name, sizeof(f->name));
    f->ofs = d->textUsed;
    f->len = len;
    memcpy(d->text + d->textUsed, text, len);
    d->text[d->textUsed + len] = 0;
    d->textUsed += len + 1;
    return d->numFiles++;
}

static qboolean HD_IsNameStart(int c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

static qboolean HD_IsDigit(int c) {
    return c >= '0' && c <= '9';
}

static qboolean HD_Lex(hdDoc_t* d, int file) {
    const char* s = d->text + d->files[file].ofs;
    int len = d->files[file].len;
    int i = 0, line = 1, start;
    byte type;

    while (i < len) {
        int c = (unsigned char)s[i];

        if (c == '\\' && i + 1 < len && (s[i + 1] == '\n' || s[i + 1] == '\r')) {
            i++;
            if (s[i] == '\r' && i + 1 < len && s[i + 1] == '\n')
                i++;
            i++;
            continue;
        }
        if (c == '\n') {
            line++;
            i++;
            continue;
        }
        if (c <= ' ') {
            i++;
            continue;
        }
        if (c == '/' && i + 1 < len && s[i + 1] == '/') {
            while (i < len && s[i] != '\n')
                i++;
            continue;
        }
        if (c == '/' && i + 1 < len && s[i + 1] == '*') {
            i += 2;
            while (i + 1 < len && !(s[i] == '*' && s[i + 1] == '/')) {
                if (s[i] == '\n')
                    line++;
                i++;
            }
            i = i + 2 > len ? len : i + 2;
            continue;
        }
        start = i;
        if (c == '"') {
            i++;
            while (i < len && s[i] != '"' && s[i] != '\n') {
                if (s[i] == '\\' && i + 1 < len && s[i + 1] != '\n')
                    i++;
                i++;
            }
            if (i < len && s[i] == '"')
                i++;
            type = HDT_STRING;
        } else if (HD_IsNameStart(c)) {
            while (i < len && (HD_IsNameStart((unsigned char)s[i]) || HD_IsDigit((unsigned char)s[i])))
                i++;
            type = HDT_NAME;
        } else if (HD_IsDigit(c) || (c == '.' && i + 1 < len && HD_IsDigit((unsigned char)s[i + 1]))) {
            while (i < len && (HD_IsDigit((unsigned char)s[i]) || s[i] == '.' || HD_IsNameStart((unsigned char)s[i])))
                i++;
            type = HDT_NUMBER;
        } else {
            i++;
            type = HDT_PUNCT;
        }
        if (d->numToks >= HD_MAX_TOKENS) {
            HD_Fail(d, "too many tokens");
            return qfalse;
        }
        d->toks[d->numToks].file = file;
        d->toks[d->numToks].from = start;
        d->toks[d->numToks].to = i;
        d->toks[d->numToks].line = line;
        d->toks[d->numToks].type = type;
        d->numToks++;
    }
    return qtrue;
}

static int HD_FindDefine(const hdDoc_t* d, int tok) {
    int i, len = HD_Len(d, tok);
    const char* s = HD_Text(d, tok);

    for (i = d->numDefines - 1; i >= 0; i--) {
        int n = d->defines[i].nameTok;
        if (n >= 0 && HD_Len(d, n) == len && !strncmp(HD_Text(d, n), s, len))
            return i;
    }
    return -1;
}

static qboolean HD_Emit(hdDoc_t* d, int tok, int origin, byte direct) {
    if (d->numX >= HD_MAX_XTOKENS) {
        HD_Fail(d, "too many tokens after macros");
        return qfalse;
    }
    d->xtoks[d->numX].tok = tok;
    d->xtoks[d->numX].origin = origin;
    d->xtoks[d->numX].direct = direct;
    d->numX++;
    return qtrue;
}

static qboolean HD_ExpandFile(hdDoc_t* d, int file, int first, int last, qboolean main, int depth);

static int hd_expanding[64];
static int hd_numExpanding;

static qboolean HD_ExpandMacro(hdDoc_t* d, int def, int origin, int depth) {
    int i, j;
    const hdDefine_t* m = &d->defines[def];

    if (depth > 32 || hd_numExpanding >= (int)ARRAY_LEN(hd_expanding)) {
        HD_Fail(d, "macros nested too deep");
        return qfalse;
    }
    if (m->params)
        d->unsupported++;
    hd_expanding[hd_numExpanding++] = def;
    for (i = m->bodyFirst; i < m->bodyFirst + m->bodyCount; i++) {
        int inner = d->toks[i].type == HDT_NAME ? HD_FindDefine(d, i) : -1;

        for (j = 0; inner >= 0 && j < hd_numExpanding; j++) {
            if (hd_expanding[j] == inner)
                inner = -1;
        }
        if (inner >= 0) {
            if (!HD_ExpandMacro(d, inner, origin, depth + 1))
                return qfalse;
        } else if (!HD_Emit(d, i, origin, 0)) {
            return qfalse;
        }
    }
    hd_numExpanding--;
    return qtrue;
}

static qboolean HD_Include(hdDoc_t* d, int tok, int depth) {
    char path[MAX_QPATH];
    static char buf[HD_MAX_TEXT];
    int len, file, first;

    HD_TokenString(d, tok, path, sizeof(path));
    if (!d->readFile || depth > 8) {
        d->unsupported++;
        return qtrue;
    }
    len = d->readFile(path, buf, sizeof(buf) - 1);
    if (len < 0) {
        d->unsupported++;
        return qtrue;
    }
    file = HD_AddFile(d, path, buf, len);
    if (file < 0)
        return qfalse;
    first = d->numToks;
    if (!HD_Lex(d, file))
        return qfalse;
    return HD_ExpandFile(d, file, first, d->numToks, qfalse, depth + 1);
}

static qboolean HD_ExpandFile(hdDoc_t* d, int file, int first, int last, qboolean main, int depth) {
    int i = first;
    int skip[32], numSkip = 0, skipping = 0;

    while (i < last) {
        hdTok_t* t = &d->toks[i];

        if (t->type == HDT_PUNCT && HD_Text(d, i)[0] == '#' && i + 1 < last && d->toks[i + 1].line == t->line) {
            int dir = i + 1, end = i + 2;

            while (end < last && d->toks[end].line == t->line)
                end++;
            if (HD_TokIs(d, dir, "ifdef") || HD_TokIs(d, dir, "ifndef")) {
                int on = dir + 1 < end && HD_FindDefine(d, dir + 1) >= 0;
                if (HD_TokIs(d, dir, "ifndef"))
                    on = !on;
                if (numSkip < (int)ARRAY_LEN(skip))
                    skip[numSkip++] = skipping;
                if (!on)
                    skipping++;
            } else if (HD_TokIs(d, dir, "if")) {
                d->unsupported++;
                if (numSkip < (int)ARRAY_LEN(skip))
                    skip[numSkip++] = skipping;
            } else if (HD_TokIs(d, dir, "else")) {
                if (numSkip > 0) {
                    int outer = skip[numSkip - 1];
                    skipping = skipping > outer ? outer : outer + 1;
                }
            } else if (HD_TokIs(d, dir, "endif")) {
                if (numSkip > 0)
                    skipping = skip[--numSkip];
            } else if (!skipping && HD_TokIs(d, dir, "define") && dir + 1 < end) {
                hdDefine_t* m;
                int existing = HD_FindDefine(d, dir + 1);

                if (existing >= 0)
                    d->defines[existing].nameTok = -1;
                if (d->numDefines >= HD_MAX_DEFINES) {
                    HD_Fail(d, "too many macros");
                    return qfalse;
                }
                m = &d->defines[d->numDefines++];
                m->nameTok = dir + 1;
                m->params = dir + 2 < end && d->toks[dir + 2].from == d->toks[dir + 1].to &&
                            HD_Text(d, dir + 2)[0] == '(';
                m->bodyFirst = dir + 2;
                m->bodyCount = end - (dir + 2);
            } else if (!skipping && HD_TokIs(d, dir, "undef") && dir + 1 < end) {
                int existing = HD_FindDefine(d, dir + 1);
                if (existing >= 0)
                    d->defines[existing].nameTok = -1;
            } else if (!skipping && HD_TokIs(d, dir, "include") && dir + 1 < end) {
                if (!HD_Include(d, dir + 1, depth))
                    return qfalse;
            } else if (!skipping) {
                d->unsupported++;
            }
            i = end;
            continue;
        }
        if (skipping) {
            i++;
            continue;
        }
        if (t->type == HDT_NAME) {
            int def = HD_FindDefine(d, i);
            if (def >= 0) {
                hd_numExpanding = 0;
                if (!HD_ExpandMacro(d, def, main ? i : -1, 0))
                    return qfalse;
                i++;
                continue;
            }
        }
        if (!HD_Emit(d, i, main ? i : -1, (byte)main))
            return qfalse;
        i++;
    }
    return qtrue;
}

static int HD_FindKeyword(const hdDoc_t* d, int kind, int x) {
    const hdKeyword_t* table = HD_Table(kind);
    int i;

    if (HD_XType(d, x) != HDT_NAME)
        return -1;
    for (i = 0; table[i].name; i++) {
        if (HD_XIs(d, x, table[i].name))
            return i;
    }
    return -1;
}

static int HD_SkipValue(const hdDoc_t* d, int x, float* value) {
    float sign = 1.0f;
    char buf[64];

    if (x >= d->numX)
        return -1;
    if (HD_XType(d, x) == HDT_PUNCT && HD_XIs(d, x, "-") && HD_XType(d, x + 1) == HDT_NUMBER) {
        sign = -1.0f;
        x++;
    }
    if (value) {
        HD_TokenString(d, d->xtoks[x].tok, buf, sizeof(buf));
        *value = sign * (float)atof(buf);
    }
    if (HD_XType(d, x) == HDT_STRING) {
        while (HD_XType(d, x + 1) == HDT_STRING)
            x++;
    }
    return x + 1;
}

static int HD_SkipBlock(const hdDoc_t* d, int x) {
    if (!HD_XIs(d, x, "{"))
        return -1;
    for (x++; x < d->numX; x++) {
        if (HD_XIs(d, x, "}"))
            return x + 1;
    }
    return -1;
}

static void HD_PropSpan(hdDoc_t* d, hdProp_t* p) {
    const hdXTok_t* k = &d->xtoks[p->x];
    int a, b, before, after;

    p->editable = qfalse;
    p->srcFrom = p->srcTo = p->valFrom = p->valTo = -1;
    if (!k->direct)
        return;
    p->srcFrom = d->toks[k->tok].from;
    p->srcTo = d->toks[k->tok].to;
    if (p->xValues >= p->xEnd) {
        p->editable = qtrue;
        return;
    }
    a = d->xtoks[p->xValues].origin;
    b = d->xtoks[p->xEnd - 1].origin;
    if (a < 0 || b < 0 || a == k->tok)
        return;
    before = p->xValues - 1;
    after = p->xEnd;
    if (d->xtoks[before].origin == a)
        return;
    if (after < d->numX && d->xtoks[after].origin == b)
        return;
    p->valFrom = d->toks[a].from;
    p->valTo = d->toks[b].to;
    p->srcTo = p->valTo;
    p->editable = qtrue;
}

static int HD_ParseProp(hdDoc_t* d, int kind, int owner, int x) {
    const hdKeyword_t* table = HD_Table(kind);
    int kw = HD_FindKeyword(d, kind, x);
    hdProp_t* p;
    int y = x + 1, n, i;

    if (d->numProps >= HD_MAX_PROPS) {
        HD_Fail(d, "too many properties");
        return -1;
    }
    p = &d->props[d->numProps];
    memset(p, 0, sizeof(*p));
    p->kind = kind;
    p->owner = owner;
    p->keyword = kw;
    p->x = x;
    p->xValues = y;
    if (kw < 0) {
        d->unknownKeywords++;
    } else if (table[kw].arity == HDA_BLOCK) {
        y = HD_SkipBlock(d, y);
    } else if (table[kw].arity == HDA_COLUMNS) {
        float num;
        y = HD_SkipValue(d, y, &num);
        for (n = 0; y >= 0 && n < (int)num * 3; n++)
            y = HD_SkipValue(d, y, NULL);
    } else {
        for (i = 0; y >= 0 && i < table[kw].arity; i++) {
            float v;
            y = HD_SkipValue(d, y, &v);
            if (i < 4)
                p->values[i] = v;
        }
        p->numValues = table[kw].arity < 4 ? table[kw].arity : 4;
    }
    if (y < 0) {
        HD_Fail(d, "unexpected end of file");
        return -1;
    }
    p->xEnd = y;
    HD_PropSpan(d, p);
    d->numProps++;
    return y;
}

static int HD_ParseItem(hdDoc_t* d, int menu, int x) {
    hdItem_t* it;
    int idx;

    if (d->numItems >= HD_MAX_ITEMS) {
        HD_Fail(d, "too many items");
        return -1;
    }
    idx = d->numItems++;
    it = &d->items[idx];
    memset(it, 0, sizeof(*it));
    it->menu = menu;
    it->firstProp = d->numProps;
    it->xOpen = x;
    if (!HD_XIs(d, x, "{")) {
        HD_Fail(d, "itemDef without {");
        return -1;
    }
    for (x++; x < d->numX; ) {
        if (HD_XIs(d, x, "}")) {
            it->xClose = x;
            it->numProps = d->numProps - it->firstProp;
            it->local = d->xtoks[it->xOpen].direct && d->xtoks[x].direct;
            it->closeFrom = it->local ? d->toks[d->xtoks[x].tok].from : -1;
            return x + 1;
        }
        x = HD_ParseProp(d, HDK_ITEM, idx, x);
        if (x < 0)
            return -1;
        if (d->props[d->numProps - 1].keyword >= 0 &&
            !Q_stricmp(hd_itemKeywords[d->props[d->numProps - 1].keyword].name, "rect")) {
            memcpy(it->rect, d->props[d->numProps - 1].values, sizeof(it->rect));
            it->hasRect = qtrue;
        }
    }
    HD_Fail(d, "end of file inside itemDef");
    return -1;
}

static int HD_ParseMenu(hdDoc_t* d, int x) {
    hdMenu_t* m;
    int idx;

    if (d->numMenus >= HD_MAX_MENUS) {
        HD_Fail(d, "too many menus");
        return -1;
    }
    idx = d->numMenus++;
    m = &d->menus[idx];
    memset(m, 0, sizeof(*m));
    m->firstProp = d->numProps;
    m->firstItem = d->numItems;
    m->xOpen = x;
    if (!HD_XIs(d, x, "{")) {
        d->numMenus--;
        return x;
    }
    for (x++; x < d->numX; ) {
        if (HD_XIs(d, x, "}")) {
            m->xClose = x;
            m->numItems = d->numItems - m->firstItem;
            m->local = d->xtoks[m->xOpen].direct && d->xtoks[x].direct;
            m->closeFrom = m->local ? d->toks[d->xtoks[x].tok].from : -1;
            return x + 1;
        }
        if (HD_XIs(d, x, "itemDef")) {
            x = HD_ParseItem(d, idx, x + 1);
            if (x < 0)
                return -1;
            continue;
        }
        x = HD_ParseProp(d, HDK_MENU, idx, x);
        if (x < 0)
            return -1;
        {
            hdProp_t* p = &d->props[d->numProps - 1];
            const char* key = HD_Keyword(HDK_MENU, p->keyword);
            if (!Q_stricmp(key, "rect")) {
                memcpy(m->rect, p->values, sizeof(m->rect));
                m->hasRect = qtrue;
            } else if (!Q_stricmp(key, "name") && p->xValues < p->xEnd) {
                HD_TokenString(d, d->xtoks[p->xValues].tok, m->name, sizeof(m->name));
            }
        }
    }
    HD_Fail(d, "end of file inside menuDef");
    return -1;
}

static int HD_ParseAsset(hdDoc_t* d, int x) {
    if (!HD_XIs(d, x, "{"))
        return x;
    for (x++; x < d->numX; ) {
        if (HD_XIs(d, x, "}"))
            return x + 1;
        x = HD_ParseProp(d, HDK_ASSET, -1, x);
        if (x < 0)
            return -1;
    }
    HD_Fail(d, "end of file inside assetGlobalDef");
    return -1;
}

qboolean HD_Load(hdDoc_t* d, const char* name, const char* text, int len, hdReadFile_t readFile) {
    int x;

    d->textUsed = d->numFiles = d->numToks = d->numX = d->numDefines = 0;
    d->numProps = d->numItems = d->numMenus = d->numEdits = d->editTextUsed = 0;
    d->unknownKeywords = d->unsupported = 0;
    d->error[0] = 0;
    d->readFile = readFile;

    if (HD_AddFile(d, name, text, len) < 0 || !HD_Lex(d, 0))
        return qfalse;
    if (!HD_ExpandFile(d, 0, 0, d->numToks, qtrue, 0))
        return qfalse;

    for (x = 0; x < d->numX; ) {
        if (HD_XIs(d, x, "}"))
            break;
        if (HD_XIs(d, x, "assetGlobalDef")) {
            x = HD_ParseAsset(d, x + 1);
        } else if (HD_XIs(d, x, "menuDef")) {
            x = HD_ParseMenu(d, x + 1);
        } else {
            x++;
        }
        if (x < 0)
            return qfalse;
    }
    return qtrue;
}

int HD_FindProp(const hdDoc_t* d, int kind, int owner, const char* key) {
    int first, count, i;

    if (kind == HDK_ITEM) {
        first = d->items[owner].firstProp;
        count = d->items[owner].numProps;
    } else {
        first = d->menus[owner].firstProp;
        count = 0;
        for (i = first; i < d->numProps; i++) {
            if (d->props[i].kind == HDK_MENU && d->props[i].owner == owner)
                count = i - first + 1;
        }
    }
    for (i = first + count - 1; i >= first; i--) {
        const hdProp_t* p = &d->props[i];
        if (p->kind == kind && p->owner == owner && !Q_stricmp(HD_Keyword(kind, p->keyword), key))
            return i;
    }
    return -1;
}

static void HD_FormatNumber(float v, char* out, int size) {
    int len;

    if (v == (float)(int)v) {
        Com_sprintf(out, size, "%d", (int)v);
        return;
    }
    Com_sprintf(out, size, "%.4f", v);
    len = strlen(out);
    while (len > 0 && out[len - 1] == '0')
        out[--len] = 0;
    if (len > 0 && out[len - 1] == '.')
        out[--len] = 0;
}

static qboolean HD_AddEdit(hdDoc_t* d, int from, int to, const char* text) {
    int i, len = strlen(text);
    hdEdit_t* e = NULL;

    for (i = 0; i < d->numEdits; i++) {
        if (d->edits[i].from == from && d->edits[i].to == to) {
            e = &d->edits[i];
            break;
        }
    }
    if (!e) {
        if (d->numEdits >= HD_MAX_EDITS)
            return qfalse;
        e = &d->edits[d->numEdits++];
        e->from = from;
        e->to = to;
    }
    if (d->editTextUsed + len + 1 > HD_MAX_EDITTEXT)
        return qfalse;
    e->textOfs = d->editTextUsed;
    e->textLen = len;
    memcpy(d->editText + d->editTextUsed, text, len + 1);
    d->editTextUsed += len + 1;
    return qtrue;
}

static int HD_LineIndent(const hdDoc_t* d, int pos, char* out, int size) {
    const char* s = d->text + d->files[0].ofs;
    int start = pos, n = 0;

    while (start > 0 && s[start - 1] != '\n')
        start--;
    while (start + n < pos && (s[start + n] == ' ' || s[start + n] == '\t') && n < size - 1) {
        out[n] = s[start + n];
        n++;
    }
    out[n] = 0;
    return n;
}

qboolean HD_SetValues(hdDoc_t* d, int kind, int owner, const char* key, const float* values, int count) {
    char text[256], num[32], indent[64];
    int i, p = HD_FindProp(d, kind, owner, key);
    int closeFrom = kind == HDK_ITEM ? d->items[owner].closeFrom : d->menus[owner].closeFrom;

    text[0] = 0;
    for (i = 0; i < count; i++) {
        HD_FormatNumber(values[i], num, sizeof(num));
        if (i)
            Q_strcat(text, sizeof(text), " ");
        Q_strcat(text, sizeof(text), num);
    }
    if (p >= 0 && d->props[p].editable && d->props[p].valFrom >= 0) {
        if (!HD_AddEdit(d, d->props[p].valFrom, d->props[p].valTo, text))
            return qfalse;
    } else if (closeFrom >= 0) {
        char line[320];
        HD_LineIndent(d, closeFrom, indent, sizeof(indent));
        Com_sprintf(line, sizeof(line), "\t%s %s\n%s", key, text, indent);
        if (!HD_AddEdit(d, closeFrom, closeFrom, line))
            return qfalse;
    } else {
        return qfalse;
    }
    if (p >= 0) {
        for (i = 0; i < count && i < 4; i++)
            d->props[p].values[i] = values[i];
    }
    return qtrue;
}

qboolean HD_SetRect(hdDoc_t* d, int item, const float rect[4]) {
    if (!HD_SetValues(d, HDK_ITEM, item, "rect", rect, 4))
        return qfalse;
    memcpy(d->items[item].rect, rect, sizeof(d->items[item].rect));
    d->items[item].hasRect = qtrue;
    return qtrue;
}

void HD_ClearEdits(hdDoc_t* d) {
    d->numEdits = 0;
    d->editTextUsed = 0;
}

int HD_Write(const hdDoc_t* d, char* out, int outSize) {
    const char* s = d->text + d->files[0].ofs;
    int len = d->files[0].len;
    int pos = 0, used = 0, i;
    int order[HD_MAX_EDITS];

    for (i = 0; i < d->numEdits; i++)
        order[i] = i;
    for (i = 1; i < d->numEdits; i++) {
        int j = i, v = order[i];
        while (j > 0 && d->edits[order[j - 1]].from > d->edits[v].from) {
            order[j] = order[j - 1];
            j--;
        }
        order[j] = v;
    }
    for (i = 0; i < d->numEdits; i++) {
        const hdEdit_t* e = &d->edits[order[i]];
        int keep = e->from - pos;
        if (keep < 0 || used + keep + e->textLen >= outSize)
            return -1;
        memcpy(out + used, s + pos, keep);
        used += keep;
        memcpy(out + used, d->editText + e->textOfs, e->textLen);
        used += e->textLen;
        pos = e->to;
    }
    if (used + (len - pos) >= outSize)
        return -1;
    memcpy(out + used, s + pos, len - pos);
    used += len - pos;
    out[used] = 0;
    return used;
}
