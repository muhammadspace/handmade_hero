#include <windows.h>
#include <stdint.h>

// TODO: This is global for now.
static bool Running;

static BITMAPINFO bitmapInfo;
static void *bitmapMemory;
static int bitmapWidth;
static int bitmapHeight;
static int bytesPerPixel = 4;

static void RenderWeirdGradient(int xOffset, int yOffset)
{
    int pitch = bitmapWidth*bytesPerPixel;
    uint8_t *row = (uint8_t *)bitmapMemory;
    for (int y = 0; y < bitmapHeight; y++)
    {
        uint32_t *pixel =(uint32_t *)row;
        for (int x = 0; x < bitmapWidth; x++)
        {
            uint8_t blue = x + xOffset;
            uint8_t green = y + yOffset;
            uint8_t red = 0;
            uint8_t pad = 0;

            *pixel++ = ((green << 8) | blue);
        } 

        row += pitch;
    }
}

// Create a bitmap image if there is none and resizes it when WM_SIZE is received
// DIB is the name that Windows uses for things WE can write into so that it can display using GDI
static void Win32ResizeDIBSection(int width, int height)
{
    bitmapWidth = width;
    bitmapHeight = height;

    if (bitmapMemory)
    {
        VirtualFree(bitmapMemory, 0, MEM_RELEASE);
    }

    bitmapInfo.bmiHeader.biSize = sizeof(bitmapInfo.bmiHeader);
    bitmapInfo.bmiHeader.biWidth = width;
    bitmapInfo.bmiHeader.biHeight = -height;
    bitmapInfo.bmiHeader.biPlanes = 1;
    bitmapInfo.bmiHeader.biBitCount = 32;
    bitmapInfo.bmiHeader.biCompression = BI_RGB;

    int bitmapMemorySize = width * height * bytesPerPixel;
    bitmapMemory = VirtualAlloc(0, bitmapMemorySize, MEM_COMMIT, PAGE_READWRITE);
}

static void Win32UpdateWindow(HDC deviceContext, RECT *windowRect, int x, int y, int width, int height)
{
    int bitmapWidth = windowRect->right - windowRect->left;
    int bitmapHeight = windowRect->bottom - windowRect->top;

    StretchDIBits(
            deviceContext,
            /*
            x, y, width, height,
            x, y, width, height,
             */
            0, 0, bitmapWidth, bitmapHeight,
            0, 0, bitmapWidth, bitmapHeight,
            bitmapMemory,
            &bitmapInfo,
            DIB_RGB_COLORS,
            SRCCOPY);
}

LRESULT CALLBACK MainWindowCallback(HWND Window, UINT Message, WPARAM WParam, LPARAM LParam)
{
    LRESULT result = 0;

    switch (Message)
    {
        case WM_ACTIVATEAPP:
        {
            OutputDebugStringA("WM_ACTIVATEAPP\n");
        } break;
        case WM_CLOSE:
        {
            // TODO: Handle with an confirmation message to the user?
            Running = false;
        } break;
        case WM_DESTROY:
        {
            Running = false;
            // TODO: Handle as an error and recreate window?
        } break;
        case WM_SIZE:
        {
            RECT clientRect;
            GetClientRect(Window, &clientRect);

            int clientWidth = clientRect.right - clientRect.left;
            int clientHeight = clientRect.bottom - clientRect.top;

            Win32ResizeDIBSection(clientWidth, clientHeight);
        } break;
        case WM_PAINT:
        {
            PAINTSTRUCT paint;
            HDC deviceContext = BeginPaint(Window, &paint);

            int x = paint.rcPaint.left;
            int y = paint.rcPaint.top;
            int width = paint.rcPaint.right - paint.rcPaint.left;
            int height = paint.rcPaint.bottom - paint.rcPaint.top;

            RECT clientRect;
            GetClientRect(Window, &clientRect);

            Win32UpdateWindow(deviceContext, &clientRect, x, y, width, height);
            EndPaint(Window, &paint);

            OutputDebugStringA("WM_PAINT\n");
        } break;
        default:
        {
            result = DefWindowProc(Window, Message, WParam, LParam);
        } break;
    };

    return(result);
}

int CALLBACK 
WinMain(HINSTANCE hInstance,
        HINSTANCE hPrevInstance,
        LPSTR lpCmdLine,
        int nCmdShow)
{

    WNDCLASS WindowClass = {};
    
    WindowClass.style = CS_HREDRAW|CS_VREDRAW;
    WindowClass.lpfnWndProc = MainWindowCallback;
    WindowClass.hInstance = hInstance;
    // WindowClass.hIcon = ;
    WindowClass.lpszClassName = "HandmadeHeroWindowClass";

    if (RegisterClass(&WindowClass))
    {
        HWND Window = CreateWindowEx(
                0, 
                WindowClass.lpszClassName, 
                "Handmade Hero",
                WS_OVERLAPPEDWINDOW|WS_VISIBLE, 
                CW_USEDEFAULT,
                CW_USEDEFAULT,
                CW_USEDEFAULT,
                CW_USEDEFAULT,
                0,
                0,
                hInstance,
                0);

        if (Window)
        {
            int xOffset = 0;
            int yOffset = 0;
            Running = true;
            while (Running)
            {
                MSG msg;
                while (PeekMessageA(&msg, Window, 0, 0, PM_REMOVE))
                {
                    if (msg.message == WM_QUIT)
                        Running = false;

                    TranslateMessage(&msg);
                    DispatchMessage(&msg);

                }

                RenderWeirdGradient(xOffset, yOffset);

                RECT clientRect;
                GetClientRect(Window, &clientRect);
                int windowWidth = clientRect.right - clientRect.left;
                int windowHeight = clientRect.bottom - clientRect.top;
                HDC deviceContext = GetDC(Window);
                Win32UpdateWindow(deviceContext, &clientRect, 0, 0, windowWidth, windowHeight);
                ReleaseDC(Window, deviceContext);

                xOffset++;
            }
        }
    }
    else
    {
        // TODO: Logging
    }

    return(0);
}
