/*
===========================================================================
ioquakelive - a small JSON builder for the stats events

Retail builds its stats events with jsoncpp (C++) and the engine writes them
with Json::StyledWriter. This is enough of both, in C, to produce the same
documents: objects keep their members sorted by key, as jsoncpp's std::map
does, and the text comes out the way StyledWriter indents it.

Nodes come from one pool that is emptied before each event, so building an
event allocates nothing.
===========================================================================
*/

#ifndef G_JSON_H
#define G_JSON_H

#include "../qcommon/q_shared.h"

typedef struct jnode_s jnode_t;

void J_Reset(void);  // before building an event

jnode_t* J_Object(void);
jnode_t* J_Array(void);

// set a member of an object; setting a key twice replaces the value
void J_SetString(jnode_t* obj, const char* key, const char* value);
void J_SetInt(jnode_t* obj, const char* key, int value);
void J_SetBool(jnode_t* obj, const char* key, qboolean value);
void J_SetFloat(jnode_t* obj, const char* key, double value);
void J_SetNode(jnode_t* obj, const char* key, jnode_t* child);
void J_SetNull(jnode_t* obj, const char* key);  // retail pone null y a veces lo reemplaza despues

void J_Push(jnode_t* array, jnode_t* child);
void J_PushInt(jnode_t* array, int value);
void J_PushString(jnode_t* array, const char* value);

// the document as Json::StyledWriter writes it; returns qfalse when it does not fit
qboolean J_Write(const jnode_t* root, char* out, int size);

#endif
