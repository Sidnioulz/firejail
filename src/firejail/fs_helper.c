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
#include "../include/exechelper-logger.h"
#include "../include/pid.h"
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

static int helper_files_generated = 0;

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

  exechelp_build_run_user_dir(sandbox_pid);


  char *runpath;
  if (asprintf(&runpath, "%s/%d/%s", EXECHELP_RUN_DIR, sandbox_pid, filename+1) == -1)
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

void fs_helper_write_net_cleanup_file(const char *content)
{
  /* write list down to appropriate file */
  if(arg_debug)
    printf("Creating a file for fireexecd to know how to cleanup the child process's network stack...\n");

  exechelp_build_run_user_dir(sandbox_pid);

  char *runpath;
  if (asprintf(&runpath, "%s/%d/%s", EXECHELP_RUN_DIR, sandbox_pid, NET_CLEANUP_FILE) == -1)
    errExit("asprintf");

  int fd = open(runpath, O_WRONLY | O_CREAT, 0600);
  if (fd == -1)
    errExit("open");

  if (content) {
    if (write(fd, content, strlen(content)) == -1)
      errExit("write");
  }

  if (close(fd) == -1)
    errExit("close");

  if (arg_debug)
      printf("Finished writing instructions to path '%s'\n", runpath);
  free(runpath);
}

void fs_helper_generate_files(void) {
	struct stat s;
	fs_build_mnt_etc_dir();
	fs_build_mnt_run_dir();
	
	// create /tmp/firejail/mnt/run/firejail directory
	if (stat(RUN_FJ_DIR, &s)) {
	  int rv = mkdir(RUN_FJ_DIR, S_IRWXU | S_IRWXG | S_IRWXO);
	  if (rv == -1)
		  errExit("mkdir");
	  if (chown(RUN_FJ_DIR, 0, 0) < 0)
		  errExit("chown");
	  if (chmod(RUN_FJ_DIR, 0755) < 0)
		  errExit("chmod");
	}
	
	// create /tmp/firejail/mnt/run/firejail/self directory
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
    if (linkastr) {
      printf("Child process has linked applications: '%s'\n", linkastr);
      exechelp_logv("firejail", "Child process has linked applications: '%s'\n", linkastr);
    } else {
      printf("Child process has no linked applications, will write an empty file\n");
      exechelp_logv("firejail", "Child process has no linked applications, will write an empty file\n");
    }
  }
  write_helper_list_to_file(LINKED_APPS_SB_PATH, linkastr);
  free(linkastr);

  /* get list of protected binaries */
  if(arg_debug)
    printf("Constructing a list of applications launchable exclusively outside the child process' sandbox...\n");
  char *protastr = get_protected_apps_for_client ();
  if(arg_debug) {
    if (protastr) {
      printf("The following applications are protected by the sandbox: '%s'\n", protastr);
      exechelp_logv("firejail", "The following applications are protected by the sandbox: '%s'\n", protastr);
    } else {
      printf("No applications are protected by the sandbox, will write an empty file\n");
      exechelp_logv("firejail", "No applications are protected by the sandbox, will write an empty file\n");
    }
  }
  write_helper_list_to_file(PROTECTED_APPS_SB_PATH, protastr);
  free(protastr);

  /* get list of protected files */
  if(arg_debug)
    printf("Constructing a list of files openable exclusively outside the child process' sandbox...\n");
  char *protfstr = get_protected_files_for_client (1);
  if(arg_debug) {
    if (protfstr) {
      printf("The following files are protected by the sandbox: '%s'\n", protfstr);
      exechelp_logv("firejail", "The following files are protected by the sandbox: '%s'\n", protfstr);
    } else {
      printf("No files are protected by the sandbox, will write an empty file\n");
      exechelp_logv("firejail", "No files are protected by the sandbox, will write an empty file\n");
    }
  }
  write_helper_list_to_file(PROTECTED_FILES_SB_PATH, protfstr);
  free(protfstr);

  /* generate list of white-listed apps from arg */
  if(arg_debug)
    printf("Constructing a list of white-listed apps passed as parameters to firejail...\n");
  if(arg_debug) {
    if (arg_whitelist_apps) {
      printf("The following apps are white-listed for this firejail instance: '%s'\n", arg_whitelist_apps);
      exechelp_logv("firejail", "The following apps are white-listed for this firejail instance: '%s'\n", arg_whitelist_apps);
    } else {
      printf("No apps are white-listed by the sandbox, will write an empty file\n");
      exechelp_logv("firejail", "No apps are white-listed by the sandbox, will write an empty file\n");
    }
  }
  write_helper_list_to_file(WHITELIST_APPS_SB_PATH, arg_whitelist_apps);

  /* generate list of white-listed files from arg */
  if(arg_debug)
    printf("Constructing a list of white-listed files passed as parameters to firejail...\n");
  if(arg_debug) {
    if (arg_whitelist_files) {
      printf("The following files are white-listed for this firejail instance: '%s'\n", arg_whitelist_files);
      exechelp_logv("firejail", "The following files are white-listed for this firejail instance: '%s'\n", arg_whitelist_files);
    } else {
      printf("No files are white-listed by the sandbox, will write an empty file\n");
      exechelp_logv("firejail", "No files are white-listed by the sandbox, will write an empty file\n");
    }
  }
  write_helper_list_to_file(WHITELIST_FILES_SB_PATH, arg_whitelist_files);

  helper_files_generated = 1;
}

void fs_helper_fix_gtk3_windows(void) {
  //mkdir .config
  //mkdir .config/gtk-3.0
  //mount bind the folder on top of itself
  //open concat gtk.css
  //add our fix!
  uid_t realuid = getuid();
  gid_t realgid = getgid();
  struct stat s;

  if (arg_debug)
	  printf("Mounting a temporary GTK+3.0 config file for child process, to disable client-side decorations\n");

  // ~/.config
  char *confpath;
	if (asprintf(&confpath, "%s/.config/", cfg.homedir) == -1)
		errExit("asprintf");
	if (stat(confpath, &s)) {
		/* coverity[toctou] */
		int rv = mkdir(confpath, S_IRWXU | S_IRWXG | S_IRWXO);
		if (rv == -1) {
	    exechelp_logerrv("firejail", FIREJAIL_WARNING, "Error: could not create directory '%s' in user home for the GTK+3.0 config file. Window decorations might be duplicate: %s\n", confpath, strerror(errno));
      return;
		}
		if (chown(confpath, realuid, realgid) < 0) {
	    exechelp_logerrv("firejail", FIREJAIL_ERROR, "Error: chown %s failed: %s.\n", confpath, strerror(errno));
      return;
		}
		if (chmod(confpath, S_IRWXU  | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH) < 0) {
	    exechelp_logerrv("firejail", FIREJAIL_ERROR, "Error: chmod %s failed: %s.\n", confpath, strerror(errno));
      return;
		}
	}
  free(confpath);

  // ~/.config/gtk-3.0
  char *gtkpath;
	if (asprintf(&gtkpath, "%s/.config/gtk-3.0/", cfg.homedir) == -1)
		errExit("asprintf");
	if (stat(gtkpath, &s)) {
		/* coverity[toctou] */
		int rv = mkdir(gtkpath, S_IRWXU | S_IRWXG | S_IRWXO);
		if (rv == -1) {
	    exechelp_logerrv("firejail", FIREJAIL_WARNING, "Error: could not create directory '%s' in user home for the GTK+3.0 config file. Window decorations might be duplicate: %s\n", gtkpath, strerror(errno));
	    return;
		}
		if (chown(gtkpath, realuid, realgid) < 0) {
	    exechelp_logerrv("firejail", FIREJAIL_ERROR, "Error: chown %s failed: %s.\n", gtkpath, strerror(errno));
      return;
		}
		if (chmod(gtkpath, S_IRWXU  | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH) < 0) {
	    exechelp_logerrv("firejail", FIREJAIL_ERROR, "Error: chmod %s failed: %s.\n", gtkpath, strerror(errno));
      return;
		}
	}

  if (mount(gtkpath, gtkpath, NULL, MS_BIND|MS_REC, NULL) < 0) {
    exechelp_logerrv("firejail", FIREJAIL_WARNING, "Error: could not mount-bind directory '%s' to make changes to its files disposable. Window decorations might be duplicate: %s\n", gtkpath, strerror(errno));
    return;
	}
  free(gtkpath);
  
  // create a file to hold our gtk config in our temporary folder
  fs_build_mnt_dir();
  char *filepath;
	if (asprintf(&filepath, "%s/gtk.css", MNT_DIR) == -1)
	  errExit("asprintf");
  int fd = open(filepath, O_WRONLY | O_APPEND | O_CREAT, 0644);
  if (fd == -1) {
    exechelp_logerrv("firejail", FIREJAIL_WARNING, "Error: could not open file '%s' to write required CSS hacks. Window decorations might be duplicate: %s\n", filepath, strerror(errno));
    return;
	}
  if (chown(filepath, realuid, realgid) < 0) {
    exechelp_logerrv("firejail", FIREJAIL_ERROR, "Error: chown %s failed: %s.\n", filepath, strerror(errno));
    return;
	}
  if (chmod(filepath, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH) < 0) {
    exechelp_logerrv("firejail", FIREJAIL_ERROR, "Error: chmod %s failed: %s.\n", filepath, strerror(errno));
    return;
	}

  // ensure gtk.css exists, if it does, copy its content into our file
  char *targetpath;
  int missing = 0;
	if (asprintf(&targetpath, "%s/.config/gtk-3.0/gtk.css", cfg.homedir) == -1)
	  errExit("asprintf");
	if (stat(targetpath, &s))
	  missing = 1;

  int targetfd = open(targetpath, O_RDWR | O_CREAT, 0644);
  if (targetfd == -1) {
    exechelp_logerrv("firejail", FIREJAIL_WARNING, "Error: could not open file '%s' to read existing CSS rules. Window decorations might be duplicate: %s\n", targetpath, strerror(errno));
    return;
	}

  // ensure the user owns the new file
  if (missing) {
	  if (chown(targetpath, realuid, realgid) < 0) {
      exechelp_logerrv("firejail", FIREJAIL_ERROR, "Error: chown %s failed: %s.\n", targetpath, strerror(errno));
      return;
	  }
	  if (chmod(targetpath, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH) < 0) {
      exechelp_logerrv("firejail", FIREJAIL_ERROR, "Error: chmod %s failed: %s.\n", targetpath, strerror(errno));
      return;
	  }
	}
	// or copy its existing contents into our temporary file
	else {
	  ssize_t len, written;
	  char buf[1024];
	  errno = 0;
	  do {
	    len = read(targetfd, buf, sizeof(buf));
	    if (len == -1)
	      errExit("read");
      else
        written = write(fd, buf, len);
	    if (written != len)
	      errExit("write");
	  } while (len > 0);
	}
  close(targetfd);

   // write our extra CSS rules
  char *border_fix = ".window-frame, .window-frame:backdrop {\n" \
                    "  box-shadow: 0 0 0 black;\n" \
                    "  border-style: none;\n" \
                    "  margin: 0;\n" \
                    "  border-radius: 0;\n" \
                    "}\n\n" \
                    ".titlebar {\n" \
                    "  border-radius: 0;\n" \
                    "}\n";

  size_t len = strlen(border_fix);
  ssize_t written = write(fd, border_fix, len);
  if(written < len)
    errExit("write");
  close(fd);

  // finally, mount our temporary file on top of the current one
  if (mount(filepath, targetpath, NULL, MS_BIND|MS_REC, NULL) < 0)
    exechelp_logerrv("firejail", FIREJAIL_WARNING, "Error: could not mount-bind file '%s' into '%s'. Window decorations might be duplicate: %s\n", filepath, targetpath, strerror(errno));

  free(filepath);
  free(targetpath);
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
		exechelp_logv("firejail", "cannot extract Linux kernel version: %s\n", u.version);
		errExit("kernel version");
	}
	
	if (arg_debug)
		printf("Linux kernel version %d.%d\n", major, minor);
	int oldkernel = 0;
	if (major < 3) {
		fprintf(stderr, "Error: minimum kernel version required 3.x\n");
		exechelp_logv("firejail", "minimum kernel version required 3.x\n");
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
	
	// create /run/firejail/self equivalent in the OverlayFS
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
	  printf("Mount-bind %s on top of OverlayFS equivalent of /run/firejail/self\n", SELF_DIR);
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
		exechelp_logv("firejail", "cannot find %s directory\n", SELF_DIR);
		exit(1);
	}

	int must_overlay = 0;
	
	// create /run/firejail
	if (stat(EXECHELP_RUN_DIR, &s)) {
	  int rv = mkdir(EXECHELP_RUN_DIR, S_IRWXU | S_IRWXG | S_IRWXO);
	  if (rv == -1) {
	    must_overlay = 1;
    } else {
	    if (chown(EXECHELP_RUN_DIR, 0, 0) < 0)
		    errExit("chown");
	    if (chmod(EXECHELP_RUN_DIR, 0755) < 0)
		    errExit("chmod");
    }
	}
	
	// create /run/firejail/self
	if (!must_overlay && stat(EXECHELP_CLIENT_ROOT, &s)) {
	  int rv = mkdir(EXECHELP_CLIENT_ROOT, S_IRWXU | S_IRWXG | S_IRWXO);
	  if (rv == -1) {
	    must_overlay = 1;
	  }
	}

  if (!must_overlay) {
    if (chown(EXECHELP_CLIENT_ROOT, 0, 0) < 0)
	    errExit("chown");
    if (chmod(EXECHELP_CLIENT_ROOT, 0755) < 0)
	    errExit("chmod");
  }

  // create an overlay fs of /etc to be able to make /run/firejail/self
  if (must_overlay) {
    if(fs_helper_overlay_etc()) {
      errExit("helper overlay");
    }
  } else {
	  if (arg_debug)
		  printf("Mount-bind %s on top of /run/firejail/self\n", SELF_DIR);
	  if (mount(SELF_DIR, EXECHELP_CLIENT_ROOT, NULL, MS_BIND|MS_REC|MS_RDONLY, NULL) < 0) {
	    fprintf(stderr, "Error: could not mount-bind %s on top of /run/firejail/self (error: %s)\n", SELF_DIR, strerror(errno));
	    exechelp_logv("firejail", "could not mount-bind %s on top of /run/firejail/self (error: %s)\n", SELF_DIR, strerror(errno));
	  }
  }
}

void fs_helper_disable_other_run_subdirs(void) {
  DIR *dir;
  if (!(dir = opendir(EXECHELP_RUN_DIR))) {
	  // sleep 2 seconds and try again
	  sleep(2);
	  if (!(dir = opendir(EXECHELP_RUN_DIR))) {
		  exechelp_logerrv("firejail", FIREJAIL_ERROR, "Error: cannot open /run/firejail directory\n");
		  exit(1);
	  }
  }

  struct dirent *entry;
  char *end;
  char *path;
  while ((entry = readdir(dir))) {
	  pid_t newpid = strtol(entry->d_name, &end, 10);
	  if (end == entry->d_name || *end)
		  continue;
	  if (newpid == sandbox_pid)
		  continue;

    if (asprintf(&path, "/run/firejail/%d/", newpid) == -1)
      continue;

	  fs_blacklist_file(path);
	  free(path);
  }

  closedir(dir);

  for (int i = 0; i < max_pids; i++) {
    if (asprintf(&path, "/run/firejail/%d/", i) == -1)
      continue;

    mkdir(path, S_IRWXU | S_IRWXG | S_IRWXO);
	  fs_blacklist_file(path);
	  free(path);
  }
}
