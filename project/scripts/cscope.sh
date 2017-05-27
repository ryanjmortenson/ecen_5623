#/usr/bin/env bash

find . -name "*.py" -o -name "*.c" -o -name "*.h" -o -name "*.cpp" -o -name "*.mk" -o -name "makefile" -o -name "*.S"> files.txt
cscope -i files.txt -bqU
rm files.txt
