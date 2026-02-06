#!/bin/sh

echo "Extracting solanum/ircd/version.c..."

if test -r version.c.last
then
   generation=`sed -n 's/^const char \*generation = "\(.*\)";/\1/p' < version.c.last`
   if test ! "$generation" ; then generation=0; fi
else
   generation=0
fi

generation=`expr $generation + 1`

if test -n "$SOURCE_DATE_EPOCH" -a "$EXTERNAL_BUILD_TIMESTAMP" = '' ; then
    EXTERNAL_BUILD_TIMESTAMP=$SOURCE_DATE_EPOCH
fi
if test "$EXTERNAL_BUILD_TIMESTAMP" = ''; then
    creation=`LC_ALL=C date | \
awk '{if (NF == 6) \
         { print $1 " "  $2 " " $3 " "  $6 " at " $4 " " $5 } \
else \
         { print $1 " "  $2 " " $3 " " $7 " at " $4 " " $5 " " $6 }}'`
else
    creation="$EXTERNAL_BUILD_TIMESTAMP"
    generation=1
fi

# Generate CREDITS content
credits=$(sed 's/^$/ /;s/.*/    "&",/' ../CREDITS)

# Read template and substitute
sed -e "s/@GENERATION@/$generation/" \
    -e "s/@CREATION@/$creation/" \
    -e "/@CREDITS@/r /dev/stdin" \
    -e "/@CREDITS@/d" \
    version.c.in > version.c << EOF
$credits
EOF
