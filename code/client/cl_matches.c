/*
===========================================================================
ioquakelive - the matches one has played, for the front end's Recent Matches

[QL] When a match ends the server sends each player its report over Steam's peer to
peer network (see sv_steamstats.c). 0x00461d40 reads it, inflates it into at most a
megabyte and hands it to the web UI, whose saveMatchStats (web.pak src/mod.js) kept it
in the browser's localStorage in three pieces: the match without its players and deaths
(match_<guid>), the players (players_<guid>) and the deaths (events_<guid>), the guid
last in match_index. Before that it worked out how the match went for oneself: the first
of one's own PLAYER_STATS that did not quit says WIN or LOSE, and with none it is QUIT;
that one is the match's PERSONAL_STATS, and TIME_PLAYED is when it was saved. Then it
printed "Match stats saved.".

Here the three pieces are files under matches/ and the index is matches/index.txt, a guid
a line. The guid comes from the server, so a report whose guid is anything but letters,
digits and dashes is not kept.
===========================================================================
*/

#include "client.h"
#include "../qcommon/q_steam.h"
#include "../qcommon/json.h"
#include "zlib.h"

#include <time.h>

#define CL_REPORT_MAX 0x100000

// a member of an object, or NULL
static const char* CL_Member(const char* obj, const char* end, const char* name) {
    if (!obj || obj >= end || *obj != '{') {
        return NULL;
    }
    return JSON_ObjectGetNamedValue(obj, end, name);
}

static qboolean CL_MatchGuid(const char* report, const char* end, char* guid, int size) {
    const char* v = CL_Member(report, end, "MATCH_GUID");
    int i;

    guid[0] = '\0';
    if (!v || JSON_ValueGetType(v, end) != JSONTYPE_STRING) {
        return qfalse;
    }
    JSON_ValueGetString(v, end, guid, (unsigned int)size);
    for (i = 0; guid[i]; i++) {
        if (!isalnum((unsigned char)guid[i]) && guid[i] != '-') {
            return qfalse;
        }
    }
    return (qboolean)(i > 0);
}

static void CL_WriteFile(const char* name, const char* a, int aLen, const char* b, int bLen, const char* c,
                         int cLen) {
    fileHandle_t f = FS_FOpenFileWrite(name);

    if (!f) {
        return;
    }
    FS_Write(a, aLen, f);
    if (b) {
        FS_Write(b, bLen, f);
    }
    if (c) {
        FS_Write(c, cLen, f);
    }
    FS_FCloseFile(f);
}

/*
===============
CL_SaveMatchReport
===============
*/
static void CL_SaveMatchReport(const char* report, int len) {
    const char* end = report + len;
    const char* players = CL_Member(report, end, "PLYR_STATS");
    const char* events = CL_Member(report, end, "PLYR_EVENTS");
    const char* personal = NULL;
    const char* status = "QUIT";
    char guid[128];
    char self[32];
    char extra[128];
    fileHandle_t f;
    const char* k;
    char* basics;
    int n = 0;

    if (!CL_MatchGuid(report, end, guid, sizeof(guid))) {
        return;
    }

    // how it went for oneself: the first of one's own entries that did not quit. Without
    // Steam there is no telling which is one's own, and a bot's Steam ID is "0"
    Com_sprintf(self, sizeof(self), "%llu", (unsigned long long)Steam_ClientSteamID());
    for (k = strcmp(self, "0") ? JSON_ArrayGetFirstValue(players, end) : NULL; k;
         k = JSON_ArrayGetNextValue(k, end)) {
        const char* id = CL_Member(k, end, "STEAM_ID");
        const char* quit = CL_Member(k, end, "QUIT");
        char text[32];

        if (!id || JSON_ValueGetType(id, end) != JSONTYPE_STRING || !quit || JSON_ValueGetType(quit, end) != JSONTYPE_VALUE) {
            continue;
        }
        JSON_ValueGetString(id, end, text, sizeof(text));
        if (strcmp(text, self) || JSON_ValueGetDouble(quit, end) != 0) {
            continue;
        }
        personal = k;
        status = JSON_ValueGetDouble(CL_Member(k, end, "WIN"), end) == 1 ? "WIN" : "LOSE";
        break;
    }

    // the match: its members but the players and the deaths, and then what was worked out
    basics = (char*)malloc(len + 256 + (personal ? JSON_ValueGetEnd(personal, end) - personal : 0));
    if (!basics) {
        return;
    }
    basics[n++] = '{';
    for (k = JSON_ArrayGetFirstValue(report, end); k; k = JSON_ArrayGetNextValue(k, end)) {
        const char* value = JSON_ArrayGetNextValue(k, end);
        const char* valueEnd;

        if (!value) {
            break;
        }
        valueEnd = JSON_ValueGetEnd(value, end);
        if (value != players && value != events) {
            if (n > 1) {
                basics[n++] = ',';
            }
            Com_Memcpy(basics + n, k, JSON_ValueGetEnd(k, end) - k);
            n += (int)(JSON_ValueGetEnd(k, end) - k);
            basics[n++] = ':';
            Com_Memcpy(basics + n, value, valueEnd - value);
            n += (int)(valueEnd - value);
        }
        k = value;
    }
    if (n > 1) {
        basics[n++] = ',';
    }
    Com_Memcpy(basics + n, "\"PERSONAL_STATS\":", 17);
    n += 17;
    if (personal) {
        Com_Memcpy(basics + n, personal, JSON_ValueGetEnd(personal, end) - personal);
        n += (int)(JSON_ValueGetEnd(personal, end) - personal);
    } else {
        Com_Memcpy(basics + n, "null", 4);
        n += 4;
    }
    Com_sprintf(extra, sizeof(extra), ",\"STATUS\":\"%s\",\"TIME_PLAYED\":%lld,\"MEDIA\":[]}", status,
                (long long)time(NULL) * 1000);
    Com_Memcpy(basics + n, extra, strlen(extra));
    n += (int)strlen(extra);

    CL_WriteFile(va("matches/%s.json", guid), basics, n, NULL, 0, NULL, 0);
    free(basics);
    if (players) {
        CL_WriteFile(va("matches/%s.players.json", guid), "{\"PLYR_STATS\":", 14, players,
                     (int)(JSON_ValueGetEnd(players, end) - players), "}", 1);
    }
    if (events) {
        CL_WriteFile(va("matches/%s.events.json", guid), "{\"PLYR_EVENTS\":", 15, events,
                     (int)(JSON_ValueGetEnd(events, end) - events), "}", 1);
    }

    f = FS_FOpenFileAppend("matches/index.txt");
    if (f) {
        FS_Write(guid, (int)strlen(guid), f);
        FS_Write("\n", 1, f);
        FS_FCloseFile(f);
    }
    Com_Printf("Match stats saved.\n");
}


/*
===============
CL_MatchReportFrame

[QL] 0x00461d40, every frame the client runs Steam's callbacks, before the voice
===============
*/
void CL_MatchReportFrame(void) {
    void* packed;
    int size;

    while ((packed = Steam_P2PReadReport(&size)) != NULL) {
        Bytef* report = (Bytef*)malloc(CL_REPORT_MAX + 1);
        uLongf len = CL_REPORT_MAX;

        if (report && uncompress(report, &len, (const Bytef*)packed, (uLong)size) == Z_OK) {
            report[len] = '\0';
            CL_SaveMatchReport((const char*)report, (int)len);
        }
        free(report);
        free(packed);
    }
}
