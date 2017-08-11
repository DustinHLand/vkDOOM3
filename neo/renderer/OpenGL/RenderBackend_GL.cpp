/*
===========================================================================

Doom 3 BFG Edition GPL Source Code
Copyright (C) 1993-2012 id Software LLC, a ZeniMax Media company. 
Copyright (C) 2016-2017 Dustin Land

This file is part of the Doom 3 BFG Edition GPL Source Code ("Doom 3 BFG Edition Source Code").  

Doom 3 BFG Edition Source Code is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Doom 3 BFG Edition Source Code is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Doom 3 BFG Edition Source Code.  If not, see <http://www.gnu.org/licenses/>.

In addition, the Doom 3 BFG Edition Source Code is also subject to certain additional terms. You should have received a copy of these additional terms immediately following the terms and conditions of the GNU General Public License which accompanied the Doom 3 BFG Edition Source Code.  If not, please request a copy in writing from id Software at the address below.

If you have questions concerning this license or the applicable additional terms, you may contact in writing id Software LLC, c/o ZeniMax Media Inc., Suite 120, Rockville, Maryland 20850 USA.

===========================================================================
*/

#pragma hdrstop
#include "../../idlib/precompiled.h"
#include "../../framework/Common_local.h"
#include "../../sys/win32/rc/doom_resource.h"
#include "../RenderSystem_local.h"
#include "../RenderBackend.h"
#include "../ResolutionScale.h"
#include "../RenderProgs.h"
#include "../RenderLog.h"
#include "../GLState.h"
#include "../GLMatrix.h"
#include "../Image.h"

#include "../../sys/win32/win_local.h"

glContext_t glcontext;

idCVar r_glDriver( "r_glDriver", "", CVAR_RENDERER, "\"opengl32\", etc." );
idCVar r_useOpenGL32( "r_useOpenGL32", "1", CVAR_INTEGER, "0 = OpenGL 2.0, 1 = OpenGL 3.2 compatibility profile, 2 = OpenGL 3.2 core profile", 0, 2 );

extern idCVar r_windowX;
extern idCVar r_windowY;
extern idCVar r_windowWidth;
extern idCVar r_windowHeight;
extern idCVar r_customWidth;
extern idCVar r_customHeight;
extern idCVar r_vidMode;
extern idCVar r_displayRefresh;
extern idCVar r_fullscreen;
extern idCVar r_swapInterval;

extern idCVar r_forceZPassStencilShadows;
extern idCVar r_lightScale;
extern idCVar r_brightness;
extern idCVar r_gamma;
extern idCVar r_multiSamples;
extern idCVar r_lodBias;
extern idCVar r_syncEveryFrame;
extern idCVar r_offsetFactor;
extern idCVar r_offsetUnits;
extern idCVar r_shadowPolygonFactor;
extern idCVar r_shadowPolygonOffset;
extern idCVar r_singleTriangle;
extern idCVar r_maxAnisotropicFiltering;
extern idCVar r_debugContext;
extern idCVar r_ignoreGLErrors;

extern idCVar r_showShadows;
extern idCVar r_showSwapBuffers;

extern idCVar r_useLightDepthBounds;
extern idCVar r_useStencilShadowPreload;
extern idCVar r_useLightStencilSelect;
extern idCVar r_useScissor;
extern idCVar r_useShadowDepthBounds;
extern idCVar r_useTrilinearFiltering;

extern idCVar r_skipShaderPasses;
extern idCVar r_skipShadows;

void PrintState( uint64 stateBits, uint64 * stencilBits );

static int		swapIndex;		// 0 or 1 into renderSync
static GLsync	renderSync[2];

BOOL  ( WINAPI * qwglSwapBuffers)(HDC);
HGLRC ( WINAPI * qwglCreateContext)(HDC);
BOOL  ( WINAPI * qwglDeleteContext)(HGLRC);
PROC  ( WINAPI * qwglGetProcAddress)(LPCSTR);
BOOL  ( WINAPI * qwglMakeCurrent)(HDC, HGLRC);

void ( APIENTRY * qglArrayElement )(GLint i);
void ( APIENTRY * qglBegin )(GLenum mode);
void ( APIENTRY * qglBindTexture )(GLenum target, GLuint texture);
void ( APIENTRY * qglBlendFunc )(GLenum sfactor, GLenum dfactor);
void ( APIENTRY * qglClear )(GLbitfield mask);
void ( APIENTRY * qglClearColor )(GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha);
void ( APIENTRY * qglClearDepth )(GLclampd depth);
void ( APIENTRY * qglClearStencil )(GLint s);
void ( APIENTRY * qglColor3fv )(const GLfloat *v);
void ( APIENTRY * qglColor4fv )(const GLfloat *v);
void ( APIENTRY * qglColor4ubv )(const GLubyte *v);
void ( APIENTRY * qglColorMask )(GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha);
void ( APIENTRY * qglCopyTexImage2D )(GLenum target, GLint level, GLenum internalFormat, GLint x, GLint y, GLsizei width, GLsizei height, GLint border);
void ( APIENTRY * qglCullFace )(GLenum mode);
void ( APIENTRY * qglDeleteTextures )(GLsizei n, const GLuint *textures);
void ( APIENTRY * qglDepthFunc )(GLenum func);
void ( APIENTRY * qglDepthMask )(GLboolean flag);
void ( APIENTRY * qglDisable )(GLenum cap);
void ( APIENTRY * qglDrawBuffer )(GLenum mode);
void ( APIENTRY * qglDrawPixels )(GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid *pixels);
void ( APIENTRY * qglEnable )(GLenum cap);
void ( APIENTRY * qglEnd )(void);
void ( APIENTRY * qglFinish )(void);
void ( APIENTRY * qglFlush )(void);
void ( APIENTRY * qglGenTextures )(GLsizei n, GLuint *textures);
GLenum ( APIENTRY * qglGetError )(void);
void ( APIENTRY * qglGetFloatv )(GLenum pname, GLfloat *params);
void ( APIENTRY * qglGetIntegerv )(GLenum pname, GLint *params);
const GLubyte * ( APIENTRY * qglGetString )(GLenum name);
void ( APIENTRY * qglLineWidth )(GLfloat width);
void ( APIENTRY * qglLoadIdentity )(void);
void ( APIENTRY * qglLoadMatrixf )(const GLfloat *m);
void ( APIENTRY * qglMatrixMode )(GLenum mode);
void ( APIENTRY * qglOrtho )(GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble zNear, GLdouble zFar);
void ( APIENTRY * qglPixelStorei )(GLenum pname, GLint param);
void ( APIENTRY * qglPolygonMode )(GLenum face, GLenum mode);
void ( APIENTRY * qglPolygonOffset )(GLfloat factor, GLfloat units);
void ( APIENTRY * qglPopAttrib )(void);
void ( APIENTRY * qglPopMatrix )(void);
void ( APIENTRY * qglPushAttrib )(GLbitfield mask);
void ( APIENTRY * qglPushMatrix )(void);
void ( APIENTRY * qglRasterPos2f )(GLfloat x, GLfloat y);
void ( APIENTRY * qglReadBuffer )(GLenum mode);
void ( APIENTRY * qglReadPixels )(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, GLvoid *pixels);
void ( APIENTRY * qglScissor )(GLint x, GLint y, GLsizei width, GLsizei height);
void ( APIENTRY * qglShadeModel )(GLenum mode);
void ( APIENTRY * qglStencilFunc )(GLenum func, GLint ref, GLuint mask);
void ( APIENTRY * qglStencilOp )(GLenum fail, GLenum zfail, GLenum zpass);
void ( APIENTRY * qglTexImage2D )(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const GLvoid *pixels);
void ( APIENTRY * qglTexParameterf )(GLenum target, GLenum pname, GLfloat param);
void ( APIENTRY * qglTexParameterfv )(GLenum target, GLenum pname, const GLfloat *params);
void ( APIENTRY * qglTexParameteri )(GLenum target, GLenum pname, GLint param);
void ( APIENTRY * qglTexSubImage2D )(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid *pixels);
void ( APIENTRY * qglVertex3f )(GLfloat x, GLfloat y, GLfloat z);
void ( APIENTRY * qglVertex3fv )(const GLfloat *v);
void ( APIENTRY * qglVertexPointer )(GLint size, GLenum type, GLsizei stride, const GLvoid *pointer);
void ( APIENTRY * qglViewport )(GLint x, GLint y, GLsizei width, GLsizei height);

// WGL_ARB_extensions_string
PFNWGLGETEXTENSIONSSTRINGARBPROC		wglGetExtensionsStringARB;

// WGL_EXT_swap_interval
PFNWGLSWAPINTERVALEXTPROC				wglSwapIntervalEXT;

// WGL_ARB_pixel_format
PFNWGLCHOOSEPIXELFORMATARBPROC			wglChoosePixelFormatARB;

// WGL_ARB_create_context
PFNWGLCREATECONTEXTATTRIBSARBPROC		wglCreateContextAttribsARB;

// GL_ARB_multitexture
PFNGLACTIVETEXTUREPROC					qglActiveTextureARB;

// GL_EXT_direct_state_access
PFNGLBINDMULTITEXTUREEXTPROC			qglBindMultiTextureEXT;

// GL_ARB_texture_compression
PFNGLCOMPRESSEDTEXIMAGE2DARBPROC		qglCompressedTexImage2DARB;
PFNGLCOMPRESSEDTEXSUBIMAGE2DARBPROC		qglCompressedTexSubImage2DARB;

// GL_ARB_vertex_buffer_object
PFNGLBINDBUFFERARBPROC					qglBindBufferARB;
PFNGLBINDBUFFERRANGEPROC				qglBindBufferRange;
PFNGLDELETEBUFFERSARBPROC				qglDeleteBuffersARB;
PFNGLGENBUFFERSARBPROC					qglGenBuffersARB;
PFNGLBUFFERDATAARBPROC					qglBufferDataARB;
PFNGLBUFFERSUBDATAARBPROC				qglBufferSubDataARB;
PFNGLMAPBUFFERARBPROC					qglMapBufferARB;
PFNGLUNMAPBUFFERARBPROC					qglUnmapBufferARB;

// GL_ARB_map_buffer_range
PFNGLMAPBUFFERRANGEPROC					qglMapBufferRange;

// GL_ARB_draw_elements_base_vertex
PFNGLDRAWELEMENTSBASEVERTEXPROC  		qglDrawElementsBaseVertex;

// GL_ARB_vertex_array_object
PFNGLGENVERTEXARRAYSPROC				qglGenVertexArrays;
PFNGLBINDVERTEXARRAYPROC				qglBindVertexArray;

// GL_ARB_vertex_program / GL_ARB_fragment_program
PFNGLVERTEXATTRIBPOINTERARBPROC			qglVertexAttribPointerARB;
PFNGLENABLEVERTEXATTRIBARRAYARBPROC		qglEnableVertexAttribArrayARB;
PFNGLDISABLEVERTEXATTRIBARRAYARBPROC	qglDisableVertexAttribArrayARB;
PFNGLPROGRAMSTRINGARBPROC				qglProgramStringARB;
PFNGLBINDPROGRAMARBPROC					qglBindProgramARB;
PFNGLGENPROGRAMSARBPROC					qglGenProgramsARB;
PFNGLDELETEPROGRAMSARBPROC				qglDeleteProgramsARB;

// GLSL / OpenGL 2.0
PFNGLCREATESHADERPROC					qglCreateShader;
PFNGLDELETESHADERPROC					qglDeleteShader;
PFNGLSHADERSOURCEPROC					qglShaderSource;
PFNGLCOMPILESHADERPROC					qglCompileShader;
PFNGLGETSHADERIVPROC					qglGetShaderiv;
PFNGLGETSHADERINFOLOGPROC				qglGetShaderInfoLog;
PFNGLCREATEPROGRAMPROC					qglCreateProgram;
PFNGLDELETEPROGRAMPROC					qglDeleteProgram;
PFNGLATTACHSHADERPROC					qglAttachShader;
PFNGLLINKPROGRAMPROC					qglLinkProgram;
PFNGLUSEPROGRAMPROC						qglUseProgram;
PFNGLGETPROGRAMIVPROC					qglGetProgramiv;
PFNGLGETPROGRAMINFOLOGPROC				qglGetProgramInfoLog;
PFNGLBINDATTRIBLOCATIONPROC				qglBindAttribLocation;
PFNGLGETUNIFORMLOCATIONPROC				qglGetUniformLocation;
PFNGLUNIFORM1IPROC						qglUniform1i;
PFNGLUNIFORM4FVPROC						qglUniform4fv;

// GL_ARB_uniform_buffer_object
PFNGLGETUNIFORMBLOCKINDEXPROC			qglGetUniformBlockIndex;
PFNGLUNIFORMBLOCKBINDINGPROC			qglUniformBlockBinding;

// GL_ATI_separate_stencil / OpenGL 2.0
PFNGLSTENCILOPSEPARATEATIPROC			qglStencilOpSeparate;

// GL_EXT_depth_bounds_test
PFNGLDEPTHBOUNDSEXTPROC                 qglDepthBoundsEXT;

// GL_ARB_sync
PFNGLFENCESYNCPROC						qglFenceSync;
PFNGLISSYNCPROC							qglIsSync;
PFNGLCLIENTWAITSYNCPROC					qglClientWaitSync;
PFNGLDELETESYNCPROC						qglDeleteSync;

// GL_ARB_occlusion_query
PFNGLGENQUERIESARBPROC					qglGenQueriesARB;
PFNGLISQUERYARBPROC						qglIsQueryARB;
PFNGLBEGINQUERYARBPROC					qglBeginQueryARB;
PFNGLENDQUERYARBPROC					qglEndQueryARB;

// GL_ARB_timer_query / GL_EXT_timer_query
PFNGLGETQUERYOBJECTUI64VEXTPROC			qglGetQueryObjectui64vEXT;

// GL_ARB_debug_output
PFNGLDEBUGMESSAGECONTROLARBPROC			qglDebugMessageControlARB;
PFNGLDEBUGMESSAGECALLBACKARBPROC		qglDebugMessageCallbackARB;

PFNGLGETSTRINGIPROC						qglGetStringi;

/*
=================
GL_CheckExtension
=================
*/
bool GL_CheckExtension( const char * name, const char * extensions_string ) {
	if ( !strstr( extensions_string, name ) ) {
		idLib::Printf( "X..%s not found\n", name );
		return false;
	}

	idLib::Printf( "...using %s\n", name );
	return true;
}

/*
=============================================================================

WglExtension Grabbing

This is gross -- creating a window just to get a context to get the wgl extensions

=============================================================================
*/

/*
====================
FakeWndProc

Only used to get wglExtensions
====================
*/
LONG WINAPI FakeWndProc (
    HWND    hWnd,
    UINT    uMsg,
    WPARAM  wParam,
    LPARAM  lParam) {

	if ( uMsg == WM_DESTROY ) {
        PostQuitMessage(0);
	}

	if ( uMsg != WM_CREATE ) {
	    return DefWindowProc(hWnd, uMsg, wParam, lParam);
	}

	const static PIXELFORMATDESCRIPTOR pfd = {
		sizeof(PIXELFORMATDESCRIPTOR),
		1,
		PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER,
		PFD_TYPE_RGBA,
		24,
		0, 0, 0, 0, 0, 0,
		8, 0,
		0, 0, 0, 0,
		24, 8,
		0,
		PFD_MAIN_PLANE,
		0,
		0,
		0,
		0,
	};
	int		pixelFormat;
	HDC hDC;
	HGLRC hGLRC;

    hDC = GetDC(hWnd);

    // Set up OpenGL
    pixelFormat = ChoosePixelFormat(hDC, &pfd);
    SetPixelFormat(hDC, pixelFormat, &pfd);
    hGLRC = qwglCreateContext(hDC);
    qwglMakeCurrent(hDC, hGLRC);

	// free things
    wglMakeCurrent(NULL, NULL);
    wglDeleteContext(hGLRC);
    ReleaseDC(hWnd, hDC);

    return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

/*
====================
CreateWindowClasses
====================
*/
void CreateWindowClasses() {
	WNDCLASS wc;

	//
	// register the window class if necessary
	//
	if ( win32.windowClassRegistered ) {
		return;
	}

	memset( &wc, 0, sizeof( wc ) );

	wc.style         = 0;
	wc.lpfnWndProc   = (WNDPROC) MainWndProc;
	wc.cbClsExtra    = 0;
	wc.cbWndExtra    = 0;
	wc.hInstance     = win32.hInstance;
	wc.hIcon         = LoadIcon( win32.hInstance, MAKEINTRESOURCE(IDI_ICON1));
	wc.hCursor       = NULL;
	wc.hbrBackground = (struct HBRUSH__ *)COLOR_GRAYTEXT;
	wc.lpszMenuName  = 0;
	wc.lpszClassName = WIN32_WINDOW_CLASS_NAME;

	if ( !RegisterClass( &wc ) ) {
		common->FatalError( "CreateGameWindow: could not register window class" );
	}
	idLib::Printf( "...registered window class\n" );

	// now register the fake window class that is only used
	// to get wgl extensions
	wc.style         = 0;
	wc.lpfnWndProc   = (WNDPROC) FakeWndProc;
	wc.cbClsExtra    = 0;
	wc.cbWndExtra    = 0;
	wc.hInstance     = win32.hInstance;
	wc.hIcon         = LoadIcon( win32.hInstance, MAKEINTRESOURCE(IDI_ICON1));
	wc.hCursor       = LoadCursor (NULL,IDC_ARROW);
	wc.hbrBackground = (struct HBRUSH__ *)COLOR_GRAYTEXT;
	wc.lpszMenuName  = 0;
	wc.lpszClassName = WIN32_FAKE_WINDOW_CLASS_NAME;

	if ( !RegisterClass( &wc ) ) {
		common->FatalError( "CreateGameWindow: could not register window class" );
	}
	idLib::Printf( "...registered fake window class\n" );

	win32.windowClassRegistered = true;
}

/*
========================
CreateOpenGLContextOnDC
========================
*/
static HGLRC CreateOpenGLContextOnDC( const HDC hdc, const bool debugContext ) {
	int useOpenGL32 = r_useOpenGL32.GetInteger();
	HGLRC m_hrc = NULL;

	for ( int i = 0; i < 2; i++ ) {
		const int glMajorVersion = ( useOpenGL32 != 0 ) ? 3 : 2;
		const int glMinorVersion = ( useOpenGL32 != 0 ) ? 2 : 0;
		const int glDebugFlag = debugContext ? WGL_CONTEXT_DEBUG_BIT_ARB : 0;
		const int glProfileMask = ( useOpenGL32 != 0 ) ? WGL_CONTEXT_PROFILE_MASK_ARB : 0;
		const int glProfile = ( useOpenGL32 == 1 ) ? WGL_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB : ( ( useOpenGL32 == 2 ) ? WGL_CONTEXT_CORE_PROFILE_BIT_ARB : 0 );
		const int attribs[] =
		{
			WGL_CONTEXT_MAJOR_VERSION_ARB,	glMajorVersion,
			WGL_CONTEXT_MINOR_VERSION_ARB,	glMinorVersion,
			WGL_CONTEXT_FLAGS_ARB,			glDebugFlag,
			glProfileMask,					glProfile,
			0
		};

		m_hrc = wglCreateContextAttribsARB( hdc, 0, attribs );
		if ( m_hrc != NULL ) {
			idLib::Printf( "created OpenGL %d.%d context\n", glMajorVersion, glMinorVersion );
			break;
		}

		idLib::Printf( "failed to create OpenGL %d.%d context\n", glMajorVersion, glMinorVersion );
		useOpenGL32 = 0;	// fall back to OpenGL 2.0
	}

	if ( m_hrc == NULL ) {
		int	err = GetLastError();
		switch( err ) {
			case ERROR_INVALID_VERSION_ARB: idLib::Printf( "ERROR_INVALID_VERSION_ARB\n" ); break;
			case ERROR_INVALID_PROFILE_ARB: idLib::Printf( "ERROR_INVALID_PROFILE_ARB\n" ); break;
			default: idLib::Printf( "unknown error: 0x%x\n", err ); break;
		}
	}

	return m_hrc;
}

/*
====================
GLW_ChoosePixelFormat

Returns -1 on failure, or a pixel format
====================
*/
static int GLW_ChoosePixelFormat( const HDC hdc, const int multisamples ) {
	FLOAT	fAttributes[] = { 0, 0 };
	int		iAttributes[] = {
		WGL_SAMPLE_BUFFERS_ARB, ( ( multisamples > 1 ) ? 1 : 0 ),
		WGL_SAMPLES_ARB, multisamples,
		WGL_DOUBLE_BUFFER_ARB, TRUE,
		WGL_STENCIL_BITS_ARB, 8,
		WGL_DEPTH_BITS_ARB, 24,
		WGL_RED_BITS_ARB, 8,
		WGL_BLUE_BITS_ARB, 8,
		WGL_GREEN_BITS_ARB, 8,
		WGL_ALPHA_BITS_ARB, 8,
		0, 0
	};

	int	pixelFormat;
	UINT numFormats;
	if ( !wglChoosePixelFormatARB( hdc, iAttributes, fAttributes, 1, &pixelFormat, &numFormats ) ) {
		return -1;
	}
	return pixelFormat;
}

/*
====================
GLW_InitDriver

Set the pixelformat for the window before it is
shown, and create the rendering context
====================
*/
static bool GLW_InitDriver( gfxImpParms_t parms ) {
    PIXELFORMATDESCRIPTOR src = 
	{
		sizeof(PIXELFORMATDESCRIPTOR),	// size of this pfd
		1,								// version number
		PFD_DRAW_TO_WINDOW |			// support window
		PFD_SUPPORT_OPENGL |			// support OpenGL
		PFD_DOUBLEBUFFER,				// double buffered
		PFD_TYPE_RGBA,					// RGBA type
		32,								// 32-bit color depth
		0, 0, 0, 0, 0, 0,				// color bits ignored
		8,								// 8 bit destination alpha
		0,								// shift bit ignored
		0,								// no accumulation buffer
		0, 0, 0, 0, 					// accum bits ignored
		24,								// 24-bit z-buffer	
		8,								// 8-bit stencil buffer
		0,								// no auxiliary buffer
		PFD_MAIN_PLANE,					// main layer
		0,								// reserved
		0, 0, 0							// layer masks ignored
    };

	idLib::Printf( "Initializing OpenGL driver\n" );

	//
	// get a DC for our window if we don't already have one allocated
	//
	if ( win32.hDC == NULL ) {
		idLib::Printf( "...getting DC: " );

		if ( ( win32.hDC = GetDC( win32.hWnd ) ) == NULL ) {
			idLib::Printf( "^3failed^0\n" );
			return false;
		}
		idLib::Printf( "succeeded\n" );
	}

	// the multisample path uses the wgl 
	if ( wglChoosePixelFormatARB ) {
		win32.pixelformat = GLW_ChoosePixelFormat( win32.hDC, parms.multiSamples );
	} else {
		// this is the "classic" choose pixel format path
		idLib::Printf( "Using classic ChoosePixelFormat\n" );

		//
		// choose, set, and describe our desired pixel format.  If we're
		// using a minidriver then we need to bypass the GDI functions,
		// otherwise use the GDI functions.
		//
		if ( ( win32.pixelformat = ChoosePixelFormat( win32.hDC, &src ) ) == 0 ) {
			idLib::Printf( "...^3GLW_ChoosePFD failed^0\n");
			return false;
		}
		idLib::Printf( "...PIXELFORMAT %d selected\n", win32.pixelformat );
	}

	// get the full info
	DescribePixelFormat( win32.hDC, win32.pixelformat, sizeof( win32.pfd ), &win32.pfd );

	// the same SetPixelFormat is used either way
	if ( SetPixelFormat( win32.hDC, win32.pixelformat, &win32.pfd ) == FALSE ) {
		idLib::Printf( "...^3SetPixelFormat failed^0\n", win32.hDC );
		return false;
	}

	//
	// startup the OpenGL subsystem by creating a context and making it current
	//
	idLib::Printf( "...creating GL context: " );
	win32.hGLRC = CreateOpenGLContextOnDC( win32.hDC, r_debugContext.GetBool() );
	if ( win32.hGLRC == 0 ) {
		idLib::Printf( "^3failed^0\n" );
		return false;
	}
	idLib::Printf( "succeeded\n" );

	idLib::Printf( "...making context current: " );
	if ( !qwglMakeCurrent( win32.hDC, win32.hGLRC ) ) {
		qwglDeleteContext( win32.hGLRC );
		win32.hGLRC = NULL;
		idLib::Printf( "^3failed^0\n" );
		return false;
	}
	idLib::Printf( "succeeded\n" );

	return true;
}

/*
==================
GLW_GetWGLExtensionsWithFakeWindow
==================
*/
static void GLW_GetWGLExtensionsWithFakeWindow() {
    HWND    hWnd;
    MSG		msg;

    // Create a window for the sole purpose of getting
    // a valid context to get the wglextensions
    hWnd = CreateWindow(WIN32_FAKE_WINDOW_CLASS_NAME, GAME_NAME,
        WS_OVERLAPPEDWINDOW,
        40, 40,
        640,
        480,
        NULL, NULL, win32.hInstance, NULL );
    if ( !hWnd ) {
        common->FatalError( "GLW_GetWGLExtensionsWithFakeWindow: Couldn't create fake window" );
    }

    HDC hDC = GetDC( hWnd );
    HGLRC gRC = wglCreateContext( hDC );
    wglMakeCurrent( hDC, gRC );
    
	// WGL_ARB_pixel_format
    wglChoosePixelFormatARB = (PFNWGLCHOOSEPIXELFORMATARBPROC)GL_ExtensionPointer("wglChoosePixelFormatARB");

    // wglCreateContextAttribsARB
    wglCreateContextAttribsARB = (PFNWGLCREATECONTEXTATTRIBSARBPROC)qwglGetProcAddress( "wglCreateContextAttribsARB" );

    wglDeleteContext( gRC );
    ReleaseDC( hWnd, hDC );

    DestroyWindow( hWnd );
    while ( GetMessage( &msg, NULL, 0, 0 ) ) {
        TranslateMessage( &msg );
        DispatchMessage( &msg );
    }
}

/*
===================
GL_Init

This is the platform specific OpenGL initialization function.  It
is responsible for loading OpenGL, initializing it,
creating a window of the appropriate size, doing
fullscreen manipulations, etc.  Its overall responsibility is
to make sure that a functional OpenGL subsystem is operating
when it returns to the ref.

If there is any failure, the renderer will revert back to safe
parameters and try again.
===================
*/
gfxImpParms_t R_GetModeParms();
bool GL_Init() {
	gfxImpParms_t parms = R_GetModeParms();

	idLib::Printf( "Initializing OpenGL subsystem with multisamples:%i fullscreen:%i\n", 
		parms.multiSamples, parms.fullScreen );

	// check our desktop attributes
	{
		HDC hDC = GetDC( GetDesktopWindow() );
		win32.desktopBitsPixel = GetDeviceCaps( hDC, BITSPIXEL );
		win32.desktopWidth = GetDeviceCaps( hDC, HORZRES );
		win32.desktopHeight = GetDeviceCaps( hDC, VERTRES );
		ReleaseDC( GetDesktopWindow(), hDC );
	}

	// we can't run in a window unless it is 32 bpp
	if ( win32.desktopBitsPixel < 32 && parms.fullScreen <= 0 ) {
		idLib::Printf("^3Windowed mode requires 32 bit desktop depth^0\n");
		return false;
	}

	// save the hardware gamma so it can be
	// restored on exit
	{
		HDC hDC = GetDC( GetDesktopWindow() );
		BOOL success = GetDeviceGammaRamp( hDC, win32.oldHardwareGamma );
		idLib::Printf( "...getting default gamma ramp: %s\n", success ? "success" : "failed" );
		ReleaseDC( GetDesktopWindow(), hDC );
	}

	// create our window classes if we haven't already
	CreateWindowClasses();

	// this will load the dll and set all our qgl* function pointers,
	// but doesn't create a window

	// r_glDriver is only intended for using instrumented OpenGL
	// dlls.  Normal users should never have to use it, and it is
	// not archived.
	const char * driverName = r_glDriver.GetString()[0] ? r_glDriver.GetString() : "opengl32";
	assert( win32.hinstOpenGL == 0 );

	idLib::Printf( "...initializing QGL\n" );
	idLib::Printf( "...calling LoadLibrary( '%s' ): ", driverName );

	if ( ( win32.hinstOpenGL = LoadLibrary( driverName ) ) == 0 ) {
		idLib::Printf( "failed\n" );
		return false;
	}

	idLib::Printf( "succeeded\n" );

	qglArrayElement     = glArrayElement;
	qglBegin            = glBegin;
	qglBindTexture      = glBindTexture;
	qglBlendFunc        = glBlendFunc;
	qglClear            = glClear;
	qglClearColor       = glClearColor;
	qglClearDepth       = glClearDepth;
	qglClearStencil     = glClearStencil;
	qglColor3fv         = glColor3fv;
	qglColor4fv         = glColor4fv;
	qglColor4ubv        = glColor4ubv;
	qglColorMask        = glColorMask;
	qglCopyTexImage2D	= glCopyTexImage2D;
	qglCullFace         = glCullFace;
	qglDeleteTextures   = glDeleteTextures;
	qglDepthFunc        = glDepthFunc;
	qglDepthMask        = glDepthMask;
	qglDisable          = glDisable;
	qglDrawBuffer       = glDrawBuffer;
	qglDrawPixels       = glDrawPixels;
	qglEnable           = glEnable;
	qglEnd              = glEnd;
	qglFinish           = glFinish;
	qglFlush            = glFlush;
	qglGenTextures      = glGenTextures;
	qglGetError         = ( GLenum (__stdcall * )(void) ) glGetError;
	qglGetFloatv        = glGetFloatv;
	qglGetIntegerv      = glGetIntegerv;
	qglGetString        = glGetString;
	qglLineWidth		= glLineWidth;
	qglLoadIdentity     = glLoadIdentity;
	qglLoadMatrixf      = glLoadMatrixf;
	qglMatrixMode       = glMatrixMode;
	qglOrtho            = glOrtho;
	qglPixelStorei      = glPixelStorei;
	qglPolygonMode      = glPolygonMode;
	qglPolygonOffset    = glPolygonOffset;
	qglPopAttrib        = glPopAttrib;
	qglPopMatrix        = glPopMatrix;
	qglPushAttrib       = glPushAttrib;
	qglPushMatrix       = glPushMatrix;
	qglRasterPos2f      = glRasterPos2f;
	qglReadBuffer       = glReadBuffer;
	qglReadPixels       = glReadPixels;
	qglScissor          = glScissor;
	qglShadeModel       = glShadeModel;
	qglStencilFunc      = glStencilFunc;
	qglStencilOp        = glStencilOp;
	qglTexImage2D       = glTexImage2D;
	qglTexParameterf    = glTexParameterf;
	qglTexParameterfv   = glTexParameterfv;
	qglTexParameteri    = glTexParameteri;
	qglTexSubImage2D    = glTexSubImage2D;
	qglVertex3f         = glVertex3f;
	qglVertex3fv        = glVertex3fv;
	qglVertexPointer    = glVertexPointer;
	qglViewport         = glViewport;

	qwglCreateContext   = wglCreateContext;
	qwglDeleteContext   = wglDeleteContext;
	qwglGetProcAddress  = wglGetProcAddress;
	qwglMakeCurrent     = wglMakeCurrent;

	qwglSwapBuffers     = SwapBuffers;

    // getting the wgl extensions involves creating a fake window to get a context,
    // which is pretty disgusting, and seems to mess with the AGP VAR allocation
    GLW_GetWGLExtensionsWithFakeWindow();

	void GL_Shutdown();

	// Optionally ChangeDisplaySettings to get a different fullscreen resolution.
	if ( !ChangeDisplaySettingsIfNeeded( parms ) ) {
		GL_Shutdown();
		return false;
	}

	// try to create a window with the correct pixel format
	if ( !CreateGameWindow( parms ) ) {
		GL_Shutdown();
		return false;
	}

	// init the renderer context
	if ( !GLW_InitDriver( parms ) ) {
		GL_Shutdown();
		return false;
	}

	win32.isFullscreen = parms.fullScreen;
	win32.nativeScreenWidth = parms.width;
	win32.nativeScreenHeight = parms.height;
	win32.multisamples = parms.multiSamples;
	win32.pixelAspect = 1.0f;	// FIXME: some monitor modes may be distorted

	return true;
}

/*
===================
GL_Shutdown

Destroys the rendering context, closes the window, resets the resolution, and resets the gamma ramps.
===================
*/
void GL_Shutdown() {
	const char *success[] = { "failed", "success" };
	int retVal;

	idLib::Printf( "Shutting down OpenGL subsystem\n" );

	// set current context to NULL
	if ( qwglMakeCurrent ) {
		retVal = qwglMakeCurrent( NULL, NULL ) != 0;
		idLib::Printf( "...wglMakeCurrent( NULL, NULL ): %s\n", success[retVal] );
	}

	// delete HGLRC
	if ( win32.hGLRC ) {
		retVal = qwglDeleteContext( win32.hGLRC ) != 0;
		idLib::Printf( "...deleting GL context: %s\n", success[retVal] );
		win32.hGLRC = NULL;
	}

	// release DC
	if ( win32.hDC ) {
		retVal = ReleaseDC( win32.hWnd, win32.hDC ) != 0;
		idLib::Printf( "...releasing DC: %s\n", success[retVal] );
		win32.hDC   = NULL;
	}

	// destroy window
	if ( win32.hWnd ) {
		idLib::Printf( "...destroying window\n" );
		ShowWindow( win32.hWnd, SW_HIDE );
		DestroyWindow( win32.hWnd );
		win32.hWnd = NULL;
	}

	// reset display settings
	if ( win32.cdsFullscreen ) {
		idLib::Printf( "...resetting display\n" );
		ChangeDisplaySettings( 0, 0 );
		win32.cdsFullscreen = 0;
	}

	// close the thread so the handle doesn't dangle
	if ( win32.renderThreadHandle ) {
		idLib::Printf( "...closing smp thread\n" );
		CloseHandle( win32.renderThreadHandle );
		win32.renderThreadHandle = NULL;
	}

	// restore gamma
	// if we never read in a reasonable looking table, don't write it out
	if ( win32.oldHardwareGamma[0][255] != 0 ) {
		HDC hDC = GetDC( GetDesktopWindow() );
		retVal = SetDeviceGammaRamp( hDC, win32.oldHardwareGamma );
		idLib::Printf( "...restoring hardware gamma: %s\n", success[retVal] );
		ReleaseDC( GetDesktopWindow(), hDC );
	}

	/*
	** GL_Shutdown
	**
	** Unloads the specified DLL then nulls out all the proc pointers.  This
	** is only called during a hard shutdown of the OGL subsystem (e.g. vid_restart).
	*/

	idLib::Printf( "...shutting down QGL\n" );

	if ( win32.hinstOpenGL )
	{
		idLib::Printf( "...unloading OpenGL DLL\n" );
		FreeLibrary( win32.hinstOpenGL );
	}

	win32.hinstOpenGL = NULL;

	qglArrayElement              = NULL;
	qglBegin                     = NULL;
	qglBindTexture               = NULL;
	qglBlendFunc                 = NULL;
	qglClear                     = NULL;
	qglClearColor                = NULL;
	qglClearDepth                = NULL;
	qglClearStencil              = NULL;
	qglColor3fv                  = NULL;
	qglColor4fv                  = NULL;
	qglColor4ubv                 = NULL;
	qglColorMask                 = NULL;
	qglCopyTexImage2D            = NULL;
	qglCullFace                  = NULL;
	qglDeleteTextures            = NULL;
	qglDepthFunc                 = NULL;
	qglDepthMask                 = NULL;
	qglDisable                   = NULL;
	qglDrawBuffer                = NULL;
	qglDrawPixels                = NULL;
	qglEnable                    = NULL;
	qglEnd                       = NULL;
	qglFinish                    = NULL;
	qglFlush                     = NULL;
	qglGenTextures               = NULL;
	qglGetError                  = NULL;
	qglGetFloatv                 = NULL;
	qglGetIntegerv               = NULL;
	qglGetString                 = NULL;
	qglLineWidth                 = NULL;
	qglLoadIdentity              = NULL;
	qglLoadMatrixf               = NULL;
	qglMatrixMode                = NULL;
	qglOrtho                     = NULL;
	qglPixelStorei               = NULL;
	qglPolygonMode               = NULL;
	qglPolygonOffset             = NULL;
	qglPopAttrib                 = NULL;
	qglPopMatrix                 = NULL;
	qglPushAttrib                = NULL;
	qglPushMatrix                = NULL;
	qglRasterPos2f               = NULL;
	qglReadBuffer                = NULL;
	qglReadPixels                = NULL;
	qglScissor                   = NULL;
	qglShadeModel                = NULL;
	qglStencilFunc               = NULL;
	qglStencilOp                 = NULL;
	qglTexImage2D                = NULL;
	qglTexParameterf             = NULL;
	qglTexParameterfv            = NULL;
	qglTexParameteri             = NULL;
	qglTexSubImage2D             = NULL;
	qglVertex3f                  = NULL;
	qglVertex3fv                 = NULL;
	qglVertexPointer             = NULL;
	qglViewport                  = NULL;

	qwglCreateContext            = NULL;
	qwglDeleteContext            = NULL;
	qwglGetProcAddress           = NULL;
	qwglMakeCurrent              = NULL;

	qwglSwapBuffers              = NULL;
}

/*
===================
GL_ExtensionPointer

Returns a function pointer for an OpenGL extension entry point
===================
*/
GLExtension_t GL_ExtensionPointer( const char *name ) {
	void	(*proc)();

	proc = (GLExtension_t)qwglGetProcAddress( name );

	if ( !proc ) {
		idLib::Printf( "Couldn't find proc address for: %s\n", name );
	}

	return proc;
}

/*
==================
GL_CheckErrors
==================
*/
void GL_CheckErrors() {
    int		err;
    char	s[64];
	int		i;

	// check for up to 10 errors pending
	for ( i = 0 ; i < 10 ; i++ ) {
		err = qglGetError();
		if ( err == GL_NO_ERROR ) {
			return;
		}
		switch( err ) {
			case GL_INVALID_ENUM:
				strcpy( s, "GL_INVALID_ENUM" );
				break;
			case GL_INVALID_VALUE:
				strcpy( s, "GL_INVALID_VALUE" );
				break;
			case GL_INVALID_OPERATION:
				strcpy( s, "GL_INVALID_OPERATION" );
				break;
			case GL_STACK_OVERFLOW:
				strcpy( s, "GL_STACK_OVERFLOW" );
				break;
			case GL_STACK_UNDERFLOW:
				strcpy( s, "GL_STACK_UNDERFLOW" );
				break;
			case GL_OUT_OF_MEMORY:
				strcpy( s, "GL_OUT_OF_MEMORY" );
				break;
			default:
				idStr::snPrintf( s, sizeof(s), "%i", err);
				break;
		}

		if ( !r_ignoreGLErrors.GetBool() ) {
			idLib::Printf( "GL_CheckErrors: %s\n", s );
		}
	}
}

/*
========================
glBindMultiTextureEXT

As of 2011/09/16 the Intel drivers for "Sandy Bridge" and "Ivy Bridge" integrated graphics do not support this extension.
========================
*/
void APIENTRY glBindMultiTextureEXT( GLenum texunit, GLenum target, GLuint texture ) {
	qglActiveTextureARB( texunit );
	qglBindTexture( target, texture );
}

/*
========================
DebugCallback

For ARB_debug_output
========================
*/
static void CALLBACK DebugCallback(unsigned int source, unsigned int type,
								   unsigned int id, unsigned int severity, int length, const char * message, void * userParam) {
	// it probably isn't safe to do an idLib::Printf at this point
	OutputDebugString( message );
	OutputDebugString( "\n" );
}

/*
==================
idRenderBackend::CheckCVars
==================
*/
void idRenderBackend::CheckCVars() {
	// gamma stuff
	if ( r_gamma.IsModified() || r_brightness.IsModified() ) {
		r_gamma.ClearModified();
		r_brightness.ClearModified();
		SetColorMappings();
	}

	// filtering
	if ( r_maxAnisotropicFiltering.IsModified() || r_useTrilinearFiltering.IsModified() || r_lodBias.IsModified() ) {
		idLib::Printf( "Updating texture filter parameters.\n" );
		r_maxAnisotropicFiltering.ClearModified();
		r_useTrilinearFiltering.ClearModified();
		r_lodBias.ClearModified();
	}

	if ( r_multiSamples.IsModified() ) {
		if ( r_multiSamples.GetInteger() > 0 ) {
			qglEnable( GL_MULTISAMPLE_ARB );
		} else {
			qglDisable( GL_MULTISAMPLE_ARB );
		}
	}
}

/*
====================
idRenderBackend::ResizeImages
====================
*/
void idRenderBackend::ResizeImages() {
	
}

/*
=============
GL_BlockingSwapBuffers

We want to exit this with the GPU idle, right at vsync
=============
*/
void idRenderBackend::BlockingSwapBuffers() {
    RENDERLOG_PRINTF( "***************** BlockingSwapBuffers *****************\n\n\n" );

	const int beforeSwap = Sys_Milliseconds();

	if ( r_swapInterval.IsModified() ) {
		r_swapInterval.ClearModified();

		int interval = 0;
		if ( r_swapInterval.GetInteger() == 1 ) {
			interval = ( m_swapControlTearAvailable ) ? -1 : 1;
		} else if ( r_swapInterval.GetInteger() == 2 ) {
			interval = 1;
		}

		if ( wglSwapIntervalEXT ) {
			wglSwapIntervalEXT( interval );
		}
	}

	qwglSwapBuffers( win32.hDC );

	const int beforeFence = Sys_Milliseconds();
	if ( r_showSwapBuffers.GetBool() && beforeFence - beforeSwap > 1 ) {
		idLib::Printf( "%i msec to swapBuffers\n", beforeFence - beforeSwap );
	}

	swapIndex ^= 1;

	if ( qglIsSync( renderSync[swapIndex] ) ) {
		qglDeleteSync( renderSync[swapIndex] );
	}
	// draw something tiny to ensure the sync is after the swap
	const int start = Sys_Milliseconds();
	qglScissor( 0, 0, 1, 1 );
	qglEnable( GL_SCISSOR_TEST );
	qglClear( GL_COLOR_BUFFER_BIT );
	renderSync[swapIndex] = qglFenceSync( GL_SYNC_GPU_COMMANDS_COMPLETE, 0 );
	const int end = Sys_Milliseconds();
	if ( r_showSwapBuffers.GetBool() && end - start > 1 ) {
		idLib::Printf( "%i msec to start fence\n", end - start );
	}

	GLsync	syncToWaitOn;
	if ( r_syncEveryFrame.GetBool() ) {
		syncToWaitOn = renderSync[swapIndex];
	} else {
		syncToWaitOn = renderSync[!swapIndex];
	}

	if ( qglIsSync( syncToWaitOn ) ) {
		for ( GLenum r = GL_TIMEOUT_EXPIRED; r == GL_TIMEOUT_EXPIRED; ) {
			r = qglClientWaitSync( syncToWaitOn, GL_SYNC_FLUSH_COMMANDS_BIT, 1000 * 1000 );
		}
	}

	const int afterFence = Sys_Milliseconds();
	if ( r_showSwapBuffers.GetBool() && afterFence - beforeFence > 1 ) {
		idLib::Printf( "%i msec to wait on fence\n", afterFence - beforeFence );
	}

	const int64 exitBlockTime = Sys_Microseconds();

	static int64 prevBlockTime;
	if ( r_showSwapBuffers.GetBool() && prevBlockTime ) {
		const int delta = (int) ( exitBlockTime - prevBlockTime );
		idLib::Printf( "blockToBlock: %i\n", delta );
	}
	prevBlockTime = exitBlockTime;

	// check for dynamic changes that require some initialization
	CheckCVars();

	GL_CheckErrors();
}

/*
=============
idRenderBackend::idRenderBackend
=============
*/
idRenderBackend::idRenderBackend() {
	memset( m_gammaTable, 0, sizeof( m_gammaTable ) );

	glcontext.bAnisotropicFilterAvailable = false;
	glcontext.bTextureLODBiasAvailable = false;
	
	memset( glcontext.tmu, 0, sizeof( glcontext.tmu ) );
	memset( glcontext.stencilOperations, 0, sizeof( glcontext.stencilOperations ) );
}

/*
=============
idRenderBackend::~idRenderBackend
=============
*/
idRenderBackend::~idRenderBackend() {

}

/*
=============
idRenderBackend::Print
=============
*/
void idRenderBackend::Print() {
	idLib::Printf( "CPU: %s\n", Sys_GetProcessorString() );

	const char *fsstrings[] =
	{
		"windowed",
		"fullscreen"
	};

	idLib::Printf( "\nGL_VENDOR: %s\n", m_vendorString.c_str() );
	idLib::Printf( "GL_RENDERER: %s\n", m_rendererString.c_str() );
	idLib::Printf( "GL_VERSION: %s\n", m_versionString.c_str() );
	idLib::Printf( "GL_EXTENSIONS: %s\n", m_extensionsString.c_str() );
	if ( !m_wglExtensionsString.IsEmpty() ) {
		idLib::Printf( "WGL_EXTENSIONS: %s\n", m_wglExtensionsString.c_str() );
	}
	idLib::Printf( "GL_MAX_TEXTURE_SIZE: %d\n", m_maxTextureSize );
	idLib::Printf( "GL_MAX_TEXTURE_COORDS_ARB: %d\n", m_maxTextureCoords );
	idLib::Printf( "GL_MAX_TEXTURE_IMAGE_UNITS_ARB: %d\n", m_maxTextureImageUnits );

	// print all the display adapters, monitors, and video modes
	void DumpAllDisplayDevices();
	DumpAllDisplayDevices();

	idLib::Printf( "\nPIXELFORMAT: color(%d-bits) Z(%d-bit) stencil(%d-bits)\n", m_colorBits, m_depthBits, m_stencilBits );
	idLib::Printf( "MODE: %d, %d x %d %s hz:", r_vidMode.GetInteger(), renderSystem->GetWidth(), renderSystem->GetHeight(), fsstrings[r_fullscreen.GetBool()] );
	if ( m_displayFrequency ) {
		idLib::Printf( "%d\n", m_displayFrequency );
	} else {
		idLib::Printf( "N/A\n" );
	}

	idLib::Printf( "-------\n" );

	// WGL_EXT_swap_interval
	typedef BOOL (WINAPI * PFNWGLSWAPINTERVALEXTPROC) (int interval);
	extern	PFNWGLSWAPINTERVALEXTPROC wglSwapIntervalEXT;

	if ( r_swapInterval.GetInteger() && wglSwapIntervalEXT != NULL ) {
		idLib::Printf( "Forcing swapInterval %i\n", r_swapInterval.GetInteger() );
	} else {
		idLib::Printf( "swapInterval not forced\n" );
	}

	idLib::Printf( "%i multisamples\n", win32.multisamples );
}

/*
=============
idRenderBackend::Init

This function is responsible for initializing a valid OpenGL subsystem
for rendering.  This is done by calling the system specific GL_Init,
which gives us a working OGL subsystem, then setting all necessary openGL
state, including images, vertex programs, and display lists.

Changes to the vertex cache size or smp state require a vid_restart.

If R_IsInitialized() is false, no rendering can take place, but
all renderSystem functions will still operate properly, notably the material
and model information functions.
=============
*/
void idRenderBackend::Init() {
	idLib::Printf( "----- idRenderBackend::Init -----\n" );

	// create the context as well as setting up the window
	if ( !GL_Init() ) {
		idLib::FatalError( "Unable to initialize OpenGL" );
	}

	m_colorBits = win32.pfd.cColorBits;
	m_depthBits = win32.pfd.cDepthBits;
	m_stencilBits = win32.pfd.cStencilBits;

	// input and sound systems need to be tied to the new window
	Sys_InitInput();

	// get our config strings
	m_vendorString = (const char *)qglGetString( GL_VENDOR );
	m_rendererString = (const char *)qglGetString( GL_RENDERER );
	m_versionString = (const char *)qglGetString( GL_VERSION );
	m_shadingLanguageString = (const char *)qglGetString( GL_SHADING_LANGUAGE_VERSION );
	m_extensionsString = (const char *)qglGetString( GL_EXTENSIONS );

	if ( m_extensionsString.IsEmpty() ) {
		// As of OpenGL 3.2, glGetStringi is requir	ed to obtain the available extensions
		qglGetStringi = (PFNGLGETSTRINGIPROC)GL_ExtensionPointer( "glGetStringi" );

		// Build the extensions string
		GLint numExtensions;
		qglGetIntegerv( GL_NUM_EXTENSIONS, &numExtensions );
		idStr extensions_string;
		for ( int i = 0; i < numExtensions; i++ ) {
			extensions_string.Append( (const char*)qglGetStringi( GL_EXTENSIONS, i ) );
			// the now deprecated glGetString method usaed to create a single string with each extension separated by a space
			if ( i < numExtensions - 1 ) {
				extensions_string.Append( ' ' );
			}
		}
		m_extensionsString = extensions_string;
	}

	float glVersion = atof( m_versionString.c_str() );
	float glslVersion = atof( m_shadingLanguageString.c_str() );
	idLib::Printf( "OpenGL Version: %3.1f\n", glVersion );
	idLib::Printf( "OpenGL Vendor : %s\n", m_vendorString.c_str() );
	idLib::Printf( "OpenGL GLSL   : %3.1f\n", glslVersion );

	// OpenGL driver constants
	GLint temp;
	qglGetIntegerv( GL_MAX_TEXTURE_SIZE, &temp );
	m_maxTextureSize = temp;

	// stubbed or broken drivers may have reported 0...
	if ( m_maxTextureSize <= 0 ) {
		m_maxTextureSize = 256;
	}

	// Get both portable and platform extension pointers.
	{
		m_glVersion = atof( m_versionString.c_str() );
		const char * badVideoCard = idLocalization::GetString( "#str_06780" );
		if ( m_glVersion < 2.0f ) {
			idLib::FatalError( badVideoCard );
		}

		if ( m_rendererString.Icmpn( "ATI", 4 ) == 0 || m_rendererString.Icmpn( "AMD", 4 ) == 0 ) {
			m_vendor = VENDOR_AMD;
		} else if ( m_rendererString.Icmpn( "NVIDIA", 6 ) == 0 ) {
			m_vendor = VENDOR_NVIDIA;
		} else if ( m_rendererString.Icmpn( "Intel", 5 ) == 0 ) {
			m_vendor = VENDOR_INTEL;
		}

#if defined( ID_PC_WIN )
		wglGetExtensionsStringARB = (PFNWGLGETEXTENSIONSSTRINGARBPROC)GL_ExtensionPointer( "wglGetExtensionsStringARB" );
		if ( wglGetExtensionsStringARB ) {
			m_wglExtensionsString = (const char *) wglGetExtensionsStringARB( win32.hDC );
		} else {
			m_wglExtensionsString = "";
		}

		// WGL_EXT_swap_control
		wglSwapIntervalEXT = (PFNWGLSWAPINTERVALEXTPROC) GL_ExtensionPointer( "wglSwapIntervalEXT" );
		r_swapInterval.SetModified();	// force a set next frame

		// WGL_EXT_swap_control_tear
		m_swapControlTearAvailable = GL_CheckExtension( "WGL_EXT_swap_control_tear", m_wglExtensionsString.c_str() );

		// WGL_ARB_pixel_format
		wglChoosePixelFormatARB = (PFNWGLCHOOSEPIXELFORMATARBPROC)GL_ExtensionPointer("wglChoosePixelFormatARB");

		// wglCreateContextAttribsARB
		wglCreateContextAttribsARB = (PFNWGLCREATECONTEXTATTRIBSARBPROC)qwglGetProcAddress( "wglCreateContextAttribsARB" );
#endif

		// GL_ARB_multitexture
		if ( GL_CheckExtension( "GL_ARB_multitexture", m_extensionsString.c_str() ) ) {
			qglActiveTextureARB = (void(APIENTRY *)(GLenum))GL_ExtensionPointer( "glActiveTextureARB" );
		} else {
			idLib::Error( "GL_ARB_multitexture not available" );
		}

		// GL_EXT_direct_state_access
		if ( GL_CheckExtension( "GL_EXT_direct_state_access", m_extensionsString.c_str() ) ) {
			qglBindMultiTextureEXT = (PFNGLBINDMULTITEXTUREEXTPROC)GL_ExtensionPointer( "glBindMultiTextureEXT" );
		} else {
			qglBindMultiTextureEXT = glBindMultiTextureEXT;
		}

		// GL_ARB_texture_compression + GL_S3_s3tc
		// DRI drivers may have GL_ARB_texture_compression but no GL_EXT_texture_compression_s3tc
		if ( GL_CheckExtension( "GL_ARB_texture_compression", m_extensionsString.c_str() ) 
			&& GL_CheckExtension( "GL_EXT_texture_compression_s3tc", m_extensionsString.c_str() ) ) {
			qglCompressedTexImage2DARB = (PFNGLCOMPRESSEDTEXIMAGE2DARBPROC)GL_ExtensionPointer( "glCompressedTexImage2DARB" );
			qglCompressedTexSubImage2DARB = (PFNGLCOMPRESSEDTEXSUBIMAGE2DARBPROC)GL_ExtensionPointer( "glCompressedTexSubImage2DARB" );
		} else {
			idLib::Error( "GL_ARB_texture_compression or GL_EXT_texture_compression_s3tc not available" );
		}

		// GL_EXT_texture_filter_anisotropic
		glcontext.bAnisotropicFilterAvailable = GL_CheckExtension( "GL_EXT_texture_filter_anisotropic", m_extensionsString.c_str() );
		if ( glcontext.bAnisotropicFilterAvailable ) {
			qglGetFloatv( GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &glcontext.maxTextureAnisotropy );
			idLib::Printf( "   maxTextureAnisotropy: %f\n", glcontext.maxTextureAnisotropy );
		} else {
			glcontext.maxTextureAnisotropy = 1;
		}

		// GL_EXT_texture_lod_bias
		// The actual extension is broken as specificed, storing the state in the texture unit instead
		// of the texture object.  The behavior in GL 1.4 is the behavior we use.
		glcontext.bTextureLODBiasAvailable = ( m_glVersion >= 1.4 || GL_CheckExtension( "GL_EXT_texture_lod_bias", m_extensionsString.c_str() ) );
		if ( glcontext.bTextureLODBiasAvailable ) {
			idLib::Printf( "...using %s\n", "GL_EXT_texture_lod_bias" );
		} else {
			idLib::Printf( "X..%s not found\n", "GL_EXT_texture_lod_bias" );
		}

		// GL_ARB_vertex_buffer_object
		if ( GL_CheckExtension( "GL_ARB_vertex_buffer_object", m_extensionsString.c_str() ) ) {
			qglBindBufferARB = (PFNGLBINDBUFFERARBPROC)GL_ExtensionPointer( "glBindBufferARB" );
			qglBindBufferRange = (PFNGLBINDBUFFERRANGEPROC)GL_ExtensionPointer( "glBindBufferRange" );
			qglDeleteBuffersARB = (PFNGLDELETEBUFFERSARBPROC)GL_ExtensionPointer( "glDeleteBuffersARB" );
			qglGenBuffersARB = (PFNGLGENBUFFERSARBPROC)GL_ExtensionPointer( "glGenBuffersARB" );
			qglBufferDataARB = (PFNGLBUFFERDATAARBPROC)GL_ExtensionPointer( "glBufferDataARB" );
			qglBufferSubDataARB = (PFNGLBUFFERSUBDATAARBPROC)GL_ExtensionPointer( "glBufferSubDataARB" );
			qglMapBufferARB = (PFNGLMAPBUFFERARBPROC)GL_ExtensionPointer( "glMapBufferARB" );
			qglUnmapBufferARB = (PFNGLUNMAPBUFFERARBPROC)GL_ExtensionPointer( "glUnmapBufferARB" );
		} else {
			idLib::Error( "GL_ARB_vertex_buffer_object not available" );
		}

		// GL_ARB_map_buffer_range, map a section of a buffer object's data store
		if ( GL_CheckExtension( "GL_ARB_map_buffer_range", m_extensionsString.c_str() ) ) {
			qglMapBufferRange = (PFNGLMAPBUFFERRANGEPROC)GL_ExtensionPointer( "glMapBufferRange" );
		} else {
			idLib::Error( "GL_ARB_map_buffer_range not available" );
		}

		// GL_ARB_vertex_array_object
		if ( GL_CheckExtension( "GL_ARB_vertex_array_object", m_extensionsString.c_str() ) ) {
			qglGenVertexArrays = (PFNGLGENVERTEXARRAYSPROC)GL_ExtensionPointer( "glGenVertexArrays" );
			qglBindVertexArray = (PFNGLBINDVERTEXARRAYPROC)GL_ExtensionPointer( "glBindVertexArray" );
		} else {
			idLib::Error( "GL_ARB_vertex_array_object not available" );
		}

		// GL_ARB_draw_elements_base_vertex
		if ( GL_CheckExtension( "GL_ARB_draw_elements_base_vertex", m_extensionsString.c_str() ) ) {
			qglDrawElementsBaseVertex = (PFNGLDRAWELEMENTSBASEVERTEXPROC)GL_ExtensionPointer( "glDrawElementsBaseVertex" );
		} else {
			idLib::Error( "GL_ARB_draw_elements_base_vertex not available" );
		}

		// GLSL, core in OpenGL > 2.0
		if ( m_glVersion >= 2.0f ) {
			qglCreateShader = (PFNGLCREATESHADERPROC)GL_ExtensionPointer( "glCreateShader" );
			qglDeleteShader = (PFNGLDELETESHADERPROC)GL_ExtensionPointer( "glDeleteShader" );
			qglShaderSource = (PFNGLSHADERSOURCEPROC)GL_ExtensionPointer( "glShaderSource" );
			qglCompileShader = (PFNGLCOMPILESHADERPROC)GL_ExtensionPointer( "glCompileShader" );
			qglGetShaderiv = (PFNGLGETSHADERIVPROC)GL_ExtensionPointer( "glGetShaderiv" );
			qglGetShaderInfoLog = (PFNGLGETSHADERINFOLOGPROC)GL_ExtensionPointer( "glGetShaderInfoLog" );
			qglCreateProgram = (PFNGLCREATEPROGRAMPROC)GL_ExtensionPointer( "glCreateProgram" );
			qglDeleteProgram = (PFNGLDELETEPROGRAMPROC)GL_ExtensionPointer( "glDeleteProgram" );
			qglAttachShader = (PFNGLATTACHSHADERPROC)GL_ExtensionPointer( "glAttachShader" );
			qglLinkProgram = (PFNGLLINKPROGRAMPROC)GL_ExtensionPointer( "glLinkProgram" );
			qglUseProgram = (PFNGLUSEPROGRAMPROC)GL_ExtensionPointer( "glUseProgram" );
			qglGetProgramiv = (PFNGLGETPROGRAMIVPROC)GL_ExtensionPointer( "glGetProgramiv" );
			qglGetProgramInfoLog = (PFNGLGETPROGRAMINFOLOGPROC)GL_ExtensionPointer( "glGetProgramInfoLog" );
			qglBindAttribLocation = (PFNGLBINDATTRIBLOCATIONPROC)GL_ExtensionPointer( "glBindAttribLocation" );
			qglGetUniformLocation = (PFNGLGETUNIFORMLOCATIONPROC)GL_ExtensionPointer( "glGetUniformLocation" );
			qglUniform1i = (PFNGLUNIFORM1IPROC)GL_ExtensionPointer( "glUniform1i" );
			qglUniform4fv = (PFNGLUNIFORM4FVPROC)GL_ExtensionPointer( "glUniform4fv" );

			qglVertexAttribPointerARB = (PFNGLVERTEXATTRIBPOINTERARBPROC)GL_ExtensionPointer( "glVertexAttribPointerARB" );
			qglEnableVertexAttribArrayARB = (PFNGLENABLEVERTEXATTRIBARRAYARBPROC)GL_ExtensionPointer( "glEnableVertexAttribArrayARB" );
			qglDisableVertexAttribArrayARB = (PFNGLDISABLEVERTEXATTRIBARRAYARBPROC)GL_ExtensionPointer( "glDisableVertexAttribArrayARB" );
			qglProgramStringARB = (PFNGLPROGRAMSTRINGARBPROC)GL_ExtensionPointer( "glProgramStringARB" );
			qglBindProgramARB = (PFNGLBINDPROGRAMARBPROC)GL_ExtensionPointer( "glBindProgramARB" );
			qglGenProgramsARB = (PFNGLGENPROGRAMSARBPROC)GL_ExtensionPointer( "glGenProgramsARB" );
			qglDeleteProgramsARB = (PFNGLDELETEPROGRAMSARBPROC)GL_ExtensionPointer( "glDeleteProgramsARB" );

			qglGetIntegerv( GL_MAX_TEXTURE_COORDS_ARB, (GLint *)&m_maxTextureCoords );
			qglGetIntegerv( GL_MAX_TEXTURE_IMAGE_UNITS_ARB, (GLint *)&m_maxTextureImageUnits );
		} else {
			idLib::Error( "GLSL not available" );
		}

		// GL_ARB_uniform_buffer_object
		if ( GL_CheckExtension( "GL_ARB_uniform_buffer_object", m_extensionsString.c_str() ) ) {
			qglGetUniformBlockIndex = (PFNGLGETUNIFORMBLOCKINDEXPROC)GL_ExtensionPointer( "glGetUniformBlockIndex" );
			qglUniformBlockBinding = (PFNGLUNIFORMBLOCKBINDINGPROC)GL_ExtensionPointer( "glUniformBlockBinding" );

			qglGetIntegerv( GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT, (GLint *)&m_uniformBufferOffsetAlignment );
			if ( m_uniformBufferOffsetAlignment < 256 ) {
				m_uniformBufferOffsetAlignment = 256;
			}
		} else {
			idLib::Error( "GL_ARB_uniform_buffer_object not available" );
		}

		// ATI_separate_stencil / OpenGL 2.0 separate stencil
		if ( ( m_glVersion >= 2.0f ) || GL_CheckExtension( "GL_ATI_separate_stencil", m_extensionsString.c_str() ) ) {
			qglStencilOpSeparate = (PFNGLSTENCILOPSEPARATEATIPROC)GL_ExtensionPointer( "glStencilOpSeparate" );
		} else {
			idLib::Error( "GL_ATI_separate_stencil not available" );
		}

		// GL_EXT_depth_bounds_test
		m_depthBoundsTestAvailable = GL_CheckExtension( "GL_EXT_depth_bounds_test", m_extensionsString.c_str() );
		if ( m_depthBoundsTestAvailable ) {
 			qglDepthBoundsEXT = (PFNGLDEPTHBOUNDSEXTPROC)GL_ExtensionPointer( "glDepthBoundsEXT" );
 		}

		// GL_ARB_sync
		if ( GL_CheckExtension( "GL_ARB_sync", m_extensionsString.c_str() ) ) {
			qglFenceSync = (PFNGLFENCESYNCPROC)GL_ExtensionPointer( "glFenceSync" );
			qglIsSync = (PFNGLISSYNCPROC)GL_ExtensionPointer( "glIsSync" );
			qglClientWaitSync = (PFNGLCLIENTWAITSYNCPROC)GL_ExtensionPointer( "glClientWaitSync" );
			qglDeleteSync = (PFNGLDELETESYNCPROC)GL_ExtensionPointer( "glDeleteSync" );
		} else {
			idLib::Error( "GL_ARB_sync not available" );
		}

		// GL_ARB_occlusion_query
		if ( GL_CheckExtension( "GL_ARB_occlusion_query", m_extensionsString.c_str() ) ) {
			// defined in GL_ARB_occlusion_query, which is required for GL_EXT_timer_query
			qglGenQueriesARB = (PFNGLGENQUERIESARBPROC)GL_ExtensionPointer( "glGenQueriesARB" );
			qglIsQueryARB = (PFNGLISQUERYARBPROC)GL_ExtensionPointer( "glIsQueryARB" );
			qglBeginQueryARB = (PFNGLBEGINQUERYARBPROC)GL_ExtensionPointer( "glBeginQueryARB" );
			qglEndQueryARB = (PFNGLENDQUERYARBPROC)GL_ExtensionPointer( "glEndQueryARB" );
		} else {
			idLib::Error( "GL_ARB_occlusion_query not available" );
		}

		// GL_ARB_timer_query
		m_timerQueryAvailable = GL_CheckExtension( "GL_ARB_timer_query", m_extensionsString.c_str() ) || GL_CheckExtension( "GL_EXT_timer_query", m_extensionsString.c_str() );
		if ( m_timerQueryAvailable ) {
			qglGetQueryObjectui64vEXT = (PFNGLGETQUERYOBJECTUI64VEXTPROC)GL_ExtensionPointer( "glGetQueryObjectui64vARB" );
			if ( qglGetQueryObjectui64vEXT == NULL ) {
				qglGetQueryObjectui64vEXT = (PFNGLGETQUERYOBJECTUI64VEXTPROC)GL_ExtensionPointer( "glGetQueryObjectui64vEXT" );
			}
		}

		// GL_ARB_debug_output
		if ( GL_CheckExtension( "GL_ARB_debug_output", m_extensionsString.c_str() ) ) {
			qglDebugMessageControlARB   = (PFNGLDEBUGMESSAGECONTROLARBPROC)GL_ExtensionPointer( "glDebugMessageControlARB" );
			qglDebugMessageCallbackARB  = (PFNGLDEBUGMESSAGECALLBACKARBPROC)GL_ExtensionPointer( "glDebugMessageCallbackARB" );

			if ( r_debugContext.GetInteger() >= 1 ) {
				qglDebugMessageCallbackARB( DebugCallback, NULL );
			}
			if ( r_debugContext.GetInteger() >= 2 ) {
				// force everything to happen in the main thread instead of in a separate driver thread
				glEnable( GL_DEBUG_OUTPUT_SYNCHRONOUS_ARB );
			}
			if ( r_debugContext.GetInteger() >= 3 ) {
				// enable all the low priority messages
				qglDebugMessageControlARB( GL_DONT_CARE,
										GL_DONT_CARE,
										GL_DEBUG_SEVERITY_LOW_ARB,
										0, NULL, true );
			}
		}
	}

	renderProgManager.Init();

	// allocate the vertex array range or vertex objects
	vertexCache.Init( m_uniformBufferOffsetAlignment );

	// Reset our gamma
	SetColorMappings();

	static bool glCheck = false;
	if ( !glCheck && win32.osversion.dwMajorVersion == 6 ) {
		glCheck = true;
		if ( !m_vendorString.Icmp( "Microsoft" ) && idStr::FindText( m_rendererString.c_str(), "OpenGL-D3D" ) != -1 ) {
			if ( cvarSystem->GetCVarBool( "r_fullscreen" ) ) {
				cmdSystem->BufferCommandText( CMD_EXEC_NOW, "vid_restart partial windowed\n" );
				Sys_GrabMouseCursor( false );
			}
			int ret = MessageBox( NULL, "Please install OpenGL drivers from your graphics hardware vendor to run " GAME_NAME ".\nYour OpenGL functionality is limited.",
				"Insufficient OpenGL capabilities", MB_OKCANCEL | MB_ICONWARNING | MB_TASKMODAL );
			if ( ret == IDCANCEL ) {
				cmdSystem->BufferCommandText( CMD_EXEC_APPEND, "quit\n" );
				cmdSystem->ExecuteCommandBuffer();
			}
			if ( cvarSystem->GetCVarBool( "r_fullscreen" ) ) {
				cmdSystem->BufferCommandText( CMD_EXEC_APPEND, "vid_restart\n" );
			}
		}
	}

	GL_CheckErrors();
}

/*
=============
idRenderBackend::Shutdown
=============
*/
void idRenderBackend::Shutdown() {
	// Shutdown input
	Sys_ShutdownInput();

	renderProgManager.Shutdown();

	GL_Shutdown();
}

/*
=========================================================================================================

BACKEND COMMANDS

=========================================================================================================
*/

/*
=============
idRenderBackend::DrawElementsWithCounters
=============
*/
void idRenderBackend::DrawElementsWithCounters( const drawSurf_t * surf ) {
	// get vertex buffer
	const vertCacheHandle_t vbHandle = surf->ambientCache;
	idVertexBuffer * vertexBuffer;
	if ( vertexCache.CacheIsStatic( vbHandle ) ) {
		vertexBuffer = &vertexCache.m_staticData.vertexBuffer;
	} else {
		const uint64 frameNum = (int)( vbHandle >> VERTCACHE_FRAME_SHIFT ) & VERTCACHE_FRAME_MASK;
		if ( frameNum != ( ( vertexCache.m_currentFrame - 1 ) & VERTCACHE_FRAME_MASK ) ) {
			idLib::Warning( "idRenderBackend::DrawElementsWithCounters, vertexBuffer == NULL" );
			return;
		}
		vertexBuffer = &vertexCache.m_frameData[ vertexCache.m_drawListNum ].vertexBuffer;
	}
	const int vertOffset = (int)( vbHandle >> VERTCACHE_OFFSET_SHIFT ) & VERTCACHE_OFFSET_MASK;

	// get index buffer
	const vertCacheHandle_t ibHandle = surf->indexCache;
	idIndexBuffer * indexBuffer;
	if ( vertexCache.CacheIsStatic( ibHandle ) ) {
		indexBuffer = &vertexCache.m_staticData.indexBuffer;
	} else {
		const uint64 frameNum = (int)( ibHandle >> VERTCACHE_FRAME_SHIFT ) & VERTCACHE_FRAME_MASK;
		if ( frameNum != ( ( vertexCache.m_currentFrame - 1 ) & VERTCACHE_FRAME_MASK ) ) {
			idLib::Warning( "idRenderBackend::DrawElementsWithCounters, indexBuffer == NULL" );
			return;
		}
		indexBuffer = &vertexCache.m_frameData[ vertexCache.m_drawListNum ].indexBuffer;
	}
	const int indexOffset = (int)( ibHandle >> VERTCACHE_OFFSET_SHIFT ) & VERTCACHE_OFFSET_MASK;

	RENDERLOG_PRINTF( "Binding Buffers(%d): %p:%i %p:%i\n", surf->numIndexes, vertexBuffer, vertOffset, indexBuffer, indexOffset );

	const renderProg_t & prog = renderProgManager.GetCurrentRenderProg();

	if ( surf->jointCache ) {
		assert( prog.usesJoints );
		if ( !prog.usesJoints ) {
			return;
		}
	} else {
		assert( !prog.usesJoints || prog.optionalSkinning );
		if ( prog.usesJoints && !prog.optionalSkinning ) {
			return;
		}
	}

	if ( surf->jointCache ) {
		idUniformBuffer jointBuffer;
		if ( !vertexCache.GetJointBuffer( surf->jointCache, &jointBuffer ) ) {
			idLib::Warning( "idRenderBackend::DrawElementsWithCounters, jointBuffer == NULL" );
			return;
		}
		assert( ( jointBuffer.GetOffset() & ( m_uniformBufferOffsetAlignment - 1 ) ) == 0 );

		qglBindBufferRange( GL_UNIFORM_BUFFER, 0, jointBuffer.GetAPIObject(), jointBuffer.GetOffset(), jointBuffer.GetSize() );
	}

	if ( m_currentIndexBuffer != indexBuffer->GetAPIObject() ) {
		qglBindBufferARB( GL_ELEMENT_ARRAY_BUFFER_ARB, indexBuffer->GetAPIObject() );
		m_currentIndexBuffer = indexBuffer->GetAPIObject();
	}

	if ( m_currentVertexBuffer != vertexBuffer->GetAPIObject() ) {
		qglBindBufferARB( GL_ARRAY_BUFFER_ARB, vertexBuffer->GetAPIObject() );
		m_currentVertexBuffer = vertexBuffer->GetAPIObject();
	}

	PrintState( m_glStateBits, glcontext.stencilOperations );
	renderProgManager.CommitCurrent( m_glStateBits );

	//idLib::Printf( "GL: indices=%d, index_offset=%d, vert_offset=%d\n", surf->numIndexes, indexOffset, vertOffset );

	qglDrawElementsBaseVertex( GL_TRIANGLES, 
							  r_singleTriangle.GetBool() ? 3 : surf->numIndexes,
							  GL_INDEX_TYPE,
							  (triIndex_t *)indexOffset,
							  vertOffset / sizeof ( idDrawVert ) );
}

/*
=========================================================================================================

GL COMMANDS

=========================================================================================================
*/

/*
==================
idRenderBackend::GL_StartFrame
==================
*/
void idRenderBackend::GL_StartFrame() {
	
}

/*
==================
idRenderBackend::GL_EndFrame
==================
*/
void idRenderBackend::GL_EndFrame() {
	// Fix for the steam overlay not showing up while in game without Shell/Debug/Console/Menu also rendering
	qglColorMask( 1, 1, 1, 1 );

	qglFlush();
}

/*
========================
idRenderBackend::GL_SetDefaultState

This should initialize all GL state that any part of the entire program
may touch, including the editor.
========================
*/
void idRenderBackend::GL_SetDefaultState() {
	RENDERLOG_PRINTF( "--- GL_SetDefaultState ---\n" );

	qglClearDepth( 1.0f );

	// make sure our GL state vector is set correctly
	memset( &glcontext.tmu, 0, sizeof( glcontext.tmu ) );
	m_currenttmu = 0;
	m_currentVertexBuffer = 0;
	m_currentIndexBuffer = 0;
	m_polyOfsScale = 0.0f;
	m_polyOfsBias = 0.0f;
	m_glStateBits = 0;

	GL_State( 0, true );

	// These are changed by GL_State
	/*qglColorMask( GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE );
	qglBlendFunc( GL_ONE, GL_ZERO );
	qglDepthMask( GL_TRUE );
	qglDepthFunc( GL_LESS );
	qglDisable( GL_STENCIL_TEST );
	qglDisable( GL_POLYGON_OFFSET_FILL );
	qglDisable( GL_POLYGON_OFFSET_LINE );
	qglPolygonMode( GL_FRONT_AND_BACK, GL_FILL );*/

	// These should never be changed
	qglShadeModel( GL_SMOOTH );
	qglDisable( GL_FRAMEBUFFER_SRGB );
	qglEnable( GL_DEPTH_TEST );
	//qglEnable( GL_BLEND );
	qglEnable( GL_SCISSOR_TEST );
	qglReadBuffer( GL_BACK );
	qglEnable( GL_TEXTURE_CUBE_MAP_SEAMLESS );

	if ( r_useScissor.GetBool() ) {
		qglScissor( 0, 0, renderSystem->GetWidth(), renderSystem->GetHeight() );
	}

	// If we have a stereo pixel format, this will draw to both
	// the back left and back right buffers, which will have a
	// performance penalty.
	qglDrawBuffer( GL_BACK );
}

/*
====================
idRenderBackend::GL_State

This routine is responsible for setting the most commonly changed state
====================
*/
void idRenderBackend::GL_State( uint64 stateBits, bool forceGlState ) {
	uint64 diff = stateBits ^ m_glStateBits;
	
	if ( forceGlState ) {
		// make sure everything is set all the time, so we
		// can see if our delta checking is screwing up
		diff = 0xFFFFFFFFFFFFFFFF;
	} else if ( diff == 0 ) {
		return;
	}

	//
	// culling
	//
	if ( diff & ( GLS_CULL_BITS ) ) {
		switch ( stateBits & GLS_CULL_BITS ) {
			case GLS_CULL_TWOSIDED: 
				qglDisable( GL_CULL_FACE );
				break;
			case GLS_CULL_BACKSIDED: 
				qglEnable( GL_CULL_FACE );
				if ( m_viewDef != NULL && m_viewDef->isMirror ) {
					stateBits |= GLS_MIRROR_VIEW;
					qglCullFace( GL_FRONT );
				} else {
					qglCullFace( GL_BACK );
				}
				break;
			case GLS_CULL_FRONTSIDED:
			default:
				qglEnable( GL_CULL_FACE );
				if ( m_viewDef != NULL && m_viewDef->isMirror ) {
					stateBits |= GLS_MIRROR_VIEW;
					qglCullFace( GL_BACK );
				} else {
					qglCullFace( GL_FRONT );
				}
				break;
		}
	}

	//
	// check depthFunc bits
	//
	if ( diff & GLS_DEPTHFUNC_BITS ) {
		switch ( stateBits & GLS_DEPTHFUNC_BITS ) {
			case GLS_DEPTHFUNC_EQUAL:	qglDepthFunc( GL_EQUAL ); break;
			case GLS_DEPTHFUNC_ALWAYS:	qglDepthFunc( GL_ALWAYS ); break;
			case GLS_DEPTHFUNC_LESS:	qglDepthFunc( GL_LEQUAL ); break;
			case GLS_DEPTHFUNC_GREATER:	qglDepthFunc( GL_GEQUAL ); break;
		}
	}

	//
	// check blend bits
	//
	if ( diff & ( GLS_SRCBLEND_BITS | GLS_DSTBLEND_BITS ) ) {
		GLenum srcFactor = GL_ONE;
		GLenum dstFactor = GL_ZERO;

		switch ( stateBits & GLS_SRCBLEND_BITS ) {
			case GLS_SRCBLEND_ZERO:					srcFactor = GL_ZERO; break;
			case GLS_SRCBLEND_ONE:					srcFactor = GL_ONE; break;
			case GLS_SRCBLEND_DST_COLOR:			srcFactor = GL_DST_COLOR; break;
			case GLS_SRCBLEND_ONE_MINUS_DST_COLOR:	srcFactor = GL_ONE_MINUS_DST_COLOR; break;
			case GLS_SRCBLEND_SRC_ALPHA:			srcFactor = GL_SRC_ALPHA; break;
			case GLS_SRCBLEND_ONE_MINUS_SRC_ALPHA:	srcFactor = GL_ONE_MINUS_SRC_ALPHA; break;
			case GLS_SRCBLEND_DST_ALPHA:			srcFactor = GL_DST_ALPHA; break;
			case GLS_SRCBLEND_ONE_MINUS_DST_ALPHA:	srcFactor = GL_ONE_MINUS_DST_ALPHA; break;
			default:
				assert( !"GL_State: invalid src blend state bits\n" );
				break;
		}

		switch ( stateBits & GLS_DSTBLEND_BITS ) {
			case GLS_DSTBLEND_ZERO:					dstFactor = GL_ZERO; break;
			case GLS_DSTBLEND_ONE:					dstFactor = GL_ONE; break;
			case GLS_DSTBLEND_SRC_COLOR:			dstFactor = GL_SRC_COLOR; break;
			case GLS_DSTBLEND_ONE_MINUS_SRC_COLOR:	dstFactor = GL_ONE_MINUS_SRC_COLOR; break;
			case GLS_DSTBLEND_SRC_ALPHA:			dstFactor = GL_SRC_ALPHA; break;
			case GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA:	dstFactor = GL_ONE_MINUS_SRC_ALPHA; break;
			case GLS_DSTBLEND_DST_ALPHA:			dstFactor = GL_DST_ALPHA; break;
			case GLS_DSTBLEND_ONE_MINUS_DST_ALPHA:  dstFactor = GL_ONE_MINUS_DST_ALPHA; break;
			default:
				assert( !"GL_State: invalid dst blend state bits\n" );
				break;
		}

		// Only actually update GL's blend func if blending is enabled.
		if ( srcFactor == GL_ONE && dstFactor == GL_ZERO ) {
			qglDisable( GL_BLEND );
		} else {
			qglEnable( GL_BLEND );
			qglBlendFunc( srcFactor, dstFactor );
		}
	}

	//
	// check depthmask
	//
	if ( diff & GLS_DEPTHMASK ) {
		if ( stateBits & GLS_DEPTHMASK ) {
			qglDepthMask( GL_FALSE );
		} else {
			qglDepthMask( GL_TRUE );
		}
	}

	//
	// check colormask
	//
	if ( diff & (GLS_REDMASK|GLS_GREENMASK|GLS_BLUEMASK|GLS_ALPHAMASK) ) {
		GLboolean r = ( stateBits & GLS_REDMASK ) ? GL_FALSE : GL_TRUE;
		GLboolean g = ( stateBits & GLS_GREENMASK ) ? GL_FALSE : GL_TRUE;
		GLboolean b = ( stateBits & GLS_BLUEMASK ) ? GL_FALSE : GL_TRUE;
		GLboolean a = ( stateBits & GLS_ALPHAMASK ) ? GL_FALSE : GL_TRUE;
		qglColorMask( r, g, b, a );
	}

	//
	// fill/line mode
	//
	if ( diff & GLS_POLYMODE_LINE ) {
		if ( stateBits & GLS_POLYMODE_LINE ) {
			qglPolygonMode( GL_FRONT_AND_BACK, GL_LINE );
		} else {
			qglPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
		}
	}

	//
	// polygon offset
	//
	if ( diff & GLS_POLYGON_OFFSET ) {
		if ( stateBits & GLS_POLYGON_OFFSET ) {
			qglPolygonOffset( m_polyOfsScale, m_polyOfsBias );
			qglEnable( GL_POLYGON_OFFSET_FILL );
			qglEnable( GL_POLYGON_OFFSET_LINE );
		} else {
			qglDisable( GL_POLYGON_OFFSET_FILL );
			qglDisable( GL_POLYGON_OFFSET_LINE );
		}
	}

	//
	// stencil
	//
	if ( diff & ( GLS_STENCIL_FUNC_BITS | GLS_STENCIL_OP_BITS ) ) {
		if ( ( stateBits & ( GLS_STENCIL_FUNC_BITS | GLS_STENCIL_OP_BITS ) ) != 0 ) {
			qglEnable( GL_STENCIL_TEST );
		} else {
			qglDisable( GL_STENCIL_TEST );
		}
	}
	if ( diff & ( GLS_STENCIL_FUNC_BITS | GLS_STENCIL_FUNC_REF_BITS | GLS_STENCIL_FUNC_MASK_BITS ) ) {
		GLuint ref = GLuint( ( stateBits & GLS_STENCIL_FUNC_REF_BITS ) >> GLS_STENCIL_FUNC_REF_SHIFT );
		GLuint mask = GLuint( ( stateBits & GLS_STENCIL_FUNC_MASK_BITS ) >> GLS_STENCIL_FUNC_MASK_SHIFT );
		GLenum func = 0;

		switch ( stateBits & GLS_STENCIL_FUNC_BITS ) {
			case GLS_STENCIL_FUNC_NEVER:		func = GL_NEVER; break;
			case GLS_STENCIL_FUNC_LESS:			func = GL_LESS; break;
			case GLS_STENCIL_FUNC_EQUAL:		func = GL_EQUAL; break;
			case GLS_STENCIL_FUNC_LEQUAL:		func = GL_LEQUAL; break;
			case GLS_STENCIL_FUNC_GREATER:		func = GL_GREATER; break;
			case GLS_STENCIL_FUNC_NOTEQUAL:		func = GL_NOTEQUAL; break;
			case GLS_STENCIL_FUNC_GEQUAL:		func = GL_GEQUAL; break;
			case GLS_STENCIL_FUNC_ALWAYS:		func = GL_ALWAYS; break;
		}
		qglStencilFunc( func, ref, mask );
	}
	if ( diff & ( GLS_STENCIL_OP_FAIL_BITS | GLS_STENCIL_OP_ZFAIL_BITS | GLS_STENCIL_OP_PASS_BITS ) ) {
		GLenum sFail = 0;
		GLenum zFail = 0;
		GLenum pass = 0;

		switch ( stateBits & GLS_STENCIL_OP_FAIL_BITS ) {
			case GLS_STENCIL_OP_FAIL_KEEP:		sFail = GL_KEEP; break;
			case GLS_STENCIL_OP_FAIL_ZERO:		sFail = GL_ZERO; break;
			case GLS_STENCIL_OP_FAIL_REPLACE:	sFail = GL_REPLACE; break;
			case GLS_STENCIL_OP_FAIL_INCR:		sFail = GL_INCR; break;
			case GLS_STENCIL_OP_FAIL_DECR:		sFail = GL_DECR; break;
			case GLS_STENCIL_OP_FAIL_INVERT:	sFail = GL_INVERT; break;
			case GLS_STENCIL_OP_FAIL_INCR_WRAP: sFail = GL_INCR_WRAP; break;
			case GLS_STENCIL_OP_FAIL_DECR_WRAP: sFail = GL_DECR_WRAP; break;
		}
		switch ( stateBits & GLS_STENCIL_OP_ZFAIL_BITS ) {
			case GLS_STENCIL_OP_ZFAIL_KEEP:		zFail = GL_KEEP; break;
			case GLS_STENCIL_OP_ZFAIL_ZERO:		zFail = GL_ZERO; break;
			case GLS_STENCIL_OP_ZFAIL_REPLACE:	zFail = GL_REPLACE; break;
			case GLS_STENCIL_OP_ZFAIL_INCR:		zFail = GL_INCR; break;
			case GLS_STENCIL_OP_ZFAIL_DECR:		zFail = GL_DECR; break;
			case GLS_STENCIL_OP_ZFAIL_INVERT:	zFail = GL_INVERT; break;
			case GLS_STENCIL_OP_ZFAIL_INCR_WRAP:zFail = GL_INCR_WRAP; break;
			case GLS_STENCIL_OP_ZFAIL_DECR_WRAP:zFail = GL_DECR_WRAP; break;
		}
		switch ( stateBits & GLS_STENCIL_OP_PASS_BITS ) {
			case GLS_STENCIL_OP_PASS_KEEP:		pass = GL_KEEP; break;
			case GLS_STENCIL_OP_PASS_ZERO:		pass = GL_ZERO; break;
			case GLS_STENCIL_OP_PASS_REPLACE:	pass = GL_REPLACE; break;
			case GLS_STENCIL_OP_PASS_INCR:		pass = GL_INCR; break;
			case GLS_STENCIL_OP_PASS_DECR:		pass = GL_DECR; break;
			case GLS_STENCIL_OP_PASS_INVERT:	pass = GL_INVERT; break;
			case GLS_STENCIL_OP_PASS_INCR_WRAP:	pass = GL_INCR_WRAP; break;
			case GLS_STENCIL_OP_PASS_DECR_WRAP:	pass = GL_DECR_WRAP; break;
		}
		qglStencilOp( sFail, zFail, pass );
	}

	m_glStateBits = stateBits | ( m_glStateBits & GLS_KEEP );

	//PrintState( m_glStateBits );
}

/*
====================
idRenderBackend::GL_SeparateStencil
====================
*/
void idRenderBackend::GL_SeparateStencil( stencilFace_t face, uint64 stencilBits ) {
	GLenum glface = 0;
	GLenum sFail = 0;
	GLenum zFail = 0;
	GLenum pass = 0;

	switch ( face ) {
	case STENCIL_FACE_BACK: 
		glface = GL_BACK;
		break;
	case STENCIL_FACE_FRONT:
	default:
		glface = GL_FRONT;
		break;
	}

	switch ( stencilBits & GLS_STENCIL_OP_FAIL_BITS ) {
		case GLS_STENCIL_OP_FAIL_KEEP:		sFail = GL_KEEP; break;
		case GLS_STENCIL_OP_FAIL_ZERO:		sFail = GL_ZERO; break;
		case GLS_STENCIL_OP_FAIL_REPLACE:	sFail = GL_REPLACE; break;
		case GLS_STENCIL_OP_FAIL_INCR:		sFail = GL_INCR; break;
		case GLS_STENCIL_OP_FAIL_DECR:		sFail = GL_DECR; break;
		case GLS_STENCIL_OP_FAIL_INVERT:	sFail = GL_INVERT; break;
		case GLS_STENCIL_OP_FAIL_INCR_WRAP: sFail = GL_INCR_WRAP; break;
		case GLS_STENCIL_OP_FAIL_DECR_WRAP: sFail = GL_DECR_WRAP; break;
	}
	switch ( stencilBits & GLS_STENCIL_OP_ZFAIL_BITS ) {
		case GLS_STENCIL_OP_ZFAIL_KEEP:		zFail = GL_KEEP; break;
		case GLS_STENCIL_OP_ZFAIL_ZERO:		zFail = GL_ZERO; break;
		case GLS_STENCIL_OP_ZFAIL_REPLACE:	zFail = GL_REPLACE; break;
		case GLS_STENCIL_OP_ZFAIL_INCR:		zFail = GL_INCR; break;
		case GLS_STENCIL_OP_ZFAIL_DECR:		zFail = GL_DECR; break;
		case GLS_STENCIL_OP_ZFAIL_INVERT:	zFail = GL_INVERT; break;
		case GLS_STENCIL_OP_ZFAIL_INCR_WRAP:zFail = GL_INCR_WRAP; break;
		case GLS_STENCIL_OP_ZFAIL_DECR_WRAP:zFail = GL_DECR_WRAP; break;
	}
	switch ( stencilBits & GLS_STENCIL_OP_PASS_BITS ) {
		case GLS_STENCIL_OP_PASS_KEEP:		pass = GL_KEEP; break;
		case GLS_STENCIL_OP_PASS_ZERO:		pass = GL_ZERO; break;
		case GLS_STENCIL_OP_PASS_REPLACE:	pass = GL_REPLACE; break;
		case GLS_STENCIL_OP_PASS_INCR:		pass = GL_INCR; break;
		case GLS_STENCIL_OP_PASS_DECR:		pass = GL_DECR; break;
		case GLS_STENCIL_OP_PASS_INVERT:	pass = GL_INVERT; break;
		case GLS_STENCIL_OP_PASS_INCR_WRAP:	pass = GL_INCR_WRAP; break;
		case GLS_STENCIL_OP_PASS_DECR_WRAP:	pass = GL_DECR_WRAP; break;
	}

	qglStencilOpSeparate( glface, sFail, zFail, pass );
	glcontext.stencilOperations[ face ] = stencilBits;
}

/*
====================
idRenderBackend::GL_SelectTexture
====================
*/
void idRenderBackend::GL_SelectTexture( int unit ) {
	if ( m_currenttmu == unit ) {
		return;
	}

	if ( unit < 0 || unit >= m_maxTextureImageUnits ) {
		idLib::Warning( "GL_SelectTexture: unit = %i", unit );
		return;
	}

	RENDERLOG_PRINTF( "GL_SelectTexture( %d );\n", unit );

	m_currenttmu = unit;
}

/*
====================
idRenderBackend::GL_BindTexture

Makes this image active on the current GL texture unit.
automatically enables or disables cube mapping
May perform file loading if the image was not preloaded.
====================
*/
void idRenderBackend::GL_BindTexture( idImage * image ) {
	RENDERLOG_PRINTF( "GL_BindTexture( %s )\n", image->GetName() );

	// load the image if necessary (FIXME: not SMP safe!)
	if ( !image->IsLoaded() ) {
		// load the image on demand here, which isn't our normal game operating mode
		image->ActuallyLoadImage( true );
	}

	const int texUnit = m_currenttmu;

	tmu_t * tmu = &glcontext.tmu[ texUnit ];

	// bind the textures
	if ( image->m_opts.textureType == TT_2D ) {
		if ( tmu->current2DMap != image->m_texnum ) {
			tmu->current2DMap = image->m_texnum;
			qglBindMultiTextureEXT( GL_TEXTURE0_ARB + texUnit, GL_TEXTURE_2D, image->m_texnum );
		}
	} else if ( image->m_opts.textureType == TT_CUBIC ) {
		if ( tmu->currentCubeMap != image->m_texnum ) {
			tmu->currentCubeMap = image->m_texnum;
			qglBindMultiTextureEXT( GL_TEXTURE0_ARB + texUnit, GL_TEXTURE_CUBE_MAP_EXT, image->m_texnum );
		}
	}
}

/*
====================
idRenderBackend::GL_CopyFrameBuffer
====================
*/
void idRenderBackend::GL_CopyFrameBuffer( idImage * image, int x, int y, int imageWidth, int imageHeight ) {
	qglBindTexture( ( image->m_opts.textureType == TT_CUBIC ) ? GL_TEXTURE_CUBE_MAP_EXT : GL_TEXTURE_2D, image->m_texnum );

	qglReadBuffer( GL_BACK );

	image->m_opts.width = imageWidth;
	image->m_opts.height = imageHeight;

	qglCopyTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA, x, y, imageWidth, imageHeight, 0 );

	// these shouldn't be necessary if the image was initialized properly.
	qglTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
	qglTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );

	qglTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
	qglTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );

	m_pc.c_copyFrameBuffer++;
}

/*
====================
idRenderBackend::GL_CopyDepthBuffer
====================
*/
void idRenderBackend::GL_CopyDepthBuffer( idImage * image, int x, int y, int imageWidth, int imageHeight ) {
	qglBindTexture( ( image->m_opts.textureType == TT_CUBIC ) ? GL_TEXTURE_CUBE_MAP_EXT : GL_TEXTURE_2D, image->m_texnum );

	qglReadBuffer( GL_BACK );

	image->m_opts.width = imageWidth;
	image->m_opts.height = imageHeight;

	qglCopyTexImage2D( GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, x, y, imageWidth, imageHeight, 0 );

	m_pc.c_copyFrameBuffer++;
}

/*
========================
idRenderBackend::GL_Clear
========================
*/
void idRenderBackend::GL_Clear( bool color, bool depth, bool stencil, byte stencilValue, float r, float g, float b, float a ) {
	int clearFlags = 0;
	if ( color ) {
		qglClearColor( r, g, b, a );
		clearFlags |= GL_COLOR_BUFFER_BIT;
	}
	if ( depth ) {
		clearFlags |= GL_DEPTH_BUFFER_BIT;
	}
	if ( stencil ) {
		qglClearStencil( stencilValue );
		clearFlags |= GL_STENCIL_BUFFER_BIT;
	}
	qglClear( clearFlags );

	RENDERLOG_PRINTF( "GL_Clear( color=%d, depth=%d, stencil=%d, stencil=%d, r=%f, g=%f, b=%f, a=%f )\n", 
		color, depth, stencil, stencilValue, r, g, b, a );
}

/*
========================
idRenderBackend::GL_DepthBoundsTest
========================
*/
void idRenderBackend::GL_DepthBoundsTest( const float zmin, const float zmax ) {
	if ( !m_depthBoundsTestAvailable || zmin > zmax ) {
		return;
	}

	if ( zmin == 0.0f && zmax == 0.0f ) {
		qglDisable( GL_DEPTH_BOUNDS_TEST_EXT );
		m_glStateBits = m_glStateBits & ~GLS_DEPTH_TEST_MASK;
	} else {
		qglEnable( GL_DEPTH_BOUNDS_TEST_EXT );
		qglDepthBoundsEXT( zmin, zmax );
		m_glStateBits |= GLS_DEPTH_TEST_MASK;
	}

	RENDERLOG_PRINTF( "GL_DepthBoundsTest( zmin=%f, zmax=%f )\n", zmin, zmax );
}

/*
====================
idRenderBackend::GL_PolygonOffset
====================
*/
void idRenderBackend::GL_PolygonOffset( float scale, float bias ) {
	m_polyOfsScale = scale;
	m_polyOfsBias = bias;
	if ( m_glStateBits & GLS_POLYGON_OFFSET ) {
		qglPolygonOffset( scale, bias );
	}

	RENDERLOG_PRINTF( "GL_PolygonOffset( scale=%f, bias=%f )\n", scale, bias );
}

/*
====================
idRenderBackend::GL_Scissor
====================
*/
void idRenderBackend::GL_Scissor( int x /* left*/, int y /* bottom */, int w, int h ) {
	qglScissor( x, y, w, h );
}

/*
====================
idRenderBackend::GL_Viewport
====================
*/
void idRenderBackend::GL_Viewport( int x /* left */, int y /* bottom */, int w, int h ) {
	qglViewport( x, y, w, h );
}

/*
==============================================================================================

STENCIL SHADOW RENDERING

==============================================================================================
*/

/*
==================
idRenderBackend::DrawStencilShadowPass
==================
*/
void idRenderBackend::DrawStencilShadowPass( const drawSurf_t * drawSurf, const bool renderZPass ) {
	if ( renderZPass ) {
		// Z-pass
		GL_SeparateStencil( STENCIL_FACE_FRONT, GLS_STENCIL_OP_FAIL_KEEP | GLS_STENCIL_OP_ZFAIL_KEEP | GLS_STENCIL_OP_PASS_INCR );
		GL_SeparateStencil( STENCIL_FACE_BACK, GLS_STENCIL_OP_FAIL_KEEP | GLS_STENCIL_OP_ZFAIL_KEEP | GLS_STENCIL_OP_PASS_DECR );
	} else if ( r_useStencilShadowPreload.GetBool() ) {
		// preload + Z-pass
		GL_SeparateStencil( STENCIL_FACE_FRONT, GLS_STENCIL_OP_FAIL_KEEP | GLS_STENCIL_OP_ZFAIL_DECR | GLS_STENCIL_OP_PASS_DECR );
		GL_SeparateStencil( STENCIL_FACE_BACK, GLS_STENCIL_OP_FAIL_KEEP | GLS_STENCIL_OP_ZFAIL_INCR | GLS_STENCIL_OP_PASS_INCR );
	} else {
		// Z-fail
	}

	// get vertex buffer
	const vertCacheHandle_t vbHandle = drawSurf->shadowCache;
	idVertexBuffer * vertexBuffer;
	if ( vertexCache.CacheIsStatic( vbHandle ) ) {
		vertexBuffer = &vertexCache.m_staticData.vertexBuffer;
	} else {
		const uint64 frameNum = (int)( vbHandle >> VERTCACHE_FRAME_SHIFT ) & VERTCACHE_FRAME_MASK;
		if ( frameNum != ( ( vertexCache.m_currentFrame - 1 ) & VERTCACHE_FRAME_MASK ) ) {
			idLib::Warning( "idRenderBackend::DrawStencilShadowPass, vertexBuffer == NULL" );
			return;
		}
		vertexBuffer = &vertexCache.m_frameData[ vertexCache.m_drawListNum ].vertexBuffer;
	}
	const int vertOffset = (int)( vbHandle >> VERTCACHE_OFFSET_SHIFT ) & VERTCACHE_OFFSET_MASK;

	// get index buffer
	const vertCacheHandle_t ibHandle = drawSurf->indexCache;
	idIndexBuffer * indexBuffer;
	if ( vertexCache.CacheIsStatic( ibHandle ) ) {
		indexBuffer = &vertexCache.m_staticData.indexBuffer;
	} else {
		const uint64 frameNum = (int)( ibHandle >> VERTCACHE_FRAME_SHIFT ) & VERTCACHE_FRAME_MASK;
		if ( frameNum != ( ( vertexCache.m_currentFrame - 1 ) & VERTCACHE_FRAME_MASK ) ) {
			idLib::Warning( "idRenderBackend::DrawStencilShadowPass, indexBuffer == NULL" );
			return;
		}
		indexBuffer = &vertexCache.m_frameData[ vertexCache.m_drawListNum ].indexBuffer;
	}
	const uint64 indexOffset = (int)( ibHandle >> VERTCACHE_OFFSET_SHIFT ) & VERTCACHE_OFFSET_MASK;

	RENDERLOG_PRINTF( "Binding Buffers(%d): %p:%i %p:%i\n", drawSurf->numIndexes, vertexBuffer, vertOffset, indexBuffer, indexOffset );

	if ( m_currentIndexBuffer != indexBuffer->GetAPIObject() ) {
		qglBindBufferARB( GL_ELEMENT_ARRAY_BUFFER_ARB, indexBuffer->GetAPIObject() );
		m_currentIndexBuffer = indexBuffer->GetAPIObject();
	}

	if ( drawSurf->jointCache ) {
		assert( renderProgManager.GetCurrentRenderProg().usesJoints );

		idUniformBuffer jointBuffer;
		if ( !vertexCache.GetJointBuffer( drawSurf->jointCache, &jointBuffer ) ) {
			idLib::Warning( "idRenderBackend::DrawStencilShadowPass, jointBuffer == NULL" );
			return;
		}
		assert( ( jointBuffer.GetOffset() & ( m_uniformBufferOffsetAlignment - 1 ) ) == 0 );

		qglBindBufferRange( GL_UNIFORM_BUFFER, 0, jointBuffer.GetAPIObject(), jointBuffer.GetOffset(), jointBuffer.GetSize() );
	}

	if ( m_currentVertexBuffer != vertexBuffer->GetAPIObject() ) {
		qglBindBufferARB( GL_ARRAY_BUFFER_ARB, vertexBuffer->GetAPIObject() );
		m_currentVertexBuffer = vertexBuffer->GetAPIObject();
	}

	PrintState( m_glStateBits, glcontext.stencilOperations );
	renderProgManager.CommitCurrent( m_glStateBits );

	const int baseVertex = vertOffset / ( drawSurf->jointCache ? sizeof( idShadowVertSkinned ) : sizeof( idShadowVert ) );

	qglDrawElementsBaseVertex( 
		GL_TRIANGLES, 
		r_singleTriangle.GetBool() ? 3 : drawSurf->numIndexes, 
		GL_INDEX_TYPE, 
		(triIndex_t *)indexOffset, 
		baseVertex );

	if ( !renderZPass && r_useStencilShadowPreload.GetBool() ) {
		// render again with Z-pass
		GL_SeparateStencil( STENCIL_FACE_FRONT, GLS_STENCIL_OP_FAIL_KEEP | GLS_STENCIL_OP_ZFAIL_KEEP | GLS_STENCIL_OP_PASS_INCR );
		GL_SeparateStencil( STENCIL_FACE_BACK, GLS_STENCIL_OP_FAIL_KEEP | GLS_STENCIL_OP_ZFAIL_KEEP | GLS_STENCIL_OP_PASS_DECR );

		qglDrawElementsBaseVertex( 
			GL_TRIANGLES, 
			r_singleTriangle.GetBool() ? 3 : drawSurf->numIndexes, 
			GL_INDEX_TYPE, 
			(triIndex_t *)indexOffset, 
			baseVertex );
	}
}
