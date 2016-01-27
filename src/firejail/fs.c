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
#include <sys/mount.h>
#include <sys/stat.h>
#include <linux/limits.h>
#include <fnmatch.h>
#include <glob.h>
#include <dirent.h>
#include <fcntl.h>
#include <errno.h>

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
			exechelp_logerrv("firejail", "Error: non-root %s directory, exiting...\n", FIREJAIL_DIR);
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
			exechelp_logerrv("firejail", "Warning: %s directory link was not blacklisted\n", filename);
		}
			
		else {
			if (arg_debug)
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
		exechelp_logerrv("firejail", "Error: failed to glob pattern %s\n", pattern);
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
				exechelp_logerrv("firejail", "Error: failed to compare path %s with pattern %s\n", path, noblacklist[j]);
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
				exechelp_logerrv("firejail", "Error: second directory missing in bind command\n");
				entry = entry->next;
				continue;
			}
			struct stat s;
			if (stat(dname1, &s) == -1) {
				exechelp_logerrv("firejail", "Error: cannot find directories for bind command\n");
				entry = entry->next;
				continue;
			}
			if (stat(dname2, &s) == -1) {
				exechelp_logerrv("firejail", "Error: cannot find directories for bind command\n");
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
			exechelp_logerrv("firejail", "Error: invalid profile line %s\n", entry->data);
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
			exechelp_logerrv("firejail", "Warning: cannot mount %s read-only\n", dir); 
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
		exechelp_logerrv("firejail", "Warning: failed to unmount /sys\n");
	}
	if (mount("sysfs", "/sys", "sysfs", MS_RDONLY|MS_NOSUID|MS_NOEXEC|MS_NODEV|MS_REC, NULL) < 0) {
		exechelp_logerrv("firejail", "Warning: failed to mount /sys\n");
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


// to do: fix the code below; also, it might work without /dev; impose seccomp/caps filters when not root
#include <sys/utsname.h>
void fs_overlayfs(void) {
	// check kernel version
	struct utsname u;
	int rv = uname(&u);
	if (rv != 0)
		errExit("uname");
	int major;
	int minor;
	if (2 != sscanf(u.release, "%d.%d", &major, &minor)) {
		exechelp_logerrv("firejail", "Error: cannot extract Linux kernel version: %s\n", u.version);
		exit(1);
	}
	
	if (arg_debug)
		printf("Linux kernel version %d.%d\n", major, minor);
	int oldkernel = 0;
	if (major < 3) {
		exechelp_logerrv("firejail", "Error: minimum kernel version required 3.x\n");
		exit(1);
	}
	if (major == 3 && minor < 18)
		oldkernel = 1;
	
	// build overlay directories
	fs_build_mnt_dir();

	char *oroot;
	if(asprintf(&oroot, "%s/oroot", MNT_DIR) == -1)
		errExit("asprintf");
	if (mkdir(oroot, S_IRWXU | S_IRWXG | S_IRWXO))
		errExit("mkdir");
	if (chown(oroot, 0, 0) < 0)
		errExit("chown");
	if (chmod(oroot, S_IRWXU  | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH) < 0)
		errExit("chmod");

	char *basedir = MNT_DIR;
	if (arg_overlay_keep) {
		// set base for working and diff directories
		basedir = cfg.overlay_dir;
		if (mkdir(basedir, S_IRWXU | S_IRWXG | S_IRWXO) != 0) {
			exechelp_logerrv("firejail", "Error: cannot create overlay directory\n");
			exit(1);
		}
	}

	char *odiff;
	if(asprintf(&odiff, "%s/odiff", basedir) == -1)
		errExit("asprintf");
	if (mkdir(odiff, S_IRWXU | S_IRWXG | S_IRWXO))
		errExit("mkdir");
	if (chown(odiff, 0, 0) < 0)
		errExit("chown");
	if (chmod(odiff, S_IRWXU  | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH) < 0)
		errExit("chmod");
	
	char *owork;
	if(asprintf(&owork, "%s/owork", basedir) == -1)
		errExit("asprintf");
	if (mkdir(owork, S_IRWXU | S_IRWXG | S_IRWXO))
		errExit("mkdir");
	if (chown(owork, 0, 0) < 0)
		errExit("chown");
	if (chmod(owork, S_IRWXU  | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH) < 0)
		errExit("chmod");
	
	// mount overlayfs
	if (arg_debug)
		printf("Mounting OverlayFS\n");
	char *option;
	if (oldkernel) { // old Ubuntu/OpenSUSE kernels
		if (arg_overlay_keep) {
			exechelp_logerrv("firejail", "Error: option --overlay= not available for kernels older than 3.18\n");
			exit(1);
		}
		if (asprintf(&option, "lowerdir=/,upperdir=%s", odiff) == -1)
			errExit("asprintf");
		if (mount("overlayfs", oroot, "overlayfs", MS_MGC_VAL, option) < 0)
			errExit("mounting overlayfs");
	}
	else { // kernel 3.18 or newer
		if (asprintf(&option, "lowerdir=/,upperdir=%s,workdir=%s", odiff, owork) == -1)
			errExit("asprintf");
//printf("option #%s#\n", option);			
		if (mount("overlay", oroot, "overlay", MS_MGC_VAL, option) < 0)
			errExit("mounting overlayfs");
	}
	printf("OverlayFS configured in %s directory\n", basedir);
	
	// mount-bind dev directory
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

	// cleanup and exit
	free(option);
	free(oroot);
	free(odiff);
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
		exechelp_logerrv("firejail", "Error: cannot find /dev in chroot directory\n");
		return 1;
	}
	free(name);

	// check /var/tmp
	if (asprintf(&name, "%s/var/tmp", rootdir) == -1)
		errExit("asprintf");
	if (stat(name, &s) == -1) {
		exechelp_logerrv("firejail", "Error: cannot find /var/tmp in chroot directory\n");
		return 1;
	}
	free(name);
	
	// check /proc
	if (asprintf(&name, "%s/proc", rootdir) == -1)
		errExit("asprintf");
	if (stat(name, &s) == -1) {
		exechelp_logerrv("firejail", "Error: cannot find /proc in chroot directory\n");
		return 1;
	}
	free(name);
	
	// check /proc
	if (asprintf(&name, "%s/tmp", rootdir) == -1)
		errExit("asprintf");
	if (stat(name, &s) == -1) {
		exechelp_logerrv("firejail", "Error: cannot find /tmp in chroot directory\n");
		return 1;
	}
	free(name);
	
	// check /bin/bash
	if (asprintf(&name, "%s/bin/bash", rootdir) == -1)
		errExit("asprintf");
	if (stat(name, &s) == -1) {
		exechelp_logerrv("firejail", "Error: cannot find /bin/bash in chroot directory\n");
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
		exechelp_logerrv("firejail", "Warning: /etc/resolv.conf not initialized\n");
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


