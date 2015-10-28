#!/bin/sh

## Clean up previous runs since we'll need to aggregate files
for dir in out-06/*
do
  rm -f $dir/linked
done

getmain()
{
  local main=""
  if [ -e out-02/$1 ]; then
    if [ "`grep -r "/usr/local/sbin/$1" out-02/$1/*deb_/applications`" != "" ]; then
      main="/usr/local/sbin/$1"
    elif [ "`grep -r "/usr/local/bin/$1" out-02/$1/*deb_/applications`" != "" ]; then
      main="/usr/local/bin/$1"
    elif [ "`grep -r "/usr/sbin/$1" out-02/$1/*deb_/applications`" != "" ]; then
      main="/usr/sbin/$1"
    elif [ "`grep -r "/usr/bin/$1" out-02/$1/*deb_/applications`" != "" ]; then
      main="/usr/bin/$1"
    elif [ "`grep -r "/sbin/$1" out-02/$1/*deb_/applications`" != "" ]; then
      main="/sbin/$1"
    elif [ "`grep -r "/bin/$1" out-02/$1/*deb_/applications`" != "" ]; then
      main="/bin/$1"
    fi
  fi

  echo "$main"
}

for dir in out-05/*
do
  package=`echo $dir | sed -e 's|out-05/||'`
  
  #echo "$package:"
  ## Identify a main binary
  main=$(getmain $package)

  ## Main binary, let's build a linked list
  if [ "" != "$main" ]; then
    echo "* Processing binary package $main..."
    
    mkdir -p out-06/$package/
    echo "$main" > out-06/$package/main
    rm -f out-06/$package/linked

    haspy=false
    hassh=false
    ## Create list of binaries linked to ourselves
    while read p; do
      for file in out-02/$p/*deb_; do
        cat $file/applications >> out-06/$package/linked
        if [ "`cat $file/python | wc -l `" -gt 0 ]; then
          haspy=true
        fi
        if [ "`cat $file/bash | wc -l `" -gt 0 ]; then
          hassh=true
        fi
      done
    done <out-05/$package/primary
    count=`cat out-06/$package/linked | wc -l`
    echo "\t$package can call $count binaries"
    
    if [ "$haspy" = "true" ]; then
      echo "\t$package can call Python"
      echo "/usr/bin/python3.4m\n/usr/bin/python3.4\n/usr/bin/python2.7\n/usr/bin/python" >> out-06/$package/linked
    fi
    
    if [ "$hassh" = "true" ]; then
      echo "\t$package can call Bash"
      echo "`which sh 2>/dev/null`\n`which bash 2>/dev/null`\n" >> out-06/$package/linked
    fi
  fi
  
  ## Now check which packages should be able to call us
  if [ "`head -n1 out-05/$package/secondary`" != "" ]; then
    echo "* Granting other packages permission to call $package..."

    while read p; do
      othermain=$(getmain $p)
      haspy=false
      hassh=false
      if [ "" != "$othermain" ]; then
        echo "\t$p (main binary: $othermain) can call $package binaries"  
        
        mkdir -p out-06/$p/
        for file in out-02/$package/*deb_; do
          cat $file/applications >> out-06/$p/linked
          if [ "`cat $file/python | wc -l `" -gt 0 ]; then
            haspy=true
          fi
          if [ "`cat $file/bash | wc -l `" -gt 0 ]; then
            hassh=true
          fi
        done    
        if [ "$haspy" = "true" ]; then
          echo "/usr/bin/python3.4m\n/usr/bin/python3.4\n/usr/bin/python2.7\n/usr/bin/python" >> out-06/$p/linked
        fi
        
        if [ "$hassh" = "true" ]; then
          echo "`which sh 2>/dev/null`\n`which bash 2>/dev/null`\n" >> out-06/$p/linked
        fi
        
      else
        echo "\t$p can call $package but is not a binary package, ignoring..."
      fi
    done <out-05/$package/secondary
  fi
done

## Cleanup
for dir in out-06/*
do
  cat $dir/main >> $dir/linked
  cat $dir/linked | sort -u > $dir/tmp
  mv $dir/tmp $dir/linked
done

echo "Done."
