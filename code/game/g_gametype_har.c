#include "g_local.h"

void Harvester_CheckTeamItems(void) {
    gentity_t *ent;
    ent = G_Find(NULL, FOFS(classname), "team_redobelisk");
    if (!ent) {
        G_Printf(S_COLOR_YELLOW "WARNING: No team_redobelisk in map\n");
    }
    ent = G_Find(NULL, FOFS(classname), "team_blueobelisk");
    if (!ent) {
        G_Printf(S_COLOR_YELLOW "WARNING: No team_blueobelisk in map\n");
    }
    ent = G_Find(NULL, FOFS(classname), "team_neutralobelisk");
    if (!ent) {
        G_Printf(S_COLOR_YELLOW "WARNING: No team_neutralobelisk in map\n");
    }
}

void Harvester_RegisterItems(void) {
    RegisterItem(BG_FindItem("Red Skull"));
    RegisterItem(BG_FindItem("Blue Skull"));
}
