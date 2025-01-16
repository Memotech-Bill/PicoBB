// periodic.h - Queue periodic events

#ifndef PERIODIC_H
#define PERIODIC_H

typedef void (*PRD_FUNC)(void);

PRD_FUNC add_periodic (PRD_FUNC new);

#endif
