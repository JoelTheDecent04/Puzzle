#define UNICODE
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <intrin.h>

#include "Utilities.cpp"
#include "Maths.cpp"

#include "Puzzle.h"

memory_arena GlobalDebugArena;
#define LOG(...) \
OutputDebugStringA(ArenaPrint(&GlobalDebugArena, __VA_ARGS__).Text); \
OutputDebugStringA("\n")

struct back_buffer
{
    u16 Width, Height;
    u16 DispWidth, DispHeight;
    f32 Scale;
    u32* Pixels;
    
    i32 ThreadY0;
    i32 ThreadY1;
};

struct render_queue_entry
{
    render_group* RenderGroup;
    back_buffer Buffer;
};

struct render_queue
{
    HANDLE Semaphore;
    
    u32 ThreadCount;
    
    volatile u32 EntryCount;
    volatile u32 EntriesStarted;
    volatile u32 EntriesDone;
    render_queue_entry Entries[128];
};

void Win32DrawTexture(int Identifier, int Index, v2 Position, v2 Size, float Angle);
void Win32Rectangle(v2 Position, v2 Dimensions, u32 FillColour, u32 BorderColour = 0);
void Win32Circle(v2 Position, f32 Radius, u32 Color);
void Win32Line(v2 Start_, v2 End_, u32 Colour, f32 Thickness);
void Win32DrawText(string String, v2 Position, v2 Size, u32 Color = 0xFF808080, bool Centered = false);
memory_arena Win32CreateMemoryArena(u64 Size, memory_arena_type Type);
std::string GlobalTextInput;
LRESULT CALLBACK WindowProc(HWND Window, UINT Message, WPARAM wParam, LPARAM lParam);
void KeyboardAndMouseInputState(input_state* InputState, HWND Window);
void SoftwareRender(render_group* RenderGroup, back_buffer BackBuffer);
back_buffer CreateBackBuffer(int Width, int Height);
void Win32Text(back_buffer Buffer, v2 Position, string String, u32 Color);
void InitFont(char* Path, back_buffer Buffer, memory_arena* Arena);

render_queue* CreateRenderQueueAndThreads(memory_arena* Arena, u32 ThreadCount);
void RenderMultithreaded(render_queue* Queue, render_group* RenderGroup, back_buffer* Buffer);

//Platform functions
void Win32DebugOut(string String);
span<u8> Win32LoadFile(memory_arena* Arena, char* Path);
void Win32SaveFile(char* Path, span<u8> Data);
f32 Win32TextWidth(string String, f32 FontSize);

#define PlatformDebugOut Win32DebugOut
#define PlatformSleep Win32Sleep
#define PlatformLoadFile Win32LoadFile
#define PlatformSaveFile Win32SaveFile
#define PlatformTextWidth Win32TextWidth

#include "Puzzle.cpp"

int WINAPI wWinMain(HINSTANCE Instance, HINSTANCE, LPWSTR CommandLine, int ShowCode)
{
    WNDCLASS WindowClass = {};
	WindowClass.lpfnWndProc = WindowProc;
	WindowClass.hInstance = Instance;
	WindowClass.hCursor = LoadCursor(0, IDC_ARROW);
	WindowClass.lpszClassName = L"MainWindow";
	WindowClass.style = CS_OWNDC;
    
	if (!RegisterClass(&WindowClass))
	{
		return -1;
	}
	
    int BufferWidth = 1920, BufferHeight = 1080;
    
	RECT ClientRect = { 0, 0, BufferWidth, BufferHeight };
	AdjustWindowRect(&ClientRect, WS_OVERLAPPEDWINDOW, FALSE);
	
	HWND Window = CreateWindow(WindowClass.lpszClassName,
                               L"Puzzle Game",
                               WS_OVERLAPPEDWINDOW,
                               CW_USEDEFAULT, CW_USEDEFAULT, 
                               ClientRect.right - ClientRect.left, ClientRect.bottom - ClientRect.top,
                               0, 0, Instance, 0);
    
	ShowWindow(Window, ShowCode);
	
    BITMAPINFO BitmapInfo = {};
    BitmapInfo.bmiHeader.biSize = sizeof(BitmapInfo);
	BitmapInfo.bmiHeader.biWidth = BufferWidth;
	BitmapInfo.bmiHeader.biHeight = BufferHeight;
	BitmapInfo.bmiHeader.biPlanes = 1;
	BitmapInfo.bmiHeader.biBitCount = 32;
    
    HDC WindowDC = GetDC(Window);
    
    back_buffer BackBuffer = CreateBackBuffer(BufferWidth, BufferHeight);
    
	memory_arena TransientArena = Win32CreateMemoryArena(Megabytes(16), TRANSIENT);
	memory_arena PermanentArena = Win32CreateMemoryArena(Megabytes(16), PERMANENT);
	GlobalDebugArena = Win32CreateMemoryArena(Megabytes(1), PERMANENT);
    
	allocator Allocator = {};
	Allocator.Transient = &TransientArena;
	Allocator.Permanent = &PermanentArena;
    
    InitFont("assets/TitilliumWeb-Regular.ttf", BackBuffer, &TransientArena);
	game_state* GameState = GameInitialise(Allocator);
	
	LARGE_INTEGER CounterFrequency;
	QueryPerformanceFrequency(&CounterFrequency); //Counts per second
    
    bool LockFPS = false;
    
	int TargetFrameRate = 60;
	float SecondsPerFrame = 1.0f / (float)TargetFrameRate;
	int CountsPerFrame = (int)(CounterFrequency.QuadPart / TargetFrameRate);
    
	game_input PreviousInput = {};
    
    render_group RenderGroup;
    RenderGroup.ShapeCount = 0;
    
    render_queue* RenderQueue = CreateRenderQueueAndThreads(&PermanentArena, 7);
    
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
        
        GameUpdateAndRender(&RenderGroup, GameState, SecondsPerFrame, &Input, Allocator);
        
        RenderMultithreaded(RenderQueue, &RenderGroup, &BackBuffer);
        RenderGroup.ShapeCount = 0;
        
        ResetArena(&TransientArena);
        GlobalTextInput.clear();
        
        StretchDIBits(WindowDC, 
                      0, 0, ClientRect.right, ClientRect.bottom,
                      0, 0, BufferWidth, BufferHeight,
                      BackBuffer.Pixels, &BitmapInfo, DIB_RGB_COLORS, SRCCOPY);
        
        LARGE_INTEGER PerformanceCount;
        QueryPerformanceCounter(&PerformanceCount);
        if (PerformanceCount.QuadPart - StartCount.QuadPart > CountsPerFrame)
        {
            OutputDebugStringA("Can't keep up\n");
        }
        float TimeTaken = (float)(PerformanceCount.QuadPart - StartCount.QuadPart) / CounterFrequency.QuadPart;
        LOG("Frame: %.2f ms", TimeTaken * 1000.0f);
        
        if (LockFPS)
        {
            while (PerformanceCount.QuadPart - StartCount.QuadPart < CountsPerFrame)
            {
                QueryPerformanceCounter(&PerformanceCount);
            }
            SecondsPerFrame = 1.0f / (f32)TargetFrameRate;
        }
        else
        {
            SecondsPerFrame = TimeTaken;
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
            
            //LOG("Char: %u\n", Char);
            
            if (Char == '\r')
                Char = '\n';
            
            GlobalTextInput.append(1, Char);
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
    if (GetAsyncKeyState(VK_SPACE) & 0x8000)
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
    if (GetAsyncKeyState(VK_LEFT) & 0x8000)
        InputState->Buttons |= Button_Left;
    if (GetAsyncKeyState(VK_RIGHT) & 0x8000)
        InputState->Buttons |= Button_Right;
    
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

static inline u32
BlendColor(u32 A, u32 B, f32 Alpha)
{
    u32 Result = 0;
    
    u8* pA = (u8*)&A;
    u8* pB = (u8*)&B;
    u8* pResult = (u8*)&Result;
    
    for (int I = 0; I < 4; I++)
    {
        pResult[I] = (1.0f - Alpha) * pA[I] + Alpha * pB[I];
    }
    
    return Result;
}

static void
DrawRotatedRectangle(back_buffer Buffer, v2 Origin, v2 XAxis, v2 YAxis, u32 Color)
{
    v2 A = Buffer.Scale * Origin;
    v2 B = Buffer.Scale * (Origin + XAxis);
    v2 C = Buffer.Scale * (Origin + XAxis + YAxis);
    v2 D = Buffer.Scale * (Origin + YAxis);
    
    f32 MinX = Min(A.X, B.X, C.X, D.X);
    f32 MaxX = Max(A.X, B.X, C.X, D.X);
    f32 MinY = Min(A.Y, B.Y, C.Y, D.Y);
    f32 MaxY = Max(A.Y, B.Y, C.Y, D.Y);
    
    i32 X0 = Max(0, (i32)MinX);
    i32 X1 = Min(Buffer.Width, (i32)MaxX);
    i32 Y0 = Max(Buffer.ThreadY0, (i32)MinY);
    i32 Y1 = Min(Buffer.ThreadY1, (i32)MaxY);
    
    v2 PerpXAxis = UnitV(Perp(XAxis));
    v2 PerpYAxis = UnitV(Perp(YAxis));
    
    for (i32 Y = Y0; Y < Y1; Y++)
    {
        u32* Row = Buffer.Pixels + Y * Buffer.Width;
        for (int X = X0; X < X1; X++)
        {
            v2 P = V2((f32)X, (f32)Y);
            
            f32 Edge0 = DotProduct(P - A, -1.0f * PerpXAxis);
            f32 Edge1 = DotProduct(P - B, -1.0f * PerpYAxis);
            f32 Edge2 = DotProduct(P - C, PerpXAxis);
            f32 Edge3 = DotProduct(P - D, PerpYAxis);
            
            f32 PixelsOutside = Max(Edge0, Edge1, Edge2, Edge3);
            
            if (PixelsOutside < 0.0f)
            {
                f32 PixelsInside = -PixelsOutside;
                
                float Alpha = 1.0f;
                if (PixelsInside < 1.0f)
                {
                    Alpha = PixelsInside;
                }
                Row[X] = BlendColor(Row[X], Color, Alpha);
            }
        }
    }
}


static void
DrawRotatedRectangleTransparent(back_buffer Buffer, v2 Origin, v2 XAxis, v2 YAxis, u32 Color)
{
    v2 A = Buffer.Scale * Origin;
    v2 B = Buffer.Scale * (Origin + XAxis);
    v2 C = Buffer.Scale * (Origin + XAxis + YAxis);
    v2 D = Buffer.Scale * (Origin + YAxis);
    
    f32 MinX = Min(A.X, B.X, C.X, D.X);
    f32 MaxX = Max(A.X, B.X, C.X, D.X);
    f32 MinY = Min(A.Y, B.Y, C.Y, D.Y);
    f32 MaxY = Max(A.Y, B.Y, C.Y, D.Y);
    
    i32 X0 = Max(0, (i32)MinX);
    i32 X1 = Min(Buffer.Width, (i32)MaxX);
    i32 Y0 = Max(Buffer.ThreadY0, (i32)MinY);
    i32 Y1 = Min(Buffer.ThreadY1, (i32)MaxY);
    
    v2 PerpXAxis = Perp(XAxis);
    v2 PerpYAxis = Perp(YAxis);
    
    __m128 Color_m128 = _mm_cvtepi32_ps(_mm_cvtepu8_epi32(*(__m128i*)&Color));
    
    u8 Alpha_u8 = Color >> 24;
    f32 Alpha = (f32)Alpha_u8 / 255.0f;
    f32 InvAlpha = 1.0f - Alpha;
    
    __m128 Alpha_4x = _mm_set1_ps(Alpha);
    __m128 InvAlpha_4x = _mm_set1_ps(InvAlpha);
    
    __m128i Shuffle = _mm_set_epi8(0, 0, 0, 0, 
                                   0, 0, 0, 0, 
                                   0, 0, 0, 0,
                                   12, 8, 4, 0);
    
    for (i32 Y = Y0; Y < Y1; Y++)
    {
        u32* Row = Buffer.Pixels + Y * Buffer.Width;
        for (int X = X0; X < X1; X++)
        {
            v2 P = V2((f32)X + 0.5f, (f32)Y + 0.5f);
            
            f32 Edge0 = DotProduct(P - A, -1.0f * PerpXAxis);
            f32 Edge1 = DotProduct(P - B, -1.0f * PerpYAxis);
            f32 Edge2 = DotProduct(P - C, PerpXAxis);
            f32 Edge3 = DotProduct(P - D, PerpXAxis);
            
            if ((Edge0 < 0) && (Edge1 < 0) && (Edge2 < 0) && (Edge3 < 0))
            {
                __m128 OldColor = _mm_cvtepi32_ps(_mm_cvtepu8_epi32(*(__m128i*)&Row[X]));
                __m128 NewColor = _mm_add_ps(_mm_mul_ps(InvAlpha_4x, OldColor), _mm_mul_ps(Alpha_4x, Color_m128));
                __m128i NewColor_u32 = _mm_shuffle_epi8(_mm_cvtps_epi32(NewColor), Shuffle);
                _mm_store_ss((float*)&Row[X], _mm_castsi128_ps(NewColor_u32));
            }
        }
    }
}

void Win32DebugOut(string String)
{
	OutputDebugStringA(String.Text);
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
	{
		OutputDebugStringA("Loaded file: ");
		OutputDebugStringA(Path);
		OutputDebugStringA("\n");
        
	}
	else
	{
		OutputDebugStringA("Could not load file: ");
		OutputDebugStringA(Path);
		OutputDebugStringA("\n");
	}
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
	{
		OutputDebugStringA("Saved file: ");
		OutputDebugStringA(Path);
		OutputDebugStringA("\n");
	}
	else
	{
		OutputDebugStringA("Could not save file: ");
		OutputDebugStringA(Path);
		OutputDebugStringA("\n");
	}
#endif
}

inline u32 
GetPixel(back_buffer Buffer, int X, int Y)
{
    u32 Index = Y * Buffer.Width + X;
    u32 RGB = Buffer.Pixels[Index];
    return RGB;
}

inline void 
SetPixel(back_buffer Buffer, int X, int Y, u32 RGB)
{
    u32 Index = Y * Buffer.Width + X;
    Buffer.Pixels[Index] = RGB;
}

void BlendPixel(back_buffer Buffer, int X, int Y, u32 Color)
{
    u32 OldColor = GetPixel(Buffer, X, Y);
    
    u8 OldR = OldColor >> 16;
    u8 OldG = OldColor >> 8;
    u8 OldB = OldColor;
    
    f32 A = (f32)(Color >> 24) / 255.0f;
    u8 R = Color >> 16;
    u8 G = Color >> 8;
    u8 B = Color;
    
    u8 NewR = (u8)(A * R + (1 - A) * OldR);
    u8 NewG = (u8)(A * G + (1 - A) * OldG);
    u8 NewB = (u8)(A * B + (1 - A) * OldB);
    
    u32 RGB = (NewR << 16) | (NewG << 8) | NewB;
    SetPixel(Buffer, X, Y, RGB);
}

void Win32Rectangle(back_buffer Buffer, v2 Position, v2 Size, uint32_t Color)
{
    Assert(Buffer.Width % 16 == 0);
    Assert(Buffer.Height % 16 == 0);
    
    i16 X0 = (i16)(Position.X * Buffer.Scale);
    i16 X1 = (i16)((Position.X + Size.X) * Buffer.Scale) + 1;
    i16 Y0 = (i16)(Position.Y * Buffer.Width);
    i16 Y1 = (i16)((Position.Y + Size.Y) * Buffer.Scale);
    
    int IterX0 = Clamp(RoundDownToMultipleOf(8, X0), 0, RoundDownToMultipleOf(8, Buffer.Width - 1));
    int IterX1 = Clamp(RoundDownToMultipleOf(8, X1), 0, RoundDownToMultipleOf(8, Buffer.Width - 1));
    int IterY0 = Clamp(Floor(Y0), Buffer.ThreadY0, Buffer.ThreadY1);
    int IterY1 = Clamp(Floor(Y1), Buffer.ThreadY0, Buffer.ThreadY1);
    
    __m256i X0_8x = _mm256_set1_epi32(X0);
    __m256i X1_8x = _mm256_set1_epi32(X1 + 1);
    
    __m256i XOffsets_8x = _mm256_set_epi32(7, 6, 5, 4, 3, 2, 1, 0);
    
    __m256i Color_8x = _mm256_set1_epi32(Color);
    
    for (int Y = IterY0; Y < IterY1; Y++)
    {
        for (int X = IterX0; X <= IterX1; X += 8)
        {
            u32 Index = Y * Buffer.Width + X;
            
            __m256i X_8x = _mm256_add_epi32(_mm256_set1_epi32(X), XOffsets_8x);
            
            // (X >= X0) & (X < X1) 
            __m256i Mask_8x = _mm256_and_si256(_mm256_cmpgt_epi32(X_8x, X0_8x), _mm256_cmpgt_epi32(X1_8x, X_8x));
            
            __m256i OldColor_8x = _mm256_load_si256((__m256i*)(Buffer.Pixels + Index));
            
            _mm256_store_si256((__m256i*)(Buffer.Pixels + Index), _mm256_blendv_epi8(OldColor_8x, Color_8x, Mask_8x));
        }
    }
}


//This routine uses SSE / 4 lane SIMD
void Win32RectangleTransparent(back_buffer Buffer, v2 Position, v2 Size, uint32_t Color)
{
    f32 X0 = (Position.X) * Buffer.Scale;
    f32 X1 = (Position.X + Size.X) * Buffer.Scale;
    f32 Y0 = (Position.Y) * Buffer.Scale;
    f32 Y1 = (Position.Y + Size.Y) * Buffer.Scale;
    
    int IterX0 = Clamp(Floor(X0), 0, Buffer.Width - 1);
    int IterX1 = Clamp(Floor(X1), 0, Buffer.Width - 1);
    int IterY0 = Clamp(Floor(Y0), Buffer.ThreadY0, Buffer.ThreadY1);
    int IterY1 = Clamp(Floor(Y1), Buffer.ThreadY0, Buffer.ThreadY1);
    
    __m128 Color_m128 = _mm_cvtepi32_ps(_mm_cvtepu8_epi32(*(__m128i*)&Color));
    
    u8 Alpha_u8 = Color >> 24;
    f32 Alpha = (f32)Alpha_u8 / 255.0f;
    f32 InvAlpha = 1.0f - Alpha;
    
    __m128 Alpha_4x = _mm_set1_ps(Alpha);
    __m128 InvAlpha_4x = _mm_set1_ps(InvAlpha);
    
    __m128i Shuffle = _mm_set_epi8(0, 0, 0, 0, 
                                   0, 0, 0, 0, 
                                   0, 0, 0, 0,
                                   12, 8, 4, 0);
    
    
    for (int Y = IterY0; Y < IterY1; Y++)
    {
        u32* Row = Buffer.Pixels + Buffer.Width * Y;
        for (int X = IterX0; X <= IterX1; X ++)
        {
            __m128 OldColor = _mm_cvtepi32_ps(_mm_cvtepu8_epi32(*(__m128i*)&Row[X]));
            __m128 NewColor = _mm_add_ps(_mm_mul_ps(InvAlpha_4x, OldColor), _mm_mul_ps(Alpha_4x, Color_m128));
            __m128i NewColor_u32 = _mm_shuffle_epi8(_mm_cvtps_epi32(NewColor), Shuffle);
            _mm_store_ss((float*)&Row[X], _mm_castsi128_ps(NewColor_u32));
        }
    }
}


void Win32Circle(back_buffer Buffer, v2 Position, f32 Radius, u32 Color)
{
    f32 X0 = (Position.X - Radius) * Buffer.Scale;
    f32 X1 = (Position.X + Radius) * Buffer.Scale;
    f32 Y0 = (Position.Y - Radius) * Buffer.Scale;
    f32 Y1 = (Position.Y + Radius) * Buffer.Scale;
    
    int IterX0 = Clamp(RoundDownToMultipleOf(8, X0), 0, RoundDownToMultipleOf(8, Buffer.Width - 1));
    int IterX1 = Clamp(RoundDownToMultipleOf(8, X1), 0, RoundDownToMultipleOf(8, Buffer.Width - 1));
    int IterY0 = Clamp(Floor(Y0), Buffer.ThreadY0, Buffer.ThreadY1);
    int IterY1 = Clamp(Floor(Y1), Buffer.ThreadY0, Buffer.ThreadY1);
    
    v2 PositionPixels = Position * Buffer.Scale;
    f32 RadiusPixelsSq = Square(Radius * Buffer.Scale);
    
    __m256 XOffsets_8x = _mm256_set_ps(7.0f, 6.0f, 5.0f, 4.0f, 3.0f, 2.0f, 1.0f, 0.0f);
    
    __m256 CenterX_8x = _mm256_set1_ps(PositionPixels.X);
    __m256 CenterY_8x = _mm256_set1_ps(PositionPixels.Y);
    
    __m256i RGB_8x = _mm256_set1_epi32(Color);
    
    __m256 RadiusPixelsSq_8x = _mm256_set1_ps(RadiusPixelsSq);
    
    for (int Y = IterY0; Y <= IterY1; Y++)
    {
        __m256 Y_8x = _mm256_set1_ps((f32)Y);
        __m256 dY_8x = _mm256_sub_ps(Y_8x, CenterY_8x);
        __m256 dYSq_8x = _mm256_mul_ps(dY_8x, dY_8x);
        
        for (int X = IterX0; X <= IterX1; X++)
        {
            u32 Index = Y * Buffer.Width + X;
            
            __m256 X_8x = _mm256_add_ps(_mm256_set1_ps((f32)X), XOffsets_8x);
            __m256 dX_8x = _mm256_sub_ps(X_8x, CenterX_8x);
            __m256 dXSq_8x = _mm256_mul_ps(dX_8x, dX_8x);
            
            __m256 Mask_8x = _mm256_cmp_ps(_mm256_add_ps(dYSq_8x, dXSq_8x), RadiusPixelsSq_8x, _CMP_LT_OS);
            
            __m256i OldRGB_8x = _mm256_load_si256((__m256i*)(Buffer.Pixels + Index));
            _mm256_store_si256((__m256i*)(Buffer.Pixels + Index), _mm256_blendv_epi8(OldRGB_8x, RGB_8x, _mm256_castps_si256(Mask_8x)));
        }
    }
}

static back_buffer
CreateBackBuffer(int Width, int Height)
{
    back_buffer Buffer = {};
    Buffer.Width = RoundUpToMultipleOf(16, Width);
    Buffer.Height = RoundUpToMultipleOf(16, Height);
    Buffer.DispWidth = Width;
    Buffer.DispHeight = Height;
    
    int PixelCount = Buffer.Width * Buffer.Height;
    Buffer.Pixels = (u32*)VirtualAlloc(0, PixelCount * 4, MEM_COMMIT|MEM_RESERVE, PAGE_READWRITE);
    Buffer.Scale = (f32)Width;
    return Buffer;
}

static void 
SoftwareRender(render_group* RenderGroup, back_buffer Buffer)
{
    for (u32 ShapeIndex = 0; ShapeIndex < RenderGroup->ShapeCount; ShapeIndex++)
    {
        render_shape Shape = RenderGroup->Shapes[ShapeIndex];
        switch (Shape.Type)
        {
            case Render_Rectangle:
            {
                u8 Alpha = Shape.Color >> 24;
                if (Alpha == 0xFF)
                {
                    Win32Rectangle(Buffer, Shape.Rectangle.Position, Shape.Rectangle.Size, Shape.Color);
                }
                else
                {
                    Win32RectangleTransparent(Buffer, Shape.Rectangle.Position, Shape.Rectangle.Size, Shape.Color);
                }
            } break;
            
            case Render_Circle:
            {
                Win32Circle(Buffer, Shape.Circle.Position, Shape.Circle.Radius, Shape.Color);
            } break;
            
            case Render_Line:
            {
                v2 XAxis = Shape.Line.End - Shape.Line.Start;
                v2 YAxis = UnitV(Perp(XAxis)) * Shape.Line.Thickness;
                v2 Origin = Shape.Line.Start - 0.5f * YAxis;
                
                u8 Alpha = Shape.Color >> 24;
                if (Alpha == 0xFF)
                {
                    DrawRotatedRectangle(Buffer, Origin, XAxis, YAxis, Shape.Color);
                }
                else
                {
                    DrawRotatedRectangleTransparent(Buffer, Origin, XAxis, YAxis, Shape.Color);
                }
                
            } break;
            
            case Render_Text:
            {
                Win32Text(Buffer, Shape.Text.Position, Shape.Text.String, Shape.Color);
            } break;
            
            default: Assert(0);
        }
    }
}

#define STB_TRUETYPE_IMPLEMENTATION
#define STB_STATIC
#include "stb_truetype.h"

stbtt_bakedchar BakedChars[128];
u8 CharBitmap[256 * 256];
static void
InitFont(char* Path, back_buffer Buffer, memory_arena* Arena)
{
    Assert(Arena->Type == TRANSIENT);
    span<u8> FontFile = Win32LoadFile(Arena, Path);
    
    f32 FontSize = Buffer.Scale * 0.02f;
    stbtt_BakeFontBitmap(FontFile.Memory, 0, FontSize, CharBitmap, 256, 256, 0, 128, BakedChars);
}

static f32 
Win32TextWidth(string String, f32 FontSize)
{
    f32 Pixels = 0;
    for (u32 I = 0; I < String.Length; I++)
    {
        char C = String.Text[I];
        Assert(C < 128);
        Pixels += BakedChars[C].xadvance;
    }
    return Pixels / 1920.0f;
}

static f32
DrawChar(back_buffer Buffer, f32 X, f32 Y, u32 Color, char C)
{
    stbtt_bakedchar BakedChar = BakedChars[C];
    int PixelX0 = (int)(X + BakedChar.xoff);
    int PixelY0 = (int)(Y - BakedChar.yoff);
    
    int PixelY = PixelY0;
    for (int TextureY = BakedChar.y0; TextureY < BakedChar.y1; TextureY++)
    {
        if (PixelY >= Buffer.ThreadY0 && PixelY < Buffer.ThreadY1)
        {
            int PixelX = PixelX0;
            for (int TextureX = BakedChar.x0; TextureX < BakedChar.x1; TextureX++)
            {
                u8 Texel = CharBitmap[TextureY * 256 + TextureX];
                
                if (Texel && PixelX >= 0 && PixelX < Buffer.Width && PixelY >= 0 && PixelY < Buffer.Height)
                {
                    u32 RGB = (Texel << 24) | (Color & 0xFFFFFF);
                    BlendPixel(Buffer, PixelX, PixelY, RGB);
                }
                
                PixelX++;
            }
        }
        PixelY--;
    }
    
    return BakedChar.xadvance;
}

static void
Win32Text(back_buffer Buffer, v2 Position, string String, u32 Color)
{
    f32 X0 = Position.X * Buffer.Width;
    f32 Y0 = Position.Y * Buffer.Width;
    
    for (u32 I = 0; I < String.Length; I++)
    {
        char C = String.Text[I];
        Assert(C < 128);
        f32 CharWidth = DrawChar(Buffer, X0, Y0 + 7.0f, Color, C);
        X0 += CharWidth;
    }
}

#if 0
void PackPixels(back_buffer Buffer)
{
    u8* R = Buffer.R;
    u8* G = Buffer.G;
    u8* B = Buffer.B;
    u32* RGB = Buffer.RGB;
    
    u32 I = 0;
    for (int Y = 0; Y < Buffer.Height; Y++)
    {
        for (int X = 0; X < Buffer.Width; X++)
        {
            *RGB = (R[I] << 16) | (G[I] << 8) | B[I];
            RGB++;
            I++;
        }
    }
}
#endif

static bool
RenderIfWorkAvailable(render_queue* Queue)
{
    bool DoneWork = false;
    
    u32 NextEntry = Queue->EntriesStarted;
    if (NextEntry != Queue->EntryCount)
    {
        u32 Index = InterlockedCompareExchange((volatile LONG*)&Queue->EntriesStarted,
                                               NextEntry + 1,
                                               NextEntry);
        if (Index == NextEntry)
        {
            render_queue_entry Entry = Queue->Entries[NextEntry];
            SoftwareRender(Entry.RenderGroup, Entry.Buffer);
            InterlockedIncrement((volatile LONG*)&Queue->EntriesDone);
            DoneWork = true;
        }
    }
    
    return DoneWork;
}

static DWORD WINAPI 
RenderThreadProc(LPVOID Param)
{
    render_queue* Queue = (render_queue*)Param;
    
    while (true)
    {
        if (!RenderIfWorkAvailable(Queue))
        {
            WaitForSingleObject(Queue->Semaphore, INFINITE);
        }
    }
}

static render_queue*
CreateRenderQueueAndThreads(memory_arena* Arena, u32 ThreadCount)
{
    Assert(Arena->Type == PERMANENT);
    
    render_queue* Queue = AllocStruct(Arena, render_queue); 
    
    Queue->ThreadCount = ThreadCount;
    Queue->Semaphore = CreateSemaphore(0, 0, ThreadCount, 0);
    
    for (u32 ThreadIndex = 0; ThreadIndex < ThreadCount; ThreadIndex++)
    {
        CreateThread(0, 0, RenderThreadProc, Queue, 0, 0);
    }
    
    return Queue;
}

static void
RenderMultithreaded(render_queue* Queue, render_group* RenderGroup, back_buffer* Buffer)
{
    u32 WorkCount = 1;
    
    Assert(Buffer->Height % WorkCount == 0);
    u32 Height = Buffer->Height / WorkCount;
    
    for (u32 WorkIndex = 0; WorkIndex < WorkCount; WorkIndex++)
    {
        render_queue_entry Entry = {};
        Entry.RenderGroup = RenderGroup;
        Entry.Buffer = *Buffer;
        Entry.Buffer.ThreadY0 = WorkIndex * Height;
        Entry.Buffer.ThreadY1 = Entry.Buffer.ThreadY0 + Height;
        Queue->Entries[WorkIndex] = Entry;
    }
    
    Queue->EntryCount = WorkCount;
    ReleaseSemaphore(Queue->Semaphore, Queue->ThreadCount, 0);
    
    while (Queue->EntriesDone != Queue->EntryCount)
    {
        RenderIfWorkAvailable(Queue);
    }
    
    Queue->EntriesDone = 0;
    Queue->EntriesStarted = 0;
    Queue->EntryCount = 0;
}