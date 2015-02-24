cpulinear: cpulinear.c Makefile
	g++ -o $@ $< -lm -lX11 -lEGL -lGLESv2

clean:
	rm -rf cpulinear *.o *~
