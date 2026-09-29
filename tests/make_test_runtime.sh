#!/bin/sh
# Generates the runtime/ dir to execute tests for meson
# Not to be run manually!

set -eu

SOURCEROOT=$(realpath -s $1)
BUILDROOT=$(realpath -s $2)
OUTPUT=$(realpath -s $3)/runtime

rm -rf $OUTPUT
mkdir -p $OUTPUT/modules/autoload
mkdir -p $OUTPUT/modules/extensions
mkdir $OUTPUT/bin
mkdir $OUTPUT/help

ln -s $BUILDROOT/authd/authd $OUTPUT/bin/authd
ln -s $BUILDROOT/bandb/bandb $OUTPUT/bin/bandb
ln -s $BUILDROOT/ssld/ssld $OUTPUT/bin/ssld

cp -r $SOURCEROOT/help/opers $OUTPUT/help
cp -r $SOURCEROOT/help/users $OUTPUT/help
$SOURCEROOT/help/make_index.SH $SOURCEROOT/help $OUTPUT/help
links=$(grep -Pazo '(?s)user_symlinks = \[.*?\]' $SOURCEROOT/help/meson.build | grep -Pao "(?<=')[a-z]+")
echo $links | tr ' ' '\n' |  while read link; do
  ln -s "$OUTPUT/help/opers/$link" "$OUTPUT/help/users/$link"
done

coremods=$(grep -Pazo '(?s)core_modules = \[.*?\]' $SOURCEROOT/modules/meson.build | grep -Pao "(?<=')[^']+(?=')")
echo $coremods | tr ' ' '\n' | while read link; do
  ln -s "$BUILDROOT/modules/$link.so" "$OUTPUT/modules/$link.so"
done

modules=$(grep -Pazo '(?s)autoload_modules = \[.*?\]' $SOURCEROOT/modules/meson.build | grep -Pao "(?<=')[^']+(?=')")
echo $modules | tr ' ' '\n' | while read link; do
  ln -s "$BUILDROOT/modules/$link.so" "$OUTPUT/modules/autoload/$link.so"
done

extensions=$(grep -Pazo '(?s)extension_modules = \[.*?\]' $SOURCEROOT/extensions/meson.build | grep -Pao "(?<=')[^']+(?=')")
echo $extensions | tr ' ' '\n' | while read link; do
  if test -e "$BUILDROOT/extensions/$link.so"; then
    ln -s "$BUILDROOT/extensions/$link.so" "$OUTPUT/modules/extensions/$link.so"
  fi
done

cp $SOURCEROOT/tests/*.conf $3
echo "Hello World!" > $OUTPUT/motd
