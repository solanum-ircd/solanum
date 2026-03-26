#!/bin/sh

set -eu

echo "Extracting librb/src/version.c..."

# Generate CREDITS content
credits=$(grep -v '^$Id' ../CREDITS | tr -d '"' | sed 's/.*/  "&",/')

# Read template and substitute
sed -e "/@CREDITS@/r /dev/stdin" \
    -e "/@CREDITS@/d" \
    version.c.in > version.c << EOF
$credits
EOF
