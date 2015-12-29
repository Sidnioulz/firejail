/*
 * Copyright (C) 2014, 2015 netblue30 (netblue30@yahoo.com)
 *
 * This file is part of firejail project
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/
#include "fireexecd.h"
#include <grp.h>
#include <pthread.h>
#include <signal.h>
#include <termios.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/prctl.h>

int arg_debug = 2; // always debugging in fireexecd
int arg_slow = 0;
struct timeval dbg_tv;
time_t         dbg_t;
struct tm      dbg_tm;
char           dbg_line[200];

static struct termios tlocal;	// startup terminal setting
static struct termios twait;	// no wait on key press
static int terminal_set = 0;
static pthread_t regthread;  // thread for registering new sandbox clients, see socket.c

static void my_handler(int s){
	// Remove unused parameter warning
	(void)s;

	if (terminal_set)
		tcsetattr(0, TCSANOW, &tlocal);

  // gracefully shut down our log
  exechelp_log_close();
  DBGOUT("[0]\tINFO:  Fireexecd exiting\n");
  
	exit(0);
}

// find the first child process for the specified pid
// return -1 if not found
int find_child(int id) {
	int i;
	for (i = 0; i < max_pids; i++) {
		if (pids[i].level == 2 && pids[i].parent == id)
			return i;
	}
	
	return -1;
}

// drop privileges
void fireexecd_drop_privs(void) {
	// drop privileges
	if (setgroups(0, NULL) < 0)
		errExit("setgroups");
	if (setgid(getgid()) < 0)
		errExit("setgid/getgid");
	if (setuid(getuid()) < 0)
		errExit("setuid/getuid");
}

// sleep and wait for a key to be pressed
void fireexecd_sleep(int st) {
	if (terminal_set == 0) {
		tcgetattr(0, &twait);          // get current terminal attirbutes; 0 is the file descriptor for stdin
		memcpy(&tlocal, &twait, sizeof(tlocal));
		twait.c_lflag &= ~ICANON;      // disable canonical mode
		twait.c_lflag &= ~ECHO;	// no echo
		twait.c_cc[VMIN] = 1;          // wait until at least one keystroke available
		twait.c_cc[VTIME] = 0;         // no timeout
		terminal_set = 1;
	}
	tcsetattr(0, TCSANOW, &twait);


	fd_set fds;
	FD_ZERO(&fds);
	FD_SET(0,&fds);
	int maxfd = 1;

	struct timeval ts;
	ts.tv_sec = st;
	ts.tv_usec = 0;

	int ready = select(maxfd, &fds, (fd_set *) 0, (fd_set *) 0, &ts);
	(void) ready;
	if( FD_ISSET(0, &fds)) {
		getchar();
		tcsetattr(0, TCSANOW, &tlocal);
		printf("\n");
		exit(0);
	}
	tcsetattr(0, TCSANOW, &tlocal);
}


int main(int argc, char **argv) {
	int i;

  // ignore children when they leave, that'll happen routinely with client handlers
  signal(SIGCHLD, SIG_IGN);

	// handle CTRL-C
	signal (SIGINT, my_handler);
	signal (SIGTERM, my_handler);

	for (i = 1; i < argc; i++) {
		// default options
		if (strcmp(argv[i], "--help") == 0 ||
		    strcmp(argv[i], "-?") == 0) {
			usage();
			return 0;
		}
		else if (strcmp(argv[i], "--version") == 0) {
			printf("fireexecd version %s\n\n", VERSION);
			return 0;
		}
		else if (strcmp(argv[i], "--slow") == 0) {
		  arg_slow = 1;
		}
		// invalid option
		else if (*argv[i] == '-') {
			fprintf(stderr, "Error: invalid option\n");		
			return 1;
		}
	}

	if (arg_slow) {
	  pid_t my_id = getpid();
	  int sleep_time = 30;
	  if(arg_debug)
	    printf("Program starting in slow mode, you now have %d seconds to attach to process %d.\n"\
	           "You can use the following command:\n\n\tsudo gdb attach %d\n\n", sleep_time, my_id, my_id);
	  sleep(sleep_time);
	  if(arg_debug)
	    printf("Resuming program...\n");
  }

  // check for root privilege before starting the registration procedure
	if (geteuid() != 0) {
		fprintf(stderr, "Error: you need to be root to get process events\n");
		exit(1);
	}

  DBGOUT("[0]\tINFO:  Fireexecd now running\n");
  if (pthread_create(&regthread, NULL, registration_run, NULL))
    errExit("pthread_create");  
	
	procevent(0); // never to return
	return 0;
}
