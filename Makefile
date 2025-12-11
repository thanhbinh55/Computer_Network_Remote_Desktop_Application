all:
	@g++ main.cpp src/modules/*_linux.cpp -lX11 -lXtst -ljpeg -o main.exe

run:
	sudo ./main.exe
