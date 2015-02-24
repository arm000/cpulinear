#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <sys/time.h>
#include <math.h>
#include <unistd.h>
#include <libgen.h>
#include <getopt.h>

#ifdef __ANDROID__
#include <ui/DisplayInfo.h>
#include <gui/Surface.h>
#include <gui/SurfaceComposerClient.h>
#include <gui/ISurfaceComposer.h>
#include <android/native_window.h>
#else
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#endif

#include <GLES2/gl2.h>
#include <EGL/egl.h>

#include "256-1.h"
#include "256-2.h"
#include "256-3.h"
#include "256-4.h"

#include "512-1.h"
#include "512-2.h"
#include "512-3.h"
#include "512-4.h"


#ifdef __ANDROID__
using namespace android;

static sp<Surface> g_android_surface;
static sp<SurfaceControl> g_android_control;
static sp<SurfaceComposerClient> g_android_composer;
static sp<ANativeWindow> g_android_window;

typedef void Display;
typedef void *Window;
#endif

GLubyte *textures[4];

int width = 256;
int height = 256;

Display    *x_display;
Window      win;
EGLDisplay  egl_display;
EGLContext  egl_context;
EGLSurface  egl_surface;

GLint position_loc;
GLint texture_loc;
GLint sampler_loc;

GLuint texture_id;

bool update_pos = false;
float upload_dt = 0.;
unsigned int upload_size = 0;
int upload = 0;
int fillrate = 0;

GLfloat vertexArray[] = {
	-1.0, -1.0,  0.0, // bottom left
	 0.0,  1.0,
	-1.0,  1.0,  0.0, // top left
	 0.0,  0.0,
	 1.0,  1.0,  0.0, // top right
	 1.0,  0.0,
	 1.0, -1.0,  0.0, // bottom right
	 1.0,  1.0,
	-1.0, -1.0,  0.0,
	 0.0,  1.0
};

GLfloat vertexArray90[] = {
	-1.0, -1.0,  0.0, // bottom left
	 1.0,  1.0,
	-1.0,  1.0,  0.0, // top left
	 0.0,  1.0,
	 1.0,  1.0,  0.0, // top right
	 0.0,  0.0,
	 1.0, -1.0,  0.0, // bottom right
	 1.0,  0.0,
	-1.0, -1.0,  0.0,
	 1.0,  1.0
};

GLfloat vertexArray180[] = {
	-1.0, -1.0,  0.0, // bottom left
	 1.0,  0.0,
	-1.0,  1.0,  0.0, // top left
	 1.0,  1.0,
	 1.0,  1.0,  0.0, // top right
	 0.0,  1.0,
	 1.0, -1.0,  0.0, // bottom right
	 0.0,  0.0,
	-1.0, -1.0,  0.0,
	 1.0,  0.0
};

GLfloat vertexArray270[] = {
	-1.0, -1.0,  0.0, // bottom left
	 0.0,  0.0,
	-1.0,  1.0,  0.0, // top left
	 1.0,  0.0,
	 1.0,  1.0,  0.0, // top right
	 1.0,  1.0,
	 1.0, -1.0,  0.0, // bottom right
	 0.0,  1.0,
	-1.0, -1.0,  0.0,
	 0.0,  0.0
};

GLfloat *vtx = &vertexArray[0];
GLfloat *tex = &vertexArray[3];

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

static void print_shader_info_log(GLuint shader)
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


static GLuint load_shader(const char *shader_source, GLenum type)
{
	GLuint shader = glCreateShader(type);

	glShaderSource(shader, 1, &shader_source, NULL);
	glCompileShader(shader);

	print_shader_info_log(shader);

	return shader;
}

static GLuint upload_texture(void)
{
   // Texture object handle
   GLuint textureId;

   // Use tightly packed data
   glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

   // Generate a texture object
   glGenTextures(1, &textureId);

   // Bind the texture object
   glBindTexture(GL_TEXTURE_2D, textureId);

   // Load the texture
   glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE,
		textures[0]);

   // Set the filtering mode
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

   return textureId;
}

static void render(void)
{
	static int donesetup = 0;
#ifndef ANDROID
	static XWindowAttributes gwa;
#endif

	// draw
	if (!donesetup) {
#ifndef ANDROID
		XWindowAttributes gwa;
		XGetWindowAttributes(x_display, win, &gwa);
		glViewport(0, 0, gwa.width, gwa.height);
#endif
		glClearColor(0.08, 0.06, 0.07, 1.);    // background color
		donesetup = 1;
	}

	glVertexAttribPointer(position_loc, 3, GL_FLOAT, GL_FALSE,
			      5 * sizeof (GLfloat), vtx);
	glEnableVertexAttribArray(position_loc);

	glVertexAttribPointer(texture_loc, 2, GL_FLOAT, GL_FALSE,
			      5 * sizeof (GLfloat), tex);
	glEnableVertexAttribArray(texture_loc);

	// Bind the texture
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, texture_id);

	if (upload) {
		static int i=0;
		struct timezone tz;
		static struct timeval t1, t2;

		gettimeofday(&t1, &tz);

		// Load the texture
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE,
			     textures[i]);
		gettimeofday(&t2, &tz);
		upload_dt += t2.tv_sec - t1.tv_sec + (t2.tv_usec - t1.tv_usec) * 1e-6;
		upload_size += width * height * 4;

		i++;
		i = i % 4;
	}

	// Set the sampler texture unit to 0
	glUniform1i(sampler_loc, 0);

	glDrawArrays(GL_TRIANGLE_STRIP, 0, 5);

	{
		struct timezone tz;
		struct timeval t1, t2;
		gettimeofday(&t1, &tz);
		eglSwapBuffers(egl_display, egl_surface);
		gettimeofday(&t2, &tz);
		float dt = t2.tv_sec*1000. - t1.tv_sec*1000. + (t2.tv_usec - t1.tv_usec) * 1e-3;
		printf("eglSwapBuffers took %fms\n", dt);
	}
}


/////////////////////////////////////////////////////////////////////


int main(int argc, char **argv)
{
	int c;
	int help = 0;

	while (1) {
		int option_index = 0;
		struct option long_options[] = {
			{"help",     no_argument,       &help,      1 },
			{"upload",   no_argument,       &upload,    1 },
			{"fillrate", no_argument,       &fillrate,  1 },
			{"rotate",   required_argument, 0,          0 },
			{"size",     required_argument, 0,          0 },
			{0,          0,                 0,          0 }
		};

		c = getopt_long_only(argc, argv, "", long_options,
				     &option_index);
		if (c == -1)
			break;

		switch (c) {
		case 0:
			if (strcmp(long_options[option_index].name, "rotate") == 0) {
				int rot = atoi(optarg);
				switch(rot) {
				case 90:
					vtx = &vertexArray90[0];
					tex = &vertexArray90[3];
					break;
				case 180:
					vtx = &vertexArray180[0];
					tex = &vertexArray180[3];
					break;
				case 270:
					vtx = &vertexArray270[0];
					tex = &vertexArray270[3];
					break;
				default:
					printf("invalid rotation, must be one of: 90, 180, 270\n");
					exit(1);
					break;
				}
			}
			else if (strcmp(long_options[option_index].name, "size") == 0) {
				int size = atoi(optarg);
				switch(size) {
				case 256:
				case 512:
					width = height = size;
					break;
				default:
					printf("invalid size, must be one of: 256, 512\n");
					exit(1);
					break;
				}
			}
			break;

		case '?':
			help = 1;
			break;

		default:
			printf("?? getopt returned character code 0%o ??\n", c);
		}
	}

	if (help || ((fillrate ^ upload) == 0)) {
		printf("usage: %s: [ --rotate 90|180|270 ] [ --size 256|512 ] [--fillrate|--upload]\n", basename(argv[0]));
		exit(0);
	}

#ifdef __ANDROID__
	g_android_composer = new SurfaceComposerClient;
	DisplayInfo dpy_info;
	g_android_composer->getDisplayInfo(
		g_android_composer->getBuiltInDisplay(ISurfaceComposer::eDisplayIdMain),
		&dpy_info);
//	g_width = dpy_info.w;
//	g_height = dpy_info.h;
	g_android_control =
		g_android_composer->createSurface(String8("glbench"),
						  width, height,
						  HAL_PIXEL_FORMAT_RGBA_8888, 0);
//	CHECK(g_android_control != NULL);
	SurfaceComposerClient::openGlobalTransaction();
	g_android_control->setLayer(200000);
	g_android_control->setPosition(0, 0);
	g_android_control->show();
	SurfaceComposerClient::closeGlobalTransaction();
	g_android_surface = g_android_control->getSurface();
	g_android_window = g_android_surface.get();
	win = g_android_window.get();
//	CHECK(g_xlib_window);
	egl_display = eglGetDisplay(NULL);
	if (egl_display == EGL_NO_DISPLAY) {
		fprintf(stderr, "Got no EGL display.\n");
		return 1;
	}
//	GetXVisual();
#else
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
		0, 0, width, height, 0,
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
	XStoreName(x_display, win, basename(argv[0])); // give the window a name

	egl_display = eglGetDisplay((EGLNativeDisplayType) x_display);
	if (egl_display == EGL_NO_DISPLAY) {
		fprintf(stderr, "Got no EGL display.\n");
		return 1;
	}
#endif

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

	egl_surface = eglCreateWindowSurface(egl_display, ecfg,
					     (EGLNativeWindowType)win, NULL);
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
//	eglSwapInterval(egl_display, 0);


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

	// prepare the textures
	if (width == 512 && height == 512) {
		textures[0] = gimp_image_512_1.pixel_data;
		textures[1] = gimp_image_512_2.pixel_data;
		textures[2] = gimp_image_512_3.pixel_data;
		textures[3] = gimp_image_512_4.pixel_data;
	} else if (width == 256 && height == 256) {
		textures[0] = gimp_image_256_1.pixel_data;
		textures[1] = gimp_image_256_2.pixel_data;
		textures[2] = gimp_image_256_3.pixel_data;
		textures[3] = gimp_image_256_4.pixel_data;
	} else {
		printf("unknown width/height (%d/%d)\n", width, height);
		exit(1);
	}

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

#ifndef ANDROID
		// check for events from the x-server
		while (XPending(x_display)) {
			XEvent  xev;
			XNextEvent(x_display, &xev);

			if (xev.type == KeyPress)
				quit = true;
		}
#endif

		render();   // now we finally put something on the screen

		if (++num_frames % 100 == 0) {
			gettimeofday(&t2, &tz);
			float dt = t2.tv_sec - t1.tv_sec + (t2.tv_usec - t1.tv_usec) * 1e-6;
			printf("fps: %f\n", num_frames / dt);
			if (fillrate) {
				printf("fill rate: %f MiB/s\n", (num_frames * width * height * 4)/ (dt * 1024. * 1024.));
			}
			if (upload) {
				printf("texture upload rate: %f MiB/s\n", (upload_size) / (upload_dt * 1024. * 1024.));
			}
			num_frames = 0;
			upload_dt = 0.;
			upload_size = 0;
			t1 = t2;
		}
	}


	//  cleaning up...
	eglDestroyContext(egl_display, egl_context);
	eglDestroySurface(egl_display, egl_surface);
	eglTerminate(egl_display);
#ifndef ANDROID
	XDestroyWindow(x_display, win);
	XCloseDisplay(x_display);
#endif

	return 0;
}
