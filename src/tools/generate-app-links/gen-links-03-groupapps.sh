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

mkdir -p out-03
cd out-02
for package in *
do
	if [ ! -e ../out-03/$package ] || [ "x${force}" = "xtrue" ]
	then
	  echo "Processing $package..."
	  mkdir -p out-03/$package
	  cd out-03/$package
	
	  cat ../../../packagelist | grep "^${package}" | sort -u > primary
	  cat ../../../packagelist | grep "lib*${package}" | sort -u > lib
	  cat ../../../packagelist | grep "${package}" | sort -u > tmp
	  comm -13 primary tmp > tmp2
	  comm -23 tmp2 lib > secondary
	  rm tmp tmp2
	
	  cd ../..
  else
    echo "Package $package already done, skipping..."
	fi
done

if [ -e out-03 ]
then
  echo "Moving files to output folder..."
  for dir in out-03/*
  do
    rm -rf "../${dir}"
    mv $dir ../out-03/
  done
fi

cd ..
echo "Done."
