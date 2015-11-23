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

void usage(void) {
	printf("fireexecd - version %s\n", VERSION);
	printf("Usage: fireexecd [OPTIONS] [PID]\n\n");
	printf("Help processes started in a Firejail sandbox to perform actions outside their\n");
	printf("sandbox, specifically to open protected files in a new sandbox or run protected\n");
	printf("applications in their pre-defined sandbox profile.\n\n");
	printf("Options:\n");
	printf("\t--help, -? - this help screen.\n\n");
	printf("\t--version - print program version and exit.\n\n");

	printf("The fireexecd daemon must be running in order for the --helper option to be used.\n\n");

	printf("License GPL version 3 or later\n");
	printf("fireexecd is an unofficial extension to Firejail.\n");
	printf("Homepage for Firejail: http://firejail.sourceforge.net\n");
	printf("Homepage for fireexecd: https://github.com/Sidnioulz/firejail\n");
	printf("\n");
}
