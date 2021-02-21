#!/bin/sh
git ls-files src | grep '\.[ch]\(pp\)\?$' | xargs clang-format -i
