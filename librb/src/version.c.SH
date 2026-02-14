#!/bin/sh

echo "Extracting librb/src/version.c..."

if test -r version.c.last
then
   generation=`sed -n 's/^const char \*librb_generation = "\(.*\)";/\1/p' < version.c.last`
   if test ! "$generation" ; then generation=0; fi
else
   generation=0
fi

generation=`expr $generation + 1`

# Generate CREDITS content
credits=$(grep -v '^$Id' ../CREDITS | tr -d '"' | sed 's/.*/  "&",/')

# Read template and substitute
sed -e "s/@GENERATION@/$generation/" \
    -e "/@CREDITS@/r /dev/stdin" \
    -e "/@CREDITS@/d" \
    version.c.in > version.c << EOF
$credits
EOF
