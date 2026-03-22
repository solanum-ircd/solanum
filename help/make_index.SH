#!/bin/sh

set -eu

SOURCE="$1"
: ${DESTDIR:=/}
OUTPUT="${DESTDIR%/}/${2#/}"

echo "Writing help indexes to $OUTPUT from $SOURCE"

USERS_INDEX="$OUTPUT/users/index"
OPERS_INDEX="$OUTPUT/opers/index"

# Build users index
mkdir -p "$OUTPUT/users/"
rm -f "$USERS_INDEX.tmp"
for help in "$SOURCE/users/"*; do
  if [ -f "$help" ]; then
    basename "$help" >> "$USERS_INDEX.tmp"
  fi
done

# Add symlinks to users index
for link in topic accept cmode admin names links away whowas version kick who invite quit join list nick oper part time credits motd userhost users whois ison lusers user help pass error challenge knock ping pong map trace chantrace extban monitor batch; do
  echo "$link" >> "$USERS_INDEX.tmp"
done

echo 'Help topics available to users:' > "$USERS_INDEX"
echo '' >> "$USERS_INDEX"
cat "$USERS_INDEX.tmp" | sed -e 's|^users/||' | sort -u | tr a-z A-Z | column -c 65 -x | expand >> "$USERS_INDEX"
rm -f "$USERS_INDEX.tmp"

# Build opers index
mkdir -p "$OUTPUT/opers/"
rm -f "$OPERS_INDEX.tmp"
for help in "$SOURCE/opers/"*; do
  if [ -f "$help" ]; then
    basename $help >> "$OPERS_INDEX.tmp"
  fi
done

echo 'Help topics available to opers:' > "$OPERS_INDEX"
echo '' >> "$OPERS_INDEX"
cat "$OPERS_INDEX.tmp" | sed -e 's|^opers/||' | sort -u | tr a-z A-Z | column -c 65 -s ' ' -x | expand >> "$OPERS_INDEX"
rm -f "$OPERS_INDEX.tmp"
