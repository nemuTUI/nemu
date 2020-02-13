#ifndef NM_MON_DAEMON_H_
#define NM_MON_DAEMON_H_

void nm_mon_start(void);
void nm_mon_loop(void);
void nm_mon_ping(void);

static const int NM_MON_SLEEP = 1000;
#endif
/* vim:set ts=4 sw=4: */
