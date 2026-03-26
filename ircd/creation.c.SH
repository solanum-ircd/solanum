#!/bin/sh

set -eu

INPUT_FILE="$1"
OUTPUT_FILE="$2"
GENERATION_FILE="$3"

# Use SOURCE_DATE_EPOCH if it is present and EXTERNAL_BUILD_TIMESTAMP is not
#
if test -n "${SOURCE_DATE_EPOCH:-}"
then
    if test -z "${EXTERNAL_BUILD_TIMESTAMP:-}"
    then
        EXTERNAL_BUILD_TIMESTAMP="${SOURCE_DATE_EPOCH}"
    fi
fi

if test -n "${EXTERNAL_BUILD_TIMESTAMP:-}"
then
    # If using reproducible builds, hardcode the build timestamp and set the generation counter to 1
    #
    CREATION=$(LC_ALL=C date -d '@'"${EXTERNAL_BUILD_TIMESTAMP}" +'%a %b %d %Y at %T %Z')
    GENERATION=1
else
    # Otherwise, use the current date as the creation date
    #
    CREATION=$(LC_ALL=C date +'%a %b %d %Y at %T %Z')

    # If this is not the first generation, read and increment the old one. Otherwise, use an initial generation
    # counter of 0, in order to ensure that when the counter gets bumped twice (by invoking 'meson compile' and
    # 'meson install'), it reads as 1 when IRCd is ultimately started.
    #
    if test -r "${GENERATION_FILE}"
    then
        GENERATION=$(cat "${GENERATION_FILE}")
        if test -n "${GENERATION:-}"
        then
            GENERATION=$((GENERATION + 1))
        else
            GENERATION=0
        fi
    else
        GENERATION=0
    fi
fi

# Write the output file
#
sed                                         \
    -e "s/@CREATION@/${CREATION}/"          \
    -e "s/@GENERATION@/${GENERATION}/"      \
    < "${INPUT_FILE}" > "${OUTPUT_FILE}"

# Write the new generation counter
#
echo "${GENERATION}" > "${GENERATION_FILE}"
