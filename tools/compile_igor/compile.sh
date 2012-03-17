#!/bin/sh
rm -f *.bin
./compile_igor --main ../decode_igor/code/001_08B7.asm ../decode_igor/code/222_2A6A.asm
while read part seg ptr comment; do
	F=../decode_igor/code/${seg}_${ptr}.asm
	if [ -f $F ]; then
		./compile_igor --part=$part $F || exit
	fi
done < ./parts.txt
#./compile_igor --part=85 ../decode_igor/code/209_0DA4.asm
