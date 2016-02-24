/*
 * Copyright (C) 2014, 2015 Firejail Authors
 *
 * This file is part of firejail project
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
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
#include "firejail.h"
#include <dirent.h>
#include <sys/stat.h>

#define MAX_READ 8192				  // line buffer for profile files

// find and read the profile specified by name from dir directory
int profile_find(const char *name, const char *dir) {
	assert(name);
	assert(dir);
	
	int rv = 0;
	DIR *dp;
	char *pname;
	if (asprintf(&pname, "%s.profile", name) == -1)
		errExit("asprintf");

	dp = opendir (dir);
	if (dp != NULL) {
		struct dirent *ep;
		while ((ep = readdir(dp)) != NULL) {
			if (strcmp(ep->d_name, pname) == 0) {
				if (arg_debug)
					printf("Found %s profile in %s directory\n", name, dir);
				char *etcpname;
				if (asprintf(&etcpname, "%s/%s", dir, pname) == -1)
					errExit("asprintf");
				profile_read(etcpname, NULL, NULL);
				free(etcpname);
				rv = 1;
				break;
			}
		}
		(void) closedir (dp);
	}

	free(pname);
	return rv;
}


//***************************************************
// run-time profiles
//***************************************************
static void check_file_name(char *ptr, int lineno) {
	if (strncmp(ptr, "${HOME}", 7) == 0)
		ptr += 7;
	else if (strncmp(ptr, "${PATH}", 7) == 0)
		ptr += 7;

	int len = strlen(ptr);
	// file globbing ('*') is allowed
	if (strcspn(ptr, "\\&!?\"'<>%^(){}[];,") != (size_t)len) {
		if (lineno == 0)
			exechelp_logerrv("firejail", "Error: \"%s\" is an invalid filename\n", ptr);
		else
			exechelp_logerrv("firejail", "Error: line %d in the custom profile is invalid\n", lineno);
		exit(1);
	}
}


// check profile line; if line == 0, this was generated from a command line option
// return 1 if the command is to be added to the linked list of profile commands
// return 0 if the command was already executed inside the function
int profile_check_line(char *ptr, int lineno) {
	// seccomp, caps, private, user namespace
	if (strcmp(ptr, "noroot") == 0) {
		check_user_namespace();
		return 0;
	}
	else if (strcmp(ptr, "seccomp") == 0) {
		arg_seccomp = 1;
		return 0;
	}
	else if (strncmp(ptr, "name ", 5) == 0) {
	  arg_hostname = strdup(ptr + 5);
		return 0;
	}
	else if (strcmp(ptr, "caps") == 0) {
		arg_caps_default_filter = 1;
		return 0;
	}
	else if (strcmp(ptr, "caps.drop all") == 0) {
		arg_caps_drop_all = 1;
		return 0;
	}
	else if (strcmp(ptr, "shell none") == 0) {
		arg_shell_none = 1;
		return 0;
	}
	else if (strncmp(ptr, "reachability ", 13) == 0) {
	  char *start = ptr + 13;
	  if (strcmp(start, "always-reachable") == 0)
	    arg_reachability = ALWAYS_REACHABLE;
    else if (strcmp(start, "not-with-foreign-apps") == 0)
	    arg_reachability = NO_FOREIGN_SANDBOX;
    else if (strcmp(start, "always-sandboxed") == 0)
      arg_reachability = ALWAYS_SANDBOXED;
	  
		return 0;
	}
	else if (strcmp(ptr, "private") == 0) {
	  if (arg_overlay_home == 1) {
		  exechelp_logerrv("firejail", "Error: 'private' at line %d conflicts with 'overlay-private-home' previously seen.\n", lineno);
  		exit(1);  
    }
	  if (arg_overlay == 1) {
		  exechelp_logerrv("firejail", "Error: 'private' at line %d conflicts with 'overlay-private-home' previously seen.\n", lineno);
  		exit(1);  
    }
		arg_private = 1;
		return 0;
	}
	else if (strcmp(ptr, "private-dev") == 0) {
		arg_private_dev = 1;
		return 0;
	}
	else if (strcmp(ptr, "overlay") == 0) {
	  if (arg_private == 1) {
		  exechelp_logerrv("firejail", "Error: 'overlay' at line %d conflicts with 'private' previously seen.\n", lineno);
  		exit(1);  
    }
    arg_overlay = 1;
		return 0;
	}
	else if (strcmp(ptr, "overlay-private-home") == 0) {
	  if (arg_private == 1) {
		  exechelp_logerrv("firejail", "Error: 'overlay-private-home' at line %d conflicts with 'private' previously seen.\n", lineno);
  		exit(1);  
    }
    arg_overlay = 1;
    arg_overlay_home = 1;
		return 0;
	}
	else if (strcmp(ptr, "nogroups") == 0) {
		arg_nogroups = 1;
		return 0;
	}
	else if (strcmp(ptr, "netfilter") == 0) {
		arg_netfilter = 1;
		return 0;
	}
	else if (strncmp(ptr, "netfilter ", 10) == 0) {
		arg_netfilter = 1;
		arg_netfilter_file = strdup(ptr + 10);
		if (!arg_netfilter_file)
			errExit("strdup");
		check_netfilter_file(arg_netfilter_file);
		return 0;
	}
	else if (strcmp(ptr, "net none") == 0) {
		arg_nonetwork  = 1;
		cfg.bridgenat.configured = 0;
		cfg.bridge0.configured = 0;
		cfg.bridge1.configured = 0;
		cfg.bridge2.configured = 0;
		cfg.bridge3.configured = 0;
		return 0;
	}
	else if (strcmp(ptr, "net any") == 0 || strcmp(ptr, "net auto") == 0) {
		cfg.bridgenat.configured = 1;
		cfg.bridge0.configured = 0;
		cfg.bridge1.configured = 0;
		cfg.bridge2.configured = 0;
		cfg.bridge3.configured = 0;
		return 0;
	}
	
	if (strncmp(ptr, "env ", 4) == 0) {
		env_store(ptr + 4);
		return 0;
	}
	
	// seccomp drop list on top of default list
	if (strncmp(ptr, "seccomp ", 8) == 0) {
		arg_seccomp = 1;
#ifdef HAVE_SECCOMP
		arg_seccomp_list = strdup(ptr + 8);
		if (!arg_seccomp_list)
			errExit("strdup");
#endif
		return 0;
	}
	
	// seccomp drop list without default list
	if (strncmp(ptr, "seccomp.drop ", 13) == 0) {
		arg_seccomp = 1;
#ifdef HAVE_SECCOMP
		arg_seccomp_list_drop = strdup(ptr + 13);
		if (!arg_seccomp_list_drop)
			errExit("strdup");
#endif
		return 0;
	}

	// seccomp keep list
	if (strncmp(ptr, "seccomp.keep ", 13) == 0) {
		arg_seccomp = 1;
#ifdef HAVE_SECCOMP
		arg_seccomp_list_keep= strdup(ptr + 13);
		if (!arg_seccomp_list_keep)
			errExit("strdup");
#endif
		return 0;
	}
	
	// caps drop list
	if (strncmp(ptr, "caps.drop ", 10) == 0) {
		arg_caps_drop = 1;
		arg_caps_list = strdup(ptr + 10);
		if (!arg_caps_list)
			errExit("strdup");
		// verify seccomp list and exit if problems
		if (caps_check_list(arg_caps_list, NULL))
			exit(1);
		return 0;
	}
	
	// caps keep list
	if (strncmp(ptr, "caps.keep ", 10) == 0) {
		arg_caps_keep = 1;
		arg_caps_list = strdup(ptr + 10);
		if (!arg_caps_list)
			errExit("strdup");
		// verify seccomp list and exit if problems
		if (caps_check_list(arg_caps_list, NULL))
			exit(1);
		return 0;
	}
	
	// dns
	if (strncmp(ptr, "dns ", 4) == 0) {
		uint32_t dns;
		if (atoip(ptr + 4, &dns)) {
			exechelp_logerrv("firejail", "Error: invalid DNS server IP address\n");
			return 1;
		}
		
		if (cfg.dns1 == 0)
			cfg.dns1 = dns;
		else if (cfg.dns2 == 0)
			cfg.dns2 = dns;
		else if (cfg.dns3 == 0)
			cfg.dns3 = dns;
		else {
			exechelp_logerrv("firejail", "Error: up to 3 DNS servers can be specified\n");
			return 1;
		}
		return 0;
	}
	
	// cpu affinity
	if (strncmp(ptr, "cpu ", 4) == 0) {
		read_cpu_list(ptr + 4);
		return 0;
	}
	
	// cgroup
	if (strncmp(ptr, "cgroup ", 7) == 0) {
		set_cgroup(ptr + 7);
		return 0;
	}
	
	// private directory
	if (strncmp(ptr, "private ", 8) == 0) {
		cfg.home_private = ptr + 8;
		fs_check_private_dir();
		arg_private = 1;
		return 0;
	}

	// private home list of files and directories
	if ((strncmp(ptr, "private.keep ", 13) == 0)
	 || (strncmp(ptr, "private-home ", 13) == 0)) {
		cfg.home_private_keep = ptr + 13;
		fs_check_home_list();
		arg_private = 1;
		return 0;
	}
	
	// private /etc list of files and directories
	if (strncmp(ptr, "private-etc ", 12) == 0) {
		cfg.etc_private_keep = ptr + 12;
		fs_check_etc_list();
		arg_private_etc = 1;
		return 0;
	}

	// filesystem bind
	if (strncmp(ptr, "bind ", 5) == 0) {
		if (getuid() != 0) {
			exechelp_logerrv("firejail", "Error: --bind option is available only if running as root\n");
			exit(1);
		}

		// extract two directories
		char *dname1 = ptr + 5;
		char *dname2 = split_comma(dname1); // this inserts a '0 to separate the two dierctories
		if (dname2 == NULL) {
			exechelp_logerrv("firejail", "Error: mising second directory for bind\n");
			exit(1);
		}
		
		// check directories
		check_file_name(dname1, lineno);
		check_file_name(dname2, lineno);
		if (strstr(dname1, "..") || strstr(dname2, "..")) {
			exechelp_logerrv("firejail", "Error: invalid file name.\n");
			exit(1);
		}
		
		// insert comma back
		*(dname2 - 1) = ',';
		return 1;
	}

	// rlimit
	if (strncmp(ptr, "rlimit", 6) == 0) {
		if (strncmp(ptr, "rlimit-nofile ", 14) == 0) {
			ptr += 14;
			if (not_unsigned(ptr)) {
				exechelp_logerrv("firejail", "Invalid rlimit option on line %d\n", lineno);
				exit(1);
			}
			sscanf(ptr, "%u", &cfg.rlimit_nofile);
			arg_rlimit_nofile = 1;
		}
		else if (strncmp(ptr, "rlimit-nproc ", 13) == 0) {
			ptr += 13;
			if (not_unsigned(ptr)) {
				exechelp_logerrv("firejail", "Invalid rlimit option on line %d\n", lineno);
				exit(1);
			}
			sscanf(ptr, "%u", &cfg.rlimit_nproc);
			arg_rlimit_nproc = 1;
		}
		else if (strncmp(ptr, "rlimit-fsize ", 13) == 0) {
			ptr += 13;
			if (not_unsigned(ptr)) {
				exechelp_logerrv("firejail", "Invalid rlimit option on line %d\n", lineno);
				exit(1);
			}
			sscanf(ptr, "%u", &cfg.rlimit_fsize);
			arg_rlimit_fsize = 1;
		}
		else if (strncmp(ptr, "rlimit-sigpending ", 18) == 0) {
			ptr += 18;
			if (not_unsigned(ptr)) {
				exechelp_logerrv("firejail", "Invalid rlimit option on line %d\n", lineno);
				exit(1);
			}
			sscanf(ptr, "%u", &cfg.rlimit_sigpending);
			arg_rlimit_sigpending = 1;
		}
		else {
			exechelp_logerrv("firejail", "Invalid rlimit option on line %d\n", lineno);
			exit(1);
		}
		
		return 0;		
	}

	// rest of filesystem
	if (strncmp(ptr, "blacklist ", 10) == 0)
		ptr += 10;
	else if (strncmp(ptr, "noblacklist ", 12) == 0)
		ptr += 12;
	else if (strncmp(ptr, "read-only ", 10) == 0)
		ptr += 10;
	else if (strncmp(ptr, "tmpfs ", 6) == 0)
		ptr += 6;
	else {
		if (lineno == 0)
			exechelp_logerrv("firejail", "Error: \"%s\" as a command line option is invalid\n", ptr);
		else
			exechelp_logerrv("firejail", "Error: line %d in the custom profile is invalid\n", lineno);
		exit(1);
	}

	// some characters just don't belong in filenames
	check_file_name(ptr, lineno);
	if (strstr(ptr, "..")) {
		if (lineno == 0)
			exechelp_logerrv("firejail", "Error: \"%s\" is an invalid filename\n", ptr);
		else
			exechelp_logerrv("firejail", "Error: line %d in the custom profile is invalid\n", lineno);
		exit(1);
	}
	return 1;
}

// add a profile entry in cfg.profile list; use str to populate the list
void profile_add(char *str) {
	ProfileEntry *prf = malloc(sizeof(ProfileEntry));
	if (!prf)
		errExit("malloc");
	prf->next = NULL;
	prf->data = str;	

	// add prf to the list
	if (cfg.profile == NULL) {
		cfg.profile = prf;
		return;
	}
	ProfileEntry *ptr = cfg.profile;
	while (ptr->next != NULL)
		ptr = ptr->next;
	ptr->next = prf;
}

// read a profile file
static int include_level = 0;
// skip1, skip2 - if the string is found in the line, the line is not interpreted
void profile_read(const char *fname, const char *skip1, const char *skip2) {
	// exit program if maximum include level was reached
	if (include_level > MAX_INCLUDE_LEVEL) {
		exechelp_logerrv("firejail", "Error: maximum profile include level was reached\n");
		exit(1);	
	}

	if (strlen(fname) == 0) {
		exechelp_logerrv("firejail", "Error: invalid profile file\n");
		exit(1);
	}

	// open profile file:
	FILE *fp = fopen(fname, "r");
	if (fp == NULL) {
		exechelp_logerrv("firejail", "Error: cannot open profile file\n");
		exit(1);
	}

	exechelp_logerrv("firejail", "Reading profile %s\n", fname);

	// read the file line by line
	char buf[MAX_READ + 1];
	int lineno = 0;
	while (fgets(buf, MAX_READ, fp)) {
		++lineno;
		// remove empty space - ptr in allocated memory
		char *ptr = line_remove_spaces(buf);
		if (ptr == NULL)
			continue;
		
		// comments
		if (*ptr == '#' || *ptr == '\0') {
			free(ptr);
			continue;
		}
		
		// process include
		if (strncmp(ptr, "include ", 8) == 0) {
			include_level++;
			
			// extract profile filename and new skip params
			char *newprofile = ptr + 8; // profile name
			char *newskip1 = NULL; // new skip1
			char *newskip2 = NULL; // new skip2
			char *p = newprofile;
			while (*p != '\0') {
				if (*p == ' ') {
					*p = '\0';
					if (newskip1 == NULL)
						newskip1 = p + 1;
					else if (newskip2 == NULL)
						newskip2 = p + 1;
				}
				p++;
			}
			
			// expand ${HOME}/ in front of the new profile file
			char *newprofile2 = expand_home(newprofile, cfg.homedir);
			
			// recursivity
			profile_read((newprofile2)? newprofile2:newprofile, newskip1, newskip2);
			include_level--;
			if (newprofile2)
				free(newprofile2);
			free(ptr);
			continue;
		}
		
		// skip
		if (skip1) {
			if (strstr(ptr, skip1)) {
				free(ptr);
				continue;
			}
		}
		if (skip2) {
			if (strstr(ptr, skip2)) {
				free(ptr);
				continue;
			}
		}
		
		// verify syntax, exit in case of error
		if (profile_check_line(ptr, lineno))
			profile_add(ptr);
	}
	fclose(fp);
}
