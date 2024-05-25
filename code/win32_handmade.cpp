#include <windows.h>
#include <stdint.h>
#include <xinput.h>

struct Win32_Offscreen_Buffer {
    BITMAPINFO info;
    void *memory;
    int width;
    int height;
    int bytesPerPixel = 4;
    int pitch;
};

struct Win32_Client_Dimension {
     int width;
     int height;
};

// TODO: This is global for now.
static bool globalRunning;
static Win32_Offscreen_Buffer globalOffscreenBuffer;


#define XINPUT_GET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_STATE *pState)
typedef XINPUT_GET_STATE(xinput_get_state);
XINPUT_GET_STATE(XInputGetStateStub)
{
    return ERROR_DEVICE_NOT_CONNECTED;
}

#define XINPUT_SET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_VIBRATION* pVibration)
typedef XINPUT_SET_STATE(xinput_set_state);
XINPUT_SET_STATE(XInputSetStateStub)
{
    return ERROR_DEVICE_NOT_CONNECTED;
}

static xinput_get_state *XInputGetState_ = XInputGetStateStub;
static xinput_set_state *XInputSetState_ = XInputSetStateStub;

static void Win32LoadXInput()
{
    HMODULE XInputLibrary = LoadLibraryA("Xinput1_4.dll");
    if (!XInputLibrary)
    {
        XInputLibrary = LoadLibraryA("Xinput1_3.dll");
    }

    if (XInputLibrary)
    {
        XInputGetState_ = (xinput_get_state *)GetProcAddress(XInputLibrary, "XInputGetState");
        XInputSetState_ = (xinput_set_state *)GetProcAddress(XInputLibrary, "XInputSetState");
    }
}

#define XInputGetState XInputGetState_
#define XInputSetState XInputSetState_


static Win32_Client_Dimension Win32GetClientDimensions(HWND window)
{
    Win32_Client_Dimension result;

    RECT clientRect;
    GetClientRect(window, &clientRect);

    result.width = clientRect.right - clientRect.left;
    result.height = clientRect.bottom - clientRect.top;

    return result;
}

static void RenderWeirdGradient(Win32_Offscreen_Buffer *buffer, int xOffset, int yOffset)
{
    uint8_t *row = (uint8_t *)buffer->memory;
    for (int y = 0; y < buffer->height; y++)
    {
        uint32_t *pixel =(uint32_t *)row;
        for (int x = 0; x < buffer->width; x++)
        {
            uint8_t blue = x + xOffset;
            uint8_t green = y + yOffset;
            uint8_t red = 0;
            uint8_t pad = 0;

            *pixel++ = ((green << 8) | blue);
        } 

        row += buffer->pitch;
    }
}

// Create a bitmap image if there is none and resizes it when WM_SIZE is received
// DIB is the name that Windows uses for things WE can write into so that it can display using GDI
static void Win32ResizeDIBSection(Win32_Offscreen_Buffer *buffer, int width, int height)
{
    if (buffer->memory)
    {
        VirtualFree(buffer->memory, 0, MEM_RELEASE);
    }

    buffer->info.bmiHeader.biSize = sizeof(buffer->info.bmiHeader);
    buffer->info.bmiHeader.biWidth = width;
    buffer->info.bmiHeader.biHeight = -height;
    buffer->info.bmiHeader.biPlanes = 1;
    buffer->info.bmiHeader.biBitCount = 32;
    buffer->info.bmiHeader.biCompression = BI_RGB;

    int bitmapMemorySize = width * height * buffer->bytesPerPixel;
    buffer->memory = VirtualAlloc(0, bitmapMemorySize, MEM_COMMIT, PAGE_READWRITE);
    buffer->pitch = width*buffer->bytesPerPixel;
    buffer->width = width;
    buffer->height = height;
}

static void Win32DisplayOffscreenBufferOnWindow(HDC deviceContext,
                                                int clientWidth, int clientHeight,
                                                Win32_Offscreen_Buffer buffer,
                                                int x, int y,
                                                int width, int height)
{
    StretchDIBits(
            deviceContext,
            /*
            x, y, width, height,
            x, y, width, height,
             */
            0, 0, buffer.width, buffer.height,
            0, 0, clientWidth, clientHeight,
            buffer.memory,
            &buffer.info,
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
            globalRunning = false;
        } break;
        case WM_DESTROY:
        {
            globalRunning = false;
            // TODO: Handle as an error and recreate window?
        } break;
        case WM_SIZE:
        {
            Win32_Client_Dimension clientDimensions = Win32GetClientDimensions(Window);

            Win32ResizeDIBSection(&globalOffscreenBuffer, clientDimensions.width, clientDimensions.height);
        } break;
        case WM_PAINT:
        {
            PAINTSTRUCT paint;
            HDC deviceContext = BeginPaint(Window, &paint);

            int x = paint.rcPaint.left;
            int y = paint.rcPaint.top;
            int width = paint.rcPaint.right - paint.rcPaint.left;
            int height = paint.rcPaint.bottom - paint.rcPaint.top;


            Win32_Client_Dimension clientDimensions = Win32GetClientDimensions(Window);
            Win32DisplayOffscreenBufferOnWindow(deviceContext,
                    clientDimensions.width, clientDimensions.height,
                    globalOffscreenBuffer,
                    x, y,
                    width, height);
            EndPaint(Window, &paint);

            OutputDebugStringA("WM_PAINT\n");
        } break;
        case WM_SYSKEYDOWN:
        case WM_SYSKEYUP:
        case WM_KEYDOWN:
        case WM_KEYUP:
        {
            bool wasDown = (LParam & (1 << 30)) != 0;
            bool isDown = (LParam & (1 << 31)) == 0;

            if (wasDown != isDown)
            {
                if (WParam == 'W')
                {
                    OutputDebugStringA("w");
                }
                else if (WParam == 'A')
                {
                    OutputDebugStringA("a");
                }
                else if (WParam == 'S')
                {
                    OutputDebugStringA("s");
                }
                else if (WParam == 'D')
                {
                    OutputDebugStringA("d");
                }
            }

            bool altKeyDown = (LParam & (1 << 29)) != 0;
            if (WParam == VK_F4 && altKeyDown)
            {
                globalRunning = false;
            }
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
    Win32LoadXInput();

    Win32ResizeDIBSection(&globalOffscreenBuffer, 1280, 720);

    WNDCLASSA WindowClass = {};
    
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
            globalRunning = true;
            while (globalRunning)
            {
                MSG msg;
                while (PeekMessageA(&msg, Window, 0, 0, PM_REMOVE))
                {
                    if (msg.message == WM_QUIT)
                        globalRunning = false;

                    TranslateMessage(&msg);
                    DispatchMessage(&msg);
                }

                for (DWORD controllerIndex = 0; controllerIndex < XUSER_MAX_COUNT; controllerIndex++)
                {
                    XINPUT_STATE state;

                    if (XInputGetState(controllerIndex, &state) == ERROR_SUCCESS)
                    {
                        XINPUT_GAMEPAD *pad = &state.Gamepad;
                        bool ABtn = pad->wButtons & XINPUT_GAMEPAD_A;
                        bool BBtn = pad->wButtons & XINPUT_GAMEPAD_B;
                        bool XBtn = pad->wButtons & XINPUT_GAMEPAD_X;
                        bool YBtn = pad->wButtons & XINPUT_GAMEPAD_Y;

                        bool leftShoulder = pad->wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER;
                        bool rightShoulder = pad->wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER;

                        bool dpadUp = pad->wButtons & XINPUT_GAMEPAD_DPAD_UP;
                        bool dpadDown = pad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN;
                        bool dpadLeft = pad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT;
                        bool dpadRight = pad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT;

                        if (leftShoulder)
                        {
                            yOffset++;
                        }

                        /*
                        XINPUT_VIBRATION vibration;
                        vibration.wLeftMotorSpeed = 60000;
                        vibration.wRightMotorSpeed = 60000;
                        XInputSetState(controllerIndex, &vibration);
                        */

                    }
                    else
                    {
                        // controller is not connected
                    }
                }

                RenderWeirdGradient(&globalOffscreenBuffer, xOffset, yOffset);

                Win32_Client_Dimension clientDimensions = Win32GetClientDimensions(Window);
                HDC deviceContext = GetDC(Window);
                Win32DisplayOffscreenBufferOnWindow(deviceContext,
                        clientDimensions.width, clientDimensions.height,
                        globalOffscreenBuffer,
                        0, 0,
                        clientDimensions.width, clientDimensions.height);
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
