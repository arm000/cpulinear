#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <sys/time.h>
#include <math.h>

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>

#include <GLES2/gl2.h>
#include <EGL/egl.h>

const char vertex_src[] =
"                                        \
   attribute vec4        position;       \
   varying mediump vec2  pos;            \
                                         \
   void main()                           \
   {                                     \
      gl_Position = position;            \
      pos = position.xy;                 \
   }                                     \
";

const char fragment_src[] =
"                                                      \
   varying mediump vec2    pos;                        \
                                                       \
   void  main()                                        \
   {                                                   \
      gl_FragColor  =  vec4( 1., 0.9, 0.7, 1.0 ) *     \
        cos( 30.*sqrt(pos.x*pos.x + 1.5*pos.y*pos.y)   \
             + atan(pos.y,pos.x));                     \
   }                                                   \
";


void print_shader_info_log(GLuint shader)
{
	GLint length;
	GLint success;

	glGetShaderiv(shader ,GL_INFO_LOG_LENGTH, &length);

	if (length > 1) {
		char *buffer = (char *)malloc(length);
		glGetShaderInfoLog (shader, length , NULL, buffer);
		printf("%s\n", buffer);
		free(buffer);
	}

	glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
	if (success != GL_TRUE) {
		fprintf(stderr, "Error compiling shader\n");
		exit(1);
	}
}


GLuint load_shader(const char *shader_source, GLenum type)
{
	GLuint shader = glCreateShader(type);

	glShaderSource(shader, 1, &shader_source, NULL);
	glCompileShader(shader);

	print_shader_info_log(shader);

	return shader;
}


Display    *x_display;
Window      win;
EGLDisplay  egl_display;
EGLContext  egl_context;
EGLSurface  egl_surface;

GLfloat
   p1_pos_x  =  0.0,
   p1_pos_y  =  0.0;

GLint
   position_loc;


bool        update_pos = false;

const float vertexArray[] = {
  -1.0, -1.0,  0.0,
  -1.0,  1.0,  0.0,
   1.0,  1.0,  0.0,
   1.0, -1.0,  0.0,
  -1.0, -1.0,  0.0
};


void render(void)
{
	static int donesetup = 0;

	static XWindowAttributes gwa;

	// draw
	if (!donesetup) {
		XWindowAttributes gwa;
		XGetWindowAttributes(x_display, win, &gwa);
		glViewport(0, 0, gwa.width, gwa.height);
		glClearColor(0.08, 0.06, 0.07, 1.);    // background color
		donesetup = 1;
	}
	glClear(GL_COLOR_BUFFER_BIT);

	glVertexAttribPointer(position_loc, 3, GL_FLOAT, false, 0, vertexArray);
	glEnableVertexAttribArray(position_loc);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 5);

	// get the rendered buffer to the screen
	eglSwapBuffers(egl_display, egl_surface);
}


/////////////////////////////////////////////////////////////////////


int main(void)
{
	///////  the X11 part  //////////////////////////////////////
	// in the first part the program opens a connection to the
	// X11 window manager
	//

	// open the standard display (the primary screen)
	x_display = XOpenDisplay(NULL);
	if (x_display == NULL) {
		fprintf(stderr, "cannot connect to X server\n");
		return 1;
	}

	// get the root window (usually the whole screen)
	Window root = DefaultRootWindow(x_display);

	XSetWindowAttributes swa;
	swa.event_mask = ExposureMask | KeyPressMask;

	// create a window with the provided parameters
	win = XCreateWindow (
		x_display, root,
		0, 0, 256, 256, 0,
		CopyFromParent, InputOutput,
		CopyFromParent, CWEventMask,
		&swa);

	XSetWindowAttributes xattr;
	Atom atom;
	int one = 1;

	xattr.override_redirect = False;
	XChangeWindowAttributes(x_display, win, CWOverrideRedirect, &xattr);

	XWMHints hints;
	hints.input = True;
	hints.flags = InputHint;
	XSetWMHints(x_display, win, &hints);

	// make the window visible on the screen
	XMapWindow(x_display, win);
	XStoreName(x_display, win, "GL test" ); // give the window a name


	///////  the egl part  /////////////////////////////////////////////
	//  egl provides an interface to connect the graphics related functionality of openGL ES
	//  with the windowing interface and functionality of the native operation system (X11
	//  in our case.

	egl_display = eglGetDisplay((EGLNativeDisplayType) x_display);
	if (egl_display == EGL_NO_DISPLAY) {
		fprintf(stderr, "Got no EGL display.\n");
		return 1;
	}

	if (!eglInitialize(egl_display, NULL, NULL)) {
		fprintf(stderr, "Unable to initialize EGL\n");
		return 1;
	}

	EGLint attr[] = {       // some attributes to set up our egl-interface
		EGL_BUFFER_SIZE, 16,
		EGL_RENDERABLE_TYPE,
		EGL_OPENGL_ES2_BIT,
		EGL_NONE
	};

	EGLConfig ecfg;
	EGLint num_config;
	if (!eglChooseConfig(egl_display, attr, &ecfg, 1, &num_config)) {
		fprintf(stderr, "Failed to choose config (eglError: %d)\n",
			eglGetError());
		return 1;
	}

	if (num_config != 1) {
		fprintf(stderr, "Didn't get exactly one config, but %d\n",
			num_config);
		return 1;
	}

	egl_surface = eglCreateWindowSurface(egl_display, ecfg, win, NULL);
	if (egl_surface == EGL_NO_SURFACE) {
		fprintf(stderr, "Unable to create EGL surface (eglError: %d)\n",
			eglGetError());
		return 1;
	}

	// egl-contexts collect all state descriptions needed required for operation
	EGLint ctxattr[] = {
		EGL_CONTEXT_CLIENT_VERSION, 2,
		EGL_NONE
	};
	egl_context = eglCreateContext(egl_display, ecfg, EGL_NO_CONTEXT, ctxattr);
	if (egl_context == EGL_NO_CONTEXT) {
		fprintf(stderr,
			"Unable to create EGL context (eglError: %d)\n",
			eglGetError());
		return 1;
	}

	// associate the egl-context with the egl-surface
	eglMakeCurrent(egl_display, egl_surface, egl_surface, egl_context);


	///////  the openGL part  /////////////////////////////////////

	// load vertex shader
	GLuint vertexShader = load_shader(vertex_src, GL_VERTEX_SHADER);
	// load fragment shader
	GLuint fragmentShader = load_shader(fragment_src, GL_FRAGMENT_SHADER);

	// create program object
	GLuint shaderProgram  = glCreateProgram();
	// and attach both...
	glAttachShader(shaderProgram, vertexShader);
	// ... shaders to it
	glAttachShader(shaderProgram, fragmentShader);

	glLinkProgram(shaderProgram);    // link the program
	glUseProgram(shaderProgram);    // and select it for usage

	// now get the locations (kind of handle) of the shaders variables
	position_loc = glGetAttribLocation(shaderProgram, "position");
	if (position_loc < 0) {
		fprintf(stderr, "Unable to get uniform location\n");
		return 1;
	}

	// this is needed for time measuring  -->  frames per second
	struct timezone tz;
	struct timeval t1, t2;
	gettimeofday(&t1, &tz);
	int num_frames = 0;

	bool quit = false;
	while (!quit) {    // the main loop

		// check for events from the x-server
		while (XPending(x_display)) {
			XEvent  xev;
			XNextEvent(x_display, &xev);

			if (xev.type == KeyPress)
				quit = true;
		}

		render();   // now we finally put something on the screen

		if (++num_frames % 100 == 0) {
			gettimeofday( &t2, &tz );
			float dt = t2.tv_sec - t1.tv_sec + (t2.tv_usec - t1.tv_usec) * 1e-6;
			printf("fps: %f\n", num_frames / dt);
			num_frames = 0;
			t1 = t2;
		}
//      usleep( 1000*10 );
	}


	//  cleaning up...
	eglDestroyContext(egl_display, egl_context);
	eglDestroySurface(egl_display, egl_surface);
	eglTerminate(egl_display);
	XDestroyWindow(x_display, win);
	XCloseDisplay(x_display);

	return 0;
}
