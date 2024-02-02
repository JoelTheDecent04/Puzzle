#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <stdio.h>
#include <cassert>
#include <d2d1.h>
#include <D2D1Helper.h>
#include <d2d1_1.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <wincodec.h>
#include <dwrite.h>

#include <string>

#define DEBUG 1
#define USE_GAME_INPUT_API 0

#if USE_GAME_INPUT_API
#include <GameInput.h>
#pragma comment(lib, "GameInput.lib")
#endif

#include "Utilities.cpp"
#include "Maths.cpp"

#include "Puzzle.h"

memory_arena GlobalDebugArena;
#define LOG(...) \
OutputDebugStringA(ArenaPrint(&GlobalDebugArena, __VA_ARGS__).Text)

//Platform functions
void Win32DrawTexture(int Identifier, int Index, v2 Position, v2 Size, float Angle);
void Win32Rectangle(v2 Position, v2 Dimensions, u32 FillColour, u32 BorderColour = 0);
void Win32Line(v2 Start_, v2 End_, u32 Colour, f32 Thickness);
void Win32DrawText(string String, v2 Position, v2 Size, u32 Color = 0xFF808080, bool Centered = false);
void Win32DebugOut(string String);
void Win32Sleep(int Milliseconds);
memory_arena Win32CreateMemoryArena(u64 Size, memory_arena_type Type);
span<u8> Win32LoadFile(memory_arena* Arena, char* Path);
void Win32SaveFile(char* Path, span<u8> Data);

#define PlatformDrawTexture Win32DrawTexture
#define PlatformRectangle   Win32Rectangle
#define PlatformLine        Win32Line
#define PlatformDrawText    Win32DrawText
#define PlatformDebugOut    Win32DebugOut
#define PlatformSleep       Win32Sleep
#define PlatformLoadFile    Win32LoadFile
#define PlatformSaveFile    Win32SaveFile

//TODO: Make into a platform function or combine with PlatformDrawText()
f32 DrawString(v2 Position, string String, f32 Size);

//TODO: Fix
static inline void 
Win32Rectangle(rect Rect, u32 FillColor, u32 BorderColor = 0)
{
	Win32Rectangle(Rect.MinCorner, Rect.MaxCorner - Rect.MinCorner, FillColor, BorderColor);
}

#include "GUI.cpp"
#include "Puzzle.cpp"

struct win32_d2d_screen_buffer
{
	int Width, Height;
    uint32_t* Buffer;
	HDC DeviceContext;
	ID2D1Factory1* D2DFactory;
	ID2D1Device* D2DDevice;
	ID2D1DeviceContext* D2DDeviceContext;
	ID2D1HwndRenderTarget* WindowRenderTarget;
	IDXGISwapChain1* SwapChain;
	D2D1::Matrix3x2F Transform;
	IDWriteTextFormat* TextFormat;
};

struct d2d_texture
{
	int Width, Height;
	ID2D1Bitmap* Bitmap;
};

d2d_texture	GlobalTextures[ 1 ];
win32_d2d_screen_buffer	GlobalScreen;

std::string GlobalTextInput;

void ToggleFullscreen(HWND Window);
//int LoadTextures(texture_info* Requests, int Count, d2d_texture* Textures);
ID2D1Bitmap1* Win32LoadBitmap(char* Path, memory_arena* Arena);
void DrawBitmap(rect Destination, rect Source, ID2D1Bitmap1* Bitmap);
ID2D1Bitmap1* Win32LoadFont(char* Path, memory_arena* Arena);

void DrawString(v2 Position, string String);

struct input_state
{
    button_state Buttons;
    v2 Cursor;
    v2 Movement;
};

void KeyboardAndMouseInputState(input_state* InputState, HWND Window);

#if USE_GAME_INPUT_API
void ControllerState(input_state* InputState, IGameInput* GameInput);
#endif

LRESULT CALLBACK WindowProc(HWND Window, UINT Message, WPARAM wParam, LPARAM lParam);

int BufferWidth = 1280, BufferHeight = 720;

int WINAPI wWinMain(HINSTANCE Instance, HINSTANCE, LPWSTR CommandLine, int ShowCode)
{
	CoInitializeEx(0, COINIT_MULTITHREADED);
    
	WNDCLASS WindowClass = {};
	WindowClass.lpfnWndProc = WindowProc;
	WindowClass.hInstance = Instance;
	WindowClass.hCursor = LoadCursor(0, IDC_ARROW);
	WindowClass.lpszClassName = L"MainWindow";
	
	if (!RegisterClass(&WindowClass))
	{
		return -1;
	}
	
	RECT ClientRect = { 0, 0, BufferWidth, BufferHeight };
	AdjustWindowRect(&ClientRect, WS_OVERLAPPEDWINDOW, FALSE);
	
	HWND Window = CreateWindow(
                               WindowClass.lpszClassName,
                               L"Puzzle Game",
                               WS_OVERLAPPEDWINDOW,
                               CW_USEDEFAULT, CW_USEDEFAULT, 
                               ClientRect.right - ClientRect.left, ClientRect.bottom - ClientRect.top,
                               0, 0, Instance, 0
                               );
    
	if (D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, __uuidof(ID2D1Factory1), (void**) &GlobalScreen.D2DFactory) != S_OK)
	{
		return -1;
	}
    
	D3D_FEATURE_LEVEL FeatureLevels[] = { D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1, 
		D3D_FEATURE_LEVEL_10_0, D3D_FEATURE_LEVEL_9_3, D3D_FEATURE_LEVEL_9_2, D3D_FEATURE_LEVEL_9_1 };
    
	ID3D11Device* D3DDevice;
	ID3D11DeviceContext* D3DDeviceContext;
	D3D_FEATURE_LEVEL FeatureLevel;
    
	D3D11CreateDevice(0, //Default adapter 
                      D3D_DRIVER_TYPE_HARDWARE,
                      0, //No software driver
                      D3D11_CREATE_DEVICE_BGRA_SUPPORT, //Creation flags
                      FeatureLevels,
                      ArrayCount(FeatureLevels),
                      D3D11_SDK_VERSION,
                      &D3DDevice,
                      &FeatureLevel,
                      &D3DDeviceContext);
    
	IDXGIDevice* DxgiDevice;
	D3DDevice->QueryInterface(IID_PPV_ARGS(&DxgiDevice));
    
	GlobalScreen.D2DFactory->CreateDevice(DxgiDevice, &GlobalScreen.D2DDevice);
    
	GlobalScreen.D2DDevice->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, &GlobalScreen.D2DDeviceContext);
    
	DXGI_SWAP_CHAIN_DESC1 SwapChainDesc = {};
    
	SwapChainDesc.Width = BufferWidth;                           // use automatic sizing
	SwapChainDesc.Height = BufferHeight;
	SwapChainDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM; // this is the most common swapchain format
	SwapChainDesc.Stereo = false;
	SwapChainDesc.SampleDesc.Count = 1;                // don't use multi-sampling
	SwapChainDesc.SampleDesc.Quality = 0;
	SwapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	SwapChainDesc.BufferCount = 2;                     // use double buffering to enable flip
	SwapChainDesc.Scaling = DXGI_SCALING_STRETCH;
	SwapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL; // all apps must use this SwapEffect
	SwapChainDesc.Flags = 0;
    
	IDXGIAdapter* DxgiAdapter;
	DxgiDevice->GetAdapter(&DxgiAdapter);
    
	IDXGIFactory2* DxgiFactory;
	DxgiAdapter->GetParent(IID_PPV_ARGS(&DxgiFactory));
    
	DxgiFactory->CreateSwapChainForHwnd(D3DDevice, Window, &SwapChainDesc, 0, 0, &GlobalScreen.SwapChain);
    
	ID3D11Texture2D* BackBuffer;
	GlobalScreen.SwapChain->GetBuffer(0, IID_PPV_ARGS(&BackBuffer));
    
	D2D1_BITMAP_PROPERTIES1 BitmapProperties = 
		D2D1::BitmapProperties1(D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW, D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_IGNORE));
    
	IDXGISurface* DxgiBackBuffer;
	GlobalScreen.SwapChain->GetBuffer(0, IID_PPV_ARGS(&DxgiBackBuffer));
    
	ID2D1Bitmap1* D2DTargetBitmap;
	GlobalScreen.D2DDeviceContext->CreateBitmapFromDxgiSurface(DxgiBackBuffer, BitmapProperties, &D2DTargetBitmap);
    
	GlobalScreen.D2DDeviceContext->SetTarget(D2DTargetBitmap);
    
	ShowWindow(Window, ShowCode);
    
	ID3D11SamplerState* samplerState;
    
	D3D11_SAMPLER_DESC samplerDesc = {};
	samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT; // Set to point sampling
	samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
	samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
	samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
	samplerDesc.MaxAnisotropy = 1; // Disable anisotropic filtering
	samplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
	samplerDesc.MinLOD = 0;
	samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;
    
	D3DDevice->CreateSamplerState(&samplerDesc, &samplerState);
	D3DDeviceContext->PSSetSamplers(0, 1, &samplerState);
    
	GlobalScreen.DeviceContext = GetDC(Window);
	
	GlobalScreen.Width = BufferWidth;
	GlobalScreen.Height = BufferHeight;
    
	IDWriteFactory* DirectWriteFactory = 0;
	DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), (IUnknown**)&DirectWriteFactory);
    
	HRESULT Result = DirectWriteFactory->CreateTextFormat(L"Calibri", 0, 
                                                          DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 
                                                          0.015f, L"", &GlobalScreen.TextFormat);
    
	//GlobalScreen.Transform = D2D1::Matrix3x2F((f32)BufferWidth, 0, 0, -(f32)BufferHeight, 0, (f32)BufferHeight);
    
	//LoadTextures(TextureLoadRequests, ArrayLength(TextureLoadRequests), GlobalTextures);
	
	memory_arena TransientArena = Win32CreateMemoryArena(Megabytes(16), TRANSIENT);
	memory_arena PermanentArena = Win32CreateMemoryArena(Megabytes(16), PERMANENT);
	GlobalDebugArena = Win32CreateMemoryArena(Megabytes(1), PERMANENT);
    
    LOG("Logging started\n");
    
	allocator Allocator = {};
	Allocator.Transient = &TransientArena;
	Allocator.Permanent = &PermanentArena;
    
#if USE_GAME_INPUT_API
    LOG("Using GameInput API");
    
	IGameInput* GameInput = 0;
	GameInputCreate(&GameInput);
#endif
    
	game_state* GameState = GameInitialise(Allocator);
	
	LARGE_INTEGER CounterFrequency;
	QueryPerformanceFrequency(&CounterFrequency); //Counts per second
    
	int TargetFrameRate = 60;
	float SecondsPerFrame = 1.0f / (float)TargetFrameRate;
	int CountsPerFrame = (int)(CounterFrequency.QuadPart / TargetFrameRate);
    
	game_input PreviousInput = {};
    
    ID2D1Bitmap1* Bitmap = Win32LoadFont("assets/TitilliumWeb-Regular.ttf", &TransientArena);
    
    while (true)
	{
		LARGE_INTEGER StartCount;
		QueryPerformanceCounter(&StartCount);
		
		MSG Message;
		while (PeekMessage(&Message, 0, 0, 0, PM_REMOVE))
		{
			if (Message.message == WM_QUIT)
			{
				return 0;
			}
			
			TranslateMessage(&Message);
			DispatchMessage(&Message);
		}
        
        game_input Input = {};
		input_state CurrentInputState = {};
        
		if (GetActiveWindow())
		{
            KeyboardAndMouseInputState(&CurrentInputState, Window);
#if USE_GAME_INPUT_API
            ControllerState(&CurrentInputState, GameInput);
#endif
        }
        
        Input.Button = CurrentInputState.Buttons;
        Input.ButtonDown = (~PreviousInput.Button & CurrentInputState.Buttons);
        Input.ButtonUp = (PreviousInput.Button & ~CurrentInputState.Buttons);
        Input.Movement = CurrentInputState.Movement;
        
        if (LengthSq(Input.Movement) > 1.0f)
            Input.Movement = UnitV(Input.Movement);
        
        Input.Cursor = CurrentInputState.Cursor;
        Input.TextInput = (char*)GlobalTextInput.c_str();
        
        PreviousInput = Input;
        
        GlobalScreen.D2DDeviceContext->BeginDraw();
        
        GlobalScreen.D2DDeviceContext->Clear(D2D1::ColorF(0));
        
        GameUpdateAndRender(GameState, SecondsPerFrame, &Input, Allocator);
        ResetArena(&TransientArena);
        
        /*
        rect BitmapDest = {{0.0f, 0.0f}, {1.0f, 0.5625f}};
        rect BitmapSrc  = {{0.0f, 0.0f}, {700.0f, 350.0f}};
        DrawBitmap(BitmapDest, BitmapSrc, Bitmap);
        */
        GlobalScreen.D2DDeviceContext->EndDraw();
        GlobalScreen.SwapChain->Present(0, 0);
        
        GlobalTextInput.clear();
        
        LARGE_INTEGER PerformanceCount;
        QueryPerformanceCounter(&PerformanceCount);
        if (PerformanceCount.QuadPart - StartCount.QuadPart > CountsPerFrame)
        {
            OutputDebugStringA("Can't keep up\n");
        }
        float TimeTaken = (float)(PerformanceCount.QuadPart - StartCount.QuadPart) / CounterFrequency.QuadPart;
        float CurrentFrameRate = 1.0f / TimeTaken;
        
        while (PerformanceCount.QuadPart - StartCount.QuadPart < CountsPerFrame)
        {
            QueryPerformanceCounter(&PerformanceCount);
        }
	}
}

LRESULT CALLBACK WindowProc(HWND Window, UINT Message, WPARAM wParam, LPARAM lParam)
{
	switch (Message)
	{
		case WM_DESTROY:
		{
			PostQuitMessage(0);
			return 0;
		}
        case WM_CHAR:
        {
            Assert(wParam < 128);
            char Char = (char)wParam;
            
            if (Char == '\r')
                Char = '\n';
            
            GlobalTextInput.append(1, Char);
        } break;
        
		case WM_SIZE:
		{
			u32 Width = LOWORD(lParam);
			u32 Height = HIWORD(lParam);
            
			HRESULT Result = GlobalScreen.SwapChain->ResizeBuffers(2, Width, Height, DXGI_FORMAT_UNKNOWN, 0);
			int x = 3;
		} break;
		default:
		{
			return DefWindowProc(Window, Message, wParam, lParam);
		}
	}	
	return 0;
}

static void
KeyboardAndMouseInputState(input_state* InputState, HWND Window)
{
    //Buttons
    if (GetAsyncKeyState('W') & 0x8000)
        InputState->Buttons |= Button_Jump;
    if (GetAsyncKeyState('E') & 0x8000)
        InputState->Buttons |= Button_Interact;
    if (GetAsyncKeyState('C') & 0x8000)
        InputState->Buttons |= Button_Menu;
    if (GetAsyncKeyState(VK_LBUTTON) & 0x8000)
        InputState->Buttons |= Button_LMouse;
    if (GetAsyncKeyState(VK_LSHIFT) & 0x8000)
        InputState->Buttons |= Button_LShift;
    if (GetAsyncKeyState(VK_F1) & 0x8000)
        InputState->Buttons |= Button_Console;
    
    //Movement
    if ((GetAsyncKeyState('A') & 0x8000))
        InputState->Movement.X -= 1.0f;
    if ((GetAsyncKeyState('D') & 0x8000))
        InputState->Movement.X += 1.0f;
    
    //Cursor
    POINT CursorPos = {};
    GetCursorPos(&CursorPos);
    ScreenToClient(Window, &CursorPos);
    
    RECT ClientRect;
    GetClientRect(Window, &ClientRect);
    
    if (ClientRect.right > 0 && ClientRect.bottom > 0)
    {
        InputState->Cursor.X += (f32)CursorPos.x / ClientRect.right;
        InputState->Cursor.Y += (f32)(ClientRect.bottom - CursorPos.y) / ClientRect.right;
    }
}

#if USE_GAME_INPUT_API
static void
ControllerState(input_state* InputState, IGameInput* GameInput)
{
    IGameInputReading* GameInputCurrentReading;
    HRESULT Result = GameInput->GetCurrentReading(GameInputKindController | GameInputKindGamepad, 
                                                  0, &GameInputCurrentReading);
    if (FAILED(Result)) return;
    
    //TODO: Disallow device being changed
    /*
    if (!GameInputDevice)
    {
        GameInputCurrentReading->GetDevice(&GameInputDevice);
    }
*/
    
    GameInputKind InputKind = GameInputCurrentReading->GetInputKind();
    
    f32 AnalogStickDeadzone = 0.2f;
    
    if (InputKind & GameInputKindGamepad)
    {
        GameInputGamepadState Gamepad;
        GameInputCurrentReading->GetGamepadState(&Gamepad);
        
        if (Abs(Gamepad.leftThumbstickX) > AnalogStickDeadzone)
            InputState->Movement.X += Gamepad.leftThumbstickX;
        
        if (Gamepad.buttons & GameInputGamepadA)
            InputState->Buttons |= Button_Jump;
    }
    else if (InputKind & GameInputKindControllerAxis)
    {
        f32 ControllerRawXAxis;
        if (GameInputCurrentReading->GetControllerAxisState(1, &ControllerRawXAxis) == 1)
        {
            f32 ControllerXAxis = -1.0f + 2.0f * ControllerRawXAxis;
            
            if (Abs(ControllerXAxis) > AnalogStickDeadzone) 
                InputState->Movement.X += ControllerXAxis;
        }
        
        bool ButtonStates[10] = {};
        GameInputCurrentReading->GetControllerButtonState(ArrayCount(ButtonStates), ButtonStates);
        
        if (ButtonStates[1])
            InputState->Buttons |= Button_Jump;
    }
    
    GameInputCurrentReading->Release();
}
#endif

void ToggleFullscreen(HWND Window)
{
	static WINDOWPLACEMENT PreviousWindowPlacement = { sizeof(PreviousWindowPlacement) };
	
	DWORD Style = GetWindowLong(Window, GWL_STYLE);
	
	if (Style & WS_OVERLAPPEDWINDOW)
	{
		MONITORINFO MonitorInfo = { sizeof(MonitorInfo) };
        
		//TODO: Check return values
		GetWindowPlacement(Window, &PreviousWindowPlacement);
		GetMonitorInfo(MonitorFromWindow(Window, MONITOR_DEFAULTTOPRIMARY), &MonitorInfo);
		SetWindowLong(Window, GWL_STYLE, Style & ~WS_OVERLAPPEDWINDOW);
		
		SetWindowPos(
                     Window, HWND_TOP,
                     MonitorInfo.rcMonitor.left, MonitorInfo.rcMonitor.top,
                     MonitorInfo.rcMonitor.right - MonitorInfo.rcMonitor.left,
                     MonitorInfo.rcMonitor.bottom - MonitorInfo.rcMonitor.top,
                     SWP_NOOWNERZORDER | SWP_FRAMECHANGED
                     );
	}
	else
	{
		SetWindowLong(Window, GWL_STYLE, Style | WS_OVERLAPPEDWINDOW);
		SetWindowPlacement(Window, &PreviousWindowPlacement);
		SetWindowPos(
                     Window, NULL, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER |
                     SWP_NOOWNERZORDER | SWP_FRAMECHANGED
                     );
	}
}

static ID2D1Bitmap1*
CreateBitmapFromData(u32* Pixels, i32 Width, i32 Height)
{
    ID2D1Bitmap1* Bitmap;
    
    D2D1_BITMAP_PROPERTIES1 BitmapProperties = {};
    BitmapProperties.pixelFormat.format = DXGI_FORMAT_R8G8B8A8_UNORM;
    BitmapProperties.pixelFormat.alphaMode = D2D1_ALPHA_MODE_PREMULTIPLIED;
    
    HRESULT Result = GlobalScreen.D2DDeviceContext->CreateBitmap(D2D1::SizeU(Width, Height),
                                                                 Pixels, Width * 4, BitmapProperties,
                                                                 &Bitmap);
    return Bitmap;
}

static ID2D1Bitmap1*
Win32LoadBitmap(char* Path, memory_arena* Arena)
{
    Assert(Arena->Type == TRANSIENT);
    
    ID2D1Bitmap1* Bitmap = 0;
    
    span<u8> Data = Win32LoadFile(Arena, Path);
    if (Data.Memory)
    {
        BITMAPFILEHEADER* Header = (BITMAPFILEHEADER*)Data.Memory;
        BITMAPINFOHEADER* InfoHeader = (BITMAPINFOHEADER*)(Data.Memory + 0x0E);
        
        u8* Pixels = Data + Header->bfOffBits;
        i32 Width = InfoHeader->biWidth;
        i32 Height = InfoHeader->biHeight;
        
        Bitmap = CreateBitmapFromData((u32*)Pixels, Width, Height);
    }
    if (!Bitmap)
    {
        LOG("Could not load texture %s\n", Path);
    }
    
    return Bitmap;
}

static void
DrawBitmap(rect Destination, rect Source, ID2D1Bitmap1* Bitmap)
{
    D2D1::Matrix3x2F Transform = D2D1::Matrix3x2F((f32)BufferWidth, 0, 0, -(f32)BufferWidth, 0, (f32)BufferHeight);
	GlobalScreen.D2DDeviceContext->SetTransform(Transform);
    
    //TODO: Is it bad to have this?
    D2D1_RECT_F Dest = *(D2D1_RECT_F*)&Destination;
    D2D1_RECT_F Src  = *(D2D1_RECT_F*)&Source;
    
    GlobalScreen.D2DDeviceContext->DrawBitmap(Bitmap, Dest, 1.0f, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR, Src);
}

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

stbtt_bakedchar FontBakedChars[128];
ID2D1Bitmap1* FontBitmap;

static ID2D1Bitmap1*
Win32LoadFont(char* Path, memory_arena* Arena)
{
    Assert(Arena->Type == TRANSIENT);
    
    span<u8> Data = Win32LoadFile(Arena, Path);
    
    f32 FontHeight = 60.0f;
    i32 Width = 512;
    i32 Height = 512;
    
    u8* STBPixels = Alloc(Arena, 512 * 512);
    
    stbtt_BakeFontBitmap(Data.Memory, 0, FontHeight, STBPixels, Width, Height, 0, 128, FontBakedChars);
    
    u32* D2DPixels = (u32*)Alloc(Arena, 512 * 512 * 4);
    
    u8* Src = STBPixels + Width * (Height - 1);
    u32* Dest = D2DPixels;
    for (int Y = 0; Y < Height; Y++)
    {
        for (int X = 0; X < Width; X++)
        {
            u32 Val = Src[X];
            Dest[X] = (Val << 24 | Val << 16 | Val << 8 | Val);
        }
        Dest += Width;
        Src -= Width;
    }
    
    ID2D1Bitmap1* D2DBitmap = CreateBitmapFromData(D2DPixels, Width, Height);
    FontBitmap = D2DBitmap;
    return D2DBitmap;
}

static f32
DrawString(v2 Position, string String, f32 Size)
{
    f32 Scale = Size / 60.0f;
    
    f32 StartX = Position.X;
    Position.Y += 0.25f * Size;
    
    for (u32 I = 0; I < String.Length; I++)
    {
        char C = String.Text[I];
        if (C < 128)
        {
            stbtt_bakedchar BakedChar = FontBakedChars[C];
            rect Source = {{(f32)BakedChar.x0, (f32)(512 - BakedChar.y0)}, {(f32)BakedChar.x1, (f32)(512 - BakedChar.y1)}};
            
            v2 CharSizePixels = Source.MaxCorner - Source.MinCorner;
            v2 OffsetPixels = {(f32)BakedChar.xoff, (f32)-BakedChar.yoff};
            
            v2 MinCorner = Position + Scale * OffsetPixels;
            v2 MaxCorner = MinCorner + Scale * CharSizePixels;
            rect Dest = {MinCorner, MaxCorner};
            
            DrawBitmap(Dest, Source, FontBitmap);
            
            Position.X += Scale * BakedChar.xadvance;
        }
    }
    f32 Width = Position.X - StartX;
    return Width;
}

#if 0
int LoadTextures(texture_info* Requests, int Count, d2d_texture* Textures)
{
    int TexturesLoaded = 0;
    
    IWICImagingFactory* WicFactory = 0;
    HRESULT Result = CoCreateInstance(CLSID_WICImagingFactory, 0, CLSCTX_INPROC_SERVER, IID_IWICImagingFactory, (LPVOID*)&WicFactory);
    
    if (WicFactory)
    {
        for (int i = 0; i < Count; i++)
        {
            texture_info Request = Requests[i];
            d2d_texture* Texture = Textures + Request.Identifier;
            
            Texture->Width = Request.Width;
            Texture->Height = Request.Height;
            
            wchar_t FilenameWide[MAX_PATH];
            if (MultiByteToWideChar(CP_UTF8, MB_PRECOMPOSED, Request.Filename, -1, FilenameWide, ArrayLength(FilenameWide)))
            {
                IWICBitmapDecoder* WicDecoder = 0;
                WicFactory->CreateDecoderFromFilename(FilenameWide, 0, GENERIC_READ, WICDecodeMetadataCacheOnLoad, &WicDecoder);
                
                if (WicDecoder)
                {
                    IWICBitmapFrameDecode* WicFrame = 0;
                    WicDecoder->GetFrame(0, &WicFrame);
                    
                    if (WicFrame)
                    {
                        IWICFormatConverter* WicConverter = 0;
                        WicFactory->CreateFormatConverter(&WicConverter);
                        
                        if (WicConverter)
                        {
                            WicConverter->Initialize(WicFrame, GUID_WICPixelFormat32bppPBGRA, WICBitmapDitherTypeNone, 0, 0.0, WICBitmapPaletteTypeCustom);
                            
                            GlobalScreen.D2DDeviceContext->CreateBitmapFromWicBitmap(WicConverter, 0, &Texture->Bitmap);
                            WicConverter->Release();
                        }
                        WicFrame->Release();
                    }
                    WicDecoder->Release();
                }
            }
            else
            {
                //Could not convert
                OutputDebugStringA("Could not convert filename to wide characters\n");
            }
        }
        WicFactory->Release();
    }
    
    return TexturesLoaded;
}
#endif

void Win32DrawTexture(int Identifier, int Index, v2 Position, v2 Size, float Angle)
{
    //assert(Identifier < Identifier_Count);
    
    d2d_texture Texture = GlobalTextures[Identifier];
    
    int TexturesAcross = (int)(Texture.Bitmap->GetSize().width / Texture.Width);
    if (TexturesAcross == 0)
    {
        TexturesAcross = 1;
    }
    
    float SrcX = (float)(Texture.Width * (Index % TexturesAcross));
    float SrcY = (float)(Texture.Height * (Index / TexturesAcross));
    float SrcWidth = (float)Texture.Width;
    float SrcHeight = (float)Texture.Height;
    
    D2D1_RECT_F Src = { SrcX, SrcY, SrcX + SrcWidth, SrcY + SrcHeight };
    D2D1_RECT_F Dest = { Position.X, Position.Y, Position.X + Size.X, Position.Y + Size.Y };
    
    if (Angle)
    {
        v2 Centre = Position + 0.5f * Size;
        GlobalScreen.D2DDeviceContext->SetTransform(D2D1::Matrix3x2F::Rotation(RadiansToDegrees(Angle), D2D1::Point2F(Centre.X, Centre.Y)));
        GlobalScreen.D2DDeviceContext->DrawBitmap(Texture.Bitmap, Dest, 1.0f, D2D1_BITMAP_INTERPOLATION_MODE_NEAREST_NEIGHBOR, Src);
        GlobalScreen.D2DDeviceContext->SetTransform(D2D1::Matrix3x2F::Identity());
    }
    else
    {
        GlobalScreen.D2DDeviceContext->DrawBitmap(Texture.Bitmap, Dest, 1.0f, D2D1_BITMAP_INTERPOLATION_MODE_NEAREST_NEIGHBOR, Src);
    }
}

void Win32Rectangle(v2 Position, v2 Size, uint32_t FillColour, uint32_t BorderColour)
{
    D2D1_RECT_F RectDim = { Position.X, Position.Y, 
        (Position.X + Size.X), (Position.Y + Size.Y) };
    
    D2D1::Matrix3x2F Transform = D2D1::Matrix3x2F((f32)BufferWidth, 0, 0, -(f32)BufferWidth, 0, (f32)BufferHeight);
    GlobalScreen.D2DDeviceContext->SetTransform(Transform);
    
    //TODO get rid of this
    static ID2D1SolidColorBrush* Brush;
    if (!Brush)
    {
        GlobalScreen.D2DDeviceContext->CreateSolidColorBrush(D2D1::ColorF(0), &Brush);
    }
    
    Brush->SetColor(D2D1::ColorF(FillColour & 0xFFFFFF, (float)((FillColour >> 24) / 255.0f)));
    GlobalScreen.D2DDeviceContext->FillRectangle(RectDim, Brush);
    
    if (BorderColour)
    {
        Brush->SetColor(D2D1::ColorF(BorderColour & 0xFFFFFF, (float)((BorderColour >> 24) / 255.0f)));
        GlobalScreen.D2DDeviceContext->DrawRectangle(RectDim, Brush, 0.005f);
    }
}

void Win32Line(v2 Start_, v2 End_, u32 Colour, f32 Thickness)
{
    D2D1_POINT_2F Start = { Start_.X, Start_.Y };
    D2D1_POINT_2F End = { End_.X, End_.Y };
    
    static ID2D1SolidColorBrush* Brush;
    if (!Brush)
    {
        GlobalScreen.D2DDeviceContext->CreateSolidColorBrush(D2D1::ColorF(0), &Brush);
    }
    
    Brush->SetColor(D2D1::ColorF(Colour & 0xFFFFFF, (float)((Colour >> 24) / 255.0f)));
    
    D2D1::Matrix3x2F Transform = D2D1::Matrix3x2F((f32)BufferWidth, 0, 0, -(f32)BufferWidth, 0, (f32)BufferHeight);
    GlobalScreen.D2DDeviceContext->SetTransform(Transform);
    GlobalScreen.D2DDeviceContext->DrawLine(Start, End, Brush, Thickness, 0);
}

void Win32DrawText(string String, v2 Position, v2 Size, u32 Color, bool Centered)
{
    Position.Y = ScreenTop - Position.Y - Size.Y;
    
    D2D1::Matrix3x2F Transform = D2D1::Matrix3x2F((f32)BufferWidth, 0, 0, (f32)BufferWidth, 0, 0);
    GlobalScreen.D2DDeviceContext->SetTransform(Transform);
    
    Assert(String.Length < 1000);
    wchar_t Text[1000];
    int TextLength = MultiByteToWideChar(CP_UTF8, MB_PRECOMPOSED, String.Text, String.Length, Text, ArrayCount(Text));
    
    D2D1_RECT_F Rect = D2D1::RectF(Position.X, Position.Y, Position.X + Size.X, Position.Y + Size.Y);
    
    static ID2D1SolidColorBrush* Brush;
    if (!Brush)
    {
        GlobalScreen.D2DDeviceContext->CreateSolidColorBrush(D2D1::ColorF(0), &Brush);
    }
    
    Brush->SetColor(D2D1::ColorF(Color & 0xFFFFFF, (float)((Color >> 24) / 255.0f)));
    
    GlobalScreen.TextFormat->SetTextAlignment(Centered ? DWRITE_TEXT_ALIGNMENT_CENTER : DWRITE_TEXT_ALIGNMENT_LEADING);
    GlobalScreen.D2DDeviceContext->DrawText(Text, TextLength, GlobalScreen.TextFormat, Rect, Brush);
}

void Win32DebugOut(string String)
{
    OutputDebugStringA(String.Text);
}

void Win32Sleep(int Milliseconds)
{
    Sleep(Milliseconds);
}

static memory_arena
Win32CreateMemoryArena(u64 Size, memory_arena_type Type)
{
    memory_arena Arena = {};
    
    Arena.Buffer = (u8*)VirtualAlloc(0, Size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    Arena.Size = Size;
    Arena.Type = Type;
    
    return Arena;
}

static void
Win32DeleteMemoryArena(memory_arena* Arena)
{
    VirtualFree(Arena->Buffer, 0, MEM_RELEASE);
    *Arena = {};
}

static span<u8> 
Win32LoadFile(memory_arena* Arena, char* Path)
{
    span<u8> Result = {};
    bool Success = false;
    
    HANDLE File = CreateFileA(Path, GENERIC_READ, 0, 0, OPEN_EXISTING, 0, 0);
    
    if (File != INVALID_HANDLE_VALUE)
    {
        u64 FileSize;
        if (GetFileSizeEx(File, (LARGE_INTEGER*)&FileSize))
        {
            Result.Memory = Alloc(Arena, FileSize);
            DWORD Length;
            if (ReadFile(File, Result.Memory, (u32)FileSize, &Length, 0))
            {
                Success = true;
                Result.Count = Length;
            }
        }
        CloseHandle(File);
    }
    
#if DEBUG
    if (Success)
        LOG("Opened file: %s\n", Path);
    else
        LOG("Could not open file: %s\n", Path); 
#endif
    
    return Result;
}

static void
Win32SaveFile(char* Path, span<u8> Data)
{
    HANDLE File = CreateFileA(Path, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, 0);
    
    bool Success = false;
    
    if (File != INVALID_HANDLE_VALUE)
    {
        DWORD BytesWritten;
        if (WriteFile(File, Data.Memory, Data.Count, &BytesWritten, 0))
        {
            Success = true;
        }
        CloseHandle(File);
    }
    
#if DEBUG
    if (Success)
        LOG("Saved file: %s\n", Path);
    else
        LOG("Could not save file: %s\n", Path); 
#endif
}