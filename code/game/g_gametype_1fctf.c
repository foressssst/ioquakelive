#include "g_local.h"

extern qboolean itemRegistered[];

void OneFCTF_CheckTeamItems(void) {
    gitem_t *item;
    item = BG_FindItem("Red Flag");
    if (!item || !itemRegistered[item - bg_itemlist]) {
        G_Printf(S_COLOR_YELLOW "WARNING: No team_CTF_redflag in map\n");
    }
    item = BG_FindItem("Blue Flag");
    if (!item || !itemRegistered[item - bg_itemlist]) {
        G_Printf(S_COLOR_YELLOW "WARNING: No team_CTF_blueflag in map\n");
    }
    item = BG_FindItem("Neutral Flag");
    if (!item || !itemRegistered[item - bg_itemlist]) {
        G_Printf(S_COLOR_YELLOW "WARNING: No team_CTF_neutralflag in map\n");
    }
}
