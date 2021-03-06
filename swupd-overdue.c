/*
 *   Clear Linux -- automatically perform OS updates if overdue
 *
 *      Copyright (C) 2016 Intel Corporation
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 *   Authors:
 *         Auke Kok <auke-jan.h.kok@intel.com>
 *
 */


#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <errno.h>
#include <netdb.h>
#include <systemd/sd-bus.h>

/* What file do we check against? */
#define DATESTAMPFILE "/usr/share/clear/versionstamp"
/* Default threshold is 14 days */
#define THRESHOLD 86400 * 14

int main(int argc, char *argv[])
{
	time_t t;
	time_t exp = THRESHOLD;
	time_t tf;
	FILE *f;
	char buf[32];
	sd_bus_error error = SD_BUS_ERROR_NULL;
	sd_bus *bus = NULL;
	int ret = 0;
	int i;

	if (argc == 2) {
		exp = strtol(argv[1], NULL, 10);
		if ( (exp == LONG_MAX || exp == LONG_MIN) && errno != 0 )
			exit(errno);
	}

	/* fetch timestamp from versionstamp file */
	memset(buf, 0, sizeof(buf));
	f = fopen(DATESTAMPFILE, "re");
	if (!f)
		exit(errno);
	if (!fgets(buf, sizeof(buf), f))
		exit(-1);
	tf = strtol(&buf[0], NULL, 10);
	if ( (tf == LONG_MAX || tf == LONG_MIN) && errno != 0 )
		exit(errno);
	fclose(f);

	/* wall clock - ignores timezones */
	t = time(NULL);

	/* nothing to do, system is not overdue for an update */
	if ((t - tf) < exp)
		exit(EXIT_SUCCESS);

	fprintf(stderr, "Software update is overdue: %.1f days "
		"(threshold = %.1f days)\n",
		(t - tf) / 86400.0,
		exp / 86400.0);

	/* Check for the network being online */
	for (i = 1; i < 128 ; i *= 2) {
		struct hostent *h = gethostbyname("download.clearlinux.org");
		if (h)
			break;
		sleep(i);
	};

	/*
	 * No network was found, exit normally!
	 *
	 * We do this because there's no reason to raise an error
	 * that is likely already known. Exiting with a failure
	 * wouldn't accomplish anything
	 */
	if (i >= 128)
		exit(EXIT_SUCCESS);

	/* online or not, attempt the update */
	do {
		if ((ret = sd_bus_open_system(&bus)) < 0)
			break;
		if ((ret = sd_bus_call_method(bus,
					"org.freedesktop.systemd1",
					"/org/freedesktop/systemd1",
					"org.freedesktop.systemd1.Manager",
					"StartUnit",
					&error,
					NULL,
					"ss",
					"swupd-update.service",
					"replace")) < 0)
			break;

	} while (0);
	sd_bus_error_free(&error);
	sd_bus_unref(bus);

	exit(ret < 0 ? ret : EXIT_SUCCESS);
}
