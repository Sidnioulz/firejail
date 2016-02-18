# system directories	
blacklist /sbin
blacklist /usr/sbin

# system management
blacklist ${PATH}/umount
blacklist ${PATH}/mount
blacklist ${PATH}/fusermount
blacklist ${PATH}/su
blacklist ${PATH}/sudo
blacklist ${PATH}/xinput
blacklist ${PATH}/strace

# Prevent manipulation of firejail configuration
read-only /etc/firejail
noblacklist /etc/firejail/self
read-only ${HOME}/.config/firejail
