################################
# Restricted GUI application profile
################################
include /etc/firejail/disable-mgmt.inc
include /etc/firejail/disable-secret.inc
include /etc/firejail/disable-common.inc
include /etc/firejail/disable-history.inc

caps.drop all
seccomp
netfilter
noroot
net none
helper
blacklist ${HOME}/.config/
blacklist ${HOME}/.cache/
blacklist ${HOME}/.local/share/
noblacklist ${HOME}/.config/firejail
noblacklist ${HOME}/.local/share/firejail
