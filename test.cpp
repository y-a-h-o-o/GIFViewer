#include <stdio.h>
#include <windows.h>
#include <gdiplus.h>
#include <pthread.h>
#include <chrono>
#include <thread>
#include <math.h>

#define global_var static

global_var UINT frameIndex = 0;
global_var Gdiplus::GdiplusStartupInput gsi;
global_var ULONG_PTR gt;
global_var HWND WindowHandle;
global_var HDC deviceContext;
global_var int RUN = 1;
global_var struct timespec rem, req;

// Stolen from https://blat-blatnik.github.io/computerBear/making-accurate-sleep-function/ jjj

// Also, there isn't a worry about overflowing from count, since it would take 
// an ABSURD amount of time to overflow (if you added 1 every nanosecond, it takes 292 years to overflow). 
// I aint running this program for 292 years bubby. 
void precise_sleep(double seconds) {
	using namespace std; 
	using namespace std::chrono; 

	static double estimate = 5e-3; 
	static double mean = 5e-3; 
	static double m2 = 0; 
	static int64_t count = 1; 

	while(seconds > estimate) {
		auto start = high_resolution_clock::now(); 
		this_thread::sleep_for(milliseconds(1));
		auto end = high_resolution_clock::now(); 

		double observed = (end - start).count() / 1e9;
		seconds -= observed; 

		++count; 
		double delta = observed - mean; 
		mean += delta / count; 
		m2 += delta * (observed - mean); 
		double stddev = sqrt(m2 / (count - 1));
		estimate = mean + stddev; 
	}	
	
	auto start = high_resolution_clock::now(); 
	while((high_resolution_clock::now() - start).count() / 1e9 < seconds); 
}

// Returns the amount of delay In milliseconds. 
long drawImage(HWND hwnd, HDC hdc, const WCHAR* path) {
    
    Gdiplus::Image* img = new Gdiplus::Image(path);

    int imgH = img->GetHeight();
    int imgW = img->GetWidth();

    Gdiplus::Bitmap* sBmp = new Gdiplus::Bitmap(imgW, imgH, PixelFormat32bppARGB);
    Gdiplus::Graphics* graphics = new Gdiplus::Graphics(sBmp);
        
    UINT frameCount = img->GetFrameCount(&Gdiplus::FrameDimensionTime); 

    img->SelectActiveFrame(&Gdiplus::FrameDimensionTime, frameIndex);
    
    int tagSize = img->GetPropertyItemSize(PropertyTagFrameDelay);
    Gdiplus::PropertyItem* pTimeDelay = (Gdiplus::PropertyItem*) malloc(tagSize);
    
    if(!pTimeDelay) {
        printf("Error: Could not allocate memory for pTimeDelay!\n");
        RUN = 0;
        Gdiplus::GdiplusShutdown(gt);
        exit(0);
    }
    
    img->GetPropertyItem(PropertyTagFrameDelay, tagSize, pTimeDelay);
    long lPause = ((long*)pTimeDelay->value)[frameIndex] * 10;

    frameIndex++;
    if(frameIndex >= frameCount) {
        frameIndex = 0;
    }

    graphics->Clear(Gdiplus::Color(0, 0, 0, 0));
    graphics->DrawImage(img, 0, 0);

    HBITMAP bmp;
    sBmp->GetHBITMAP(Gdiplus::Color(0, 0, 0, 0), &bmp);

    HDC memdc = CreateCompatibleDC(hdc);
    HGDIOBJ org = SelectObject(memdc, bmp);

    BLENDFUNCTION blend = { 0 };
    blend.BlendOp = AC_SRC_OVER;
    blend.SourceConstantAlpha = 255;
    blend.AlphaFormat = AC_SRC_ALPHA;

    POINT pSrc = {0, 0};

    SIZE size = {imgW, imgH};

    UpdateLayeredWindow(hwnd, hdc, 0, &size, memdc, &pSrc, 0, &blend, ULW_ALPHA);

    SelectObject(hdc, org);

    DeleteObject(bmp);
    DeleteObject(memdc);

    delete img;
    delete sBmp;
    delete graphics;
    
    free(pTimeDelay);

    return lPause;
}

void* threadFunc(void* vargp) {
    long delay, msec;
    clock_t endTime, t_start, diff;
    while(RUN) {
        t_start = clock();
	// We need to delay for this much (in milliseconds)
        delay = drawImage(WindowHandle, deviceContext, L"Image.gif");
        diff = clock() - t_start;
        // # of milliseconds of delay used up by drawing. 
	msec = diff * 1000 / CLOCKS_PER_SEC;
	// Sleep for this much so the GIF look smooth. (Milliseconds) 
        delay = (delay - msec >= 0) ? delay - msec: 0;
	
	endTime = clock() + (delay * CLOCKS_PER_SEC) / 1000; 
	
	// Use precise sleep out of my tests, this was the most balanced in terms of CPU + Chopiness.
	// Not perfect; But still good. 
	double s_delay = (double)(delay) / 1000.0; 
	precise_sleep(s_delay); 	
    }
    pthread_exit(NULL);
    return NULL;
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) 
{
    LRESULT Result = 0;
    switch(uMsg) {
        case WM_DESTROY:
        {
            PostQuitMessage(0);
            printf("WM_DESTROY\n");
        } break;
        case WM_CLOSE:
        {
            DestroyWindow(hwnd);
            printf("WM_CLOSE\n");
        } break;
        case WM_NCHITTEST: 
        {
            return HTCAPTION;
        } break;
        default:
        {
            Result = DefWindowProc(hwnd, uMsg, wParam, lParam);
        } break;
    }
    return Result;
}

int CALLBACK WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) 
{
    WNDCLASS WindowClass = {};

    HINSTANCE instance = GetModuleHandle(0);

    WindowClass.lpfnWndProc = WindowProc;
    WindowClass.hInstance = instance;
    WindowClass.lpszClassName = "win32";

    Gdiplus::GdiplusStartup(&gt, &gsi, NULL);

    if(RegisterClass(&WindowClass)) {
        WindowHandle = CreateWindowEx(
            WS_EX_LAYERED|WS_EX_TOPMOST, 
            WindowClass.lpszClassName,
            "Animation",
            WS_POPUP,
            0, 0, 
            GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN),
            0,
            0,
            hInstance, 
            0);

        deviceContext = GetDC(WindowHandle);

        pthread_t tid;
        
        pthread_create(&tid, NULL, threadFunc, NULL);

        ShowWindow(WindowHandle, SW_SHOW);

        if(WindowHandle) {
            MSG Message = {};
            while(1) {
                BOOL MsgResult = GetMessage(&Message, 0, 0, 0);
                if(MsgResult > 0) {
                    TranslateMessage(&Message);
                    DispatchMessage(&Message);
                } else {
                    break;
                }
            }
        } else {
            printf("Window Handle NULL");
        }
    } else {
        printf("Could Not Register Window Class!\n");
    }

    RUN = 0;
    Gdiplus::GdiplusShutdown(gt);

    return 0;
} 
