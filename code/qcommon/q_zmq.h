/*
===========================================================================
ioquakelive - ZeroMQ stats and rcon

Quake Live's server publishes match statistics and takes remote console
commands over ZeroMQ. Retail links libzmq statically (zmq\id_zmq.cpp, class
idZMQ); this is an implementation of just the part of the ZeroMQ wire protocol
(ZMTP 3.0) those two sockets need, so no third-party library is added.

Clients are plain ZeroMQ: a SUB socket for the stats, a DEALER for rcon, with
the PLAIN mechanism when a password is set. See zmq.c for what was read in the
binary.
===========================================================================
*/

#ifndef Q_ZMQ_H
#define Q_ZMQ_H

#include "q_shared.h"
#include "qcommon.h"

void ZMQ_Init(void);          // the cvars and the rcon socket, from Com_Init
void ZMQ_StatsInit(void);     // the stats socket, once the server has a port
void ZMQ_Frame(void);         // accept, handshake, read, from Com_Frame
void ZMQ_Shutdown(void);

// one statistics event, a JSON document, to every subscriber
void ZMQ_PublishStats(const char* json);

#endif
