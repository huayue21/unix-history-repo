/*
 * Copyright (c) 1995-1998 John Birrell <jb@cimlogic.com.au>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by John Birrell.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY JOHN BIRRELL AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $Id: uthread_select.c,v 1.8 1999/06/23 15:01:22 dt Exp $
 */
#include <unistd.h>
#include <errno.h>
#include <poll.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/fcntl.h>
#ifdef _THREAD_SAFE
#include <pthread.h>
#include "pthread_private.h"

int 
select(int numfds, fd_set * readfds, fd_set * writefds,
       fd_set * exceptfds, struct timeval * timeout)
{
	struct timespec ts;
	int             i, ret = 0, f_wait = 1;
	int		pfd_index, got_one = 0, fd_count = 0;
	struct pthread_poll_data data;

	if (numfds > _thread_dtablesize) {
		numfds = _thread_dtablesize;
	}
	/* Check if a timeout was specified: */
	if (timeout) {
		/* Convert the timeval to a timespec: */
		TIMEVAL_TO_TIMESPEC(timeout, &ts);

		/* Set the wake up time: */
		_thread_kern_set_timeout(&ts);
		if (ts.tv_sec == 0 && ts.tv_nsec == 0)
			f_wait = 0;
	} else {
		/* Wait for ever: */
		_thread_kern_set_timeout(NULL);
	}

	/* Count the number of file descriptors to be polled: */
	if (readfds || writefds || exceptfds) {
		for (i = 0; i < numfds; i++) {
			if ((readfds && FD_ISSET(i, readfds)) ||
			    (exceptfds && FD_ISSET(i, exceptfds)) ||
			    (writefds && FD_ISSET(i, writefds))) {
				fd_count++;
			}
		}
	}

	/*
	 * Allocate memory for poll data if it hasn't already been
	 * allocated or if previously allocated memory is insufficient.
	 */
	if ((_thread_run->poll_data.fds == NULL) ||
	    (_thread_run->poll_data.nfds < fd_count)) {
		data.fds = (struct pollfd *) realloc(_thread_run->poll_data.fds,
		    sizeof(struct pollfd) * MAX(128, fd_count));
		if (data.fds == NULL) {
			errno = ENOMEM;
			ret = -1;
		}
		else {
			/*
			 * Note that the threads poll data always
			 * indicates what is allocated, not what is
			 * currently being polled.
			 */
			_thread_run->poll_data.fds = data.fds;
			_thread_run->poll_data.nfds = MAX(128, fd_count);
		}
	}
	if (ret == 0) {
		/* Setup the wait data. */
		data.fds = _thread_run->poll_data.fds;
		data.nfds = fd_count;

		/*
		 * Setup the array of pollfds.  Optimize this by
		 * running the loop in reverse and stopping when
		 * the number of selected file descriptors is reached.
		 */
		for (i = numfds - 1, pfd_index = fd_count - 1;
		    (i >= 0) && (pfd_index >= 0); i--) {
			data.fds[pfd_index].events = 0;
			if (readfds && FD_ISSET(i, readfds)) {
				data.fds[pfd_index].events = POLLRDNORM;
			}
			if (exceptfds && FD_ISSET(i, exceptfds)) {
				data.fds[pfd_index].events |= POLLRDBAND;
			}
			if (writefds && FD_ISSET(i, writefds)) {
				data.fds[pfd_index].events |= POLLWRNORM;
			}
			if (data.fds[pfd_index].events != 0) {
				/*
				 * Set the file descriptor to be polled and
				 * clear revents in case of a timeout which
				 * leaves fds unchanged:
				 */
				data.fds[pfd_index].fd = i;
				data.fds[pfd_index].revents = 0;
				pfd_index--;
			}
		}
		if (((ret = _thread_sys_poll(data.fds, data.nfds, 0)) == 0) &&
		   (f_wait != 0)) {
			_thread_run->data.poll_data = &data;
			_thread_run->interrupted = 0;
			_thread_kern_sched_state(PS_SELECT_WAIT, __FILE__, __LINE__);
			if (_thread_run->interrupted) {
				errno = EINTR;
				data.nfds = 0;
				ret = -1;
			} else
				ret = data.nfds;
		}
	}

	if (ret >= 0) {
		numfds = 0;
		for (i = 0; i < fd_count; i++) {
			/*
			 * Check the results of the poll and clear
			 * this file descriptor from the fdset if
			 * the requested event wasn't ready.
			 */
			got_one = 0;
			if (readfds != NULL) {
				if (FD_ISSET(data.fds[i].fd, readfds)) {
					if (data.fds[i].revents & (POLLIN |
					    POLLRDNORM))
						got_one = 1;
					else
						FD_CLR(data.fds[i].fd, readfds);
				}
			}
			if (writefds != NULL) {
				if (FD_ISSET(data.fds[i].fd, writefds)) {
					if (data.fds[i].revents & (POLLOUT |
					    POLLWRNORM | POLLWRBAND))
						got_one = 1;
					else
						FD_CLR(data.fds[i].fd,
						    writefds);
				}
			}
			if (exceptfds != NULL) {
				if (FD_ISSET(data.fds[i].fd, exceptfds)) {
					if (data.fds[i].revents & (POLLRDBAND |
					    POLLPRI | POLLHUP | POLLERR |
					    POLLNVAL))
						got_one = 1;
					else
						FD_CLR(data.fds[i].fd,
						    exceptfds);
				}
			}
			if (got_one)
				numfds++;
		}
		ret = numfds;
	}

	return (ret);
}
#endif
