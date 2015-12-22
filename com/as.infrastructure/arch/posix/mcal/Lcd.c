/**
 * AS - the open source Automotive Software on https://github.com/parai
 *
 * Copyright (C) 2015  AS <parai@foxmail.com>
 *
 * This source code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by the
 * Free Software Foundation; See <http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt>.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 */
/* ============================ [ INCLUDES  ] ====================================================== */
#ifdef GUI_USE_GTK
#include <gtk/gtk.h>
#include <gdk/gdk.h>
#else
#include "VG/openvg.h"
#include "VG/vgu.h"
#include "EGL/egl.h"
#endif
#ifdef __WINDOWS__
#include <windows.h>
#else /* __LINUX__ */
#include <pthread.h>
#endif

#include <Sg.h>
#include "Lcd.h"

/* ============================ [ MACROS    ] ====================================================== */
// 0 --> use GtkImage
// 1 --> use GtkDrawingArea  : this is more powerful, so use this
#define LCD_IMAGE        0
#define LCD_DRAWING_AREA 1
#define cfgLcdHandle   LCD_DRAWING_AREA
/* static malloc 1MB buffer */
#define LCD_MAX_WIDTH    (4096)
#define LCD_MAX_HEIGHT   (2048)
#define LCD_WIDTH        (lcdWidth*lcdPixel)
#define LCD_HEIGHT       (lcdHeight*lcdPixel)

#define LCD_X0(x)	(x*lcdPixel)
#define LCD_Y0(y)	(y*lcdPixel)

#define LCD_X1(x)	(x*lcdPixel + lcdPixel)
#define LCD_Y1(y)	(y*lcdPixel + lcdPixel)
/* ============================ [ TYPES     ] ====================================================== */
/* ============================ [ DATAS     ] ====================================================== */
#ifdef GUI_USE_GTK
static GtkWidget*       pLcd        = NULL;
static GdkPixbuf*       pLcdImage   = NULL;
static GtkWidget*       pStatusbar  = NULL;
#else
EGLDisplay			egldisplay;
EGLConfig			eglconfig;
EGLSurface			eglsurface;
EGLContext			eglcontext;
#endif


static void* 			lcdThread   = NULL;
static uint32           pLcdBuffer[LCD_MAX_WIDTH*LCD_MAX_HEIGHT];
static uint32           lcdWidth    = 0;
static uint32           lcdHeight   = 0;
static uint8            lcdPixel    = 0;

/* ============================ [ DECLARES  ] ====================================================== */
/* ============================ [ LOCALS    ] ====================================================== */
#ifdef GUI_USE_GTK
#if(cfgLcdHandle == LCD_DRAWING_AREA)
static gboolean scribble_draw (GtkWidget *widget,
         cairo_t   *cr,
         gpointer   data)
{
	gdk_cairo_set_source_pixbuf (cr, pLcdImage, 0, 0);
	cairo_paint (cr);
	return TRUE;
}
static gboolean
scribble_motion_notify_event (GtkWidget      *widget,
                              GdkEventMotion *event,
                              gpointer        data)
{
  int x, y;
  guchar text[256];
  GdkModifierType state;

  /* This call is very important; it requests the next motion event.
   * If you don't call gdk_window_get_pointer() you'll only get
   * a single motion event. The reason is that we specified
   * GDK_POINTER_MOTION_HINT_MASK to gtk_widget_set_events().
   * If we hadn't specified that, we could just use event->x, event->y
   * as the pointer location. But we'd also get deluged in events.
   * By requesting the next event as we handle the current one,
   * we avoid getting a huge number of events faster than we
   * can cope.
   */

  gdk_window_get_device_position (event->window, event->device, &x, &y, &state);

  sprintf((char*)text,"X=%d,Y=%d",(x/lcdPixel),(y/lcdPixel));
  gtk_statusbar_pop (GTK_STATUSBAR(pStatusbar), 0); /* clear any previous message,
  										    * underflow is allowed
  										    */

  gtk_statusbar_push (GTK_STATUSBAR(pStatusbar), 0, (const gchar*)text);
  /* We've handled it, stop processing */
  return TRUE;
}
#endif
static gboolean Refresh(gpointer data)
{
	uint32 x,y;
	int width, height, rowstride, n_channels;
	guchar *pixels, *p;
	uint32 index;
	uint32 color;
	if(FALSE == Sg_IsDataReady()) { return TRUE; }

	n_channels = gdk_pixbuf_get_n_channels (pLcdImage);

	g_assert (gdk_pixbuf_get_colorspace (pLcdImage) == GDK_COLORSPACE_RGB);
	g_assert (gdk_pixbuf_get_bits_per_sample (pLcdImage) == 8);

	g_assert (!gdk_pixbuf_get_has_alpha (pLcdImage));
	g_assert (n_channels == 3);

	width = gdk_pixbuf_get_width (pLcdImage);
	height = gdk_pixbuf_get_height (pLcdImage);

	g_assert (LCD_WIDTH==width);
	g_assert (LCD_HEIGHT==height);

	rowstride = gdk_pixbuf_get_rowstride (pLcdImage);
	pixels = gdk_pixbuf_get_pixels (pLcdImage);

	for(x=0;x<width;x++)
	{
		for(y=0;y<height;y++)
		{
			index = y*width + x;
			assert(index < (width*height));
			color = pLcdBuffer[index];
			p = pixels + y * rowstride + x * n_channels;
			p[0] = (color>>16)&0xFF; // red
			p[1] = (color>>8 )&0xFF; // green
			p[2] = (color>>0 )&0xFF; // blue
		}
	}
#if(cfgLcdHandle == LCD_DRAWING_AREA)
	gtk_widget_queue_draw (pLcd);
#else
	gtk_image_set_from_pixbuf(GTK_IMAGE(pLcd),pLcdImage);
#endif

	return TRUE;
}
static GtkWidget* Lcd(void)
{
	GtkWidget* pBox;
	pBox = gtk_box_new(GTK_ORIENTATION_VERTICAL,0);
	gtk_box_set_homogeneous(GTK_BOX(pBox),FALSE);

	g_timeout_add(10,Refresh,NULL); // Refresh LCD 100 times each 1s

	pLcdImage = gdk_pixbuf_new(GDK_COLORSPACE_RGB,FALSE,8,LCD_WIDTH,LCD_HEIGHT);

#if(cfgLcdHandle == LCD_DRAWING_AREA)
	pLcd = gtk_drawing_area_new ();
	gtk_widget_set_size_request (pLcd, LCD_WIDTH, LCD_HEIGHT);
	g_signal_connect (pLcd, "draw",
	                        G_CALLBACK (scribble_draw), NULL);

	g_signal_connect (pLcd, "motion-notify-event",
	                        G_CALLBACK (scribble_motion_notify_event), NULL);
	/* Ask to receive events the drawing area doesn't normally
	 * subscribe to
	 */
	gtk_widget_set_events (pLcd, gtk_widget_get_events (pLcd)
							 /*| GDK_LEAVE_NOTIFY_MASK
							 | GDK_BUTTON_PRESS_MASK*/
							 | GDK_POINTER_MOTION_MASK
							 | GDK_POINTER_MOTION_HINT_MASK);
#else
	pLcd = gtk_image_new();
#endif
	gtk_box_pack_start(GTK_BOX(pBox),pLcd,FALSE,FALSE,0);

	pStatusbar = gtk_statusbar_new ();
	gtk_box_pack_start(GTK_BOX(pBox),pStatusbar,FALSE,FALSE,0);

	return pBox;
}
static void lcd_main_quit(void)
{
	lcdThread = NULL;

	gtk_main_quit();
}
#ifdef __WINDOWS__
static DWORD Lcd_Thread(LPVOID param)
#else
static void* Lcd_Thread(void* param)
#endif
{
	GtkWidget* pWindow;
	PRINTF("# Lcd_Thread Enter\n");
	gtk_init (NULL, NULL);
	pWindow = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title(GTK_WINDOW(pWindow),"LCD");

	gtk_container_add(GTK_CONTAINER (pWindow), Lcd());

	gtk_widget_show_all (pWindow);

	g_signal_connect (pWindow, "destroy", G_CALLBACK (lcd_main_quit), NULL);

	gtk_main ();

	PRINTF("# Lcd_Thread Exit\n");

	return 0;
}
#else
#ifdef __WINDOWS__
static void init(NativeWindowType window)
{
	static const EGLint s_configAttribs[] =
	{
		EGL_RED_SIZE,		8,
		EGL_GREEN_SIZE, 	8,
		EGL_BLUE_SIZE,		8,
		EGL_ALPHA_SIZE, 	8,
		EGL_LUMINANCE_SIZE, EGL_DONT_CARE,			//EGL_DONT_CARE
		EGL_SURFACE_TYPE,	EGL_WINDOW_BIT,
		EGL_SAMPLES,		1,
		EGL_NONE
	};
	EGLint numconfigs;

	egldisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY);
	eglInitialize(egldisplay, NULL, NULL);
	assert(eglGetError() == EGL_SUCCESS);
	eglBindAPI(EGL_OPENVG_API);

	eglChooseConfig(egldisplay, s_configAttribs, &eglconfig, 1, &numconfigs);
	assert(eglGetError() == EGL_SUCCESS);
	assert(numconfigs == 1);

	eglsurface = eglCreateWindowSurface(egldisplay, eglconfig, window, NULL);
	assert(eglGetError() == EGL_SUCCESS);
	eglcontext = eglCreateContext(egldisplay, eglconfig, NULL, NULL);
	assert(eglGetError() == EGL_SUCCESS);
	eglMakeCurrent(egldisplay, eglsurface, eglsurface, eglcontext);
	assert(eglGetError() == EGL_SUCCESS);

}
static void deinit(void)
{
	eglMakeCurrent(egldisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
	assert(eglGetError() == EGL_SUCCESS);
	eglTerminate(egldisplay);
	assert(eglGetError() == EGL_SUCCESS);
	eglReleaseThread();
}
static void render(int w, int h)
{
	if(FALSE == Sg_IsDataReady()) { return; }

	uint32 x,y;
	uint8 n_channels = 4;
	VGint dataStride = LCD_WIDTH*n_channels;
	uint8* vgdata= malloc(n_channels*LCD_WIDTH*LCD_HEIGHT);

	eglSwapBuffers(egldisplay, eglsurface);	//force EGL to recognize resize
/*
 * for VG x-y                    for SG x-y
 *  Y ^   this is not            ------------> X
 *    |  the same as my          |
 *    |  SG design               |
 *    |                          |
 *    --------------> X          V  Y
 */

	for(x=0;x<LCD_WIDTH;x++)
	{
		for(y=0;y<LCD_HEIGHT;y++)
		{
			uint32 index = y*LCD_WIDTH + x;
			assert(index < (LCD_WIDTH*LCD_HEIGHT));
			uint32 color = pLcdBuffer[index];

			uint8* p = vgdata + (LCD_HEIGHT - y -1) * dataStride + x * n_channels;
			p[0] = (color>>24)&0xFF; // alpha
			p[3] = (color>>16)&0xFF; // red
			p[2] = (color>>8 )&0xFF; // green
			p[1] = (color>>0 )&0xFF; // blue
		}
	}
	vgWritePixels(vgdata,dataStride,VG_sRGBA_8888_PRE,0,0,LCD_WIDTH,LCD_HEIGHT);

	assert(vgGetError() == VG_NO_ERROR);

	eglSwapBuffers(egldisplay, eglsurface);
	assert(eglGetError() == EGL_SUCCESS);
	free(vgdata);
}
static LONG WINAPI windowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
	case WM_CLOSE:
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	case WM_PAINT:
		{
			RECT rect;
			InvalidateRect(hWnd, NULL, 0);
			GetClientRect(hWnd, &rect);
			render(rect.right - rect.left, rect.bottom - rect.top);
			return 0;
		}
	default:
		break;
	}
	return DefWindowProc(hWnd, uMsg, wParam, lParam);
}
static DWORD Lcd_Thread(LPVOID param)
{
	HWND window;
	{
		WNDCLASS wndclass;
		wndclass.style		   = 0;
		wndclass.lpfnWndProc   = windowProc;
		wndclass.cbClsExtra    = 0;
		wndclass.cbWndExtra    = 0;
		wndclass.hInstance	   = (HINSTANCE)GetModuleHandle(NULL);
		wndclass.hIcon		   = LoadIcon(wndclass.hInstance, MAKEINTRESOURCE(101));
		wndclass.hCursor	   = LoadCursor(NULL, IDC_ARROW);
		wndclass.hbrBackground = CreateSolidBrush(RGB(0, 0, 0));
		wndclass.lpszMenuName  = NULL;
		wndclass.lpszClassName = "MainWndClass";
		if (!wndclass.hIcon)
			wndclass.hIcon = LoadIcon(NULL, IDI_EXCLAMATION);
		RegisterClass(&wndclass);
	}

	window = CreateWindow(
		"MainWndClass",
		"LCD",
		WS_OVERLAPPEDWINDOW,
		200, 200, LCD_WIDTH+20, LCD_HEIGHT+20,
		NULL,
		NULL,
		(HINSTANCE)GetModuleHandle(NULL),
		NULL);
	if (!window)
		return -1;

	init((NativeWindowType)window);

	{
		MSG msg;
		ShowWindow(window, SW_SHOW);
		while (GetMessage(&msg, NULL, 0, 0))
		{
			DispatchMessage(&msg);
			if (msg.message == WM_QUIT)
				break;
		}
	}

	deinit();

	DestroyWindow(window);
	return 0;
}
#endif /* __WINDOWS__ */
#endif /* GUI_USE_GTK */

/* ============================ [ FUNCTIONS ] ====================================================== */
void Lcd_Init(uint32 width,uint32 height,uint8 pixel)
{
	if(NULL == lcdThread)
	{
		lcdWidth  = width;
		lcdHeight = height;
		lcdPixel  = pixel;

		assert(LCD_WIDTH  < LCD_MAX_WIDTH);
		assert(LCD_HEIGHT < LCD_MAX_HEIGHT);

#ifdef __WINDOWS__
		lcdThread = CreateThread( NULL, 0, ( LPTHREAD_START_ROUTINE ) Lcd_Thread, NULL, CREATE_SUSPENDED, NULL );
		assert(lcdThread!=NULL);

		ResumeThread( lcdThread );
#else /* __LINUX__ */

		pthread_create( (pthread_t*)&lcdThread, NULL, Lcd_Thread, (void *)NULL );
#endif

	}
	else
	{
		// do nothing as already started.
	}
}

void LCD_DrawPixel( uint32 x, uint32 y, uint32 color )
{
	uint32 x0,y0;
	if(NULL != lcdThread)
	{
		if((x<lcdWidth) && (y<lcdHeight))
		{
			for(x0=LCD_X0(x);x0<LCD_X1(x);x0++)
			{
				for(y0=LCD_Y0(y);y0<LCD_Y1(y);y0++)
				{
					uint32 index = y0*LCD_WIDTH + x0;
					assert(index < (LCD_WIDTH*LCD_HEIGHT));
					pLcdBuffer[index] = color;
				}
			}
		}
		else
		{
			/* out of range */
		}
	}
	else
	{
		/* device not ready */
	}
}
