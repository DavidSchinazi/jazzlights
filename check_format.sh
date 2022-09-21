#!/bin/bash

# Set current working directory to the project directory.
cd -- "$(dirname -- "${BASH_SOURCE[0]}")" &> /dev/null

FIX="$([ "$#" -ge "1" ] && [ "$1" == "--fix" ] && echo 1 || echo 0)"

RET_VAL=0
for f in $(echo "$(find . -name '*.h' ; find . -name '*.cpp')" | grep -v /.pio/ | grep -v /build/) ; do
  if [ "$FIX" == "1" ] ; then
    clang-format -i "$f"
  else
    if [ -n "$(clang-format --output-replacements-xml "$f" | grep '<replacement ')" ] ; then
      echo "Format issue detected in $f"
      echo ""
      TMP_FILE="$(mktemp /tmp/jazzlights-clang-format.XXXXXXXX)"
      clang-format "$f" > "$TMP_FILE"
      diff -u "$f" "$TMP_FILE"
      rm "$TMP_FILE"
      echo "" ; echo ""
      RET_VAL=1
    fi
  fi
done
exit $RET_VAL
