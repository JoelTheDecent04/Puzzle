#define UNICODE
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include "Utilities.cpp"
#include "Maths.cpp"

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
#define PlatformRectangle Win32Rectangle
#define PlatformLine Win32Line
#define PlatformDrawText Win32DrawText
#define PlatformDebugOut Win32DebugOut
#define PlatformSleep Win32Sleep
#define PlatformLoadFile Win32LoadFile
#define PlatformSaveFile Win32SaveFile

//TODO: Fix
static inline void 
Win32Rectangle(rect Rect, u32 FillColor, u32 BorderColor = 0)
{
	Win32Rectangle(Rect.MinCorner, Rect.MaxCorner - Rect.MinCorner, FillColor, BorderColor);
}

struct game_input
{
	struct
	{
		bool OpenShop;
		bool NextWave;
		bool Interact;
		bool ShowHideMap;
		bool MouseLeft;
		bool MouseDownLeft;
		bool MouseUpLeft;
		bool Jump;
        bool Menu;
        
		bool TestKey;
	} Buttons;
	struct
	{
		float MovementX; //normalised values [-1, 1]
		float MovementY;
		bool Jump;
        
		bool Test;
		bool Interact;
		bool LShift;
        bool Menu;
	} Controls;
    
	v2 Cursor;
};

memory_arena GlobalDebugArena;

#include "GUI.cpp"
#include "Puzzle.cpp"

#define Log(...) \
OutputDebugStringA(ArenaPrint(&GlobalDebugArena, __VA_ARGS__).Text); \
OutputDebugStringA("\n")

LRESULT CALLBACK WindowProc(HWND Window, UINT Message, WPARAM wParam, LPARAM lParam);

int BufferWidth = 1280, BufferHeight = 720;
u32* GlobalBackBuffer;

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
    
    void* Bits = 0;
    HBITMAP Bitmap = CreateDIBSection(WindowDC, &BitmapInfo,DIB_RGB_COLORS, &Bits, 0, 0);
    GlobalBackBuffer = (u32*)Bits;
    
	memory_arena TransientArena = Win32CreateMemoryArena(Megabytes(16), TRANSIENT);
	memory_arena PermanentArena = Win32CreateMemoryArena(Megabytes(16), PERMANENT);
	GlobalDebugArena = Win32CreateMemoryArena(Megabytes(1), PERMANENT);
    
	allocator Allocator = {};
	Allocator.Transient = &TransientArena;
	Allocator.Permanent = &PermanentArena;
    
	game_state* GameState = GameInitialise(Allocator);
	
	LARGE_INTEGER CounterFrequency;
	QueryPerformanceFrequency(&CounterFrequency); //Counts per second
    
    bool LockFPS = false;
    
	int TargetFrameRate = 60;
	float SecondsPerFrame = 1.0f / (float)TargetFrameRate;
	int CountsPerFrame = (int)(CounterFrequency.QuadPart / TargetFrameRate);
    
	game_input PreviousInput = {};
    
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
		
		game_input CurrentInput = {};
        
		if (GetActiveWindow())
		{
			CurrentInput.Controls.Jump = (GetAsyncKeyState('W') & 0x8000);
			CurrentInput.Buttons.Jump = (CurrentInput.Controls.Jump && !PreviousInput.Controls.Jump);
            
			if ((GetAsyncKeyState('A') & 0x8000))
			{
				CurrentInput.Controls.MovementX -= 1.0f;
			}
			if ((GetAsyncKeyState('D') & 0x8000))
			{
				CurrentInput.Controls.MovementX += 1.0f;
			}
            
			bool MouseIsDown = (GetAsyncKeyState(VK_LBUTTON) & 0x8000);
			bool MouseWasDown = PreviousInput.Buttons.MouseLeft;
            
			CurrentInput.Buttons.MouseLeft = MouseIsDown;
			CurrentInput.Buttons.MouseUpLeft = (MouseWasDown && !MouseIsDown);
			CurrentInput.Buttons.MouseDownLeft = (!MouseWasDown && MouseIsDown);
            
			bool InteractKeyDown = GetAsyncKeyState('E') & 0x8000;
			CurrentInput.Buttons.Interact = InteractKeyDown && (!PreviousInput.Controls.Interact);
			CurrentInput.Controls.Interact = InteractKeyDown;
            
			bool MenuKeyDown = GetAsyncKeyState('C') & 0x8000;
			CurrentInput.Buttons.Menu = MenuKeyDown && (!PreviousInput.Controls.Menu);
			CurrentInput.Controls.Menu = MenuKeyDown;
            
			CurrentInput.Controls.LShift = (GetAsyncKeyState(VK_LSHIFT) & 0x8000);
            
            POINT CursorPos = {};
            GetCursorPos(&CursorPos);
            ScreenToClient(Window, &CursorPos);
            
            GetClientRect(Window, &ClientRect);
            
            if (ClientRect.right > 0 && ClientRect.bottom > 0)
            {
                CurrentInput.Cursor.X = (f32)CursorPos.x / ClientRect.right;
                CurrentInput.Cursor.Y = (f32)(ClientRect.bottom - CursorPos.y) / ClientRect.right;
            }
            
            PreviousInput = CurrentInput;
        }
        
        GameUpdateAndRender(GameState, SecondsPerFrame, &CurrentInput, Allocator);
        
        ResetArena(&TransientArena);
        
        StretchDIBits(WindowDC, 
                      0, 0, ClientRect.right, ClientRect.bottom,
                      0, 0, BufferWidth, BufferHeight,
                      Bits, &BitmapInfo, DIB_RGB_COLORS, SRCCOPY);
        
        LARGE_INTEGER PerformanceCount;
        QueryPerformanceCounter(&PerformanceCount);
        if (PerformanceCount.QuadPart - StartCount.QuadPart > CountsPerFrame)
        {
            OutputDebugStringA("Can't keep up\n");
        }
        float TimeTaken = (float)(PerformanceCount.QuadPart - StartCount.QuadPart) / CounterFrequency.QuadPart;
        float CurrentFrameRate = 1.0f / TimeTaken;
        
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
            Log("Current Frame Rate: %.0f", CurrentFrameRate);
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
        
        default:
        {
            return DefWindowProc(Window, Message, wParam, lParam);
        }
    }	
    return 0;
}

void Win32DrawTexture(int Identifier, int Index, v2 Position, v2 Size, float Angle)
{
}

static void
DrawRotatedRectangle(v2 Origin, v2 XAxis, v2 YAxis, u32 Color)
{
    f32 Scale = (f32)BufferWidth;
    
    v2 A = Scale * Origin;
    v2 B = Scale * (Origin + XAxis);
    v2 C = Scale * (Origin + XAxis + YAxis);
    v2 D = Scale * (Origin + YAxis);
    
    f32 MinX = Min(A.X, B.X, C.X, D.X);
    f32 MaxX = Max(A.X, B.X, C.X, D.X);
    f32 MinY = Min(A.Y, B.Y, C.Y, D.Y);
    f32 MaxY = Max(A.Y, B.Y, C.Y, D.Y);
    
    i32 X0 = Max(0, (i32)MinX);
    i32 X1 = Min(BufferWidth, (i32)MaxX);
    i32 Y0 = Max(0, (i32)MinY);
    i32 Y1 = Min(BufferHeight, (i32)MaxY);
    
    v2 PerpXAxis = Perp(XAxis);
    v2 PerpYAxis = Perp(YAxis);
    
    for (i32 Y = Y0; Y < Y1; Y++)
    {
        u32* Row = GlobalBackBuffer + Y * BufferWidth;
        for (int X = X0; X < X1; X++)
        {
            v2 P = V2((f32)X, (f32)Y);
            
            f32 Edge0 = DotProduct(P - A, -1.0f * PerpXAxis);
            f32 Edge1 = DotProduct(P - B, -1.0f * PerpYAxis);
            f32 Edge2 = DotProduct(P - C, PerpXAxis);
            f32 Edge3 = DotProduct(P - D, PerpXAxis);
            
            if ((Edge0 < 0) && (Edge1 < 0) && (Edge2 < 0) && (Edge3 < 0))
            {
                Row[X] = Color;
            }
        }
    }
}

void Win32Rectangle(v2 Position, v2 Size, uint32_t FillColour, uint32_t BorderColour)
{
    int X0 = Round(Position.X * BufferWidth);
    int X1 = Round((Position.X + Size.X) * BufferWidth);
    int Y0 = Round(Position.Y * BufferWidth);
    int Y1 = Round((Position.Y + Size.Y) * BufferWidth);
    
    X0 = Clamp(X0, 0, BufferWidth);
    X1 = Clamp(X1, 0, BufferWidth);
    Y0 = Clamp(Y0, 0, BufferHeight);
    Y1 = Clamp(Y1, 0, BufferHeight);
    
    for (i32 Y = Y0; Y < Y1; Y++)
    {
        u32* Row = GlobalBackBuffer + Y * BufferWidth;
        for (i32 X = X0; X < X1; X++)
        {
            Row[X] = FillColour;
        }
    }
}

void Win32Line(v2 Start, v2 End, u32 Color, f32 Thickness)
{
    v2 XAxis = End - Start;
    v2 YAxis = Thickness * UnitV(Perp(XAxis));
    v2 Origin = Start - 0.5f * YAxis;
    
    f32 StepLength = 0.1f;
    f32 LineLength = Length(XAxis);
    
    f32 Section = 0.0f;
    
    while (Section < LineLength)
    {
        f32 SectionLength = StepLength;
        if (LineLength - Section < SectionLength)
        {
            SectionLength = LineLength - Section;
        }
        
        DrawRotatedRectangle(Origin, SectionLength * UnitV(XAxis), YAxis, Color);
        
        Origin += SectionLength * UnitV(XAxis);
        Section += StepLength;
    }
}

void Win32DrawText(string String, v2 Position, v2 Size, u32 Color, bool Centered)
{
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