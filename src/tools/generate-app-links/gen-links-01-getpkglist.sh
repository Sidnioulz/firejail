#!/bin/sh
echo "Downloading latest package list..."
apt-cache dumpavail |grep -oP "(?<=Package: ).*" | grep -v dbgsym | sort -u > packagelist
echo "Done."
