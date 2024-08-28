#!/bin/bash
 g++ -O3 test.cpp -o test.exe -lGdi32 -lGdiplus -mwindows
 echo "Program Compiled"
 exit 0