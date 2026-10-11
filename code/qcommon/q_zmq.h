
#ifndef Q_ZMQ_H
#define Q_ZMQ_H

#include "q_shared.h"
#include "qcommon.h"

void ZMQ_Init(void);
void ZMQ_StatsInit(void);
void ZMQ_Frame(void);
void ZMQ_Shutdown(void);

void ZMQ_PublishStats(const char* json);

#endif
