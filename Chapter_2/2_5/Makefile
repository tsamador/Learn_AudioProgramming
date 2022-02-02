
synes: envx.c portsf/breakpoints.c
	gcc envx.c portsf/breakpoints.c -lportsf -lm -o envx -g

sfenv: sfenv.c portsf/breakpoints.c
	gcc sfenv.c portsf/breakpoints.c -lportsf -lm -o sfenv -g

siggen: siggen.c portsf/breakpoints.c wave.c
	gcc siggen.c portsf/breakpoints.c wave.c -lportsf -lm -o siggen -g