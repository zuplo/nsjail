/*

   nsjail - logging
   -----------------------------------------

   Copyright 2014 Google Inc. All Rights Reserved.

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

     http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.

*/

#include "logs.h"

#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "nsjail.h"

#include <string.h>

namespace logs {

static int _log_fd = STDERR_FILENO;
static bool _log_fd_isatty = true;
static enum llevel_t _log_level = INFO;

__attribute__((constructor)) static void log_init(void) {
	_log_fd_isatty = isatty(_log_fd);
}

/*
 * Log to stderr by default. Use a dup()d fd, because in the future we'll associate the
 * connection socket with fd (0, 1, 2).
 */
bool initLog(const std::string& logfile, llevel_t level) {
	/* Close previous log_fd */
	if (_log_fd > STDERR_FILENO) {
		close(_log_fd);
		_log_fd = STDERR_FILENO;
	}
	_log_level = level;
	if (logfile.empty()) {
		_log_fd = fcntl(_log_fd, F_DUPFD_CLOEXEC, 0);
	} else {
		if (TEMP_FAILURE_RETRY(_log_fd = open(logfile.c_str(),
					   O_CREAT | O_RDWR | O_APPEND | O_CLOEXEC, 0640)) == -1) {
			_log_fd = STDERR_FILENO;
			_log_fd_isatty = (isatty(_log_fd) == 1 ? true : false);
			PLOG_E("Couldn't open logfile open('%s')", logfile.c_str());
			return false;
		}
	}
	_log_fd_isatty = (isatty(_log_fd) == 1 ? true : false);
	return true;
}

void logMsg(enum llevel_t ll, const char* fn, int ln, bool perr, const char* fmt, ...) {
	if (ll < _log_level) {
		return;
	}

	char strerr[512];
	if (perr) {
		snprintf(strerr, sizeof(strerr), "%s", strerror(errno));
	}
	struct ll_t {
		const char* const descr;
		const char* const prefix;
		const bool print_funcline;
		const bool print_time;
	};
	static struct ll_t const logLevels[] = {
	    {"D", "\033[0;4m", true, true},
	    {"I", "\033[1m", false, true},
	    {"W", "\033[0;33m", true, true},
	    {"E", "\033[1;31m", true, true},
	    {"F", "\033[7;35m", true, true},
	    {"HR", "\033[0m", false, false},
	    {"HB", "\033[1m", false, false},
	};

	time_t ltstamp = time(NULL);
	struct tm utctime;
	localtime_r(&ltstamp, &utctime);
	char timestr[32];
	if (strftime(timestr, sizeof(timestr) - 1, "%FT%T%z", &utctime) == 0) {
		timestr[0] = '\0';
	}

	/* Start printing logs */
	if (_log_fd_isatty) {
		dprintf(_log_fd, "%s", logLevels[ll].prefix);
	}
	if (logLevels[ll].print_time) {
		dprintf(_log_fd, "[%s] ", timestr);
	}
	if (logLevels[ll].print_funcline) {
		dprintf(_log_fd, "[%s][%d] %s():%d ", logLevels[ll].descr, (int)getpid(), fn, ln);
	}

	va_list args;
	va_start(args, fmt);
	vdprintf(_log_fd, fmt, args);
	va_end(args);
	if (perr) {
		dprintf(_log_fd, ": %s", strerr);
	}
	if (_log_fd_isatty) {
		dprintf(_log_fd, "\033[0m");
	}
	dprintf(_log_fd, "\n");
	/* End printing logs */

	if (ll == FATAL) {
		exit(0xff);
	}
}

void logStop(int sig) {
	LOG_I("Server stops due to fatal signal (%d) caught. Exiting", sig);
}

}  // namespace logs