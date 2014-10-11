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

#define WIDTH 256
#define HEIGHT 256

const char vertex_src[] =
	"attribute vec4 a_position;   \n"
	"attribute vec2 a_texCoord;   \n"
	"varying vec2 v_texCoord;     \n"
	"void main()                  \n"
	"{                            \n"
	"   gl_Position = a_position; \n"
	"   v_texCoord = a_texCoord;  \n"
	"}                            \n";

const char fragment_src[] =
	"precision mediump float;                            \n"
	"varying vec2 v_texCoord;                            \n"
	"uniform sampler2D s_texture;                        \n"
	"void main()                                         \n"
	"{                                                   \n"
	"  gl_FragColor = texture2D(s_texture, v_texCoord);  \n"
	"}                                                   \n";

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

GLuint upload_texture(void)
{
   // Texture object handle
   GLuint textureId;

   // 2x2 Image, 3 bytes per pixel (R, G, B)
   GLubyte pixels[4 * 3] =
   {
      255,   0,   0, // Red
        0, 255,   0, // Green
        0,   0, 255, // Blue
      255, 255,   0  // Yellow
   };

   // Use tightly packed data
   glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

   // Generate a texture object
   glGenTextures(1, &textureId);

   // Bind the texture object
   glBindTexture(GL_TEXTURE_2D, textureId);

   // Load the texture
   glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 2, 2, 0, GL_RGB, GL_UNSIGNED_BYTE,
		pixels);

   // Set the filtering mode
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

   return textureId;
}

Display    *x_display;
Window      win;
EGLDisplay  egl_display;
EGLContext  egl_context;
EGLSurface  egl_surface;

GLfloat
   p1_pos_x  =  0.0,
   p1_pos_y  =  0.0;

GLint position_loc;
GLint texture_loc;
GLint sampler_loc;

GLuint texture_id;

bool        update_pos = false;

const GLfloat vertexArray[] = {
  -1.0, -1.0,  0.0,
   0.0,  0.0,
  -1.0,  1.0,  0.0,
   0.0,  1.0,
   1.0,  1.0,  0.0,
   1.0,  1.0,
   1.0, -1.0,  0.0,
   1.0,  0.0,
  -1.0, -1.0,  0.0,
   0.0,  0.0
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
//	glClear(GL_COLOR_BUFFER_BIT);

	glVertexAttribPointer(position_loc, 3, GL_FLOAT, GL_FALSE,
			      5 * sizeof (GLfloat), vertexArray);
	glEnableVertexAttribArray(position_loc);

	glVertexAttribPointer(texture_loc, 2, GL_FLOAT, GL_FALSE,
			      5 * sizeof (GLfloat), &vertexArray[3]);
	glEnableVertexAttribArray(texture_loc);

	// Bind the texture
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, texture_id);

	// Set the sampler texture unit to 0
	glUniform1i(sampler_loc, 0);

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
		0, 0, WIDTH, HEIGHT, 0,
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
		EGL_BUFFER_SIZE, 32,
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

	// upload the texture
	texture_id = upload_texture();

	// now get the locations of the shaders variables
	position_loc = glGetAttribLocation(shaderProgram, "a_position");
	if (position_loc < 0) {
		fprintf(stderr, "Unable to get position location\n");
		return 1;
	}
	texture_loc = glGetAttribLocation(shaderProgram, "a_texCoord");
	if (texture_loc < 0) {
		fprintf(stderr, "Unable to get texture location\n");
		return 1;
	}

	// Get the sampler location
	sampler_loc = glGetUniformLocation(shaderProgram, "s_texture");
	if (sampler_loc < 0) {
		fprintf(stderr, "Unable to get sampler location\n");
		return 1;
	}

	// this is needed for time measuring  -->  frames per second
	struct timezone tz;
	struct timeval t1, t2;
	gettimeofday(&t1, &tz);
	int num_frames = 0;

	// main draw loop
	bool quit = false;
	while (!quit) {

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
			printf("fill rate: %f MiB/s\n", (num_frames * WIDTH * HEIGHT * 4)/ (dt * 1024. * 1024.));
			num_frames = 0;
			t1 = t2;
		}
	}


	//  cleaning up...
	eglDestroyContext(egl_display, egl_context);
	eglDestroySurface(egl_display, egl_surface);
	eglTerminate(egl_display);
	XDestroyWindow(x_display, win);
	XCloseDisplay(x_display);

	return 0;
}
