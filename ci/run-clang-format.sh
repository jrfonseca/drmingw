#!/bin/sh
git ls-files src | grep '\.[ch]\(pp\)\?$' | xargs -d '\n' clang-format -i
