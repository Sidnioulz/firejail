/*
 * Copyright (C) 2014, 2015 Firejail Authors
 * Copyright (C) 2015 Steve Dodier-Lazaro (sidnioulz@gmail.com)
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
#include "../include/exechelper.h"
#include <errno.h>
#include <fcntl.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

static int helper_files_generated = 0;

static void check_dir_or_file(const char *name) {
	assert(name);
	struct stat s;
	char *fname;
	if (asprintf(&fname, "/etc/%s", name) == -1)
		errExit("asprintf");
	if (arg_debug)
		printf("Checking %s\n", fname);		
	if (stat(fname, &s) == -1) {
		fprintf(stderr, "Error: file %s not found.\n", fname);
		exit(1);
	}
	
	// dir or regular file
	if (S_ISDIR(s.st_mode) || S_ISREG(s.st_mode)) {
		free(fname);
		return;
	}

	if (!is_link(fname)) {
		free(fname);
		return;
	}
	
	fprintf(stderr, "Error: invalid file type, %s.\n", fname);
	exit(1);
}

static void write_helper_list_to_file(const char *path, const char *list)
{
  /* write list down to appropriate file */
  if(arg_debug)
    printf("Creating the '%s' file...\n", path);
  int fd = open(path, O_WRONLY | O_CREAT, 0644);
  if (fd == -1)
    errExit("open");

  if (list) {
    if(arg_debug)
      printf("Writing list '%s' to '%s'...\n", list, path);

    if (write(fd, list, strlen(list)) == -1)
      errExit("write");
  }

  if (close(fd) == -1)
    errExit("close");
  else if (arg_debug)
      printf("Finished preparing file '%s'\n", path);

  /* also write a copy in the /run/firejail directory for helper daemon */
  if(arg_debug)
    printf("Writing a copy in directory '%s' for fireexecd...\n", EXECHELP_RUN_DIR);

  char *filename = strrchr(path, '/');
  if (!filename)
    errExit("strrchr");

  exechelp_build_run_dir();

  char *runpath;
  if (asprintf(&runpath, "%s/%d-%s", EXECHELP_RUN_DIR, sandbox_pid, filename+1) == -1)
    errExit("asprintf");

  fd = open(runpath, O_WRONLY | O_CREAT, 0644);
  if (fd == -1)
    errExit("open");

  if (list) {
    if (write(fd, list, strlen(list)) == -1)
      errExit("write");
  }

  if (close(fd) == -1)
    errExit("close");

  if (arg_debug)
      printf("Finished writing copy to path '%s'\n", runpath);
  free(runpath);
}

void fs_helper_generate_files(void) {
	struct stat s;
	fs_build_mnt_etc_dir();
	
	// create /tmp/firejail/mnt/etc/firejail directory
	if (stat(ETC_FJ_DIR, &s)) {
	  int rv = mkdir(ETC_FJ_DIR, S_IRWXU | S_IRWXG | S_IRWXO);
	  if (rv == -1)
		  errExit("mkdir");
	  if (chown(ETC_FJ_DIR, 0, 0) < 0)
		  errExit("chown");
	  if (chmod(ETC_FJ_DIR, 0755) < 0)
		  errExit("chmod");
	}
	
	// create /tmp/firejail/mnt/etc/firejail/self directory
	if (stat(SELF_DIR, &s)) {
	  int rv = mkdir(SELF_DIR, S_IRWXU | S_IRWXG | S_IRWXO);
	  if (rv == -1)
		  errExit("mkdir");
	  if (chown(SELF_DIR, 0, 0) < 0)
		  errExit("chown");
	  if (chmod(SELF_DIR, 0755) < 0)
		  errExit("chmod");
	}

  /* get list of linked helper applications */
  if(arg_debug)
    printf("Constructing a list of linked applications for child process '%s'...\n", cfg.command_name);

  char *linkastr = get_linked_apps_for_client();
  if(arg_debug) {
    if (linkastr)
      printf("Child process has linked applications: '%s'\n", linkastr);
    else
      printf("Child process has no linked applications, will write an empty file\n");
  }
  write_helper_list_to_file(LINKED_APPS_SB_PATH, linkastr);
  free(linkastr);

  /* get list of protected binaries */
  if(arg_debug)
    printf("Constructing a list of applications launchable exclusively outside the child process' sandbox...\n");
  char *protastr = get_protected_apps_for_client ();
  if(arg_debug) {
    if (protastr)
      printf("The following applications are protected by the sandbox: '%s'\n", protastr);
    else
      printf("No applications are protected by the sandbox, will write an empty file\n");
  }
  write_helper_list_to_file(PROTECTED_APPS_SB_PATH, protastr);
  free(protastr);

  /* get list of protected files */
  if(arg_debug)
    printf("Constructing a list of files openable exclusively outside the child process' sandbox...\n");
  char *protfstr = get_protected_files_for_client ();
  if(arg_debug) {
    if (protfstr)
      printf("The following files are protected by the sandbox: '%s'\n", protfstr);
    else
      printf("No files are protected by the sandbox, will write an empty file\n");
  }
  write_helper_list_to_file(PROTECTED_FILES_SB_PATH, protfstr);
  free(protfstr);

  /* generate list of white-listed apps from arg */
  if (arg_whitelist_apps) {
    if(arg_debug)
      printf("Constructing a list of white-listed apps passed as parameters to firejail...\n");
    if(arg_debug) {
      if (arg_whitelist_apps)
        printf("The following apps are white-listed for this firejail instance: '%s'\n", arg_whitelist_apps);
      else
        printf("No apps are white-listed by the sandbox, will write an empty file\n");
    }
  }
  write_helper_list_to_file(WHITELIST_APPS_SB_PATH, arg_whitelist_apps);

  /* generate list of white-listed files from arg */
  if (arg_whitelist_files) {
    if(arg_debug)
      printf("Constructing a list of white-listed files passed as parameters to firejail...\n");
    if(arg_debug) {
      if (arg_whitelist_files)
        printf("The following files are white-listed for this firejail instance: '%s'\n", arg_whitelist_files);
      else
        printf("No files are white-listed by the sandbox, will write an empty file\n");
    }
  }
  write_helper_list_to_file(WHITELIST_FILES_SB_PATH, arg_whitelist_files);

  helper_files_generated = 1;
}

#include <sys/utsname.h>
static int fs_helper_overlay_etc(void) {
	// check kernel version
	struct utsname u;
	int rv = uname(&u);
	if (rv != 0)
		errExit("uname");
	int major;
	int minor;
	if (2 != sscanf(u.release, "%d.%d", &major, &minor)) {
		fprintf(stderr, "Error: cannot extract Linux kernel version: %s\n", u.version);
		errExit("kernel version");
	}
	
	if (arg_debug)
		printf("Linux kernel version %d.%d\n", major, minor);
	int oldkernel = 0;
	if (major < 3) {
		fprintf(stderr, "Error: minimum kernel version required 3.x\n");
		errExit("kernel version");
	}
	if (major == 3 && minor < 18)
		oldkernel = 1;

	if (mkdir(SELF_OVERLAY_DIR, S_IRWXU | S_IRWXG | S_IRWXO))
		errExit("mkdir");
	if (chown(SELF_OVERLAY_DIR, 0, 0) < 0)
		errExit("chown");
	if (chmod(SELF_OVERLAY_DIR, S_IRWXU  | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH) < 0)
		errExit("chmod");

	char *oroot;
	if(asprintf(&oroot, "%s/oroot", SELF_OVERLAY_DIR) == -1)
		errExit("asprintf");
	if (mkdir(oroot, S_IRWXU | S_IRWXG | S_IRWXO))
		errExit("mkdir");
	if (chown(oroot, 0, 0) < 0)
		errExit("chown");
	if (chmod(oroot, S_IRWXU  | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH) < 0)
		errExit("chmod");

	char *odiff;
	if(asprintf(&odiff, "%s/odiff", SELF_OVERLAY_DIR) == -1)
		errExit("asprintf");
	if (mkdir(odiff, S_IRWXU | S_IRWXG | S_IRWXO))
		errExit("mkdir");
	if (chown(odiff, 0, 0) < 0)
		errExit("chown");
	if (chmod(odiff, S_IRWXU  | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH) < 0)
		errExit("chmod");
	
	char *owork;
	if(asprintf(&owork, "%s/owork", SELF_OVERLAY_DIR) == -1)
		errExit("asprintf");
	if (mkdir(owork, S_IRWXU | S_IRWXG | S_IRWXO))
		errExit("mkdir");
	if (chown(owork, 0, 0) < 0)
		errExit("chown");
	if (chmod(owork, S_IRWXU  | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH) < 0)
		errExit("chmod");
	
	// mount overlayfs
	if (arg_debug)
		printf("Mounting OverlayFS for execution helper files\n");
	char *option;
	if (oldkernel) { // old Ubuntu/OpenSUSE kernels
		if (asprintf(&option, "lowerdir=/,upperdir=%s", odiff) == -1)
			errExit("asprintf");
		if (mount("overlayfs", oroot, "overlayfs", MS_MGC_VAL, option) < 0)
			errExit("mounting overlayfs");
	}
	else { // kernel 3.18 or newer
		if (asprintf(&option, "lowerdir=/etc,upperdir=%s,workdir=%s", odiff, owork) == -1)
			errExit("asprintf");
//printf("option #%s#\n", option);			
		if (mount("overlay", oroot, "overlay", MS_MGC_VAL, option) < 0)
			errExit("mounting overlayfs");
	}
	if(arg_debug)
	  printf("OverlayFS configured in %s directory\n", SELF_OVERLAY_DIR);
	
	// create /etc/firejail/self equivalent in the OverlayFS
	struct stat s;
	if (stat(SELF_OVERLAY_DIR"/oroot/firejail/self", &s)) {
	  int rv = mkdir(SELF_OVERLAY_DIR"/oroot/firejail/self", S_IRWXU | S_IRWXG | S_IRWXO);
	  if (rv == -1)
	    errExit("mkdir");
    if (chown(SELF_OVERLAY_DIR"/oroot/firejail/self", 0, 0) < 0)
	    errExit("chown");
    if (chmod(SELF_OVERLAY_DIR"/oroot/firejail/self", 0755) < 0)
	    errExit("chmod");
	}
	
  if (arg_debug)
	  printf("Mount-bind %s on top of OverlayFS equivalent of /etc/firejail/self\n", SELF_DIR);
  if (mount(SELF_DIR, SELF_OVERLAY_DIR"/oroot/firejail/self", NULL, MS_BIND|MS_REC, NULL) < 0)
	  errExit("mount bind");
	
	// mount-bind overlay directory on top of etc
	if (arg_debug)
		printf("Mounting execution helper's OverlayFS over actual /etc\n");
	if (mount(oroot, "/etc", NULL, MS_BIND|MS_REC, NULL) < 0)
		errExit("mounting /etc");
	// cleanup and exit
	free(option);
	free(oroot);
	free(odiff);
}

void fs_helper_mount_self_dir(void) {
	struct stat s;
	if (stat(SELF_DIR, &s) == -1) {
		fprintf(stderr, "Error: cannot find %s directory\n", SELF_DIR);
		exit(1);
	}

	int must_overlay = 0;
	
	// create /etc/firejail
	if (stat("/etc/firejail", &s)) {
	  int rv = mkdir("/etc/firejail", S_IRWXU | S_IRWXG | S_IRWXO);
	  if (rv == -1) {
	    must_overlay = 1;
    } else {
	    if (chown("/etc/firejail", 0, 0) < 0)
		    errExit("chown");
	    if (chmod("/etc/firejail", 0755) < 0)
		    errExit("chmod");
    }
	}
	
	// create /etc/firejail/self
	if (!must_overlay && stat("/etc/firejail/self", &s)) {
	  int rv = mkdir("/etc/firejail/self", S_IRWXU | S_IRWXG | S_IRWXO);
	  if (rv == -1) {
	    must_overlay = 1;
	  }
	}

  // we must let all uids reach /etc/firejail/self for normal operation
  if (!must_overlay) {
    if (chmod("/etc/firejail", 0511) < 0)
	    errExit("chmod");
    if (chown("/etc/firejail/self", 0, 0) < 0)
	    errExit("chown");
    if (chmod("/etc/firejail/self", 0755) < 0)
	    errExit("chmod");
  }

  // create an overlay fs of /etc to be able to make /etc/firejail/self
  if (must_overlay) {
    if(fs_helper_overlay_etc()) {
      errExit("helper overlay");
    }
  } else {
	  if (arg_debug)
		  printf("Mount-bind %s on top of /etc/firejail/self\n", SELF_DIR);
	  if (mount(SELF_DIR, "/etc/firejail/self", NULL, MS_BIND|MS_REC, NULL) < 0) {
	    fprintf(stderr, "Error: could not mount-bind %s on top of /etc/firejail/self (error: %s)", SELF_DIR, strerror(errno));
	  }
  }
}
