#!/bin/bash
 g++ -O3 test.cpp -o test2.exe -lGdi32 -lGdiplus -mwindows -static -static-libgcc -static-libstdc++
 echo "Program Compiled"
 exit 0
