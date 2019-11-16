set PATH=tools
mipsas -o sys.o sys.S
rem mipsgcc -O0 -S -nostdlib -nostartfiles main.c
rem mipsgcc -O0 -c -nostdlib -nostartfiles main.c
mipsgcc -O0 -nostdlib -Wall -nostartfiles -Xlinker -mpsx -Xlinker -Map=output.map -o bin\main.exe start.c main.c sys.o
exefixup bin\main.exe
pause