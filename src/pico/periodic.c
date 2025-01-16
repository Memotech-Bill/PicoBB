// periodic.c - Queue periodic events.

#include "periodic.h"
#include <stdbool.h>
#include <pico/time.h>

static PRD_FUNC ptop = NULL;
static struct repeating_timer s_prd_timer;

static bool periodic_cb (struct repeating_timer *prt)
    {
    if (ptop != NULL) ptop ();
    return true;
    }


PRD_FUNC add_periodic (PRD_FUNC new)
    {
    PRD_FUNC old = ptop;
    if (ptop == NULL)
        {
        add_repeating_timer_ms (100, periodic_cb, NULL, &s_prd_timer);
        }
    ptop = new;
    return old;
    }
