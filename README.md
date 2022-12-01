![4scii](file.png)
# 4scii - ASCII art with the mouse in the terminal

### OpenBSD:
```
cc -O2 -pipe -c 4scii.c
cc -o 4scii 4scii.o -lcurses
```

### Linux:
```
sudo apt install -y ncurses-dev
gcc -O2 -pipe -c 4scii.c
gcc -o 4scii 4scii.o -lncurses
```
### Usage:
```
Press any key to change brush
Save: ctrl-s
Quit: ctrl-q
```
