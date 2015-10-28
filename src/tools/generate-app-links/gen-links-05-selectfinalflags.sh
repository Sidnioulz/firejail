#!/bin/sh

usage() { echo "Usage: $0 [-f] [-k] [-r]" 1>&2; exit 1; }
force=false
alwayskeep=false
alwaysremove=false

if [ -e "/usr/bin/colordiff" ]
then
  diff=colordiff
else
  diff=diff
fi

while getopts ":fkr" o; do
    case "${o}" in
        f)
            force=true
            ;;
        k)
            alwayskeep=true
            ;;
        r)
            alwaysremove=true
            ;;
        *)
            usage
            ;;
    esac
done
shift $((OPTIND-1))

mkdir -p out-05
for dir in out-03/*
do
  package=`echo $dir | sed -e 's|out-03/||'`
  
	if [ ! -e out-05/$package ] || [ "x${force}" = "xtrue" ]
	then
    mkdir out-05/$package/ -p

    for file in "primary" "secondary" "lib"
    do
      next=0
      echo "* Package #[    ${package}    ]#, $file dependencies"

      if [ -e "out-04/$package/$file" ]
      then
        comm -23 out-03/$package/$file out-04/$package/$file > __tmp_removed
        echo "* File if removing detected mistaches:"
        $diff -u  out-03/$package/$file __tmp_removed | tail -n +4
      else
        echo "* Original file:"
        cat out-03/$package/$file
      fi
      while [ "$next" -eq "0" ]
      do
        next=1
        if [ -e "out-04/$package/$file" ]
        then
          if  [ "x${alwaysremove}" = "xtrue" ]
          then
            echo "Will [r]emove mismatches automatically..."
            response="keep"
          else
            read -r -p "Do you want to [e]dit list, [k]eep original, [r]emove mismatches? (leave blank -> r) " response
          fi
        elif  [ "x${alwayskeep}" = "xtrue" ]
        then
          echo "Will [k]eep original since no mismatches were found..."
          response="keep"
        else
          read -r -p "Do you want to [e]dit list or [k]eep original? (leave blank -> k) " response
        fi
        case $response in
          [eE][dD][iI][tT]|[eE])
            cp -f out-03/$package/$file out-05/$package/$file
            nano out-05/$package/$file
            echo "Saved edits to original.\n"
              ;;
          [kK][eE][eE][pP]|[kK])
            cp -f out-03/$package/$file out-05/$package/$file
            echo "Kept original.\n"
              ;;
          [rR][eE][mM][oO][vV][eE]|[rR])
            cp -f __tmp_removed out-05/$package/$file
            echo "Removed mismatches.\n"
              ;;
          "")
            if [ -e "out-04/$package/$file" ]
            then
              cp -f __tmp_removed out-05/$package/$file
              echo "Removed mismatches.\n"
            else
              cp -f out-03/$package/$file out-05/$package/$file
              echo "Kept original.\n"
            fi
              ;;
          *)
            echo "did not understand option... retrying..."
            next=0
              ;;
        esac
        echo
      done
    done
  else
    echo "Package $package already done, skipping..."
	fi

  rm -f __tmp_removed

done

echo "Done."
