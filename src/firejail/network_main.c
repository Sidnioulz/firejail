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
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <net/if.h>
#include <unistd.h>

#define _GNU_SOURCE     /* To get defns of NI_MAXSERV and NI_MAXHOST */
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>
#include <ifaddrs.h>
#include <stdio.h>
#include <stdlib.h>
#include <linux/if_link.h>

void net_auto_bridge(void) {

	//************************
	// build command
	//************************
  char *cmd;
  if (asprintf(&cmd, "%s/lib/firejail/fnetnsadd.sh %d", PREFIX, sandbox_pid) == -1)
		errExit("asprintf");

  // backups to restore process after script execution
  char **envtmp = environ;
  uid_t euid = geteuid();
  uid_t ruid = getuid();

	// wipe out environment variables
	environ = NULL;

	// elevate privileges
	if (setreuid(0, 0))
		errExit("setreuid");

  // execute command
  int retst = system(cmd);
  if (WEXITSTATUS(retst) != 0)
    errExit("system");

  // reset privileges
	if (setreuid(ruid, euid))
		errExit("setreuid");

  // reset environment variables
  environ = envtmp;
}

void net_nat_bridge(Bridge *br) {
	assert(br);
  if (br->configured)
    return;

	br->macvlan = 0;
	br->nat = 1;

	char *dev;
	if (asprintf(&dev, "firejail-%u", sandbox_pid) < 0)
		errExit("asprintf");
	br->dev = dev;

	char *newname;
	if (asprintf(&newname, "%s-%u", br->devsandbox, getpid()) == -1)
		errExit("asprintf");
	br->devsandbox = newname;

	// create a veth pair
  net_create_veth(dev, newname, sandbox_pid);
	
	// set a range of IPs and a mask to use if none were given
	if (!br->iprange_start || !br->iprange_end) {
	  if (atoip("10.1.1.0", &br->iprange_start)) {
			exechelp_logerrv("firejail", "Error:  internal atoip() error\n");
			exit(1);
		}
	  if (atoip("10.253.253.253", &br->iprange_end)) {
			exechelp_logerrv("firejail", "Error:  internal atoip() error\n");
			exit(1);
		}
	}
  if (!br->mask) {
    if (atoip("255.255.255.254", &br->mask)) {
		  exechelp_logerrv("firejail", "Error:  internal atoip() error\n");
		  exit(1);
	  }
  }
	// this software is not supported for /31 networks
  else if ((~br->mask + 1) < 4) {
		exechelp_logerrv("firejail", "Error: the software is not supported for /31 networks\n");
		exit(1);
	}

  // get an IP for the internal veth interface
  if (!br->ipsandbox || !br->ip) {
    uint32_t first = 0, second = 0;
    while (!first || !second) {
      net_get_next_ip(br, &first);
      br->iprange_start = first + 1;
      net_get_next_ip(br, &second);

      // we exhausted all the options
      if (!first || !second) {
			  exechelp_logerrv("firejail", "Error: cannot find two available IP addresses in the same range (/%d), cannot instantiate the network namespace\n", mask2bits(br->mask));
			  exit(1);
      }

		  // our IPs are not in the same range, try again
		  char *rv = in_netrange(first, second, br->mask);
		  if (rv) {
		    first = 0;
		    second = 0;
		  } else {
		    br->ipsandbox = first;
		    br->ip = second;
		  }
    }
    if (arg_debug)
      printf("IP addresses %d.%d.%d.%d/%d and  %d.%d.%d.%d/%d are available for child process's network sandbox\n", PRINT_IP(br->ipsandbox), mask2bits(br->mask), PRINT_IP(br->ip), mask2bits(br->mask));
  }

	br->configured = 1;
}

void net_nat_parent_finalize(Bridge *br, pid_t child) {
	assert(br);
	assert(child);
  if (br->configured == 0)
    return;

  // move one veth iface into the namespace
  char *cmd;
  if (asprintf(&cmd, "ip link set %s netns %d", br->devsandbox, child) == -1)
		errExit("asprintf");
  if(system(cmd))
    errExit("net_nat_parent_finalise: move veth into namespace");
  free(cmd);

  // assign an IP to the other one and up it
  net_if_up(br->dev);
  net_if_ip(br->dev, br->ip, br->mask);
  
  // tell the outer system to treat the veth iface as a NAT client and reroute its traffic to the main gateway
  if (asprintf(&cmd, "iptables -t nat -A POSTROUTING -s %d.%d.%d.%d/%d -d 0.0.0.0/0 -j MASQUERADE", PRINT_IP(br->ip), mask2bits(br->mask)) == -1)
		errExit("asprintf");
  if(system(cmd))
    errExit("net_nat_finalize: route namespace traffic via NAT");
  free(cmd);

  // tell the outer system to treat the veth iface as a NAT client and reroute its traffic to the main gateway
  if (asprintf(&cmd, "%s", "sysctl net.ipv4.ip_forward=1") == -1)
		errExit("asprintf");
  if(system(cmd))
    errExit("net_nat_finalize: enable IPv4 forwarding");
  free(cmd);
  
	if (arg_debug)
		printf("NAT device %s in host system configured\n", br->dev);
}

void net_nat_finalize(Bridge *br, pid_t child) {
	assert(br);
	assert(child);
  if (br->configured == 0)
    return;

  net_if_up(br->devsandbox);
  net_if_ip(br->devsandbox, br->ipsandbox, br->mask);

  char *cmd;
  // add a route inside the sandbox towards the outside veth interface
  if (asprintf(&cmd, "ip route add default via %d.%d.%d.%d", PRINT_IP(br->ip)) == -1)
		errExit("asprintf");
  if(system(cmd))
    errExit("net_nat_finalize: add route inside namespace");
  free(cmd);

	if (arg_debug)
		printf("NAT device %s in child process's network namespace configured\n", br->devsandbox);
}

// configure bridge structure
// - extract ip address and mask from the bridge interface
void net_configure_bridge(Bridge *br, char *dev_name) {
	assert(br);
	assert(dev_name);

	br->dev = dev_name;

	// check the bridge device exists
	char sysbridge[30 + strlen(br->dev)];
	sprintf(sysbridge, "/sys/class/net/%s/bridge", br->dev);
	struct stat s;
	int rv = stat(sysbridge, &s);
	if (rv == 0) {
		// this is a bridge device
		br->macvlan = 0;
	}
	else {
		// is this a regular Ethernet interface
		if (if_nametoindex(br->dev) > 0) {
			br->macvlan = 1;
			char *newname;
			if (asprintf(&newname, "%s-%u", br->devsandbox, getpid()) == -1)
				errExit("asprintf");
			br->devsandbox = newname;
		}			
		else {
			exechelp_logerrv("firejail", "Error: cannot find network device %s\n", br->dev);
			exit(1);
		}
	}

	if (net_get_if_addr(br->dev, &br->ip, &br->mask, br->mac)) {
		exechelp_logerrv("firejail", "Error: interface %s is not configured\n", br->dev);
		exit(1);
	}
	if (arg_debug) {
		if (br->macvlan == 0)
			printf("Bridge device %s at %d.%d.%d.%d/%d\n",
				br->dev, PRINT_IP(br->ip), mask2bits(br->mask));
		else
			printf("macvlan parent device %s at %d.%d.%d.%d/%d\n",
				br->dev, PRINT_IP(br->ip), mask2bits(br->mask));
	}
	
	uint32_t range = ~br->mask + 1;		  // the number of potential addresses
	// this software is not supported for /31 networks
	if (range < 4) {
		exechelp_logerrv("firejail", "Error: the software is not supported for /31 networks\n");
		exit(1);
	}
	br->configured = 1;
}

// stores the next available IP address within br's range in ip.
int net_get_next_ip(Bridge *br, uint32_t *ip) {
  assert(br);
  assert(ip);

  *ip = 0;

  struct ifaddrs *ifaddr, *ifa;
  int family, s, n;
  char host[NI_MAXHOST];

  if (getifaddrs(&ifaddr) == -1) {
    exechelp_logerrv("firejail", "Error: call to getifaddrs() failed, cannot calculate an IP for the sandbox's network namespace\n");
    errExit("getifaddrs");
	}
	
	if (!br->iprange_start || !br->iprange_end) {
    exechelp_logerrv("firejail", "Error: the automatic NAT interface requires an IP range to be set\n");
    return -1;
	}

	uint32_t candidate = br->iprange_start;
	uint32_t end = br->iprange_end;
  uint32_t current;
  int taken = -1;

  do {
    taken = 0;
    
    for (ifa = ifaddr, n = 0; ifa != NULL && !taken; ifa = ifa->ifa_next, n++) {
      if (ifa->ifa_addr == NULL)
        continue;
      if (ifa->ifa_addr->sa_family != AF_INET)
        continue;

      s = getnameinfo(ifa->ifa_addr, sizeof(struct sockaddr_in), host, NI_MAXHOST, NULL, 0, NI_NUMERICHOST);
      if (s != 0) {
        exechelp_logerrv("firejail", "getnameinfo() failed on interface '%s': %s", ifa->ifa_name, gai_strerror(s));
        continue;
      }

      if (atoip(host, &current) == 0)
        taken = current == candidate;
    }
    if (!taken)
      *ip = candidate;
  } while (taken && candidate++ < end);

  freeifaddrs(ifaddr);
  return (candidate < end)? 0 : -1;
}

void net_configure_sandbox_ip(Bridge *br) {
	assert(br);
	if (br->configured == 0)
		return;

	if (br->arg_ip_none)
		br->ipsandbox = 0;
	else if (br->ipsandbox) {
		// check network range
		char *rv = in_netrange(br->ipsandbox, br->ip, br->mask);
		if (rv) {
			exechelp_logerrv("firejail", "%s", rv);
			exit(1);
		}
		// send an ARP request and check if there is anybody on this IP address
		if (arp_check(br->dev, br->ipsandbox, br->ip)) {
			exechelp_logerrv("firejail", "Error: IP address %d.%d.%d.%d is already in use\n", PRINT_IP(br->ipsandbox));
			exit(1);
		}
	}
	else
		// ip address assigned by arp-scan for a bridge device
		br->ipsandbox = arp_assign(br->dev, br); //br->ip, br->mask);
}


// create a veth pair
// - br - bridge device
// - ifname - interface name in sandbox namespace
// - child - child process running the namespace

void net_configure_veth_pair(Bridge *br, const char *ifname, pid_t child) {
	assert(br);
	if (br->configured == 0)
		return;

	// create a veth pair
	char *dev;
	if (asprintf(&dev, "veth%u%s", getpid(), ifname) < 0)
		errExit("asprintf");
	net_create_veth(dev, ifname, child);

	// bring up the interface
	net_if_up(dev);

	// add interface to the bridge
	net_bridge_add_interface(br->dev, dev);

	char *msg;
	if (asprintf(&msg, "%d.%d.%d.%d address assigned to sandbox", PRINT_IP(br->ipsandbox)) == -1)
		errExit("asprintf");
	logmsg(msg);
	fflush(0);
	free(msg);
}

// the default address should be in the range of at least on of the bridge devices
void check_default_gw(uint32_t defaultgw) {
	assert(defaultgw);

	if (cfg.bridge0.configured) {
		char *rv = in_netrange(defaultgw, cfg.bridge0.ip, cfg.bridge0.mask);
		if (rv == 0)
			return;
	}
	if (cfg.bridge1.configured) {
		char *rv = in_netrange(defaultgw, cfg.bridge1.ip, cfg.bridge1.mask);
		if (rv == 0)
			return;
	}
	if (cfg.bridge2.configured) {
		char *rv = in_netrange(defaultgw, cfg.bridge2.ip, cfg.bridge2.mask);
		if (rv == 0)
			return;
	}
	if (cfg.bridge3.configured) {
		char *rv = in_netrange(defaultgw, cfg.bridge3.ip, cfg.bridge3.mask);
		if (rv == 0)
			return;
	}

	exechelp_logerrv("firejail", "Error: default gateway %d.%d.%d.%d is not in the range of any network\n", PRINT_IP(defaultgw));
	exit(1);
}

void net_check_cfg(void) {
	int net_configured = 0;
	if (cfg.bridge0.configured)
		net_configured++;
	if (cfg.bridge1.configured)
		net_configured++;
	if (cfg.bridge2.configured)
		net_configured++;
	if (cfg.bridge3.configured)
		net_configured++;

	// --defaultgw requires a network
	if (cfg.defaultgw && net_configured == 0) {
		exechelp_logerrv("firejail", "Error: option --defaultgw requires at least one network to be configured\n");
		exit(1);
	}

	if (net_configured == 0) // nothing to check
		return;

	// --net=none
	if (arg_nonetwork && net_configured) {
		exechelp_logerrv("firejail", "Error: --net and --net=none are mutually exclusive\n");
		exit(1);
	}

	// check default gateway address or assign one
	assert(cfg.bridge0.configured);
	if (cfg.defaultgw)
		check_default_gw(cfg.defaultgw);
	else {
		// first network is a regular bridge
		if (cfg.bridge0.macvlan == 0)
			cfg.defaultgw = cfg.bridge0.ip;
		// first network is a mac device
		else {
			// get the host default gw
			uint32_t gw = network_get_defaultgw();
			// check the gateway is network range
			if (in_netrange(gw, cfg.bridge0.ip, cfg.bridge0.mask))
				gw = 0;
			cfg.defaultgw = gw;
		}
	}
}



void net_dns_print_name(const char *name) {
	if (!name || strlen(name) == 0) {
		exechelp_logerrv("firejail", "Error: invalid sandbox name\n");
		exit(1);
	}
	pid_t pid;
	if (name2pid(name, &pid)) {
		exechelp_logerrv("firejail", "Error: cannot find sandbox %s\n", name);
		exit(1);
	}

	net_dns_print(pid);
}

#define MAXBUF 4096
void net_dns_print(pid_t pid) {
	// drop privileges - will not be able to read /etc/resolv.conf for --noroot option
//	drop_privs(1);

	// if the pid is that of a firejail  process, use the pid of the first child process
	char *comm = pid_proc_comm(pid);
	if (comm) {
		// remove \n
		char *ptr = strchr(comm, '\n');
		if (ptr)
			*ptr = '\0';
		if (strcmp(comm, "firejail") == 0) {
			pid_t child;
			if (find_child(pid, &child) == 0) {
				pid = child;
			}
		}
		free(comm);
	}
	
	char *fname;
	if (asprintf(&fname, "/proc/%d/root/etc/resolv.conf", pid) == -1)
		errExit("asprintf");

	// access /etc/resolv.conf
	FILE *fp = fopen(fname, "r");
	if (!fp) {
		exechelp_logerrv("firejail", "Error: cannot access /etc/resolv.conf\n");
		exit(1);
	}
	
	char buf[MAXBUF];
	while (fgets(buf, MAXBUF, fp))
		printf("%s", buf);
	printf("\n");
	fclose(fp);
	free(fname);
	exit(0);
}
