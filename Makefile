cpulinear: cpulinear.c Makefile
	gcc -o $@ $< -lm -lX11 -lEGL -lGLESv2

clean:
	rm -rf cpulinear *.o *~
