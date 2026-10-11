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
#include "q_shared.h"
#include "qcommon.h"

static huffman_t msgHuff;

static qboolean msgInit = qfalse;

int pcount[256];

int oldsize = 0;

void MSG_initHuffman(void);

void MSG_Init(msg_t* buf, byte* data, int length) {
    if (!msgInit) {
        MSG_initHuffman();
    }
    Com_Memset(buf, 0, sizeof(*buf));
    buf->data = data;
    buf->maxsize = length;
}

void MSG_InitOOB(msg_t* buf, byte* data, int length) {
    if (!msgInit) {
        MSG_initHuffman();
    }
    Com_Memset(buf, 0, sizeof(*buf));
    buf->data = data;
    buf->maxsize = length;
    buf->oob = qtrue;
}

void MSG_Clear(msg_t* buf) {
    buf->cursize = 0;
    buf->overflowed = qfalse;
    buf->bit = 0;
}

void MSG_Bitstream(msg_t* buf) {
    buf->oob = qfalse;
}

void MSG_BeginReading(msg_t* msg) {
    msg->readcount = 0;
    msg->bit = 0;
    msg->oob = qfalse;
}

void MSG_BeginReadingOOB(msg_t* msg) {
    msg->readcount = 0;
    msg->bit = 0;
    msg->oob = qtrue;
}

void MSG_Copy(msg_t* buf, byte* data, int length, msg_t* src) {
    if (length < src->cursize) {
        Com_Error(ERR_DROP, "MSG_Copy: can't copy into a smaller msg_t buffer");
    }
    Com_Memcpy(buf, src, sizeof(msg_t));
    buf->data = data;
    Com_Memcpy(buf->data, src->data, src->cursize);
}

void MSG_WriteBits(msg_t* msg, int value, int bits) {
    int i;

    oldsize += bits;

    if (msg->overflowed) {
        return;
    }

    if (bits == 0 || bits < -31 || bits > 32) {
        Com_Error(ERR_DROP, "MSG_WriteBits: bad bits %i", bits);
    }

    if (bits < 0) {
        bits = -bits;
    }

    if (msg->oob) {
        if (msg->cursize + (bits >> 3) > msg->maxsize) {
            msg->overflowed = qtrue;
            return;
        }

        if (bits == 8) {
            msg->data[msg->cursize] = value;
            msg->cursize += 1;
            msg->bit += 8;
        } else if (bits == 16) {
            short temp = value;

            CopyLittleShort(&msg->data[msg->cursize], &temp);
            msg->cursize += 2;
            msg->bit += 16;
        } else if (bits == 32) {
            CopyLittleLong(&msg->data[msg->cursize], &value);
            msg->cursize += 4;
            msg->bit += 32;
        } else {
            Com_Error(ERR_DROP, "can't write %d bits", bits);
        }
    } else {
        value &= (0xffffffff >> (32 - bits));
        if (bits & 7) {
            int nbits;
            nbits = bits & 7;
            if (msg->bit + nbits > msg->maxsize << 3) {
                msg->overflowed = qtrue;
                return;
            }
            for (i = 0; i < nbits; i++) {
                Huff_putBit((value & 1), msg->data, &msg->bit);
                value = (value >> 1);
            }
            bits = bits - nbits;
        }
        if (bits) {
            for (i = 0; i < bits; i += 8) {
                Huff_offsetTransmit(&msgHuff.compressor, (value & 0xff), msg->data, &msg->bit, msg->maxsize << 3);
                value = (value >> 8);

                if (msg->bit > msg->maxsize << 3) {
                    msg->overflowed = qtrue;
                    return;
                }
            }
        }
        msg->cursize = (msg->bit >> 3) + 1;
    }
}

int MSG_ReadBits(msg_t* msg, int bits) {
    int value;
    int get;
    qboolean sgn;
    int i, nbits;

    if (msg->readcount > msg->cursize) {
        return 0;
    }

    value = 0;

    if (bits < 0) {
        bits = -bits;
        sgn = qtrue;
    } else {
        sgn = qfalse;
    }

    if (msg->oob) {
        if (msg->readcount + (bits >> 3) > msg->cursize) {
            msg->readcount = msg->cursize + 1;
            return 0;
        }

        if (bits == 8) {
            value = msg->data[msg->readcount];
            msg->readcount += 1;
            msg->bit += 8;
        } else if (bits == 16) {
            short temp;

            CopyLittleShort(&temp, &msg->data[msg->readcount]);
            value = temp;
            msg->readcount += 2;
            msg->bit += 16;
        } else if (bits == 32) {
            CopyLittleLong(&value, &msg->data[msg->readcount]);
            msg->readcount += 4;
            msg->bit += 32;
        } else
            Com_Error(ERR_DROP, "can't read %d bits", bits);
    } else {
        nbits = 0;
        if (bits & 7) {
            nbits = bits & 7;
            if (msg->bit + nbits > msg->cursize << 3) {
                msg->readcount = msg->cursize + 1;
                return 0;
            }
            for (i = 0; i < nbits; i++) {
                value |= (Huff_getBit(msg->data, &msg->bit) << i);
            }
            bits = bits - nbits;
        }
        if (bits) {
            for (i = 0; i < bits; i += 8) {
                Huff_offsetReceive(msgHuff.decompressor.tree, &get, msg->data, &msg->bit, msg->cursize << 3);
                value = (unsigned int)value | ((unsigned int)get << (i + nbits));

                if (msg->bit > msg->cursize << 3) {
                    msg->readcount = msg->cursize + 1;
                    return 0;
                }
            }
        }
        msg->readcount = (msg->bit >> 3) + 1;
    }
    if (sgn && bits > 0 && bits < 32) {
        if (value & (1 << (bits - 1))) {
            value |= -1 ^ ((1 << bits) - 1);
        }
    }

    return value;
}

void MSG_WriteChar(msg_t* sb, int c) {
#ifdef PARANOID
    if (c < -128 || c > 127)
        Com_Error(ERR_FATAL, "MSG_WriteChar: range error");
#endif

    MSG_WriteBits(sb, c, 8);
}

void MSG_WriteByte(msg_t* sb, int c) {
#ifdef PARANOID
    if (c < 0 || c > 255)
        Com_Error(ERR_FATAL, "MSG_WriteByte: range error");
#endif

    MSG_WriteBits(sb, c, 8);
}

void MSG_WriteData(msg_t* buf, const void* data, int length) {
    int i;
    for (i = 0; i < length; i++) {
        MSG_WriteByte(buf, ((byte*)data)[i]);
    }
}

void MSG_WriteShort(msg_t* sb, int c) {
#ifdef PARANOID
    if (c < ((short)0x8000) || c > (short)0x7fff)
        Com_Error(ERR_FATAL, "MSG_WriteShort: range error");
#endif

    MSG_WriteBits(sb, c, 16);
}

void MSG_WriteLong(msg_t* sb, int c) {
    MSG_WriteBits(sb, c, 32);
}

void MSG_WriteFloat(msg_t* sb, float f) {
    floatint_t dat;
    dat.f = f;
    MSG_WriteBits(sb, dat.i, 32);
}

void MSG_WriteString(msg_t* sb, const char* s) {
    if (!s) {
        MSG_WriteData(sb, "", 1);
    } else {
        int l, i;
        char string[MAX_STRING_CHARS];

        l = strlen(s);
        if (l >= MAX_STRING_CHARS) {
            Com_Printf("MSG_WriteString: MAX_STRING_CHARS");
            MSG_WriteData(sb, "", 1);
            return;
        }
        Q_strncpyz(string, s, sizeof(string));

        for (i = 0; i < l; i++) {
            if (string[i] == '%') {
                string[i] = '.';
            }
        }

        MSG_WriteData(sb, string, l + 1);
    }
}

void MSG_WriteBigString(msg_t* sb, const char* s) {
    if (!s) {
        MSG_WriteData(sb, "", 1);
    } else {
        int l, i;
        char string[BIG_INFO_STRING];

        l = strlen(s);
        if (l >= BIG_INFO_STRING) {
            Com_Printf("MSG_WriteString: BIG_INFO_STRING");
            MSG_WriteData(sb, "", 1);
            return;
        }
        Q_strncpyz(string, s, sizeof(string));

        for (i = 0; i < l; i++) {
            if (string[i] == '%') {
                string[i] = '.';
            }
        }

        MSG_WriteData(sb, string, l + 1);
    }
}

void MSG_WriteAngle(msg_t* sb, float f) {
    MSG_WriteByte(sb, (int)(f * 256 / 360) & 255);
}

void MSG_WriteAngle16(msg_t* sb, float f) {
    MSG_WriteShort(sb, ANGLE2SHORT(f));
}

int MSG_ReadChar(msg_t* msg) {
    int c;

    c = (signed char)MSG_ReadBits(msg, 8);
    if (msg->readcount > msg->cursize) {
        c = -1;
    }

    return c;
}

int MSG_ReadByte(msg_t* msg) {
    int c;

    c = (unsigned char)MSG_ReadBits(msg, 8);
    if (msg->readcount > msg->cursize) {
        c = -1;
    }
    return c;
}

int MSG_LookaheadByte(msg_t* msg) {
    const int bloc = Huff_getBloc();
    const int readcount = msg->readcount;
    const int bit = msg->bit;
    int c = MSG_ReadByte(msg);
    Huff_setBloc(bloc);
    msg->readcount = readcount;
    msg->bit = bit;
    return c;
}

int MSG_ReadShort(msg_t* msg) {
    int c;

    c = (short)MSG_ReadBits(msg, 16);
    if (msg->readcount > msg->cursize) {
        c = -1;
    }

    return c;
}

int MSG_ReadLong(msg_t* msg) {
    int c;

    c = MSG_ReadBits(msg, 32);
    if (msg->readcount > msg->cursize) {
        c = -1;
    }

    return c;
}

float MSG_ReadFloat(msg_t* msg) {
    floatint_t dat;

    dat.i = MSG_ReadBits(msg, 32);
    if (msg->readcount > msg->cursize) {
        dat.f = -1;
    }

    return dat.f;
}

char* MSG_ReadString(msg_t* msg) {
    static char string[MAX_STRING_CHARS];
    int l, c;

    l = 0;
    do {
        c = MSG_ReadByte(msg);
        if (c == -1 || c == 0) {
            break;
        }
        if (c == '%') {
            c = '.';
        }
        if (l >= sizeof(string) - 1) {
            break;
        }
        string[l++] = c;
    } while (1);

    string[l] = '\0';

    return string;
}

char* MSG_ReadBigString(msg_t* msg) {
    static char string[BIG_INFO_STRING];
    int l, c;

    l = 0;
    do {
        c = MSG_ReadByte(msg);
        if (c == -1 || c == 0) {
            break;
        }
        if (c == '%') {
            c = '.';
        }
        if (l >= sizeof(string) - 1) {
            break;
        }
        string[l++] = c;
    } while (1);

    string[l] = '\0';

    return string;
}

char* MSG_ReadStringLine(msg_t* msg) {
    static char string[MAX_STRING_CHARS];
    int l, c;

    l = 0;
    do {
        c = MSG_ReadByte(msg);
        if (c == -1 || c == 0 || c == '\n') {
            break;
        }
        if (c == '%') {
            c = '.';
        }
        if (l >= sizeof(string) - 1) {
            break;
        }
        string[l++] = c;
    } while (1);

    string[l] = '\0';

    return string;
}

float MSG_ReadAngle16(msg_t* msg) {
    return SHORT2ANGLE(MSG_ReadShort(msg));
}

void MSG_ReadData(msg_t* msg, void* data, int len) {
    int i;

    for (i = 0; i < len; i++) {
        ((byte*)data)[i] = MSG_ReadByte(msg);
    }
}

int MSG_HashKey(const char* string, int maxlen) {
    int hash, i;

    hash = 0;
    for (i = 0; i < maxlen && string[i] != '\0'; i++) {
        if (string[i] == '%')
            hash += '.' * (119 + i);
        else
            hash += (signed char)string[i] * (119 + i);
    }
    hash = (hash ^ (hash >> 10) ^ (hash >> 20));
    return hash;
}

extern cvar_t* cl_shownet;

#define LOG(x)                                    \
    if (cl_shownet && cl_shownet->integer == 4) { \
        Com_Printf("%s ", x);                     \
    };

int kbitmask[32] = {
    0x00000001,
    0x00000003,
    0x00000007,
    0x0000000F,
    0x0000001F,
    0x0000003F,
    0x0000007F,
    0x000000FF,
    0x000001FF,
    0x000003FF,
    0x000007FF,
    0x00000FFF,
    0x00001FFF,
    0x00003FFF,
    0x00007FFF,
    0x0000FFFF,
    0x0001FFFF,
    0x0003FFFF,
    0x0007FFFF,
    0x000FFFFF,
    0x001FFFFf,
    0x003FFFFF,
    0x007FFFFF,
    0x00FFFFFF,
    0x01FFFFFF,
    0x03FFFFFF,
    0x07FFFFFF,
    0x0FFFFFFF,
    0x1FFFFFFF,
    0x3FFFFFFF,
    0x7FFFFFFF,
    0xFFFFFFFF,
};

void MSG_WriteDeltaKey(msg_t* msg, int key, int oldV, int newV, int bits) {
    if (oldV == newV) {
        MSG_WriteBits(msg, 0, 1);
        return;
    }
    MSG_WriteBits(msg, 1, 1);
    MSG_WriteBits(msg, newV ^ key, bits);
}

int MSG_ReadDeltaKey(msg_t* msg, int key, int oldV, int bits) {
    if (MSG_ReadBits(msg, 1)) {
        return MSG_ReadBits(msg, bits) ^ (key & kbitmask[bits - 1]);
    }
    return oldV;
}

void MSG_WriteDeltaKeyFloat(msg_t* msg, int key, float oldV, float newV) {
    floatint_t fi;
    if (oldV == newV) {
        MSG_WriteBits(msg, 0, 1);
        return;
    }
    fi.f = newV;
    MSG_WriteBits(msg, 1, 1);
    MSG_WriteBits(msg, fi.i ^ key, 32);
}

float MSG_ReadDeltaKeyFloat(msg_t* msg, int key, float oldV) {
    if (MSG_ReadBits(msg, 1)) {
        floatint_t fi;

        fi.i = MSG_ReadBits(msg, 32) ^ key;
        return fi.f;
    }
    return oldV;
}

void MSG_WriteDeltaUsercmdKey(msg_t* msg, int key, usercmd_t* from, usercmd_t* to) {
    if (to->serverTime - from->serverTime < 256) {
        MSG_WriteBits(msg, 1, 1);
        MSG_WriteBits(msg, to->serverTime - from->serverTime, 8);
    } else {
        MSG_WriteBits(msg, 0, 1);
        MSG_WriteBits(msg, to->serverTime, 32);
    }
    if (from->angles[0] == to->angles[0] &&
        from->angles[1] == to->angles[1] &&
        from->angles[2] == to->angles[2] &&
        from->forwardmove == to->forwardmove &&
        from->rightmove == to->rightmove &&
        from->upmove == to->upmove &&
        from->buttons == to->buttons &&
        from->weapon == to->weapon &&
        from->weaponPrimary == to->weaponPrimary &&
        from->fov == to->fov) {
        MSG_WriteBits(msg, 0, 1);
        oldsize += 7;
        return;
    }
    key ^= to->serverTime;
    MSG_WriteBits(msg, 1, 1);
    MSG_WriteDeltaKey(msg, key, from->angles[0], to->angles[0], 16);
    MSG_WriteDeltaKey(msg, key, from->angles[1], to->angles[1], 16);
    MSG_WriteDeltaKey(msg, key, from->angles[2], to->angles[2], 16);
    MSG_WriteDeltaKey(msg, key, from->forwardmove, to->forwardmove, 8);
    MSG_WriteDeltaKey(msg, key, from->rightmove, to->rightmove, 8);
    MSG_WriteDeltaKey(msg, key, from->upmove, to->upmove, 8);
    MSG_WriteDeltaKey(msg, key, from->buttons, to->buttons, 16);
    MSG_WriteDeltaKey(msg, key, from->weapon, to->weapon, 8);
    MSG_WriteDeltaKey(msg, key, from->weaponPrimary, to->weaponPrimary, 8);
    MSG_WriteDeltaKey(msg, key, from->fov, to->fov, 8);
}

void MSG_ReadDeltaUsercmdKey(msg_t* msg, int key, usercmd_t* from, usercmd_t* to) {
    if (MSG_ReadBits(msg, 1)) {
        to->serverTime = from->serverTime + MSG_ReadBits(msg, 8);
    } else {
        to->serverTime = MSG_ReadBits(msg, 32);
    }
    if (MSG_ReadBits(msg, 1)) {
        key ^= to->serverTime;
        to->angles[0] = MSG_ReadDeltaKey(msg, key, from->angles[0], 16);
        to->angles[1] = MSG_ReadDeltaKey(msg, key, from->angles[1], 16);
        to->angles[2] = MSG_ReadDeltaKey(msg, key, from->angles[2], 16);
        to->forwardmove = MSG_ReadDeltaKey(msg, key, from->forwardmove, 8);
        if (to->forwardmove == -128)
            to->forwardmove = -127;
        to->rightmove = MSG_ReadDeltaKey(msg, key, from->rightmove, 8);
        if (to->rightmove == -128)
            to->rightmove = -127;
        to->upmove = MSG_ReadDeltaKey(msg, key, from->upmove, 8);
        if (to->upmove == -128)
            to->upmove = -127;
        to->buttons = MSG_ReadDeltaKey(msg, key, from->buttons, 16);
        to->weapon = MSG_ReadDeltaKey(msg, key, from->weapon, 8);
        to->weaponPrimary = MSG_ReadDeltaKey(msg, key, from->weaponPrimary, 8);
        to->fov = MSG_ReadDeltaKey(msg, key, from->fov, 8);
    } else {
        to->angles[0] = from->angles[0];
        to->angles[1] = from->angles[1];
        to->angles[2] = from->angles[2];
        to->forwardmove = from->forwardmove;
        to->rightmove = from->rightmove;
        to->upmove = from->upmove;
        to->buttons = from->buttons;
        to->weapon = from->weapon;
        to->weaponPrimary = from->weaponPrimary;
        to->fov = from->fov;
    }
}

void MSG_ReportChangeVectors_f(void) {
    int i;
    for (i = 0; i < 256; i++) {
        if (pcount[i]) {
            Com_Printf("%d used %d\n", i, pcount[i]);
        }
    }
}

typedef struct {
    char* name;
    int offset;
    int bits;
} netField_t;

#define NETF(x) #x, (size_t)&((entityState_t*)0)->x

netField_t entityStateFields[] =
    {
        {NETF(pos.trTime), 32},
        {NETF(pos.trBase[0]), 0},
        {NETF(pos.trBase[1]), 0},
        {NETF(pos.trDelta[0]), 0},
        {NETF(pos.trDelta[1]), 0},
        {NETF(pos.trBase[2]), 0},
        {NETF(apos.trBase[1]), 0},
        {NETF(pos.trDelta[2]), 0},
        {NETF(apos.trBase[0]), 0},
        {NETF(pos.gravity), 32},
        {NETF(event), 10},
        {NETF(angles2[1]), 0},
        {NETF(eType), 8},
        {NETF(torsoAnim), 8},
        {NETF(eventParm), 8},
        {NETF(legsAnim), 8},
        {NETF(groundEntityNum), GENTITYNUM_BITS},
        {NETF(pos.trType), 8},
        {NETF(eFlags), 19},
        {NETF(otherEntityNum), GENTITYNUM_BITS},
        {NETF(weapon), 8},
        {NETF(clientNum), 8},
        {NETF(angles[1]), 0},
        {NETF(pos.trDuration), 32},
        {NETF(apos.trType), 8},
        {NETF(origin[0]), 0},
        {NETF(origin[1]), 0},
        {NETF(origin[2]), 0},
        {NETF(solid), 24},
        {NETF(powerups), MAX_POWERUPS},
        {NETF(modelindex), 8},
        {NETF(otherEntityNum2), GENTITYNUM_BITS},
        {NETF(loopSound), 8},
        {NETF(generic1), 8},
        {NETF(origin2[2]), 0},
        {NETF(origin2[0]), 0},
        {NETF(origin2[1]), 0},
        {NETF(modelindex2), 8},
        {NETF(angles[0]), 0},
        {NETF(time), 32},
        {NETF(apos.trTime), 32},
        {NETF(apos.trDuration), 32},
        {NETF(apos.trBase[2]), 0},
        {NETF(apos.trDelta[0]), 0},
        {NETF(apos.trDelta[1]), 0},
        {NETF(apos.trDelta[2]), 0},
        {NETF(apos.gravity), 32},
        {NETF(time2), 32},
        {NETF(angles[2]), 0},
        {NETF(angles2[0]), 0},
        {NETF(angles2[2]), 0},
        {NETF(constantLight), 32},
        {NETF(frame), 16},
        {NETF(jumpTime), 32},
        {NETF(doubleJumped), 1},
        {NETF(health), 16},
        {NETF(armor), 16},
        {NETF(location), 8},
};

#define FLOAT_INT_BITS 13
#define FLOAT_INT_BIAS (1 << (FLOAT_INT_BITS - 1))

void MSG_WriteDeltaEntity(msg_t* msg, struct entityState_s* from, struct entityState_s* to, qboolean force) {
    int i, lc;
    int numFields;
    netField_t* field;
    int trunc;
    float fullFloat;
    int *fromF, *toF;

    numFields = ARRAY_LEN(entityStateFields);

    assert(numFields + 1 == sizeof(*from) / 4);

    if (to == NULL) {
        if (from == NULL) {
            return;
        }
        MSG_WriteBits(msg, from->number, GENTITYNUM_BITS);
        MSG_WriteBits(msg, 1, 1);
        return;
    }

    if (to->number < 0 || to->number >= MAX_GENTITIES) {
        Com_Error(ERR_FATAL, "MSG_WriteDeltaEntity: Bad entity number: %i", to->number);
    }

    lc = 0;
    for (i = 0, field = entityStateFields; i < numFields; i++, field++) {
        fromF = (int*)((byte*)from + field->offset);
        toF = (int*)((byte*)to + field->offset);
        if (*fromF != *toF) {
            lc = i + 1;
        }
    }

    if (lc == 0) {
        if (!force) {
            return;
        }
        MSG_WriteBits(msg, to->number, GENTITYNUM_BITS);
        MSG_WriteBits(msg, 0, 1);
        MSG_WriteBits(msg, 0, 1);
        return;
    }

    MSG_WriteBits(msg, to->number, GENTITYNUM_BITS);
    MSG_WriteBits(msg, 0, 1);
    MSG_WriteBits(msg, 1, 1);

    MSG_WriteByte(msg, lc);

    oldsize += numFields;

    for (i = 0, field = entityStateFields; i < lc; i++, field++) {
        fromF = (int*)((byte*)from + field->offset);
        toF = (int*)((byte*)to + field->offset);

        if (*fromF == *toF) {
            MSG_WriteBits(msg, 0, 1);
            continue;
        }

        MSG_WriteBits(msg, 1, 1);

        if (field->bits == 0) {
            fullFloat = *(float*)toF;
            trunc = (int)fullFloat;

            if (fullFloat == 0.0f) {
                MSG_WriteBits(msg, 0, 1);
                oldsize += FLOAT_INT_BITS;
            } else {
                MSG_WriteBits(msg, 1, 1);
                if (trunc == fullFloat && trunc + FLOAT_INT_BIAS >= 0 &&
                    trunc + FLOAT_INT_BIAS < (1 << FLOAT_INT_BITS)) {
                    MSG_WriteBits(msg, 0, 1);
                    MSG_WriteBits(msg, trunc + FLOAT_INT_BIAS, FLOAT_INT_BITS);
                } else {
                    MSG_WriteBits(msg, 1, 1);
                    MSG_WriteBits(msg, *toF, 32);
                }
            }
        } else {
            if (*toF == 0) {
                MSG_WriteBits(msg, 0, 1);
            } else {
                MSG_WriteBits(msg, 1, 1);
                MSG_WriteBits(msg, *toF, field->bits);
            }
        }
    }
}

void MSG_ReadDeltaEntity(msg_t* msg, entityState_t* from, entityState_t* to, int number) {
    int i, lc;
    int numFields;
    netField_t* field;
    int *fromF, *toF;
    int print;
    int trunc;
    int startBit, endBit;

    if (number < 0 || number >= MAX_GENTITIES) {
        Com_Error(ERR_DROP, "Bad delta entity number: %i", number);
    }

    if (msg->bit == 0) {
        startBit = msg->readcount * 8 - GENTITYNUM_BITS;
    } else {
        startBit = (msg->readcount - 1) * 8 + msg->bit - GENTITYNUM_BITS;
    }

    if (MSG_ReadBits(msg, 1) == 1) {
        Com_Memset(to, 0, sizeof(*to));
        to->number = MAX_GENTITIES - 1;
        if (cl_shownet && (cl_shownet->integer >= 2 || cl_shownet->integer == -1)) {
            Com_Printf("%3i: #%-3i remove\n", msg->readcount, number);
        }
        return;
    }

    if (MSG_ReadBits(msg, 1) == 0) {
        *to = *from;
        to->number = number;
        return;
    }

    numFields = ARRAY_LEN(entityStateFields);
    lc = MSG_ReadByte(msg);

    if (lc > numFields || lc < 0) {
        Com_Error(ERR_DROP, "invalid entityState field count");
    }

    if (cl_shownet && (cl_shownet->integer >= 2 || cl_shownet->integer == -1)) {
        print = 1;
        Com_Printf("%3i: #%-3i ", msg->readcount, to->number);
    } else {
        print = 0;
    }

    to->number = number;

    for (i = 0, field = entityStateFields; i < lc; i++, field++) {
        fromF = (int*)((byte*)from + field->offset);
        toF = (int*)((byte*)to + field->offset);

        if (!MSG_ReadBits(msg, 1)) {
            *toF = *fromF;
        } else {
            if (field->bits == 0) {
                if (MSG_ReadBits(msg, 1) == 0) {
                    *(float*)toF = 0.0f;
                } else {
                    if (MSG_ReadBits(msg, 1) == 0) {
                        trunc = MSG_ReadBits(msg, FLOAT_INT_BITS);
                        trunc -= FLOAT_INT_BIAS;
                        *(float*)toF = trunc;
                        if (print) {
                            Com_Printf("%s:%i ", field->name, trunc);
                        }
                    } else {
                        *toF = MSG_ReadBits(msg, 32);
                        if (print) {
                            Com_Printf("%s:%f ", field->name, *(float*)toF);
                        }
                    }
                }
            } else {
                if (MSG_ReadBits(msg, 1) == 0) {
                    *toF = 0;
                } else {
                    *toF = MSG_ReadBits(msg, field->bits);
                    if (print) {
                        Com_Printf("%s:%i ", field->name, *toF);
                    }
                }
            }
        }
    }
    for (i = lc, field = &entityStateFields[lc]; i < numFields; i++, field++) {
        fromF = (int*)((byte*)from + field->offset);
        toF = (int*)((byte*)to + field->offset);
        *toF = *fromF;
    }

    if (print) {
        if (msg->bit == 0) {
            endBit = msg->readcount * 8 - GENTITYNUM_BITS;
        } else {
            endBit = (msg->readcount - 1) * 8 + msg->bit - GENTITYNUM_BITS;
        }
        Com_Printf(" (%i bits)\n", endBit - startBit);
    }
}

#define PSF(x) #x, (size_t)&((playerState_t*)0)->x

netField_t playerStateFields[] =
    {
        {PSF(commandTime), 32},
        {PSF(origin[0]), 0},
        {PSF(origin[1]), 0},
        {PSF(bobCycle), 8},
        {PSF(velocity[0]), 0},
        {PSF(velocity[1]), 0},
        {PSF(viewangles[1]), 0},
        {PSF(viewangles[0]), 0},
        {PSF(weaponTime), -16},
        {PSF(origin[2]), 0},
        {PSF(velocity[2]), 0},
        {PSF(legsTimer), 8},
        {PSF(pm_time), -16},
        {PSF(eventSequence), 16},
        {PSF(torsoAnim), 8},
        {PSF(movementDir), 4},
        {PSF(events[0]), 8},
        {PSF(legsAnim), 8},
        {PSF(events[1]), 8},
        {PSF(pm_flags), 24},
        {PSF(groundEntityNum), GENTITYNUM_BITS},
        {PSF(weaponstate), 4},
        {PSF(eFlags), 16},
        {PSF(externalEvent), 10},
        {PSF(gravity), 16},
        {PSF(speed), 16},
        {PSF(delta_angles[1]), 16},
        {PSF(externalEventParm), 8},
        {PSF(viewheight), -8},
        {PSF(damageEvent), 8},
        {PSF(damageYaw), 8},
        {PSF(damagePitch), 8},
        {PSF(damageCount), 8},
        {PSF(generic1), 8},
        {PSF(pm_type), 8},
        {PSF(delta_angles[0]), 16},
        {PSF(delta_angles[2]), 16},
        {PSF(torsoTimer), 12},
        {PSF(eventParms[0]), 8},
        {PSF(eventParms[1]), 8},
        {PSF(clientNum), 8},
        {PSF(weapon), 5},
        {PSF(weaponPrimary), 8},
        {PSF(viewangles[2]), 0},
        {PSF(grapplePoint[0]), 0},
        {PSF(grapplePoint[1]), 0},
        {PSF(grapplePoint[2]), 0},
        {PSF(jumppad_ent), GENTITYNUM_BITS},
        {PSF(loopSound), 16},
        {PSF(jumpTime), 32},
        {PSF(doubleJumped), 1},
        {PSF(crouchTime), 32},
        {PSF(crouchSlideTime), 32},
        {PSF(location), 8},
        {PSF(fov), 8},
        {PSF(forwardmove), 8},
        {PSF(rightmove), 8},
        {PSF(upmove), 8},
};

void MSG_WriteDeltaPlayerstate(msg_t* msg, struct playerState_s* from, struct playerState_s* to) {
    int i;
    playerState_t dummy;
    int statsbits;
    int persistantbits;
    int ammobits;
    int powerupbits;
    int numFields;
    netField_t* field;
    int *fromF, *toF;
    float fullFloat;
    int trunc, lc;

    if (!from) {
        from = &dummy;
        Com_Memset(&dummy, 0, sizeof(dummy));
    }

    numFields = ARRAY_LEN(playerStateFields);

    lc = 0;
    for (i = 0, field = playerStateFields; i < numFields; i++, field++) {
        fromF = (int*)((byte*)from + field->offset);
        toF = (int*)((byte*)to + field->offset);
        if (*fromF != *toF) {
            lc = i + 1;
        }
    }

    MSG_WriteByte(msg, lc);

    oldsize += numFields - lc;

    for (i = 0, field = playerStateFields; i < lc; i++, field++) {
        fromF = (int*)((byte*)from + field->offset);
        toF = (int*)((byte*)to + field->offset);

        if (*fromF == *toF) {
            MSG_WriteBits(msg, 0, 1);
            continue;
        }

        MSG_WriteBits(msg, 1, 1);

        if (field->bits == 0) {
            fullFloat = *(float*)toF;
            trunc = (int)fullFloat;

            if (trunc == fullFloat && trunc + FLOAT_INT_BIAS >= 0 &&
                trunc + FLOAT_INT_BIAS < (1 << FLOAT_INT_BITS)) {
                MSG_WriteBits(msg, 0, 1);
                MSG_WriteBits(msg, trunc + FLOAT_INT_BIAS, FLOAT_INT_BITS);
            } else {
                MSG_WriteBits(msg, 1, 1);
                MSG_WriteBits(msg, *toF, 32);
            }
        } else {
            MSG_WriteBits(msg, *toF, field->bits);
        }
    }

    statsbits = 0;
    for (i = 0; i < MAX_STATS; i++) {
        if (to->stats[i] != from->stats[i]) {
            statsbits |= 1 << i;
        }
    }
    persistantbits = 0;
    for (i = 0; i < MAX_PERSISTANT; i++) {
        if (to->persistant[i] != from->persistant[i]) {
            persistantbits |= 1 << i;
        }
    }
    ammobits = 0;
    for (i = 0; i < MAX_WEAPONS; i++) {
        if (to->ammo[i] != from->ammo[i]) {
            ammobits |= 1 << i;
        }
    }
    powerupbits = 0;
    for (i = 0; i < MAX_POWERUPS; i++) {
        if (to->powerups[i] != from->powerups[i]) {
            powerupbits |= 1 << i;
        }
    }

    if (!statsbits && !persistantbits && !ammobits && !powerupbits) {
        MSG_WriteBits(msg, 0, 1);
        oldsize += 4;
        return;
    }
    MSG_WriteBits(msg, 1, 1);

    if (statsbits) {
        MSG_WriteBits(msg, 1, 1);
        MSG_WriteBits(msg, statsbits, MAX_STATS);
        for (i = 0; i < MAX_STATS; i++)
            if (statsbits & (1 << i))
                MSG_WriteShort(msg, to->stats[i]);
    } else {
        MSG_WriteBits(msg, 0, 1);
    }

    if (persistantbits) {
        MSG_WriteBits(msg, 1, 1);
        MSG_WriteBits(msg, persistantbits, MAX_PERSISTANT);
        for (i = 0; i < MAX_PERSISTANT; i++)
            if (persistantbits & (1 << i))
                MSG_WriteShort(msg, to->persistant[i]);
    } else {
        MSG_WriteBits(msg, 0, 1);
    }

    if (ammobits) {
        MSG_WriteBits(msg, 1, 1);
        MSG_WriteBits(msg, ammobits, MAX_WEAPONS);
        for (i = 0; i < MAX_WEAPONS; i++)
            if (ammobits & (1 << i))
                MSG_WriteShort(msg, to->ammo[i]);
    } else {
        MSG_WriteBits(msg, 0, 1);
    }

    if (powerupbits) {
        MSG_WriteBits(msg, 1, 1);
        MSG_WriteBits(msg, powerupbits, MAX_POWERUPS);
        for (i = 0; i < MAX_POWERUPS; i++)
            if (powerupbits & (1 << i))
                MSG_WriteLong(msg, to->powerups[i]);
    } else {
        MSG_WriteBits(msg, 0, 1);
    }
}

void MSG_ReadDeltaPlayerstate(msg_t* msg, playerState_t* from, playerState_t* to) {
    int i, lc;
    int bits;
    netField_t* field;
    int numFields;
    int startBit, endBit;
    int print;
    int *fromF, *toF;
    int trunc;
    playerState_t dummy;

    if (!from) {
        from = &dummy;
        Com_Memset(&dummy, 0, sizeof(dummy));
    }
    *to = *from;

    if (msg->bit == 0) {
        startBit = msg->readcount * 8 - GENTITYNUM_BITS;
    } else {
        startBit = (msg->readcount - 1) * 8 + msg->bit - GENTITYNUM_BITS;
    }

    if (cl_shownet && (cl_shownet->integer >= 2 || cl_shownet->integer == -2)) {
        print = 1;
        Com_Printf("%3i: playerstate ", msg->readcount);
    } else {
        print = 0;
    }

    numFields = ARRAY_LEN(playerStateFields);
    lc = MSG_ReadByte(msg);

    if (lc > numFields || lc < 0) {
        Com_Error(ERR_DROP, "invalid playerState field count");
    }

    for (i = 0, field = playerStateFields; i < lc; i++, field++) {
        fromF = (int*)((byte*)from + field->offset);
        toF = (int*)((byte*)to + field->offset);

        if (!MSG_ReadBits(msg, 1)) {
            *toF = *fromF;
        } else {
            if (field->bits == 0) {
                if (MSG_ReadBits(msg, 1) == 0) {
                    trunc = MSG_ReadBits(msg, FLOAT_INT_BITS);
                    trunc -= FLOAT_INT_BIAS;
                    *(float*)toF = trunc;
                    if (print) {
                        Com_Printf("%s:%i ", field->name, trunc);
                    }
                } else {
                    *toF = MSG_ReadBits(msg, 32);
                    if (print) {
                        Com_Printf("%s:%f ", field->name, *(float*)toF);
                    }
                }
            } else {
                *toF = MSG_ReadBits(msg, field->bits);
                if (print) {
                    Com_Printf("%s:%i ", field->name, *toF);
                }
            }
        }
    }
    for (i = lc, field = &playerStateFields[lc]; i < numFields; i++, field++) {
        fromF = (int*)((byte*)from + field->offset);
        toF = (int*)((byte*)to + field->offset);
        *toF = *fromF;
    }

    if (MSG_ReadBits(msg, 1)) {
        if (MSG_ReadBits(msg, 1)) {
            LOG("PS_STATS");
            bits = MSG_ReadBits(msg, MAX_STATS);
            for (i = 0; i < MAX_STATS; i++) {
                if (bits & (1 << i)) {
                    to->stats[i] = MSG_ReadShort(msg);
                }
            }
        }

        if (MSG_ReadBits(msg, 1)) {
            LOG("PS_PERSISTANT");
            bits = MSG_ReadBits(msg, MAX_PERSISTANT);
            for (i = 0; i < MAX_PERSISTANT; i++) {
                if (bits & (1 << i)) {
                    to->persistant[i] = MSG_ReadShort(msg);
                }
            }
        }

        if (MSG_ReadBits(msg, 1)) {
            LOG("PS_AMMO");
            bits = MSG_ReadBits(msg, MAX_WEAPONS);
            for (i = 0; i < MAX_WEAPONS; i++) {
                if (bits & (1 << i)) {
                    to->ammo[i] = MSG_ReadShort(msg);
                }
            }
        }

        if (MSG_ReadBits(msg, 1)) {
            LOG("PS_POWERUPS");
            bits = MSG_ReadBits(msg, MAX_POWERUPS);
            for (i = 0; i < MAX_POWERUPS; i++) {
                if (bits & (1 << i)) {
                    to->powerups[i] = MSG_ReadLong(msg);
                }
            }
        }
    }

    if (print) {
        if (msg->bit == 0) {
            endBit = msg->readcount * 8 - GENTITYNUM_BITS;
        } else {
            endBit = (msg->readcount - 1) * 8 + msg->bit - GENTITYNUM_BITS;
        }
        Com_Printf(" (%i bits)\n", endBit - startBit);
    }
}

int msg_hData[256] = {
    250315,
    41193,
    6292,
    7106,
    3730,
    3750,
    6110,
    23283,
    33317,
    6950,
    7838,
    9714,
    9257,
    17259,
    3949,
    1778,
    8288,
    1604,
    1590,
    1663,
    1100,
    1213,
    1238,
    1134,
    1749,
    1059,
    1246,
    1149,
    1273,
    4486,
    2805,
    3472,
    21819,
    1159,
    1670,
    1066,
    1043,
    1012,
    1053,
    1070,
    1726,
    888,
    1180,
    850,
    960,
    780,
    1752,
    3296,
    10630,
    4514,
    5881,
    2685,
    4650,
    3837,
    2093,
    1867,
    2584,
    1949,
    1972,
    940,
    1134,
    1788,
    1670,
    1206,
    5719,
    6128,
    7222,
    6654,
    3710,
    3795,
    1492,
    1524,
    2215,
    1140,
    1355,
    971,
    2180,
    1248,
    1328,
    1195,
    1770,
    1078,
    1264,
    1266,
    1168,
    965,
    1155,
    1186,
    1347,
    1228,
    1529,
    1600,
    2617,
    2048,
    2546,
    3275,
    2410,
    3585,
    2504,
    2800,
    2675,
    6146,
    3663,
    2840,
    14253,
    3164,
    2221,
    1687,
    3208,
    2739,
    3512,
    4796,
    4091,
    3515,
    5288,
    4016,
    7937,
    6031,
    5360,
    3924,
    4892,
    3743,
    4566,
    4807,
    5852,
    6400,
    6225,
    8291,
    23243,
    7838,
    7073,
    8935,
    5437,
    4483,
    3641,
    5256,
    5312,
    5328,
    5370,
    3492,
    2458,
    1694,
    1821,
    2121,
    1916,
    1149,
    1516,
    1367,
    1236,
    1029,
    1258,
    1104,
    1245,
    1006,
    1149,
    1025,
    1241,
    952,
    1287,
    997,
    1713,
    1009,
    1187,
    879,
    1099,
    929,
    1078,
    951,
    1656,
    930,
    1153,
    1030,
    1262,
    1062,
    1214,
    1060,
    1621,
    930,
    1106,
    912,
    1034,
    892,
    1158,
    990,
    1175,
    850,
    1121,
    903,
    1087,
    920,
    1144,
    1056,
    3462,
    2240,
    4397,
    12136,
    7758,
    1345,
    1307,
    3278,
    1950,
    886,
    1023,
    1112,
    1077,
    1042,
    1061,
    1071,
    1484,
    1001,
    1096,
    915,
    1052,
    995,
    1070,
    876,
    1111,
    851,
    1059,
    805,
    1112,
    923,
    1103,
    817,
    1899,
    1872,
    976,
    841,
    1127,
    956,
    1159,
    950,
    7791,
    954,
    1289,
    933,
    1127,
    3207,
    1020,
    927,
    1355,
    768,
    1040,
    745,
    952,
    805,
    1073,
    740,
    1013,
    805,
    1008,
    796,
    996,
    1057,
    11457,
    13504,
};

void MSG_initHuffman(void) {
    int i, j;

    msgInit = qtrue;
    Huff_Init(&msgHuff);
    for (i = 0; i < 256; i++) {
        for (j = 0; j < msg_hData[i]; j++) {
            Huff_addRef(&msgHuff.compressor, (byte)i);
            Huff_addRef(&msgHuff.decompressor, (byte)i);
        }
    }
}

