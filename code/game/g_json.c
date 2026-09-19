/*
===========================================================================
ioquakelive - a small JSON builder for the stats events

The writer follows jsoncpp's StyledWriter, which is the only JSON writer in the
retail engine (quakelive_steam.exe has Json::StyledWriter and no FastWriter):

  - an object is "{", then each member on its own line as "key" : value, with a
    comma after all but the last, then "}" on its own line; members are sorted
    by key, because jsoncpp keeps them in a std::map
  - an empty object is {}, an empty array is []
  - an array goes on one line, "[ a, b ]", unless it holds a non-empty object
    or array, or the line would reach 74 characters; then one element a line
  - the indentation is three spaces, and the document ends with a newline
  - a real is printed with %#.16g and its trailing zeros are cut, keeping one
    after the point
===========================================================================
*/

#include "g_local.h"
#include "g_json.h"

#define J_MAX_NODES 4096
#define J_MAX_TEXT 65536
#define J_RIGHT_MARGIN 74

typedef enum { JN_NULL, JN_OBJECT, JN_ARRAY, JN_STRING, JN_INT, JN_BOOL, JN_REAL } jtype_t;

struct jnode_s {
    jtype_t type;
    const char* key;   // as a member of an object
    const char* text;  // JN_STRING
    int integer;       // JN_INT, JN_BOOL
    double real;       // JN_REAL
    jnode_t* first;    // children: members in key order, or elements in order
    jnode_t* next;
};

static jnode_t jNodes[J_MAX_NODES];
static int jNumNodes;
static char jText[J_MAX_TEXT];  // the strings the nodes point to
static int jTextLen;

void J_Reset(void) {
    jNumNodes = 0;
    jTextLen = 0;
}

static jnode_t* J_Alloc(jtype_t type) {
    jnode_t* n;

    if (jNumNodes >= J_MAX_NODES) {
        return NULL;
    }
    n = &jNodes[jNumNodes++];
    Com_Memset(n, 0, sizeof(*n));
    n->type = type;
    return n;
}

static const char* J_Copy(const char* s) {
    int len = (int)strlen(s) + 1;
    char* out;

    if (jTextLen + len > J_MAX_TEXT) {
        return "";
    }
    out = jText + jTextLen;
    Com_Memcpy(out, s, len);
    jTextLen += len;
    return out;
}

jnode_t* J_Object(void) {
    return J_Alloc(JN_OBJECT);
}

jnode_t* J_Array(void) {
    return J_Alloc(JN_ARRAY);
}

// a member goes in key order; one with the same key takes the old one's place
static void J_Set(jnode_t* obj, const char* key, jnode_t* value) {
    jnode_t** link;

    if (!obj || !value || obj->type != JN_OBJECT) {
        return;
    }
    value->key = J_Copy(key);
    for (link = &obj->first; *link; link = &(*link)->next) {
        int cmp = strcmp((*link)->key, key);

        if (cmp == 0) {
            value->next = (*link)->next;
            *link = value;
            return;
        }
        if (cmp > 0) {
            break;
        }
    }
    value->next = *link;
    *link = value;
}

void J_SetString(jnode_t* obj, const char* key, const char* value) {
    jnode_t* n = J_Alloc(JN_STRING);

    if (n) {
        n->text = J_Copy(value ? value : "");
        J_Set(obj, key, n);
    }
}

void J_SetInt(jnode_t* obj, const char* key, int value) {
    jnode_t* n = J_Alloc(JN_INT);

    if (n) {
        n->integer = value;
        J_Set(obj, key, n);
    }
}

void J_SetBool(jnode_t* obj, const char* key, qboolean value) {
    jnode_t* n = J_Alloc(JN_BOOL);

    if (n) {
        n->integer = value ? 1 : 0;
        J_Set(obj, key, n);
    }
}

void J_SetFloat(jnode_t* obj, const char* key, double value) {
    jnode_t* n = J_Alloc(JN_REAL);

    if (n) {
        n->real = value;
        J_Set(obj, key, n);
    }
}

void J_SetNode(jnode_t* obj, const char* key, jnode_t* child) {
    J_Set(obj, key, child);
}

void J_SetNull(jnode_t* obj, const char* key) {
    jnode_t* n = J_Alloc(JN_NULL);

    if (n) {
        J_Set(obj, key, n);
    }
}

void J_Push(jnode_t* array, jnode_t* child) {
    jnode_t** link;

    if (!array || !child || array->type != JN_ARRAY) {
        return;
    }
    for (link = &array->first; *link; link = &(*link)->next) {
    }
    *link = child;
}

void J_PushInt(jnode_t* array, int value) {
    jnode_t* n = J_Alloc(JN_INT);

    if (n) {
        n->integer = value;
        J_Push(array, n);
    }
}

void J_PushString(jnode_t* array, const char* value) {
    jnode_t* n = J_Alloc(JN_STRING);

    if (n) {
        n->text = J_Copy(value ? value : "");
        J_Push(array, n);
    }
}

/*
===============================================================================

WRITING

===============================================================================
*/

typedef struct {
    char* out;
    int size;
    int len;
    int indent;
    qboolean overflow;
} jwriter_t;

static void JW_Add(jwriter_t* w, const char* s) {
    int n = (int)strlen(s);

    if (w->len + n >= w->size) {
        w->overflow = qtrue;
        return;
    }
    Com_Memcpy(w->out + w->len, s, n);
    w->len += n;
    w->out[w->len] = '\0';
}

// StyledWriter::writeIndent: a new line and the indentation, unless the line already ends in
// a space -- which is how "key" : { keeps the brace on the key's line
static void JW_Indent(jwriter_t* w) {
    int i;

    if (w->len > 0) {
        char last = w->out[w->len - 1];

        if (last == ' ') {
            return;
        }
        if (last != '\n') {
            JW_Add(w, "\n");
        }
    }
    for (i = 0; i < w->indent; i++) {
        JW_Add(w, "   ");
    }
}

static void JW_WithIndent(jwriter_t* w, const char* s) {
    JW_Indent(w);
    JW_Add(w, s);
}

static void J_Quote(const char* s, char* out, int size) {
    int n = 0;

    if (size < 3) {
        return;
    }
    out[n++] = '"';
    for (; *s && n < size - 8; s++) {
        unsigned char c = (unsigned char)*s;

        switch (c) {
            case '"':  out[n++] = '\\'; out[n++] = '"'; break;
            case '\\': out[n++] = '\\'; out[n++] = '\\'; break;
            case '\b': out[n++] = '\\'; out[n++] = 'b'; break;
            case '\f': out[n++] = '\\'; out[n++] = 'f'; break;
            case '\n': out[n++] = '\\'; out[n++] = 'n'; break;
            case '\r': out[n++] = '\\'; out[n++] = 'r'; break;
            case '\t': out[n++] = '\\'; out[n++] = 't'; break;
            default:
                if (c < 0x20) {
                    n += Com_sprintf(out + n, size - n, "\\u%04X", c);
                } else {
                    out[n++] = (char)c;
                }
                break;
        }
    }
    out[n++] = '"';
    out[n] = '\0';
}

// jsoncpp's valueToString(double): %#.16g, then the trailing zeros go, keeping one after the point
static void J_Real(double v, char* out, int size) {
    char* p;
    char* last;

    Com_sprintf(out, size, "%#.16g", v);
    p = strchr(out, '.');
    if (!p || strchr(out, 'e')) {
        return;
    }
    last = out + strlen(out) - 1;
    while (last > p + 1 && *last == '0') {
        *last-- = '\0';
    }
}

// a scalar as text; qfalse for objects and arrays
static qboolean J_Scalar(const jnode_t* n, char* out, int size) {
    switch (n->type) {
        case JN_NULL: Q_strncpyz(out, "null", size); return qtrue;
        case JN_STRING: J_Quote(n->text, out, size); return qtrue;
        case JN_INT: Com_sprintf(out, size, "%i", n->integer); return qtrue;
        case JN_BOOL: Q_strncpyz(out, n->integer ? "true" : "false", size); return qtrue;
        case JN_REAL: J_Real(n->real, out, size); return qtrue;
        default: return qfalse;
    }
}

static void JW_Value(jwriter_t* w, const jnode_t* n);

// StyledWriter::isMultineArray
static qboolean J_Multiline(const jnode_t* arr) {
    const jnode_t* c;
    int count = 0;
    int line;

    for (c = arr->first; c; c = c->next) {
        count++;
        if ((c->type == JN_OBJECT || c->type == JN_ARRAY) && c->first) {
            return qtrue;
        }
    }
    if (count * 3 >= J_RIGHT_MARGIN) {
        return qtrue;
    }
    line = 4 + (count - 1) * 2;
    for (c = arr->first; c; c = c->next) {
        char text[1024];

        if (J_Scalar(c, text, sizeof(text))) {
            line += (int)strlen(text);
        } else {
            line += 2;  // an empty {} or []
        }
    }
    return (qboolean)(line >= J_RIGHT_MARGIN);
}

static void JW_Array(jwriter_t* w, const jnode_t* n) {
    const jnode_t* c;

    if (!n->first) {
        JW_Add(w, "[]");
        return;
    }
    if (J_Multiline(n)) {
        JW_WithIndent(w, "[");
        w->indent++;
        for (c = n->first; c; c = c->next) {
            JW_Indent(w);
            JW_Value(w, c);
            if (c->next) {
                JW_Add(w, ",");
            }
        }
        w->indent--;
        JW_WithIndent(w, "]");
        return;
    }
    JW_Add(w, "[ ");
    for (c = n->first; c; c = c->next) {
        JW_Value(w, c);
        if (c->next) {
            JW_Add(w, ", ");
        }
    }
    JW_Add(w, " ]");
}

static void JW_Object(jwriter_t* w, const jnode_t* n) {
    const jnode_t* c;

    if (!n->first) {
        JW_Add(w, "{}");
        return;
    }
    JW_WithIndent(w, "{");
    w->indent++;
    for (c = n->first; c; c = c->next) {
        char key[256];

        J_Quote(c->key, key, sizeof(key));
        JW_WithIndent(w, key);
        JW_Add(w, " : ");
        JW_Value(w, c);
        if (c->next) {
            JW_Add(w, ",");
        }
    }
    w->indent--;
    JW_WithIndent(w, "}");
}

static void JW_Value(jwriter_t* w, const jnode_t* n) {
    char text[4096];

    if (n->type == JN_OBJECT) {
        JW_Object(w, n);
    } else if (n->type == JN_ARRAY) {
        JW_Array(w, n);
    } else if (J_Scalar(n, text, sizeof(text))) {
        JW_Add(w, text);
    }
}

qboolean J_Write(const jnode_t* root, char* out, int size) {
    jwriter_t w;

    if (!root || size < 2) {
        return qfalse;
    }
    Com_Memset(&w, 0, sizeof(w));
    w.out = out;
    w.size = size;
    out[0] = '\0';
    JW_Value(&w, root);
    JW_Add(&w, "\n");
    return (qboolean)!w.overflow;
}
