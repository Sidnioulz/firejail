#!/bin/sh

usage() { echo "Usage: $0 [-f]" 1>&2; exit 1; }
force=false

while getopts ":f" o; do
    case "${o}" in
        f)
            force=true
            ;;
        *)
            usage
            ;;
    esac
done
shift $((OPTIND-1))

for package in `cat packagelist | grep -v dbgsym`
do
	if [ ! -e out-02/$package ] || [ "x${force}" = "xtrue" ]
	then
	  echo "Processing $package..."
    mkdir -p out-02/$package
    cd out-02/$package
    apt-get -qqq download $package
    for db in `ls *.deb`
    do
      echo "\t$db"
	    dpkg -c $db > cached-dpkg
	    mkdir "${db}_" -p
	
	    cat cached-dpkg | grep -e "^[^d]..x" | sed -e's/  */ /g' | cut -d " " -f 6 | cut -c 2- | grep -v "/usr/share/help" | grep -v "/usr/share/doc" > "${db}_/applications"
	    cat cached-dpkg | grep -e ".py$" | sed -e's/  */ /g' | cut -d " " -f 6 | cut -c 2- | grep -v "/usr/share/help" | grep -v "/usr/share/doc" > "${db}_/python"
	    cat cached-dpkg | grep -e ".sh$" | sed -e's/  */ /g' | cut -d " " -f 6 | cut -c 2- | grep -v "/usr/share/help" | grep -v "/usr/share/doc" > "${db}_/bash"
	    cat cached-dpkg | grep -e ".js$" | sed -e's/  */ /g' | cut -d " " -f 6 | cut -c 2- | grep -v "/usr/share/help" | grep -v "/usr/share/doc" > "${db}_/javascript"
    done
	
	  rm *.deb
	  cd ../..
	  
	  ## Remove directory if errors, so a second run can be done after updating packagelist
	  if [ ! -e "out-02/$package/cached-dpkg" ] || [ "" = "`head -n1 out-02/$package/cached-dpkg`" ]
	  then
	    rm -rf out-02/$package
	  fi
  else
    echo "Package $package already done, skipping..."
	fi
done
echo "Done."
