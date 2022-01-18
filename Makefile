
synes: synesthetic.c
	gcc synesthetic.c -lportsf -lm -o build/synesthetic

run: build/synesthetic
	./synesthetic