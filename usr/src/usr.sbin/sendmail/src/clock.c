# include "sendmail.h"

SCCSID(@(#)clock.c	3.4		%G%);

/*
**  SETEVENT -- set an event to happen at a specific time.
**
**	Parameters:
**		intvl -- intvl until next event occurs.
**		func -- function to call on event.
**		arg -- argument to func on event.
**
**	Returns:
**		none.
**
**	Side Effects:
**		none.
*/

EVENT *
setevent(intvl, func, arg)
	time_t intvl;
	int (*func)();
	int arg;
{
	register EVENT **evp;
	register EVENT *ev;
	auto time_t now;
	extern tick();

	(void) time(&now);

	/* search event queue for correct position */
	for (evp = &EventQueue; (ev = *evp) != NULL; evp = &ev->ev_link)
	{
		if (ev->ev_time >= now + intvl)
			break;
	}

	/* insert new event */
	ev = (EVENT *) xalloc(sizeof *ev);
	ev->ev_time = now + intvl;
	ev->ev_func = func;
	ev->ev_arg = arg;
	ev->ev_link = *evp;
	*evp = ev;

	/* reschedule next clock tick if appropriate */
	if (ev == EventQueue)
	{
		/* we have a new event */
		(void) signal(SIGALRM, tick);
		(void) alarm(intvl);
	}

# ifdef DEBUG
	if (tTd(5, 2))
		printf("setevent: intvl=%ld, for=%ld, func=%x, arg=%d, ev=%x\n",
			intvl, now + intvl, func, arg, ev);
# endif DEBUG

	return (ev);
}
/*
**  CLREVENT -- remove an event from the event queue.
**
**	Parameters:
**		ev -- pointer to event to remove.
**
**	Returns:
**		none.
**
**	Side Effects:
**		arranges for event ev to not happen.
*/

clrevent(ev)
	register EVENT *ev;
{
	register EVENT **evp;

# ifdef DEBUG
	if (tTd(5, 2))
		printf("clrevent: ev=%x\n", ev);
# endif DEBUG

	/* find the parent event */
	for (evp = &EventQueue; *evp != NULL; evp = &(*evp)->ev_link)
	{
		if (*evp == ev)
			break;
	}

	/* now remove it */
	if (*evp == NULL)
	{
		/* hmmmmm.... must have happened. */
		return;
	}

	*evp = ev->ev_link;
	free(ev);
}
/*
**  TICK -- take a clock tick
**
**	Called by the alarm clock.  This routine runs events as needed.
**
**	Parameters:
**		none.
**
**	Returns:
**		none.
**
**	Side Effects:
**		calls the next function in EventQueue.
*/

tick()
{
	auto time_t now;
	register EVENT *ev;

	signal(SIGALRM, SIG_IGN);
	(void) time(&now);

# ifdef DEBUG
	if (tTd(5, 1))
		printf("tick: now=%ld\n", now);
# endif DEBUG

	while (EventQueue != NULL && EventQueue->ev_time <= now)
	{
		/* process the event on the top of the queue */
		ev = EventQueue;
		EventQueue = EventQueue->ev_link;
# ifdef DEBUG
		if (tTd(5, 3))
			printf("tick: ev=%x, func=%x, arg=%d\n", ev,
				ev->ev_func, ev->ev_arg);
# endif DEBUG
		(*ev->ev_func)(ev->ev_arg);
		free(ev);
		(void) time(&now);
	}

	/* schedule the next clock tick */
	signal(SIGALRM, tick);
	if (EventQueue != NULL)
		(void) alarm(EventQueue->ev_time - now);
}
/*
**  SLEEP -- a version of sleep that works with this stuff
**
**	Because sleep uses the alarm facility, I must reimplement
**	it here.
**
**	Parameters:
**		intvl -- time to sleep.
**
**	Returns:
**		none.
**
**	Side Effects:
**		waits for intvl time.  However, other events can
**		be run during that interval.
*/

static bool	SleepDone;

sleep(intvl)
	int intvl;
{
	extern endsleep();

	SleepDone = FALSE;
	setevent(intvl, endsleep, 0);
	while (!SleepDone)
		pause();
}

static
endsleep()
{
	SleepDone = TRUE;
}
