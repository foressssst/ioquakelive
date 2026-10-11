
#ifndef G_JSON_H
#define G_JSON_H

#include "../qcommon/q_shared.h"

typedef struct jnode_s jnode_t;

void J_Reset(void);

jnode_t* J_Object(void);
jnode_t* J_Array(void);

void J_SetString(jnode_t* obj, const char* key, const char* value);
void J_SetInt(jnode_t* obj, const char* key, int value);
void J_SetBool(jnode_t* obj, const char* key, qboolean value);
void J_SetFloat(jnode_t* obj, const char* key, double value);
void J_SetNode(jnode_t* obj, const char* key, jnode_t* child);
void J_SetNull(jnode_t* obj, const char* key);

void J_Push(jnode_t* array, jnode_t* child);
void J_PushInt(jnode_t* array, int value);
void J_PushString(jnode_t* array, const char* value);

qboolean J_Write(const jnode_t* root, char* out, int size);

#endif
