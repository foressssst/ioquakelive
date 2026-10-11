#include "g_local.h"

void TeamDeathmatchScoreboardMessage(gentity_t *ent) {
    char entry[1024];
    char string[1024];
    int listEnd[MAX_CLIENTS];
    int stringlength;
    int i, j;
    gclient_t *cl;
    int viewerTeam;
    int redStats[14], blueStats[14];

    string[0] = 0;
    stringlength = 0;

    redStats[0]  = level.numRedArmorPickups[TEAM_RED];
    redStats[1]  = level.numYellowArmorPickups[TEAM_RED];
    redStats[2]  = level.numGreenArmorPickups[TEAM_RED];
    redStats[3]  = level.numMegaHealthPickups[TEAM_RED];
    redStats[4]  = level.numQuadDamagePickups[TEAM_RED];
    redStats[5]  = level.numBattleSuitPickups[TEAM_RED];
    redStats[6]  = level.numRegenerationPickups[TEAM_RED];
    redStats[7]  = level.numHastePickups[TEAM_RED];
    redStats[8]  = level.numInvisibilityPickups[TEAM_RED];
    redStats[9]  = level.quadDamagePossessionTime[TEAM_RED];
    redStats[10] = level.battleSuitPossessionTime[TEAM_RED];
    redStats[11] = level.regenerationPossessionTime[TEAM_RED];
    redStats[12] = level.hastePossessionTime[TEAM_RED];
    redStats[13] = level.invisibilityPossessionTime[TEAM_RED];

    blueStats[0]  = level.numRedArmorPickups[TEAM_BLUE];
    blueStats[1]  = level.numYellowArmorPickups[TEAM_BLUE];
    blueStats[2]  = level.numGreenArmorPickups[TEAM_BLUE];
    blueStats[3]  = level.numMegaHealthPickups[TEAM_BLUE];
    blueStats[4]  = level.numQuadDamagePickups[TEAM_BLUE];
    blueStats[5]  = level.numBattleSuitPickups[TEAM_BLUE];
    blueStats[6]  = level.numRegenerationPickups[TEAM_BLUE];
    blueStats[7]  = level.numHastePickups[TEAM_BLUE];
    blueStats[8]  = level.numInvisibilityPickups[TEAM_BLUE];
    blueStats[9]  = level.quadDamagePossessionTime[TEAM_BLUE];
    blueStats[10] = level.battleSuitPossessionTime[TEAM_BLUE];
    blueStats[11] = level.regenerationPossessionTime[TEAM_BLUE];
    blueStats[12] = level.hastePossessionTime[TEAM_BLUE];
    blueStats[13] = level.invisibilityPossessionTime[TEAM_BLUE];

    for (i = 0; i < level.numConnectedClients; i++) {
        int ping, accuracy, bestWeapon;

        cl = &level.clients[level.sortedClients[i]];

        if (cl->accuracy_shots) {
            accuracy = cl->accuracy_hits * 100 / cl->accuracy_shots;
        } else {
            accuracy = 0;
        }

        if (cl->pers.connected == CON_CONNECTING) {
            ping = -1;
        } else {
            ping = cl->ps.ping < 999 ? cl->ps.ping : 999;
        }

        bestWeapon = STAT_GetBestWeapon(cl);

        viewerTeam = ent->client->sess.sessionTeam;
        if (viewerTeam == TEAM_RED && level.intermissionTime == 0) {
            for (j = 0; j < 14; j++) {
                if (j != 8)
                    blueStats[j] = 0;
            }
        } else if (viewerTeam == TEAM_BLUE && level.intermissionTime == 0) {
            for (j = 0; j < 14; j++) {
                if (j != 8)
                    redStats[j] = 0;
            }
        }

        Com_sprintf(entry, sizeof(entry), " %i %i %i %i %i %i %i %i %i %i %i %i %i %i %i",
                    level.sortedClients[i],
                    cl->sess.sessionTeam,
                    cl->ps.persistant[PERS_SCORE], ping,
                    (level.time - cl->pers.enterTime) / 60000,
                    cl->expandedStats.numKills, cl->expandedStats.numDeaths,
                    accuracy, bestWeapon,
                    cl->ps.persistant[PERS_IMPRESSIVE_COUNT],
                    cl->ps.persistant[PERS_EXCELLENT_COUNT],
                    cl->ps.persistant[PERS_GAUNTLET_FRAG_COUNT],
                    cl->expandedStats.numTeamKills,
                    cl->expandedStats.numTeamKilled,
                    cl->expandedStats.totalDamageDealt);
        j = strlen(entry);
        if (stringlength + j >= (int)sizeof(string))
            break;
        strcpy(string + stringlength, entry);
        stringlength += j;
        listEnd[i] = stringlength;
    }

    for (;;) {
        const char *cmd = va("scores_tdm %i %i %i %i %i %i %i %i %i %i %i %i %i %i "
               "%i %i %i %i %i %i %i %i %i %i %i %i %i %i %i %i %i%s",
               redStats[0], redStats[1], redStats[2], redStats[3], redStats[4],
               redStats[5], redStats[6], redStats[7], redStats[8],
               redStats[9], redStats[10], redStats[11], redStats[12], redStats[13],
               blueStats[0], blueStats[1], blueStats[2], blueStats[3], blueStats[4],
               blueStats[5], blueStats[6], blueStats[7], blueStats[8],
               blueStats[9], blueStats[10], blueStats[11], blueStats[12], blueStats[13],
               i, level.teamScores[TEAM_RED], level.teamScores[TEAM_BLUE],
               string);

        if (strlen(cmd) <= 1022 || i == 0) {
            trap_SendServerCommand(ent - g_entities, cmd);
            break;
        }
        i--;
        string[i ? listEnd[i - 1] : 0] = '\0';
    }
}

void TeamDeathmatchStatisticsMessage(gentity_t *ent) {
    char entry[1024];
    char string[1024];
    int i;
    gclient_t *cl;

    for (i = 0; i < level.numConnectedClients; i++) {
        cl = &level.clients[level.sortedClients[i]];

        string[0] = 0;
        Com_sprintf(entry, sizeof(entry),
                    " %i %i %i %i %i %i %i %i %i %i %i",
                    cl->expandedStats.numSuicides,
                    cl->expandedStats.numTeamKills,
                    cl->expandedStats.numTeamKilled,
                    cl->expandedStats.totalDamageDealt,
                    cl->expandedStats.totalDamageTaken,
                    cl->expandedStats.numRedArmorPickups,
                    cl->expandedStats.numYellowArmorPickups,
                    cl->expandedStats.numGreenArmorPickups,
                    cl->expandedStats.numMegaHealthPickups,
                    cl->expandedStats.numQuadDamagePickups,
                    cl->expandedStats.numBattleSuitPickups);
        if (strlen(entry) > sizeof(string) - 1)
            break;
        strcpy(string, entry);
        trap_SendServerCommand(ent - g_entities,
            va("tdmstats %i%s", i, string));
    }
}
