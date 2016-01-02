#!/bin/sh

echo "Creating a compact applinks.conf file..."
rm applinks.conf -f || die "Could not start with a clean applinks.conf file, aborting"

for i in out-06/*;
do
  echo "[`cat $i/main`]" >> applinks.conf
  cat $i/linked >> applinks.conf
  echo "" >> applinks.conf
done

echo "Done."
