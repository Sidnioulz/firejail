#!/bin/bash

DEB_BUILD_DATE=$(dpkg-parsechangelog -S Date)

sed "s/VERSION/$1/g" $2 > $3
MONTH=`LC_ALL=C date --date="$DEB_BUILD_DATE" +%b`
sed -i "s/MONTH/$MONTH/g" $3
YEAR=`LC_ALL=C date --date="$DEB_BUILD_DATE" +%Y`
sed -i "s/YEAR/$YEAR/g" $3
