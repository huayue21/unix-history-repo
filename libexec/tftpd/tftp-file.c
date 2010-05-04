/*
 * Copyright (C) 2008 Edwin Groothuis. All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>
#include <sys/stat.h>

#include <netinet/in.h>
#include <arpa/tftp.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

#include "tftp-file.h"
#include "tftp-utils.h"

static FILE	*file;
static int	convert;

static char	convbuffer[66000];
static int	gotcr = 0;

static size_t
convert_from_net(char *buffer, size_t count)
{
	size_t i, n;

	/*
	 * Convert all CR/LF to LF and all CR,NUL to CR
	 */

	n = 0;
	for (i = 0; i < count; i++) {

		if (gotcr == 0) {
			convbuffer[n++] = buffer[i];
			gotcr = (buffer[i] == '\r');
			continue;
		}

		/* CR, NULL -> CR */
		if (buffer[i] == '\0') {
			gotcr = 0;
			continue;
		}

		/* CR, LF -> LF */
		if (buffer[i] == '\n') {
			if (n == 0) {
				if (ftell(file) != 0) {
					fseek(file, -1, SEEK_END);
					convbuffer[n++] = '\n';
				} else {
					/* This shouldn't happen */
					tftp_log(LOG_ERR,
					    "Received LF as first character");
					abort();
				}
			} else
				convbuffer[n-1] = '\n';
			gotcr = 0;
			continue;
		}

		/* Everything else just accept as is */
		convbuffer[n++] = buffer[i];
		gotcr = (buffer[i] == '\r');
		continue;
	}

	return fwrite(convbuffer, 1, n, file);
}

static size_t
convert_to_net(char *buffer, size_t count, int init)
{
	size_t i;
	static size_t n = 0, read = 0;
	static int newline = 0;

	if (init) {
		newline = 0;
		n = 0;
		read = 0;
		return 0 ;
	}

	/*
	 * Convert all LF to CR,LF and all CR to CR,NUL
	 */
	i = 0;

	if (newline) {
		buffer[i++] = newline;
		newline = 0;
	}

	while (i < count) {
		if (n == read) {
			/* When done we're done */
			if (feof(file)) break;

			/* Otherwise read another bunch */
			read = fread(convbuffer, 1, count, file);
			if (read == 0) break;
			n = 0;
		}

		/* CR -> CR,NULL */
		if (convbuffer[n] == '\r') {
			buffer[i++] = '\r';
			buffer[i++] = '\0';
			n++;
			continue;
		}

		/* LF -> CR,LF */
		if (convbuffer[n] == '\n') {
			buffer[i++] = '\r';
			buffer[i++] = '\n';
			n++;
			continue;
		}

		buffer[i++] = convbuffer[n++];
	}

	if (i > count) {
		/*
		 * Whoops... that isn't alllowed (but it will happen
		 * when there is a CR or LF at the end of the buffer)
		 */
		newline = buffer[i-1];
	}

	if (i < count) {
		/* We are done! */
		return i;
	} else
		return count;

}

int
write_init(int fd, FILE *f, const char *mode)
{

	if (f == NULL) {
		file = fdopen(fd, "w");
		if (file == NULL) {
			int en = errno;
			tftp_log(LOG_ERR, "fdopen() failed: %s",
			    strerror(errno));
			return en;
		}
	} else
		file = f;
	convert = !strcmp(mode, "netascii");
	return 0;
}

size_t
write_file(char *buffer, int count)
{

	if (convert == 0)
		return fwrite(buffer, 1, count, file);

	return convert_from_net(buffer, count);
}

int
write_close(void)
{

	if (fclose(file) != 0) {
		tftp_log(LOG_ERR, "fclose() failed: %s", strerror(errno));
		return 1;
	}
	return 0;
}

int
read_init(int fd, FILE *f, const char *mode)
{

	convert_to_net(NULL, 0, 1);
	if (f == NULL) {
		file = fdopen(fd, "r");
		if (file == NULL) {
			int en = errno;
			tftp_log(LOG_ERR, "fdopen() failed: %s",
			    strerror(errno));
			return en;
		}
	} else
		file = f;
	convert = !strcmp(mode, "netascii");
	return 0;
}

size_t
read_file(char *buffer, int count)
{

	if (convert == 0)
		return fread(buffer, 1, count, file);

	return convert_to_net(buffer, count, 0);
}

int
read_close(void)
{

	if (fclose(file) != 0) {
		tftp_log(LOG_ERR, "fclose() failed: %s", strerror(errno));
		return 1;
	}
	return 0;
}


int
synchnet(int peer)
{

	return 0;
}
