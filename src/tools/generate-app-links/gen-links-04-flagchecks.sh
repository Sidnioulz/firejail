#!/bin/sh

usage() { echo "Usage: $0" 1>&2; exit 1; }
force=false

while getopts ":" o; do
    case "${o}" in
        *)
            usage
            ;;
    esac
done
shift $((OPTIND-1))

mkdir -p out-04
cd out-03
for package in *
do
  prim=`cat $package/primary | grep -E "^${package}[^-]" | sort -u`
  primt=`echo "$prim" | sed -e 's/\(.*\)/\t\1/'`

  tmp=`comm -23 $package/secondary $package/lib`
  secpre=`echo "$tmp" | grep -E "[^-]${package}"`
  secpost=`echo "$tmp" | grep -E "${package}[^-]"`
  if [ ! "$secpost" = "" ] && [ ! "$secpre" = "" ]
  then
    sep="\n"
  else
    sep=""
  fi
  sec=`echo "${secpre}${sep}${secpost}" | sort -u`
  sect=`echo "$sec" | sed -e 's/\(.*\)/\t\1/'`

  lib=`cat $package/lib | grep -E "^lib${package}[^-0123456789]" | sort -u`
  libt=`echo "$lib" | sed -e 's/\(.*\)/\t\1/'`

  echo "\n${package}:"
  if [ ! "" = "${prim}${sec}${lib}" ]
  then
    mkdir -p out-04/$package
  fi

  if [ ! "" = "${prim}" ]
  then
    echo "* primary mismatches:\n${primt}\n"
    echo "${prim}" > out-04/$package/primary
  fi

  if [ ! "" = "${sec}" ]
  then
    echo "* secondary mismatches:\n${sect}\n"
    echo "${sec}" > out-04/$package/secondary
  fi

  if [ ! "" = "${lib}" ]
  then
    echo "* library mismatches:\n${libt}\n"
    echo "${lib}" > out-04/$package/lib
  fi
done

if [ -e out-04 ]
then
  echo "Moving files to output folder..."
  for dir in out-04/*
  do
    rm -rf "../${dir}"
    mv $dir ../out-04/
  done
fi

rmdir out-04
cd ..
echo "Done."
