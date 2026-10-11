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
#include "client.h"

unsigned frame_msec;
int old_com_frameTime;

kbutton_t in_left, in_right, in_forward, in_back;
kbutton_t in_lookup, in_lookdown, in_moveleft, in_moveright;
kbutton_t in_strafe, in_speed;
kbutton_t in_up, in_down;

kbutton_t in_buttons[16];

qboolean in_mlooking;

void IN_MLookDown(void) {
    in_mlooking = qtrue;
}

void IN_MLookUp(void) {
    in_mlooking = qfalse;
    if (!cl_freelook->integer) {
        IN_CenterView();
    }
}

void IN_KeyDown(kbutton_t* b) {
    int k;
    char* c;

    c = Cmd_Argv(1);
    if (c[0]) {
        k = atoi(c);
    } else {
        k = -1;
    }

    if (k == b->down[0] || k == b->down[1]) {
        return;
    }

    if (!b->down[0]) {
        b->down[0] = k;
    } else if (!b->down[1]) {
        b->down[1] = k;
    } else {
        Com_Printf("Three keys down for a button!\n");
        return;
    }

    if (b->active) {
        return;
    }

    c = Cmd_Argv(2);
    b->downtime = atoi(c);

    b->active = qtrue;
    b->wasPressed = qtrue;
}

void IN_KeyUp(kbutton_t* b) {
    int k;
    char* c;
    unsigned uptime;

    c = Cmd_Argv(1);
    if (c[0]) {
        k = atoi(c);
    } else {
        b->down[0] = b->down[1] = 0;
        b->active = qfalse;
        return;
    }

    if (b->down[0] == k) {
        b->down[0] = 0;
    } else if (b->down[1] == k) {
        b->down[1] = 0;
    } else {
        return;
    }
    if (b->down[0] || b->down[1]) {
        return;
    }

    b->active = qfalse;

    c = Cmd_Argv(2);
    uptime = atoi(c);
    if (uptime) {
        b->msec += uptime - b->downtime;
    } else {
        b->msec += frame_msec / 2;
    }

    b->active = qfalse;
}

float CL_KeyState(kbutton_t* key) {
    float val;
    int msec;

    msec = key->msec;
    key->msec = 0;

    if (key->active) {
        if (!key->downtime) {
            msec = com_frameTime;
        } else {
            msec += com_frameTime - key->downtime;
        }
        key->downtime = com_frameTime;
    }

#if 0
	if (msec) {
		Com_Printf ("%i ", msec);
	}
#endif

    val = (float)msec / frame_msec;
    if (val < 0) {
        val = 0;
    }
    if (val > 1) {
        val = 1;
    }

    return val;
}

void IN_UpDown(void) {
    IN_KeyDown(&in_up);
}
void IN_UpUp(void) {
    IN_KeyUp(&in_up);
}
void IN_DownDown(void) {
    IN_KeyDown(&in_down);
}
void IN_DownUp(void) {
    IN_KeyUp(&in_down);
}
void IN_LeftDown(void) {
    IN_KeyDown(&in_left);
}
void IN_LeftUp(void) {
    IN_KeyUp(&in_left);
}
void IN_RightDown(void) {
    IN_KeyDown(&in_right);
}
void IN_RightUp(void) {
    IN_KeyUp(&in_right);
}
void IN_ForwardDown(void) {
    IN_KeyDown(&in_forward);
}
void IN_ForwardUp(void) {
    IN_KeyUp(&in_forward);
}
void IN_BackDown(void) {
    IN_KeyDown(&in_back);
}
void IN_BackUp(void) {
    IN_KeyUp(&in_back);
}
void IN_LookupDown(void) {
    IN_KeyDown(&in_lookup);
}
void IN_LookupUp(void) {
    IN_KeyUp(&in_lookup);
}
void IN_LookdownDown(void) {
    IN_KeyDown(&in_lookdown);
}
void IN_LookdownUp(void) {
    IN_KeyUp(&in_lookdown);
}
void IN_MoveleftDown(void) {
    IN_KeyDown(&in_moveleft);
}
void IN_MoveleftUp(void) {
    IN_KeyUp(&in_moveleft);
}
void IN_MoverightDown(void) {
    IN_KeyDown(&in_moveright);
}
void IN_MoverightUp(void) {
    IN_KeyUp(&in_moveright);
}

void IN_SpeedDown(void) {
    IN_KeyDown(&in_speed);
}
void IN_SpeedUp(void) {
    IN_KeyUp(&in_speed);
}
void IN_StrafeDown(void) {
    IN_KeyDown(&in_strafe);
}
void IN_StrafeUp(void) {
    IN_KeyUp(&in_strafe);
}

void IN_Button0Down(void) {
    IN_KeyDown(&in_buttons[0]);
}
void IN_Button0Up(void) {
    IN_KeyUp(&in_buttons[0]);
}
void IN_Button1Down(void) {
    IN_KeyDown(&in_buttons[1]);
}
void IN_Button1Up(void) {
    IN_KeyUp(&in_buttons[1]);
}
void IN_Button2Down(void) {
    IN_KeyDown(&in_buttons[2]);
}
void IN_Button2Up(void) {
    IN_KeyUp(&in_buttons[2]);
}
void IN_Button3Down(void) {
    IN_KeyDown(&in_buttons[3]);
}
void IN_Button3Up(void) {
    IN_KeyUp(&in_buttons[3]);
}
void IN_Button4Down(void) {
    IN_KeyDown(&in_buttons[4]);
}
void IN_Button4Up(void) {
    IN_KeyUp(&in_buttons[4]);
}
void IN_Button5Down(void) {
    IN_KeyDown(&in_buttons[5]);
}
void IN_Button5Up(void) {
    IN_KeyUp(&in_buttons[5]);
}
void IN_Button6Down(void) {
    IN_KeyDown(&in_buttons[6]);
}
void IN_Button6Up(void) {
    IN_KeyUp(&in_buttons[6]);
}
void IN_Button7Down(void) {
    IN_KeyDown(&in_buttons[7]);
}
void IN_Button7Up(void) {
    IN_KeyUp(&in_buttons[7]);
}
void IN_Button8Down(void) {
    IN_KeyDown(&in_buttons[8]);
}
void IN_Button8Up(void) {
    IN_KeyUp(&in_buttons[8]);
}
void IN_Button9Down(void) {
    IN_KeyDown(&in_buttons[9]);
}
void IN_Button9Up(void) {
    IN_KeyUp(&in_buttons[9]);
}
void IN_Button10Down(void) {
    IN_KeyDown(&in_buttons[10]);
}
void IN_Button10Up(void) {
    IN_KeyUp(&in_buttons[10]);
}
void IN_Button11Down(void) {
    IN_KeyDown(&in_buttons[11]);
}
void IN_Button11Up(void) {
    IN_KeyUp(&in_buttons[11]);
}
void IN_Button12Down(void) {
    IN_KeyDown(&in_buttons[12]);
}
void IN_Button12Up(void) {
    IN_KeyUp(&in_buttons[12]);
}
void IN_Button13Down(void) {
    IN_KeyDown(&in_buttons[13]);
}
void IN_Button13Up(void) {
    IN_KeyUp(&in_buttons[13]);
}
void IN_Button14Down(void) {
    IN_KeyDown(&in_buttons[14]);
}
void IN_Button14Up(void) {
    IN_KeyUp(&in_buttons[14]);
}
void IN_Button15Down(void) {
    IN_KeyDown(&in_buttons[15]);
}
void IN_Button15Up(void) {
    IN_KeyUp(&in_buttons[15]);
}

void IN_CenterView(void) {
    cl.viewangles[PITCH] = -SHORT2ANGLE(cl.snap.ps.delta_angles[PITCH]);
}

cvar_t* cl_yawspeed;
cvar_t* cl_pitchspeed;

cvar_t* cl_run;

cvar_t* cl_anglespeedkey;

void CL_AdjustAngles(void) {
    float speed;

    if (in_speed.active) {
        speed = 0.001 * cls.frametime * cl_anglespeedkey->value;
    } else {
        speed = 0.001 * cls.frametime;
    }

    if (!in_strafe.active) {
        cl.viewangles[YAW] -= speed * cl_yawspeed->value * CL_KeyState(&in_right);
        cl.viewangles[YAW] += speed * cl_yawspeed->value * CL_KeyState(&in_left);
    }

    cl.viewangles[PITCH] -= speed * cl_pitchspeed->value * CL_KeyState(&in_lookup);
    cl.viewangles[PITCH] += speed * cl_pitchspeed->value * CL_KeyState(&in_lookdown);
}

void CL_KeyMove(usercmd_t* cmd) {
    int movespeed;
    int forward, side, up;

    if (in_speed.active ^ cl_run->integer) {
        movespeed = 127;
        cmd->buttons &= ~BUTTON_WALKING;
    } else {
        cmd->buttons |= BUTTON_WALKING;
        movespeed = 64;
    }

    forward = 0;
    side = 0;
    up = 0;
    if (in_strafe.active) {
        side += movespeed * CL_KeyState(&in_right);
        side -= movespeed * CL_KeyState(&in_left);
    }

    side += movespeed * CL_KeyState(&in_moveright);
    side -= movespeed * CL_KeyState(&in_moveleft);

    if (in_up.active) {
        cmd->buttons |= BUTTON_JUMP;
    } else {
        cmd->buttons &= ~BUTTON_JUMP;
    }

    up += movespeed * CL_KeyState(&in_up);
    up -= movespeed * CL_KeyState(&in_down);

    forward += movespeed * CL_KeyState(&in_forward);
    forward -= movespeed * CL_KeyState(&in_back);

    cmd->forwardmove = ClampChar(forward);
    cmd->rightmove = ClampChar(side);
    cmd->upmove = ClampChar(up);
}

void CL_MouseAbsEvent(int x, int y) {
    if (cg_ignoreMouseInput->integer) {
        return;
    }
    if (Key_GetCatcher() & KEYCATCH_CONSOLE) {
        Con_MousePosition(x, y);
    } else if (Key_GetCatcher() & KEYCATCH_UI) {
        VM_Call(uivm, UI_MOUSE_POSITION, x, y);
    } else if (Key_GetCatcher() & KEYCATCH_CGAME) {
        VM_Call(cgvm, CG_MOUSE_EVENT, x, y);
    }
}

void CL_MouseEvent(int dx, int dy, int time) {
    if (cg_ignoreMouseInput->integer) {
        return;
    }

    if (Key_GetCatcher() & KEYCATCH_UI) {
        VM_Call(uivm, UI_MOUSE_EVENT, dx, dy);
    } else if (Key_GetCatcher() & KEYCATCH_CGAME) {
    } else if (!(Key_GetCatcher() & ~0x10)) {
        cl.mouseDx[0] += dx;
        cl.mouseDy[0] += dy;
    }
}

void CL_JoystickEvent(int axis, int value, int time) {
    if (axis < 0 || axis >= MAX_JOYSTICK_AXIS) {
        Com_Error(ERR_DROP, "CL_JoystickEvent: bad axis %i", axis);
    }
    cl.joystickAxis[axis] = value;
}

void CL_JoystickMove(usercmd_t* cmd) {
    float anglespeed;

    float yaw = j_yaw->value * cl.joystickAxis[j_yaw_axis->integer];
    float right = j_side->value * cl.joystickAxis[j_side_axis->integer];
    float forward = j_forward->value * cl.joystickAxis[j_forward_axis->integer];
    float pitch = j_pitch->value * cl.joystickAxis[j_pitch_axis->integer];
    float up = j_up->value * cl.joystickAxis[j_up_axis->integer];

    if (!(in_speed.active ^ cl_run->integer)) {
        cmd->buttons |= BUTTON_WALKING;
    }

    if (in_speed.active) {
        anglespeed = 0.001 * cls.frametime * cl_anglespeedkey->value;
    } else {
        anglespeed = 0.001 * cls.frametime;
    }

    if (!in_strafe.active) {
        cl.viewangles[YAW] += anglespeed * yaw;
        cmd->rightmove = ClampChar(cmd->rightmove + (int)right);
    } else {
        cl.viewangles[YAW] += anglespeed * right;
        cmd->rightmove = ClampChar(cmd->rightmove + (int)yaw);
    }

    if (in_mlooking) {
        cl.viewangles[PITCH] += anglespeed * forward;
        cmd->forwardmove = ClampChar(cmd->forwardmove + (int)pitch);
    } else {
        cl.viewangles[PITCH] += anglespeed * pitch;
        cmd->forwardmove = ClampChar(cmd->forwardmove + (int)forward);
    }

    cmd->upmove = ClampChar(cmd->upmove + (int)up);
}

#define MOUSE_FILTER_SIZE 32

static float mouseFilterYaw[MOUSE_FILTER_SIZE];
static float mouseFilterPitch[MOUSE_FILTER_SIZE];
static int mouseFilterCount;
static int mouseFilterIndex;
static float mouseFilterAimYaw;
static float mouseFilterAimPitch;

static void CL_MouseFilterBegin(void) {
    if (!m_filter->integer) {
        return;
    }

    if (m_filter->integer < 0) {
        Cvar_Set("m_filter", "0");
        return;
    }

    if (m_filter->integer >= MOUSE_FILTER_SIZE) {
        Cvar_Set("m_filter", va("%d", MOUSE_FILTER_SIZE - 1));
    }

    if (m_filter->modified) {
        mouseFilterAimYaw = cl.viewangles[YAW];
        mouseFilterAimPitch = cl.viewangles[PITCH];
        Com_Memset(mouseFilterYaw, 0, sizeof(mouseFilterYaw));
        Com_Memset(mouseFilterPitch, 0, sizeof(mouseFilterPitch));
        mouseFilterCount = 0;
        mouseFilterIndex = 0;
        m_filter->modified = qfalse;
    }

    cl.viewangles[YAW] = mouseFilterAimYaw;
    cl.viewangles[PITCH] = mouseFilterAimPitch;
}

static void CL_MouseFilterEnd(void) {
    float yaw, pitch;
    int i, j;

    if (!m_filter->integer) {
        return;
    }

    mouseFilterYaw[mouseFilterIndex] = cl.viewangles[YAW];
    mouseFilterPitch[mouseFilterIndex] = cl.viewangles[PITCH];

    mouseFilterCount++;
    if (mouseFilterCount > m_filter->integer) {
        mouseFilterCount = m_filter->integer;
    }

    yaw = 0.0f;
    pitch = 0.0f;
    for (i = 0, j = mouseFilterIndex; i < mouseFilterCount; i++, j--) {
        int k = j < 0 ? j + MOUSE_FILTER_SIZE : j;

        yaw += mouseFilterYaw[k];
        pitch += mouseFilterPitch[k];
    }

    mouseFilterAimYaw = cl.viewangles[YAW];
    mouseFilterAimPitch = cl.viewangles[PITCH];
    cl.viewangles[YAW] = yaw / mouseFilterCount;
    cl.viewangles[PITCH] = pitch / mouseFilterCount;

    mouseFilterIndex = (mouseFilterIndex + 1) % MOUSE_FILTER_SIZE;
}

static FILE* mouseAccelLog;

void CL_MouseMove(usercmd_t* cmd) {
    qboolean debug = cl_mouseAccelDebug->integer != 0;
    float mx, my;
    float sensitivity;
    float rate, power;
    float angle;

    if (debug) {
        if (!mouseAccelLog) {
            mouseAccelLog = Sys_FOpen(FS_BuildOSPath(Cvar_VariableString("fs_homepath"), "", "mouse.log"), "w");
            if (mouseAccelLog) {
                fprintf(mouseAccelLog, "mx my frame_msec rate power\n");
            }
        }
    } else if (mouseAccelLog) {
        fclose(mouseAccelLog);
        mouseAccelLog = NULL;
    }
    if (!mouseAccelLog) {
        debug = qfalse;
    }

    mx = cl.mouseDx[0];
    my = cl.mouseDy[0];
    cl.mouseDx[0] = 0;
    cl.mouseDy[0] = 0;

    if (m_cpi->value != 0.0f) {
        float countsPerCm = m_cpi->value / 2.54f;

        mx /= countsPerCm;
        my /= countsPerCm;
    }

    sensitivity = cl_sensitivity->value;

    if (debug) {
        fprintf(mouseAccelLog, "%g %g %d ", mx, my, frame_msec);
    }

    if (cl_mouseAccel->value != 0.0f) {
        rate = sqrt(mx * mx + my * my) / (float)frame_msec;
        if (m_cpi->value != 0.0f) {
            rate *= 1000.0f;
        }
        rate -= cl_mouseAccelOffset->value;

        power = 0.0f;
        if (rate > 0.0f) {
            rate *= fabs(cl_mouseAccel->value);

            power = cl_mouseAccelPower->value - 1.0f;
            if (power < 0.0f) {
                power = 0.0f;
            }

            if (cl_mouseAccel->value > 0.0f) {
                sensitivity += pow(rate, power);
            } else {
                sensitivity -= pow(rate, power);
            }
        }

        if (cl_mouseSensCap->value > 0.0f && sensitivity > cl_mouseSensCap->value) {
            sensitivity = cl_mouseSensCap->value;
        }

        if (debug) {
            fprintf(mouseAccelLog, "%g %g ", rate, power);
        }
    }

    sensitivity *= cl.cgameSensitivity;

    if (debug) {
        fprintf(mouseAccelLog, "\n");
    }

    mx *= sensitivity;
    my *= sensitivity;

    if (mx == 0.0f && my == 0.0f && !m_filter->integer) {
        return;
    }

    CL_MouseFilterBegin();

    if (in_strafe.active) {
        cmd->rightmove = ClampChar(cmd->rightmove + m_side->value * mx);
    } else {
        angle = m_yaw->value;
        if (m_cpi->value != 0.0f) {
            angle *= 1.0f / 0.022f;
        }
        cl.viewangles[YAW] -= angle * mx;
    }

    if ((in_mlooking || cl_freelook->integer) && !in_strafe.active) {
        angle = m_pitch->value;
        if (m_cpi->value != 0.0f) {
            angle *= 1.0f / 0.022f;
        }
        cl.viewangles[PITCH] += angle * my;
    } else {
        cmd->forwardmove = ClampChar(cmd->forwardmove - m_forward->value * my);
    }

    CL_MouseFilterEnd();
}

void CL_CmdButtons(usercmd_t* cmd) {
    int i;

    for (i = 0; i < 15; i++) {
        if (in_buttons[i].active || in_buttons[i].wasPressed) {
            cmd->buttons |= 1 << i;
        }
        in_buttons[i].wasPressed = qfalse;
    }

    if (Key_GetCatcher() & ~0x10) {
        cmd->buttons |= BUTTON_TALK;
    }

    if (anykeydown && (Key_GetCatcher() & ~0x10) == 0) {
        cmd->buttons |= BUTTON_ANY;
    }
}

void CL_FinishMove(usercmd_t* cmd) {
    int i;

    cmd->weapon = cl.cgameUserCmdValue;
    cmd->weaponPrimary = (byte)cl.cgameUserCmdWeaponPrimary;
    cmd->fov = (byte)cl.cgameUserCmdFov;

    cmd->serverTime = cl.serverTime;

    for (i = 0; i < 3; i++) {
        cmd->angles[i] = ANGLE2SHORT(cl.viewangles[i]);
    }
}

usercmd_t CL_CreateCmd(void) {
    usercmd_t cmd;
    vec3_t oldAngles;

    VectorCopy(cl.viewangles, oldAngles);

    CL_AdjustAngles();

    Com_Memset(&cmd, 0, sizeof(cmd));

    CL_CmdButtons(&cmd);

    CL_KeyMove(&cmd);

    CL_MouseMove(&cmd);

    CL_JoystickMove(&cmd);

    if (cl.viewangles[PITCH] - oldAngles[PITCH] > 90) {
        cl.viewangles[PITCH] = oldAngles[PITCH] + 90;
    } else if (oldAngles[PITCH] - cl.viewangles[PITCH] > 90) {
        cl.viewangles[PITCH] = oldAngles[PITCH] - 90;
    }

    CL_FinishMove(&cmd);

    if (cl_debugMove->integer) {
        if (cl_debugMove->integer == 1) {
            SCR_DebugGraph(fabs(cl.viewangles[YAW] - oldAngles[YAW]));
        }
        if (cl_debugMove->integer == 2) {
            SCR_DebugGraph(fabs(cl.viewangles[PITCH] - oldAngles[PITCH]));
        }
    }

    return cmd;
}

void CL_CreateNewCommands(void) {
    int cmdNum;

    if (clc.state < CA_PRIMED) {
        return;
    }

    frame_msec = com_frameTime - old_com_frameTime;

    if (frame_msec < 1) {
        frame_msec = 1;
    }

    if (frame_msec > 200) {
        frame_msec = 200;
    }
    old_com_frameTime = com_frameTime;

    cl.cmdNumber++;
    cmdNum = cl.cmdNumber & CMD_MASK;
    cl.cmds[cmdNum] = CL_CreateCmd();
}

qboolean CL_ReadyToSendPacket(void) {
    int oldPacketNum;
    int delta;

    if (clc.demoplaying || clc.state == CA_CINEMATIC) {
        return qfalse;
    }

    if (*clc.downloadTempName &&
        cls.realtime - clc.lastPacketSentTime < 50) {
        return qfalse;
    }

    if (clc.state != CA_ACTIVE &&
        clc.state != CA_PRIMED &&
        !*clc.downloadTempName &&
        cls.realtime - clc.lastPacketSentTime < 1000) {
        return qfalse;
    }

    if (clc.netchan.remoteAddress.type == NA_LOOPBACK) {
        return qtrue;
    }

    if (cl_lanForcePackets->integer && Sys_IsLANAddress(clc.netchan.remoteAddress)) {
        return qtrue;
    }

    if (cl_maxpackets->integer < 15) {
        Cvar_Set("cl_maxpackets", "15");
    } else if (cl_maxpackets->integer > 125) {
        Cvar_Set("cl_maxpackets", "125");
    }
    oldPacketNum = (clc.netchan.outgoingSequence - 1) & PACKET_MASK;
    delta = cls.realtime - cl.outPackets[oldPacketNum].p_realtime;
    if (delta < 1000 / cl_maxpackets->integer) {
        return qfalse;
    }

    return qtrue;
}

void CL_WritePacket(void) {
    msg_t buf;
    byte data[MAX_MSGLEN];
    int i, j;
    usercmd_t *cmd, *oldcmd;
    usercmd_t nullcmd;
    int packetNum;
    int oldPacketNum;
    int count, key;

    if (clc.demoplaying || clc.state == CA_CINEMATIC) {
        return;
    }

    Com_Memset(&nullcmd, 0, sizeof(nullcmd));
    oldcmd = &nullcmd;

    MSG_Init(&buf, data, sizeof(data));

    MSG_Bitstream(&buf);
    MSG_WriteLong(&buf, cl.serverId);

    MSG_WriteLong(&buf, clc.serverMessageSequence);

    MSG_WriteLong(&buf, clc.serverCommandSequence);

    MSG_WriteByte(&buf, 0);

    for (i = clc.reliableAcknowledge + 1; i <= clc.reliableSequence; i++) {
        MSG_WriteByte(&buf, clc_clientCommand);
        MSG_WriteLong(&buf, i);
        MSG_WriteString(&buf, clc.reliableCommands[i & (MAX_RELIABLE_COMMANDS - 1)]);
    }

    if (cl_packetdup->integer < 0) {
        Cvar_Set("cl_packetdup", "0");
    } else if (cl_packetdup->integer > 5) {
        Cvar_Set("cl_packetdup", "5");
    }
    oldPacketNum = (clc.netchan.outgoingSequence - 1 - cl_packetdup->integer) & PACKET_MASK;
    count = cl.cmdNumber - cl.outPackets[oldPacketNum].p_cmdNumber;
    if (count > MAX_PACKET_USERCMDS) {
        count = MAX_PACKET_USERCMDS;
        Com_Printf("MAX_PACKET_USERCMDS\n");
    }

    if (count >= 1) {
        if (cl_showSend->integer) {
            Com_Printf("(%i)", count);
        }

        if (cl_nodelta->integer || !cl.snap.valid || clc.demowaiting || clc.serverMessageSequence != cl.snap.messageNum) {
            MSG_WriteByte(&buf, clc_moveNoDelta);
        } else {
            MSG_WriteByte(&buf, clc_move);
        }

        MSG_WriteByte(&buf, count);

        key = clc.checksumFeed;
        key ^= clc.serverMessageSequence;
        key ^= MSG_HashKey(clc.serverCommands[clc.serverCommandSequence & (MAX_RELIABLE_COMMANDS - 1)], 32);

        for (i = 0; i < count; i++) {
            j = (cl.cmdNumber - count + i + 1) & CMD_MASK;
            cmd = &cl.cmds[j];
            MSG_WriteDeltaUsercmdKey(&buf, key, oldcmd, cmd);
            oldcmd = cmd;
        }
    }

    packetNum = clc.netchan.outgoingSequence & PACKET_MASK;
    cl.outPackets[packetNum].p_realtime = cls.realtime;
    cl.outPackets[packetNum].p_serverTime = oldcmd->serverTime;
    cl.outPackets[packetNum].p_cmdNumber = cl.cmdNumber;
    clc.lastPacketSentTime = cls.realtime;

    if (cl_showSend->integer) {
        Com_Printf("%i ", buf.cursize);
    }

    CL_Netchan_Transmit(&clc.netchan, &buf);
}

void CL_SendCmd(void) {
    if (clc.state < CA_CONNECTED) {
        return;
    }

    if (com_sv_running->integer && sv_paused->integer && cl_paused->integer) {
        return;
    }

    CL_CreateNewCommands();

    if (!CL_ReadyToSendPacket()) {
        if (cl_showSend->integer) {
            Com_Printf(". ");
        }
        return;
    }

    CL_WritePacket();
}

void CL_InitInput(void) {
    Cmd_AddCommand("centerview", IN_CenterView);

    Cmd_AddCommand("+moveup", IN_UpDown);
    Cmd_AddCommand("-moveup", IN_UpUp);
    Cmd_AddCommand("+movedown", IN_DownDown);
    Cmd_AddCommand("-movedown", IN_DownUp);
    Cmd_AddCommand("+left", IN_LeftDown);
    Cmd_AddCommand("-left", IN_LeftUp);
    Cmd_AddCommand("+right", IN_RightDown);
    Cmd_AddCommand("-right", IN_RightUp);
    Cmd_AddCommand("+forward", IN_ForwardDown);
    Cmd_AddCommand("-forward", IN_ForwardUp);
    Cmd_AddCommand("+back", IN_BackDown);
    Cmd_AddCommand("-back", IN_BackUp);
    Cmd_AddCommand("+lookup", IN_LookupDown);
    Cmd_AddCommand("-lookup", IN_LookupUp);
    Cmd_AddCommand("+lookdown", IN_LookdownDown);
    Cmd_AddCommand("-lookdown", IN_LookdownUp);
    Cmd_AddCommand("+strafe", IN_StrafeDown);
    Cmd_AddCommand("-strafe", IN_StrafeUp);
    Cmd_AddCommand("+moveleft", IN_MoveleftDown);
    Cmd_AddCommand("-moveleft", IN_MoveleftUp);
    Cmd_AddCommand("+moveright", IN_MoverightDown);
    Cmd_AddCommand("-moveright", IN_MoverightUp);
    Cmd_AddCommand("+speed", IN_SpeedDown);
    Cmd_AddCommand("-speed", IN_SpeedUp);
    Cmd_AddCommand("+attack", IN_Button0Down);
    Cmd_AddCommand("-attack", IN_Button0Up);
    Cmd_AddCommand("+button0", IN_Button0Down);
    Cmd_AddCommand("-button0", IN_Button0Up);
    Cmd_AddCommand("+button1", IN_Button1Down);
    Cmd_AddCommand("-button1", IN_Button1Up);
    Cmd_AddCommand("+button2", IN_Button2Down);
    Cmd_AddCommand("-button2", IN_Button2Up);
    Cmd_AddCommand("+button3", IN_Button3Down);
    Cmd_AddCommand("-button3", IN_Button3Up);
    Cmd_AddCommand("+button4", IN_Button4Down);
    Cmd_AddCommand("-button4", IN_Button4Up);
    Cmd_AddCommand("+button5", IN_Button5Down);
    Cmd_AddCommand("-button5", IN_Button5Up);
    Cmd_AddCommand("+button6", IN_Button6Down);
    Cmd_AddCommand("-button6", IN_Button6Up);
    Cmd_AddCommand("+button7", IN_Button7Down);
    Cmd_AddCommand("-button7", IN_Button7Up);
    Cmd_AddCommand("+button8", IN_Button8Down);
    Cmd_AddCommand("-button8", IN_Button8Up);
    Cmd_AddCommand("+button9", IN_Button9Down);
    Cmd_AddCommand("-button9", IN_Button9Up);
    Cmd_AddCommand("+button10", IN_Button10Down);
    Cmd_AddCommand("-button10", IN_Button10Up);
    Cmd_AddCommand("+button11", IN_Button11Down);
    Cmd_AddCommand("-button11", IN_Button11Up);
    Cmd_AddCommand("+button12", IN_Button12Down);
    Cmd_AddCommand("-button12", IN_Button12Up);
    Cmd_AddCommand("+button13", IN_Button13Down);
    Cmd_AddCommand("-button13", IN_Button13Up);
    Cmd_AddCommand("+button14", IN_Button14Down);
    Cmd_AddCommand("-button14", IN_Button14Up);
    Cmd_AddCommand("+mlook", IN_MLookDown);
    Cmd_AddCommand("-mlook", IN_MLookUp);
    Cmd_AddCommand("ListInputDevices", IN_ListInputDevices);

    cl_nodelta = Cvar_Get("cl_nodelta", "0", 0);
    cl_debugMove = Cvar_Get("cl_debugMove", "0", 0);
}

void CL_ShutdownInput(void) {
    Cmd_RemoveCommand("centerview");

    Cmd_RemoveCommand("+moveup");
    Cmd_RemoveCommand("-moveup");
    Cmd_RemoveCommand("+movedown");
    Cmd_RemoveCommand("-movedown");
    Cmd_RemoveCommand("+left");
    Cmd_RemoveCommand("-left");
    Cmd_RemoveCommand("+right");
    Cmd_RemoveCommand("-right");
    Cmd_RemoveCommand("+forward");
    Cmd_RemoveCommand("-forward");
    Cmd_RemoveCommand("+back");
    Cmd_RemoveCommand("-back");
    Cmd_RemoveCommand("+lookup");
    Cmd_RemoveCommand("-lookup");
    Cmd_RemoveCommand("+lookdown");
    Cmd_RemoveCommand("-lookdown");
    Cmd_RemoveCommand("+strafe");
    Cmd_RemoveCommand("-strafe");
    Cmd_RemoveCommand("+moveleft");
    Cmd_RemoveCommand("-moveleft");
    Cmd_RemoveCommand("+moveright");
    Cmd_RemoveCommand("-moveright");
    Cmd_RemoveCommand("+speed");
    Cmd_RemoveCommand("-speed");
    Cmd_RemoveCommand("+attack");
    Cmd_RemoveCommand("-attack");
    Cmd_RemoveCommand("+button0");
    Cmd_RemoveCommand("-button0");
    Cmd_RemoveCommand("+button1");
    Cmd_RemoveCommand("-button1");
    Cmd_RemoveCommand("+button2");
    Cmd_RemoveCommand("-button2");
    Cmd_RemoveCommand("+button3");
    Cmd_RemoveCommand("-button3");
    Cmd_RemoveCommand("+button4");
    Cmd_RemoveCommand("-button4");
    Cmd_RemoveCommand("+button5");
    Cmd_RemoveCommand("-button5");
    Cmd_RemoveCommand("+button6");
    Cmd_RemoveCommand("-button6");
    Cmd_RemoveCommand("+button7");
    Cmd_RemoveCommand("-button7");
    Cmd_RemoveCommand("+button8");
    Cmd_RemoveCommand("-button8");
    Cmd_RemoveCommand("+button9");
    Cmd_RemoveCommand("-button9");
    Cmd_RemoveCommand("+button10");
    Cmd_RemoveCommand("-button10");
    Cmd_RemoveCommand("+button11");
    Cmd_RemoveCommand("-button11");
    Cmd_RemoveCommand("+button12");
    Cmd_RemoveCommand("-button12");
    Cmd_RemoveCommand("+button13");
    Cmd_RemoveCommand("-button13");
    Cmd_RemoveCommand("+button14");
    Cmd_RemoveCommand("-button14");
    Cmd_RemoveCommand("+mlook");
    Cmd_RemoveCommand("-mlook");
    Cmd_RemoveCommand("ListInputDevices");
}
