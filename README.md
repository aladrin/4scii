![4scii](file.png)
#4scii - ASCII art with the mouse in the terminal

Tested on OpenBSD and Linux

#To compile on OpenBSD:
```
 cc -O2 -pipe -c 4scii.c
 cc -o 4scii 4scii.o -lcurses
```

#To compile on Linux:
```
 gcc -O2 -pipe -c 4scii.c
 gcc -o 4scii 4scii.o -lncurses
```
#Usage:
```
Press any key to change brush
Save: ctrl-s
Quit: ctrl-q
```
