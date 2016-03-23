#!/bin/sh
set -x

cd decode_igor
mkdir -p assets/
mkdir -p code/
mkdir -p dump/
./decode_igor ../../__/data_sp_cdrom ../spa_cd_funcs.txt
cd ..

cd compile_igor
mkdir -p out/
rm -f *.bin
./compile_igor --main ../decode_igor/code/001_08B7.asm ../decode_igor/code/222_2A6A.asm
while read part seg ptr comment; do
	F=../decode_igor/code/${seg}_${ptr}.asm
	if [ -f $F ]; then
		./compile_igor --part=$part $F || exit
	fi
done < ./parts.txt
cd ..

rm -f igor.bin
./make_igor/make_igor decode_igor/dump/ compile_igor/out/
mv igor.bin ..
