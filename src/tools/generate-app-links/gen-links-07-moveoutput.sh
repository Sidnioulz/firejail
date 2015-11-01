#!/bin/sh

echo "Moving files to local etc directory..."
#FIXME mv out-06 ../../../etc/applinks.d
echo "Done."

echo "Generating Makefile for 'make install'..."
echo "install:" > ../../../etc/applinks.d/Makefile

cd ../../../etc/applinks.d
for dir in *
do
  if [ -d "$dir" ]
  then
    echo -e "\tmkdir -p \$(DESTDIR)/etc/firejail/applinks.d/${dir}" >> Makefile
	  echo -e "\tinstall -c -m 0644 ${dir}/main \$(DESTDIR)/etc/firejail/applinks.d/${dir}/." >> Makefile
	  echo -e "\tinstall -c -m 0644 ${dir}/linked \$(DESTDIR)/etc/firejail/applinks.d/${dir}/." >> Makefile
  fi
done
echo "Done."
