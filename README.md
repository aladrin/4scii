# 4scii - ASCII art with the mouse in the terminal

Tested on OpenBSD and Linux

 To compile on OpenBSD:

```
 cc -O2 -pipe -c 4scii.c
 cc -o 4scii 4scii.o -lcurses
```

 To compile on Linux:

```
 gcc -O2 -pipe -c 4scii.c
 gcc -o 4scii 4scii.o -lncurses
```
