#!/bin/bash

cd -- "$(dirname -- "${BASH_SOURCE[0]}")" &> /dev/null

RET_VAL=0
for f in $(echo "$(find . -name '*.h' ; find . -name '*.cpp')" | grep -v /.pio/ | grep -v /build/) ; do
  if [ -n "$(clang-format --output-replacements-xml "$f" | grep '<replacement ')" ] ; then
    echo "Format issue detected in $f"
    RET_VAL=1
  fi
done
exit $RET_VAL
