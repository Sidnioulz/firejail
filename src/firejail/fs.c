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
#include "../include/exechelper.h"
#include <sys/mount.h>
#include <sys/stat.h>
#include <linux/limits.h>
#include <fnmatch.h>
#include <glob.h>
#include <dirent.h>
#include <fcntl.h>
#include <errno.h>
#include <libmount.h>
#include <sys/types.h>
#include <pwd.h>


int mkdir_if_not_exists(const char *pathname, mode_t mode)
{
	struct stat s;

	if (stat(pathname, &s) == 0)
	  return S_ISDIR(s.st_mode)? 0 : EEXIST;
  else
    return mkdir(pathname, mode);
}


int mkdir_if_not_exists_recursive(const char *pathname, mode_t mode)
{
  if (!pathname) {
    errno = EFAULT;
    return -1;
  }

  char *path = strdup(pathname);
  char* p;
  int failure = 0;

  for (p = strchr(path+1, '/'); !failure && p; p = strchr(p+1, '/')) {
    *p = '\0';
    failure = mkdir_if_not_exists(path, mode);
    *p = '/';
  }
  free (path);

  if (!failure)
    failure = mkdir_if_not_exists(pathname, mode);

  return failure;
}

static int clear_recursive(const char *dirpath) {
	struct dirent *dir;
  DIR *d = opendir(dirpath);
  if (d == NULL)
    return errno == ENOENT? 0:-1;

  while ((dir = readdir(d))) {
    if(strcmp(dir->d_name, "." ) == 0 || strcmp(dir->d_name, ".." ) == 0)
	    continue;

    if (dir->d_type == DT_DIR ) {
      clear_recursive(dir->d_name);
      rmdir(dir->d_name);
    } else
      unlink(dir->d_name);
  }

  closedir(d);
  return 0;
}


// build /tmp/firejail directory
void fs_build_firejail_dir(void) {
	struct stat s;

	if (stat(FIREJAIL_DIR, &s)) {
		if (arg_debug)
			printf("Creating %s directory\n", FIREJAIL_DIR);
		/* coverity[toctou] */
		int rv = mkdir(FIREJAIL_DIR, S_IRWXU | S_IRWXG | S_IRWXO);
		if (rv == -1)
			errExit("mkdir");
		if (chown(FIREJAIL_DIR, 0, 0) < 0)
			errExit("chown");
		if (chmod(FIREJAIL_DIR, S_IRWXU  | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH) < 0)
			errExit("chmod");
	}
	else { // check /tmp/firejail directory belongs to root end exit if doesn't!
		if (s.st_uid != 0 || s.st_gid != 0) {
			exechelp_logerrv("firejail", FIREJAIL_ERROR, "Error: non-root %s directory, exiting...\n", FIREJAIL_DIR);
			exit(1);
		}
	}
}

// build /tmp/firejail/mnt directory
static int tmpfs_mounted = 0;
void fs_build_mnt_dir(void) {
	struct stat s;
	fs_build_firejail_dir();
	
	// create /tmp/firejail directory
	if (stat(MNT_DIR, &s)) {
		if (arg_debug)
			printf("Creating %s directory\n", MNT_DIR);
		/* coverity[toctou] */
		int rv = mkdir(MNT_DIR, S_IRWXU | S_IRWXG | S_IRWXO);
		if (rv == -1)
			errExit("mkdir");
		if (chown(MNT_DIR, 0, 0) < 0)
			errExit("chown");
		if (chmod(MNT_DIR, S_IRWXU  | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH) < 0)
			errExit("chmod");
	}

	// ... and mount tmpfs on top of it
	if (!tmpfs_mounted) {
		// mount tmpfs on top of /tmp/firejail/mnt
		if (arg_debug)
			printf("Mounting tmpfs on %s directory\n", MNT_DIR);
		if (mount("tmpfs", MNT_DIR, "tmpfs", MS_NOSUID | MS_STRICTATIME | MS_REC,  "mode=755,gid=0") < 0)
			errExit("mounting /tmp/firejail/mnt");
		tmpfs_mounted = 1;
	}
}
void fs_build_mnt_etc_dir(void) {
	struct stat s;
	fs_build_mnt_dir();
	
	// create /tmp/firejail directory
	if (stat(ETC_DIR, &s)) {
		if (arg_debug)
			printf("Creating %s directory\n", ETC_DIR);
		/* coverity[toctou] */
		int rv = mkdir(ETC_DIR, S_IRWXU | S_IRWXG | S_IRWXO);
		if (rv == -1)
			errExit("mkdir");
		if (chown(ETC_DIR, 0, 0) < 0)
			errExit("chown");
		if (chmod(ETC_DIR, S_IRWXU  | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH) < 0)
			errExit("chmod");
	}
}
void fs_build_mnt_run_dir(void) {
	struct stat s;
	fs_build_mnt_dir();
	
	// create /tmp/firejail directory
	if (stat(RUN_DIR, &s)) {
		if (arg_debug)
			printf("Creating %s directory\n", RUN_DIR);
		/* coverity[toctou] */
		int rv = mkdir(RUN_DIR, S_IRWXU | S_IRWXG | S_IRWXO);
		if (rv == -1)
			errExit("mkdir");
		if (chown(RUN_DIR, 0, 0) < 0)
			errExit("chown");
		if (chmod(RUN_DIR, S_IRWXU  | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH) < 0)
			errExit("chmod");
	}
}

//***********************************************
// process profile file
//***********************************************
typedef enum {
	BLACKLIST_FILE,
	MOUNT_READONLY,
	MOUNT_TMPFS,
	OPERATION_MAX
} OPERATION;


static char *create_empty_dir(void) {
	struct stat s;
	fs_build_firejail_dir();
	
	if (stat(RO_DIR, &s)) {
		/* coverity[toctou] */
		int rv = mkdir(RO_DIR, S_IRUSR | S_IXUSR);
		if (rv == -1)
			errExit("mkdir");	
		if (chown(RO_DIR, 0, 0) < 0)
			errExit("chown");
	}
	
	return RO_DIR;
}

static char *create_empty_file(void) {
	struct stat s;
	fs_build_firejail_dir();

	if (stat(RO_FILE, &s)) {
		/* coverity[toctou] */
		FILE *fp = fopen(RO_FILE, "w");
		if (!fp)
			errExit("fopen");
		fclose(fp);
		if (chown(RO_FILE, 0, 0) < 0)
			errExit("chown");
		if (chmod(RO_FILE, S_IRUSR) < 0)
			errExit("chown");
	}
	
	return RO_FILE;
}

static void disable_file(OPERATION op, const char *filename, const char *emptydir, const char *emptyfile) {
	assert(filename);
	assert(emptydir);
	assert(emptyfile);
	assert(op <OPERATION_MAX);
	
	// Resolve all symlinks
	char* fname = realpath(filename, NULL);
	if (fname == NULL) {
		if (arg_debug)
			printf("Warning: %s is an invalid file, skipping...\n", filename);
		return;
	}
	
	// if the file is not present, do nothing
	struct stat s;
	if (stat(fname, &s) == -1) {
		if (arg_debug)
			printf("Warning: %s does not exist, skipping...\n", fname);
		free(fname);
		return;
	}

	// modify the file
	if (op == BLACKLIST_FILE) {
		// some distros put all executables under /usr/bin and make /bin a symbolic link
		if ((strcmp(fname, "/bin") == 0 || strcmp(fname, "/usr/bin") == 0) &&
		      is_link(filename) &&
		      S_ISDIR(s.st_mode)) {
			exechelp_logerrv("firejail", FIREJAIL_WARNING, "Error: %s directory link was not blacklisted\n", filename);
		}
			
		else {
			if (arg_debug && strncmp (fname, EXECHELP_RUN_DIR, strlen (EXECHELP_RUN_DIR)))
				printf("Disable %s\n", fname);
			if (S_ISDIR(s.st_mode)) {
				if (mount(emptydir, fname, "none", MS_BIND, "mode=400,gid=0") < 0)
					errExit("disable file");
			}
			else {
				if (mount(emptyfile, fname, "none", MS_BIND, "mode=400,gid=0") < 0)
					errExit("disable file");
			}
		}
	}
	else if (op == MOUNT_READONLY) {
		if (arg_debug)
			printf("Mounting read-only %s\n", fname);
		fs_rdonly(fname);
	}
	else if (op == MOUNT_TMPFS) {
		if (S_ISDIR(s.st_mode)) {
			if (arg_debug)
				printf("Mounting tmpfs on %s\n", fname);
			// preserve owner and mode for the directory
			if (mount("tmpfs", fname, "tmpfs", MS_NOSUID | MS_NODEV | MS_STRICTATIME | MS_REC,  0) < 0)
				errExit("mounting tmpfs");
			/* coverity[toctou] */
			if (chown(fname, s.st_uid, s.st_gid) == -1)
				errExit("mounting tmpfs chmod");
		}
		else
			printf("Warning: %s is not a directory; cannot mount a tmpfs on top of it.\n", fname);
	}
	else
		assert(0);

	free(fname);
}

/* This is a cheap hack to match specific patterns, especially lock files that,
   like in Firefox, might be dangling and hidden in multiple folders. The alternative
   would be to copy the entire glob implementation of the glibc and remove their
   spurious dangling symlink check. This is likely less error-prone. */
static int linksmartglob(const char *pattern, int flags, int (*errfunc) (const char *epath, int eerrno), glob_t *pglob) {
  const char *laststar = strrchr(pattern, '*');
  const char *lastdir = strrchr(pattern, '/');

  // match the directory, then append the leftover on all entries
  if (laststar && lastdir && lastdir > laststar) {
    char *copy = strdup(pattern);
    char *clastdir = strrchr(copy, '/');
    *clastdir = '\0';
    clastdir++;

    int ret = glob(copy, flags, errfunc, pglob);
    if (!ret) {
      size_t i;
	    for (i = 0; pglob && i < pglob->gl_pathc; i++) {
		    char *entry = pglob->gl_pathv[i];

	      char *newentry;
	      if (asprintf(&newentry, "%s/%s", entry, clastdir) == -1)
	        errExit("asprintf");

        pglob->gl_pathv[i] = newentry;
        free(entry);
      }
    }

    free(copy);
    return ret;
  } else
    return glob(pattern, flags, errfunc, pglob);
}

// Treat pattern as a shell glob pattern and blacklist matching files
static void globbing(OPERATION op, const char *pattern, const char *noblacklist[], size_t noblacklist_len, const char *emptydir, const char *emptyfile) {
	assert(pattern);
	assert(emptydir);
	assert(emptyfile);

	glob_t globbuf;
	// Profiles contain blacklists for files that might not exist on a user's machine.
	// GLOB_NOCHECK makes that okay.
	int globerr = linksmartglob(pattern, GLOB_NOCHECK | GLOB_NOSORT, NULL, &globbuf);
	if (globerr) {
		exechelp_logerrv("firejail", FIREJAIL_ERROR, "Error: failed to glob pattern %s\n", pattern);
		exit(1);
	}
	
	size_t i, j;
	for (i = 0; i < globbuf.gl_pathc; i++) {
		char* path = globbuf.gl_pathv[i];
		assert(path);
		// noblacklist is expected to be short in normal cases, so stupid and correct brute force is okay
		bool okay_to_blacklist = true;
		for (j = 0; j < noblacklist_len; j++) {
			int result = fnmatch(noblacklist[j], path, FNM_PATHNAME);
			if (result == FNM_NOMATCH)
				continue;
			else if (result == 0) {
				okay_to_blacklist = false;
				break;
			}
			else {
				exechelp_logerrv("firejail", FIREJAIL_ERROR, "Error: failed to compare path %s with pattern %s\n", path, noblacklist[j]);
				exit(1);
			}
		}
		if (okay_to_blacklist)
			disable_file(op, path, emptydir, emptyfile);
	}
	globfree(&globbuf);
}

// blacklist files or directoies by mounting empty files on top of them
void fs_blacklist(const char *homedir) {
	ProfileEntry *entry = cfg.profile;
	if (!entry)
		return;
		
	char *emptydir = create_empty_dir();
	char *emptyfile = create_empty_file();

	// a statically allocated buffer works for all current needs
	// TODO: if dynamic allocation is ever needed, we should probably add
	// libraries that make it easy to do without introducing security bugs
	char *noblacklist[32];
	size_t noblacklist_c = 0;

	while (entry) {
		OPERATION op = OPERATION_MAX;
		char *ptr;

		// process blacklist command
		if (strncmp(entry->data, "bind ", 5) == 0)  {
			char *dname1 = entry->data + 5;
			char *dname2 = split_comma(dname1);
			if (dname2 == NULL) {
				exechelp_logerrv("firejail", FIREJAIL_ERROR, "Error: second directory missing in bind command\n");
				entry = entry->next;
				continue;
			}
			struct stat s;
			if (stat(dname1, &s) == -1) {
				exechelp_logerrv("firejail", FIREJAIL_ERROR, "Error: cannot find directories for bind command\n");
				entry = entry->next;
				continue;
			}
			if (stat(dname2, &s) == -1) {
				exechelp_logerrv("firejail", FIREJAIL_ERROR, "Error: cannot find directories for bind command\n");
				entry = entry->next;
				continue;
			}
			
			// mount --bind olddir newdir
			if (arg_debug)
				printf("Mount-bind %s on top of %s\n", dname1, dname2);
			// preserve dname2 mode and ownership
			if (mount(dname1, dname2, NULL, MS_BIND|MS_REC, NULL) < 0)
				errExit("mount bind");
			/* coverity[toctou] */
			if (chown(dname2, s.st_uid, s.st_gid) == -1)
				errExit("mount-bind chown");
			/* coverity[toctou] */
			if (chmod(dname2, s.st_mode) == -1)
				errExit("mount-bind chmod");
				
			entry = entry->next;
			continue;
		}

		// Process noblacklist command
		if (strncmp(entry->data, "noblacklist ", 12) == 0) {
			if (noblacklist_c >= sizeof(noblacklist) / sizeof(noblacklist[0])) {
				fputs("Error: out of memory for noblacklist entries\n", stderr);
				exit(1);
			}
			else
				noblacklist[noblacklist_c++] = expand_home(entry->data + 12, homedir);
			entry = entry->next;
			continue;
		}

		// process blacklist command
		if (strncmp(entry->data, "blacklist ", 10) == 0)  {
			ptr = entry->data + 10;
			op = BLACKLIST_FILE;
		}
		else if (strncmp(entry->data, "read-only ", 10) == 0) {
			ptr = entry->data + 10;
			op = MOUNT_READONLY;
		}			
		else if (strncmp(entry->data, "tmpfs ", 6) == 0) {
			ptr = entry->data + 6;
			op = MOUNT_TMPFS;
		}			
		else {
			exechelp_logerrv("firejail", FIREJAIL_ERROR, "Error: invalid profile line %s\n", entry->data);
			entry = entry->next;
			continue;
		}

		// replace home macro in blacklist array
		char *new_name = expand_home(ptr, homedir);
		ptr = new_name;

		// expand path macro - look for the file in /bin, /usr/bin, /sbin and  /usr/sbin directories
		// TODO: should we look for more bin paths?
		if (strncmp(ptr, "${PATH}", 7) == 0) {
			char *fname = ptr + 7;
			size_t fname_len = strlen(fname);
			char **path, *paths[] = {"/bin", "/sbin", "/usr/bin", "/usr/sbin", NULL};
			for (path = &paths[0]; *path; path++) {
				char newname[strlen(*path) + fname_len + 1];
				sprintf(newname, "%s%s", *path, fname);
				globbing(op, newname, (const char**)noblacklist, noblacklist_c, emptydir, emptyfile);
			}
		}
		else
			globbing(op, ptr, (const char**)noblacklist, noblacklist_c, emptydir, emptyfile);

		if (new_name)
			free(new_name);
		entry = entry->next;
	}

	size_t i;
	for (i = 0; i < noblacklist_c; i++) free(noblacklist[i]);
}

// blacklist a single file
void fs_blacklist_file(const char *file) {
	char *emptydir = create_empty_dir();
	char *emptyfile = create_empty_file();
	disable_file(BLACKLIST_FILE, file, emptydir, emptyfile);
}

//***********************************************
// mount namespace
//***********************************************

// remount a directory read-only
void fs_rdonly(const char *dir) {
	assert(dir);
	// check directory exists
	struct stat s;
	int rv = stat(dir, &s);
	if (rv == 0) {
		// mount --bind /bin /bin
		if (mount(dir, dir, NULL, MS_BIND|MS_REC, NULL) < 0)
			errExit("mount read-only");
		// mount --bind -o remount,ro /bin
		if (mount(NULL, dir, NULL, MS_BIND|MS_REMOUNT|MS_RDONLY|MS_REC, NULL) < 0)
			errExit("mount read-only");
	}
}
void fs_rdonly_noexit(const char *dir) {
	assert(dir);
	// check directory exists
	struct stat s;
	int rv = stat(dir, &s);
	if (rv == 0) {
		int merr = 0;
		// mount --bind /bin /bin
		if (mount(dir, dir, NULL, MS_BIND|MS_REC, NULL) < 0)
			merr = 1;
		// mount --bind -o remount,ro /bin
		if (mount(NULL, dir, NULL, MS_BIND|MS_REMOUNT|MS_RDONLY|MS_REC, NULL) < 0)
			merr = 1;
		if (merr)
			exechelp_logerrv("firejail", FIREJAIL_WARNING, "Error: cannot mount %s read-only\n", dir); 
	}
}

// mount /proc and /sys directories
void fs_proc_sys_dev_boot(void) {
	struct stat s;

	if (arg_debug)
		printf("Remounting /proc and /proc/sys filesystems\n");
	if (mount("proc", "/proc", "proc", MS_NOSUID | MS_NOEXEC | MS_NODEV | MS_REC, NULL) < 0)
		errExit("mounting /proc");

	// remount /proc/sys readonly
	if (mount("/proc/sys", "/proc/sys", NULL, MS_BIND | MS_REC, NULL) < 0)
		errExit("mounting /proc/sys");

	if (mount(NULL, "/proc/sys", NULL, MS_BIND | MS_REMOUNT | MS_RDONLY | MS_REC, NULL) < 0)
		errExit("mounting /proc/sys");


	/* Mount a version of /sys that describes the network namespace */
	if (arg_debug)
		printf("Remounting /sys directory\n");
	if (umount2("/sys", MNT_DETACH) < 0) {
		exechelp_logerrv("firejail", FIREJAIL_WARNING, "Error: failed to unmount /sys\n");
	}
	if (mount("sysfs", "/sys", "sysfs", MS_RDONLY|MS_NOSUID|MS_NOEXEC|MS_NODEV|MS_REC, NULL) < 0) {
		exechelp_logerrv("firejail", FIREJAIL_WARNING, "Error: failed to mount /sys\n");
	}

//	if (mount("sysfs", "/sys", "sysfs", MS_RDONLY|MS_NOSUID|MS_NOEXEC|MS_NODEV|MS_REC, NULL) < 0)
//		errExit("mounting /sys");

	// Disable SysRq
	// a linux box can be shut down easily using the following commands (as root):
	// # echo 1 > /proc/sys/kernel/sysrq
	// #echo b > /proc/sysrq-trigger
	// for more information see https://www.kernel.org/doc/Documentation/sysrq.txt
	if (arg_debug)
		printf("Disable /proc/sysrq-trigger\n");
	fs_rdonly_noexit("/proc/sysrq-trigger");
	
	// disable hotplug and uevent_helper
	if (arg_debug)
		printf("Disable /proc/sys/kernel/hotplug\n");
	fs_rdonly_noexit("/proc/sys/kernel/hotplug");
	if (arg_debug)
		printf("Disable /sys/kernel/uevent_helper\n");
	fs_rdonly_noexit("/sys/kernel/uevent_helper");
	
	// read-only /proc/irq and /proc/bus
	if (arg_debug)
		printf("Disable /proc/irq\n");
	fs_rdonly_noexit("/proc/irq");
	if (arg_debug)
		printf("Disable /proc/bus\n");
	fs_rdonly_noexit("/proc/bus");
	
	// disable /proc/kcore
	disable_file(BLACKLIST_FILE, "/proc/kcore", "not used", "/dev/null");

	// disable /proc/kallsyms
	disable_file(BLACKLIST_FILE, "/proc/kallsyms", "not used", "/dev/null");
	
	// disable /boot
	if (stat("/boot", &s) == 0) {
		if (arg_debug)
			printf("Mounting a new /boot directory\n");
		if (mount("tmpfs", "/boot", "tmpfs", MS_NOSUID | MS_NODEV | MS_STRICTATIME | MS_REC,  "mode=777,gid=0") < 0)
			errExit("mounting /boot directory");
	}
	
	// disable /dev/port
	if (stat("/dev/port", &s) == 0) {
		disable_file(BLACKLIST_FILE, "/dev/port", "not used", "/dev/null");
	}
}

static void sanitize_home(void) {
	// extract current /home directory data
	struct dirent *dir;
	DIR *d = opendir("/home");
	if (d == NULL)
		return;

	char *emptydir = create_empty_dir();
	while ((dir = readdir(d))) {
		if(strcmp(dir->d_name, "." ) == 0 || strcmp(dir->d_name, ".." ) == 0)
			continue;

		if (dir->d_type == DT_DIR ) {
			// get properties
			struct stat s;
			char *name;
			if (asprintf(&name, "/home/%s", dir->d_name) == -1)
				continue;
			if (stat(name, &s) == -1)
				continue;
			if (S_ISLNK(s.st_mode)) {
				free(name);
				continue;
			}
			
			if (strcmp(name, cfg.homedir) == 0)
				continue;

//			printf("directory %u %u:%u #%s#\n",
//				s.st_mode,
//				s.st_uid,
//				s.st_gid,
//				name);
			
			// disable directory
			disable_file(BLACKLIST_FILE, name, emptydir, "not used");
			free(name);
		}			
	}
	closedir(d);
}






// build a basic read-only filesystem
void fs_basic_fs(void) {
	if (arg_debug)
		printf("Mounting read-only /bin, /sbin, /lib, /lib64, /usr, /etc, /var\n");
	fs_rdonly("/bin");
	fs_rdonly("/sbin");
	fs_rdonly("/lib");
	fs_rdonly("/lib64");
	fs_rdonly("/usr");
	fs_rdonly("/etc");
	if (!cfg.var_rw)
    fs_rdonly("/var");

	// update /var directory in order to support multiple sandboxes running on the same root directory
	if (!arg_private_dev)
		fs_dev_shm();
	fs_var_lock();
	fs_var_tmp();
	fs_var_log();
	fs_var_lib();
	fs_var_cache();
	fs_var_utmp();

	// only in user mode
	if (getuid())
		sanitize_home();
}


// mount overlayfs on top of / directory
// mounting an overlay and chrooting into it:
//
// Old Ubuntu kernel
// # cd ~
// # mkdir -p overlay/root
// # mkdir -p overlay/diff
// # mount -t overlayfs -o lowerdir=/,upperdir=/root/overlay/diff overlayfs /root/overlay/root
// # chroot /root/overlay/root
// to shutdown, first exit the chroot and then  unmount the overlay
// # exit
// # umount /root/overlay/root
//
// Kernels 3.18+
// # cd ~
// # mkdir -p overlay/root
// # mkdir -p overlay/diff
// # mkdir -p overlay/work
// # mount -t overlay -o lowerdir=/,upperdir=/root/overlay/diff,workdir=/root/overlay/work overlay /root/overlay/root
// # cat /etc/mtab | grep overlay
// /root/overlay /root/overlay/root overlay rw,relatime,lowerdir=/,upperdir=/root/overlay/diff,workdir=/root/overlay/work 0 0
// # chroot /root/overlay/root
// to shutdown, first exit the chroot and then  unmount the overlay
// # exit
// # umount /root/overlay/root


static void
fs_overlayfs_dir(const char *oroot, const char *dest, const char *basedir, int oldkernel);

static void mount_automounts_into_overlay(const char *oroot, const char *basedir, int oldkernel) {
  if (arg_debug) {
    printf("Mounting host filesystem's mount points into OverlayFS layer...\n");
  }

  /* get the /proc/mounts pseudo-file and cache it somewhere */
  FILE *mounts = fopen(MOUNTS_FILE, "rb");
  if (!mounts) {
		exechelp_logerrv("firejail", FIREJAIL_ERROR, "Error: could not open the mounts file used for --overlay mode to produce a fully-working system, expecting further issues: %s (%s)\n", MOUNTS_FILE, strerror(errno));
    return;
  }

  char *cpath = MNT_DIR"/mounts";
  FILE *cached = fopen(cpath, "w+");
  if (!cached) {
		exechelp_logerrv("firejail", FIREJAIL_ERROR, "Error: could not create a temporary file to process mount units, needed for --overlay mode to produce a fully-working system, expecting further issues: %s (%s)\n", cpath, strerror(errno));
    return;
  }

  char buffer[4096];
  size_t bytes;
  errno = 0;
  while (!errno && 0 < (bytes = fread(buffer, 1, sizeof(buffer), mounts)))
    fwrite(buffer, 1, bytes, cached);
  if (errno)
    errExit("fread/fwrite");

  fclose(mounts);
  rewind(cached);

  /* start processing */
  char *format;
	if(asprintf(&format, "%%%ds %%%ds %%%ds %%%ds %%d %%d\n", PATH_MAX-1, PATH_MAX-1, PATH_MAX-1, PATH_MAX-1) == -1)
		errExit("asprintf");
  int matched;
  char origin[PATH_MAX], dest[PATH_MAX], type[PATH_MAX], opt[PATH_MAX];
  uid_t uid;
  gid_t gid;

  do {
    matched = fscanf(cached, format, &origin, &dest, &type, &opt, &uid, &gid);
    if (matched == 6) {
      /* some rules just ought to be ignored */
      if (strcmp(dest, "/") == 0 ||
          strncmp(dest, "/dev", 4) == 0 ||
          strncmp(dest, "/sys", 4) == 0 ||
          strncmp(dest, "/proc", 5) == 0 ||
          strncmp(dest, "/boot", 5) == 0 ||
          strncmp(dest, "/tmp/firejail", 13) == 0)
        continue;

      /* home is only allowed if we don't create a special home directory */
      if (strncmp(dest, "/home", 5) == 0 && arg_overlay_home == 0)
        continue;

      /* we might need to mount the original /var for some apps to work (e.g. Dropbox) */
      if (strncmp(dest, "/var", 4) == 0 && cfg.var_rw)
        continue;

      /* typical data-containing mount points, make them writable in the overlayfs, by overlayfs'ing them too */
      else if (strncmp(dest, "/home", 5) == 0 ||
          strncmp(dest, "/mnt", 4) == 0 ||
          strncmp(dest, "/media", 6) == 0 ||
          uid == getuid()) {
        /* debug */
	      if (arg_debug) {
	        char *tmp;
	        if (asprintf(&tmp, "Mounting %s of type %s as an OverlayFS layer into OverlayFS\n", dest, type) == -1)
		        errExit("asprintf");
		      printf("%s", tmp);
		      free(tmp);
	      }

        char *neworoot;
        if (asprintf(&neworoot, "%s%s", oroot, dest) == -1)
		        errExit("asprintf");
        fs_overlayfs_dir(neworoot, dest, basedir, oldkernel);
        free(neworoot);
      }

	    /* mount-bind the directory, read-only */
      else {
        /* debug */
	      if (arg_debug) {
	        char *tmp;
	        if (asprintf(&tmp, "Mounting %s of type %s into OverlayFS\n", dest, type) == -1)
		        errExit("asprintf");
		      printf("%s", tmp);
		      free(tmp);
	      }

        unsigned long mountflags = 0;
        char *mountpt;
        if (asprintf(&mountpt, "%s%s", oroot, dest) == -1)
	        errExit("asprintf");

        if (mnt_optstr_get_flags(opt, &mountflags, mnt_get_builtin_optmap(MNT_LINUX_MAP))) {
	        exechelp_logerrv("firejail", FIREJAIL_ERROR, "Error: mnt_optstr_get_flags: failed to process mount flags for %s\n", dest);
	        continue;
        }

        if (mount(dest, mountpt, type, mountflags|MS_BIND|MS_REC, NULL) < 0) {
          char *errmsg;
          if (asprintf(&errmsg, "mounting %s", mountpt) == -1)
	          errExit("asprintf");
          errExit(errmsg);
        }

        free(mountpt);
      }
    }
  } while (matched > 0);

  free(format);

  /* close and destroy the tmp file used for scanning */
  fclose(cached);
  unlink(cpath);
}

static void
build_private_home_into_overlay(const char *oroot, const char *basedir) {
  char *odiff;
  if(asprintf(&odiff, "%s/Users", basedir) == -1)
	  errExit("asprintf");

	if (mkdir_if_not_exists(odiff, S_IRWXU | S_IRWXG | S_IRWXO)) {
		exechelp_logerrv("firejail", FIREJAIL_ERROR, "Error: cannot create persistent diff directory in user home: %s\n", odiff);
		exit(1);
	}
	if (chown(odiff, 0, 0) < 0)
		errExit("chown");
	if (chmod(odiff, S_IRWXU  | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH) < 0)
		errExit("chmod");

  struct passwd pwd;
  struct passwd *result = NULL;
  char buffer[8192];
  if (getpwuid_r(getuid(), &pwd, buffer, sizeof(buffer), &result))
    errExit("getpwuid_r");
  if (!result || !pwd.pw_name)
    errExit("getpwuid_r");

  char *userdir;
  if(asprintf(&userdir, "%s/%s", odiff, pwd.pw_name) == -1)
	  errExit("asprintf");

	if (mkdir_if_not_exists(userdir, S_IRWXU | S_IRWXG | S_IRWXO)) {
		exechelp_logerrv("firejail", FIREJAIL_ERROR, "Error: cannot create user home inside overlay: %s\n", userdir);
		exit(1);
	}
	if (chown(userdir, getuid(), getgid()) < 0)
		errExit("chown");
	if (chmod(userdir, S_IRWXU  | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH) < 0)
		errExit("chmod");


  char *mountpt;
  if(asprintf(&mountpt, "%s/home", oroot) == -1)
	  errExit("asprintf");
  if (mount(odiff, mountpt, "ext4", MS_RELATIME|MS_BIND|MS_REC, NULL) < 0) {
    char *errmsg;
    if (asprintf(&errmsg, "mounting %s", mountpt) == -1)
      errExit("asprintf");
    errExit(errmsg);
  }

  free(mountpt);
  free(odiff);
  free(userdir);
}

void mount_direct_accesses_into_overlay(const char *oroot) {
  if (!arg_overlay_direct_access)
    return;

  size_t i;
  char *realpath;
  char *newpath;
  char *ptr;
  char *oldpath;
  int failure;
  struct stat st;
  
  for (i = 0; arg_overlay_direct_access[i]; i++) {
    realpath = exechelp_coreutils_realpath(arg_overlay_direct_access[i]);
    if (!realpath) {
		  exechelp_logerrv("firejail", FIREJAIL_WARNING, "Error: cannot find path for argument %s passed to --overlay-sync, ignoring: %s\n", arg_overlay_direct_access[i], strerror(errno));
      continue;
    }

		if (asprintf(&newpath, "%s%s/", oroot, realpath) == -1)
			errExit("asprintf");

    if (arg_debug)
      printf("Mounting %s into OverlayFS as a synchronised directory\n", realpath);

    // go through the original path and create directories one by one with appropriate owners and permissions
    oldpath = newpath + strlen(oroot);
    failure = 0;

    for (ptr = strchr(newpath+strlen(oroot)+1, '/'); !failure && ptr; ptr = strchr(ptr+1, '/')) {
      *ptr = '\0';
      failure = mkdir_if_not_exists(newpath, S_IRWXU | S_IRWXG | S_IRWXO);
      if (!failure) {
        failure = stat(oldpath, &st);
        if (!failure) {
          failure = chown(newpath, st.st_uid, st.st_gid);
          if (!failure) {
            failure = chmod(newpath, st.st_mode & 07777);
          }
        }
      }

      if (arg_debug) {
        if (!failure)
          printf ("Path %s mounted with permissions %o and owner %d:%d\n", newpath, st.st_mode & 07777, st.st_uid, st.st_gid);
        else
          exechelp_logerrv("firejail", FIREJAIL_ERROR, "Error: failed to create and configure partial path %s for synchronised folder %s: %s\n", newpath, arg_overlay_direct_access[i], strerror(errno));
      }

      *ptr = '/';
    }

    if (failure) {
      exechelp_logerrv("firejail", FIREJAIL_WARNING, "Error: cannot mount path %s into sandbox as synchronised directory: %s\n", realpath, strerror(errno));
      continue;
    }
  
	  if (mount(realpath, newpath, NULL, MS_BIND|MS_REC, NULL) < 0)
		  errExit("mounting direct sync folder");

    free(realpath);
    free(newpath);
  }
}

// to do: fix the code below; also, it might work without /dev; impose seccomp/caps filters when not root
#include <sys/utsname.h>
static void
fs_overlayfs_dir(const char *oroot, const char *dest, const char *basedir, int oldkernel) {
  if (!dest || !oroot)
    errExit("fs_overlayfs_dir");

  /* first ensure that oroot exists */
	if (mkdir_if_not_exists(oroot, S_IRWXU | S_IRWXG | S_IRWXO)) {
		exechelp_logerrv("firejail", FIREJAIL_ERROR, "Error: cannot create overlay directory in temporary mountpoint: %s\n", oroot);
		exit(1);
	}
	if (chown(oroot, 0, 0) < 0)
		errExit("chown");
	if (chmod(oroot, S_IRWXU  | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH) < 0)
		errExit("chmod");

	char *baseworkdir;
  if(asprintf(&baseworkdir, "%s/.temp", basedir) == -1)
	  errExit("asprintf");
	if (mkdir_if_not_exists(baseworkdir, S_IRWXU | S_IRWXG | S_IRWXO) != 0) {
		exechelp_logerrv("firejail", FIREJAIL_ERROR, "Error: cannot create overlay work directory in user home: %s\n", baseworkdir);
		exit(1);
	}
	free(baseworkdir);

  /* choose a name for odiff's persistent folder, and the owork temp folder */
	char *odiff;
	char *owork;
  /* use the special "System" folder name for the system FS */
  if (strcmp("/", dest) == 0) {
	  if(asprintf(&odiff, "%s/System", basedir) == -1)
		  errExit("asprintf");
	  if(asprintf(&owork, "%s/.temp/System", basedir) == -1)
		  errExit("asprintf");
  }
  /* use the Home folder for users' homes */
  else if (strcmp("/home", dest) == 0 || strcmp("/home/", dest) == 0) {
	  if(asprintf(&odiff, "%s/Users", basedir) == -1)
		  errExit("asprintf");
	  if(asprintf(&owork, "%s/.temp/Users", basedir) == -1)
		  errExit("asprintf");
  }
  /* construct a name from the mountpoint */
  else {
    char *tmp = strdup(dest);
    char *ptr;
    for (ptr = strchr(tmp, '/'); ptr; ptr = strchr(ptr, '/'))
      ptr[0]='-';
    ptr = (tmp[0] == '-')? tmp+1 : tmp;

	  if(asprintf(&odiff, "%s/%s", basedir, tmp) == -1)
		  errExit("asprintf");
	  if(asprintf(&owork, "%s/.temp/%s", basedir, tmp) == -1)
		  errExit("asprintf");

    free(tmp);
  }

	if (mkdir_if_not_exists(odiff, S_IRWXU | S_IRWXG | S_IRWXO)) {
		exechelp_logerrv("firejail", FIREJAIL_ERROR, "Error: cannot create overlay persistent diff directory in user home: %s\n", odiff);
		exit(1);
	}
	if (chown(odiff, 0, 0) < 0)
		errExit("chown");
	if (chmod(odiff, S_IRWXU  | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH) < 0)
		errExit("chmod");

	if (mkdir_if_not_exists(owork, S_IRWXU | S_IRWXG | S_IRWXO)) {
		exechelp_logerrv("firejail", FIREJAIL_ERROR, "Error: cannot create overlay work directory in user home: %s\n", owork);
		exit(1);
	}
	if (chown(owork, 0, 0) < 0)
		errExit("chown");
	if (chmod(owork, S_IRWXU  | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH) < 0)
		errExit("chmod");
	
	// mount overlayfs
	if (arg_debug)
		printf("Mounting OverlayFS\n");

	char *option;
	if (oldkernel) {
		if (asprintf(&option, "lowerdir=%s,upperdir=%s", dest, odiff) == -1)
			errExit("asprintf");
		if (mount("overlayfs", oroot, "overlayfs", MS_MGC_VAL, option) < 0)
			errExit("mounting overlayfs");
	} else { // kernel 3.18 or newer
		if (asprintf(&option, "lowerdir=%s,upperdir=%s,workdir=%s", dest, odiff, owork) == -1)
			errExit("asprintf");

  if (arg_debug)
  {
		printf("The root  directory is %s\n", oroot);
		printf("The lower directory is %s\n", dest);
		printf("The upper directory is %s\n", odiff);
		printf("The work  directory is %s\n", owork);
  }

		if (mount("overlay", oroot, "overlay", MS_MGC_VAL, option) < 0)
			errExit("mounting overlayfs");
	}

	printf("OverlayFS configured in %s directory\n", odiff);

	free(option);
	free(odiff);
	free(owork);
}

void fs_overlayfs(void) {
	// check kernel version
	struct utsname u;
	int rv = uname(&u);
	if (rv != 0)
		errExit("uname");
	int major;
	int minor;
	if (2 != sscanf(u.release, "%d.%d", &major, &minor)) {
		exechelp_logerrv("firejail", FIREJAIL_ERROR, "Error: cannot extract Linux kernel version: %s\n", u.version);
		exit(1);
	}
	
	if (arg_debug)
		printf("Linux kernel version %d.%d\n", major, minor);
	int oldkernel = 0;
	if (major < 3) {
		exechelp_logerrv("firejail", FIREJAIL_ERROR, "Error: minimum kernel version required 3.x\n");
		exit(1);
	}
	if (major == 3 && minor < 18)
		oldkernel = 1;

 // old Ubuntu/OpenSUSE kernels
	if (oldkernel && arg_overlay_keep) {
		exechelp_logerrv("firejail", FIREJAIL_ERROR, "Error: option --overlay= not available for kernels older than 3.18\n");
		exit(1);
	}

	// build overlay directories
	fs_build_mnt_dir();
	
  /* then determine where we'll keep the odiff and owork folders */
	char *basedir = MNT_DIR;
	if (arg_overlay_keep) {
		// set base for working and diff directories
		basedir = cfg.overlay_dir;
		if (mkdir_if_not_exists(basedir, S_IRWXU | S_IRWXG | S_IRWXO) != 0) {
			exechelp_logerrv("firejail", FIREJAIL_ERROR, "Error: cannot create overlay directory in user home\n");
			exit(1);
		}
	} else {
	  if (asprintf(&basedir, "%s/overlayfs/%d", MNT_DIR, sandbox_pid) == -1)
	    errExit("asprintf");
		if (mkdir_if_not_exists_recursive(basedir, S_IRWXU | S_IRWXG | S_IRWXO) != 0) {
			exechelp_logerrv("firejail", FIREJAIL_ERROR, "Error: cannot create overlay directory in user home\n");
			exit(1);
		}
    if (clear_recursive(basedir))
      errExit("clear_recursive");
	}

  // build a root and a first directory for "/"
	char *oroot;
	if(asprintf(&oroot, "%s/oroot", MNT_DIR) == -1)
		errExit("asprintf");
  fs_overlayfs_dir(oroot, "/", basedir, oldkernel);

  // mount other mount points as overlayfs filesystems
	mount_automounts_into_overlay(oroot, basedir, oldkernel);

  // but if home ought to be private, then use an empty dir instead
  if (arg_overlay_home == 0)
    build_private_home_into_overlay(oroot, basedir);
  free(basedir);

  // if /var ought to be read-write, add it to the list of sync'd directories
  if (cfg.var_rw)
    string_list_append(&arg_overlay_direct_access, "/var");

  // mount directories directly sync'd with the system
  mount_direct_accesses_into_overlay(oroot);
	
	// mount-bind dev directory
  //FIXME might want to mount *some* /dev, esp /dev/shm
	if (arg_debug)
		printf("Mounting /dev\n");
	char *dev;
	if (asprintf(&dev, "%s/dev", oroot) == -1)
		errExit("asprintf");
	if (mount("/dev", dev, NULL, MS_BIND|MS_REC, NULL) < 0)
		errExit("mounting /dev");

	// chroot in the new filesystem
	if (chroot(oroot) == -1)
		errExit("chroot");
	free(oroot);

	// update /var directory in order to support multiple sandboxes running on the same root directory
	if (!arg_private_dev)
		fs_dev_shm();
	if (!cfg.var_rw)
  	fs_var_lock();
	if (!cfg.var_rw)
  	fs_var_tmp();
	if (!cfg.var_rw)
  	fs_var_log();
	if (!cfg.var_rw)
  	fs_var_lib();
	if (!cfg.var_rw)
  	fs_var_cache();
	if (!cfg.var_rw)
	  fs_var_utmp();

  // don't let the inside of the box reach the Sandboxes folder
  struct passwd pwd;
  struct passwd *result = NULL;
  char buffer[8192];
  if (getpwuid_r(getuid(), &pwd, buffer, sizeof(buffer), &result))
    errExit("getpwuid_r");
  if (!result || !pwd.pw_dir)
    errExit("getpwuid_r");

  char *sdbdir;
  if(asprintf(&sdbdir, "%s/Sandboxes", pwd.pw_dir) == -1)
	  errExit("asprintf");
  fs_blacklist_file(sdbdir);
  free(sdbdir);

  //FIXME handle file sync, when you want to retrieve the global file
  //FIXME handle file version visibility

	// only in user mode
	if (getuid())
		sanitize_home();
}



#ifdef HAVE_CHROOT		
// return 1 if error
int fs_check_chroot_dir(const char *rootdir) {
	assert(rootdir);
	struct stat s;
	char *name;

	// check /dev
	if (asprintf(&name, "%s/dev", rootdir) == -1)
		errExit("asprintf");
	if (stat(name, &s) == -1) {
		exechelp_logerrv("firejail", FIREJAIL_ERROR, "Error: cannot find /dev in chroot directory\n");
		return 1;
	}
	free(name);

	// check /var/tmp
	if (asprintf(&name, "%s/var/tmp", rootdir) == -1)
		errExit("asprintf");
	if (stat(name, &s) == -1) {
		exechelp_logerrv("firejail", FIREJAIL_ERROR, "Error: cannot find /var/tmp in chroot directory\n");
		return 1;
	}
	free(name);
	
	// check /proc
	if (asprintf(&name, "%s/proc", rootdir) == -1)
		errExit("asprintf");
	if (stat(name, &s) == -1) {
		exechelp_logerrv("firejail", FIREJAIL_ERROR, "Error: cannot find /proc in chroot directory\n");
		return 1;
	}
	free(name);
	
	// check /proc
	if (asprintf(&name, "%s/tmp", rootdir) == -1)
		errExit("asprintf");
	if (stat(name, &s) == -1) {
		exechelp_logerrv("firejail", FIREJAIL_ERROR, "Error: cannot find /tmp in chroot directory\n");
		return 1;
	}
	free(name);
	
	// check /bin/bash
	if (asprintf(&name, "%s/bin/bash", rootdir) == -1)
		errExit("asprintf");
	if (stat(name, &s) == -1) {
		exechelp_logerrv("firejail", FIREJAIL_ERROR, "Error: cannot find /bin/bash in chroot directory\n");
		return 1;
	}
	free(name);

	return 0;	
}

// chroot into an existing directory; mount exiting /dev and update /etc/resolv.conf
void fs_chroot(const char *rootdir) {
	assert(rootdir);
	
	//***********************************
	// mount-bind a /dev in rootdir
	//***********************************
	// mount /dev
	char *newdev;
	if (asprintf(&newdev, "%s/dev", rootdir) == -1)
		errExit("asprintf");
	if (arg_debug)
		printf("Mounting /dev on %s\n", newdev);
	if (mount("/dev", newdev, NULL, MS_BIND|MS_REC, NULL) < 0)
		errExit("mounting /dev");
	
	// some older distros don't have a /run directory
	// create one by default
	// no exit on error, let the user deal with any problems
	char *rundir;
	if (asprintf(&rundir, "%s/run", rootdir) == -1)
		errExit("asprintf");
	if (!is_dir(rundir)) {
		int rv = mkdir(rundir, S_IRWXU | S_IRWXG | S_IRWXO);
		(void) rv;
		rv = chown(rundir, 0, 0);
		(void) rv;
	}
	
	// copy /etc/resolv.conf in chroot directory
	// if resolv.conf in chroot is a symbolic link, this will fail
	// no exit on error, let the user deal with the problem
	char *fname;
	if (asprintf(&fname, "%s/etc/resolv.conf", rootdir) == -1)
		errExit("asprintf");
	if (arg_debug)
		printf("Updating /etc/resolv.conf in %s\n", fname);
	if (copy_file("/etc/resolv.conf", fname) == -1) {
		exechelp_logerrv("firejail", FIREJAIL_WARNING, "Error: /etc/resolv.conf not initialized\n");
	}
		
	// chroot into the new directory
	if (arg_debug)
		printf("Chrooting into %s\n", rootdir);
	if (chroot(rootdir) < 0)
		errExit("chroot");
		
	// update /var directory in order to support multiple sandboxes running on the same root directory
	if (!arg_private_dev)
		fs_dev_shm();
	fs_var_lock();
	fs_var_tmp();
	fs_var_log();
	fs_var_lib();
	fs_var_cache();
	fs_var_utmp();

	// only in user mode
	if (getuid())
		sanitize_home();
	
}
#endif


