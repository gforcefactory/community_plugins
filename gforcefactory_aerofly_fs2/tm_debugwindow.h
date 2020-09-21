//////////////////////////////////////////////////////////////////////////////////////////////////
//
// code that opens a window and displays the received messages as text
// its meant just as a simple helper to get started with the DLL
//
// THIS CODE SHOULD NOT BE USED IN A PRODUCTION DLL
//
//////////////////////////////////////////////////////////////////////////////////////////////////
#include <gdiplus.h>
#pragma message("including lib: gdiplus.lib")
#pragma comment(lib, "gdiplus.lib")

static ULONG_PTR                         Global_DebugOutput_gdiplusToken = NULL;
static std::thread                       Global_DebugOutput_Thread;
static HWND                              Global_DebugOutput_Window = NULL;
static bool                              Global_DebugOutput_WindowCloseMessage = false;


const int SAMPLE_WINDOW_WIDTH = 640;
const int SAMPLE_WINDOW_HEIGHT = 1080;
void DebugOutput_Draw(HDC hDC);

LRESULT WINAPI DebugOutput_WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
	case WM_PAINT:
	{
		PAINTSTRUCT ps;
		auto hDC = BeginPaint(hWnd, &ps);
		DebugOutput_Draw(hDC);
		EndPaint(hWnd, &ps);
	}
	return 0;

	case WM_TIMER:
		InvalidateRect(hWnd, 0, FALSE);
		return 0;

	case WM_CLOSE:
		Global_DebugOutput_WindowCloseMessage = true;
		break;

	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	}

	return DefWindowProc(hWnd, msg, wParam, lParam);
}

void DebugOutput_CreateWindow(HINSTANCE hInstance)
{
	const char classname[] = "aerofly_external_dll_sample";

	//
	// init gdi+
	//
	Gdiplus::GdiplusStartupInput startupinput;
	auto status = GdiplusStartup(&Global_DebugOutput_gdiplusToken, &startupinput, NULL);

	//
	// fill in window class structure and register the class
	//
	WNDCLASS wc;
	wc.style = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc = DebugOutput_WndProc;                // Window Procedure
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = hInstance;                          // Owner of this class
	wc.hIcon = LoadIcon(NULL, IDI_INFORMATION);
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)(COLOR_BACKGROUND + 1);      // Default color
	wc.lpszMenuName = NULL;
	wc.lpszClassName = classname;
	RegisterClass(&wc);

	Global_DebugOutput_WindowCloseMessage = false;

	Global_DebugOutput_Window = CreateWindow(classname, "Aerofly External DLL Sample",
		WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN | WS_CLIPSIBLINGS, CW_USEDEFAULT,
		0, SAMPLE_WINDOW_WIDTH, SAMPLE_WINDOW_HEIGHT,
		NULL,       // no parent window
		NULL,       // Use the window class menu.
		hInstance,  // This instance owns this window
		NULL);     // We don't use any extra data


	auto s_width = GetSystemMetrics(SM_CXSCREEN);
	auto s_height = GetSystemMetrics(SM_CYSCREEN);

	SetWindowPos(Global_DebugOutput_Window, HWND_TOP, s_width - SAMPLE_WINDOW_WIDTH, 0, SAMPLE_WINDOW_WIDTH, SAMPLE_WINDOW_HEIGHT, SWP_SHOWWINDOW);
	// set up timers
	SetTimer(Global_DebugOutput_Window, 0, 50, 0);

	MSG msg;
	while (!Global_DebugOutput_WindowCloseMessage && GetMessage(&msg, Global_DebugOutput_Window, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	DestroyWindow(Global_DebugOutput_Window);
	Global_DebugOutput_Window = NULL;

	//
	// shutdown gdi+
	//
	Gdiplus::GdiplusShutdown(Global_DebugOutput_gdiplusToken);
}


void DebugOutput_WindowOpen()
{
	Global_DebugOutput_Thread = std::thread(DebugOutput_CreateWindow, global_hDLLinstance);
}

void DebugOutput_WindowClose()
{
	if (Global_DebugOutput_Window != NULL)
	{
		PostMessage(Global_DebugOutput_Window, WM_QUIT, 0, 0);
	}
	Global_DebugOutput_Thread.join();
}
