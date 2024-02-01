#include <SDL.h>

#include "Utilities.cpp"
#include "Maths.cpp"

//Platform functions
void SDLRectangle(v2 Position, v2 Dimensions, u32 FillColour, u32 BorderColour = 0);
void SDLLine(v2 Start_, v2 End_, u32 Colour, f32 Thickness);
void SDLDrawText(string String, v2 Position, v2 Size, u32 Color = 0xFF808080, bool Centered = false);
void SDLDebugOut(string String);
void SDLSleep(int Milliseconds);
memory_arena SDLCreateMemoryArena(u64 Size, memory_arena_type Type);
span<u8> SDLLoadFile(memory_arena* Arena, char* Path);
void SDLSaveFile(char* Path, span<u8> Data);

#define PlatformDrawTexture SDLDrawTexture
#define PlatformRectangle SDLRectangle
#define PlatformLine SDLLine
#define PlatformDrawText SDLDrawText
#define PlatformDebugOut SDLDebugOut
#define PlatformSleep SDLSleep
#define PlatformLoadFile SDLLoadFile
#define PlatformSaveFile SDLSaveFile

int GlobalWidth, GlobalHeight;
SDL_Renderer* GlobalRenderer;

//TODO: Fix
static inline void 
SDLRectangle(rect Rect, u32 FillColor, u32 BorderColor = 0)
{
	SDLRectangle(Rect.MinCorner, Rect.MaxCorner - Rect.MinCorner, FillColor, BorderColor);
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
	} Controls;
    
	v2 Cursor;
};

memory_arena GlobalDebugArena;

#include "GUI.cpp"
#include "Puzzle.cpp"

int main(int ArgCount, char** Args)
{
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS);
    
    SDL_Window* Window = SDL_CreateWindow("Puzzle Game",
                                          SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 720,
                                          SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    
    GlobalRenderer = SDL_CreateRenderer(Window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    
    GlobalDebugArena = SDLCreateMemoryArena(Megabytes(1), PERMANENT);
    
    memory_arena TransientArena = SDLCreateMemoryArena(Megabytes(16), TRANSIENT);
    memory_arena PermanentArena = SDLCreateMemoryArena(Megabytes(16), PERMANENT);
    
    allocator Allocator = {};
    Allocator.Transient = &TransientArena;
    Allocator.Permanent = &PermanentArena;
    
    game_state* GameState = GameInitialise(Allocator);
    
    u64 CounterFrequency = SDL_GetPerformanceFrequency();
    
    int TargetFrameRate = 60;
    f32 SecondsPerFrame = 1.0f / (f32)TargetFrameRate;
    
    int CountsPerFrame = CounterFrequency / TargetFrameRate;
    
    int KeyboardStateLength;
    const Uint8* KeyboardState = SDL_GetKeyboardState(&KeyboardStateLength);
    
    while (true)
    {
        u64 StartCount = SDL_GetPerformanceCounter();
        
        SDL_Event Event = {};
        
        game_input Input = {};
        while (SDL_PollEvent(&Event))
        {
            switch (Event.type)
            {
                case SDL_QUIT:
                {
                    SDL_Quit();
                } break;
                case SDL_KEYDOWN:
                {
                    switch (Event.key.keysym.scancode)
                    {
                        case SDL_SCANCODE_SPACE:
                        {
                            Input.Buttons.Jump = true;
                        } break;
                        case SDL_SCANCODE_E:
                        {
                            Input.Buttons.Interact = true;
                        }
                    }
                }
                case SDL_MOUSEBUTTONDOWN:
                {
                    Input.Buttons.MouseUpLeft = true;
                } break;
                
                case SDL_MOUSEBUTTONUP:
                {
                    Input.Buttons.MouseDownLeft = true;
                } break;
            }
        }
        
        Input.Controls.Jump = KeyboardState[SDL_SCANCODE_W];
        Input.Controls.Interact = KeyboardState[SDL_SCANCODE_E]; 
        Input.Controls.LShift = KeyboardState[SDL_SCANCODE_LSHIFT];
        
        if (KeyboardState[SDL_SCANCODE_A])
        {
            Input.Controls.MovementX -= 1.0f;
        }
        if (KeyboardState[SDL_SCANCODE_D])
        {
            Input.Controls.MovementX += 1.0f;
        }
        
        SDL_GetWindowSize(Window, &GlobalWidth, &GlobalHeight);
        
        int CursorX = 0;
        int CursorY = 0;
        SDL_GetMouseState(&CursorX, &CursorY);
        
        if (GlobalWidth > 0)
        {
            Input.Cursor.X = CursorX / GlobalWidth;
            Input.Cursor.Y = CursorY / GlobalWidth;
        }
        
        SDL_SetRenderDrawColor(GlobalRenderer, 0, 0, 0, SDL_ALPHA_OPAQUE);
        SDL_RenderClear(GlobalRenderer);
        
        GameUpdateAndRender(GameState, SecondsPerFrame, &Input, Allocator);
        SDL_RenderPresent(GlobalRenderer);
        
        ResetArena(Allocator.Transient);
        
        while (SDL_GetPerformanceCounter() - StartCount < CountsPerFrame)
        {
            SDL_Delay(1);
        }
    }
}

void SDLRectangle(v2 Position, v2 Dimensions, u32 FillColour, u32 BorderColour)
{
    f32 ScreenTop = 0.5625;
    SDL_FRect Rect = {
        Position.X * GlobalWidth, 
        (ScreenTop - Position.Y) * GlobalWidth, 
        Dimensions.X * GlobalWidth, 
        -Dimensions.Y * GlobalWidth
    };
    SDL_SetRenderDrawColor(GlobalRenderer, (FillColour >> 16) & 0xFF, (FillColour >> 8) & 0xFF, FillColour & 0xFF, (FillColour >> 24) & 0xFF);
    SDL_RenderFillRectF(GlobalRenderer, &Rect);
}
void SDLLine(v2 Start_, v2 End_, u32 Colour, f32 Thickness)
{
    f32 ScreenTop = 0.5625;
    v2 Start = {Start_.X * GlobalWidth, (ScreenTop - Start_.Y) * GlobalWidth}; 
    v2 End  = {End_.X * GlobalWidth, (ScreenTop - End_.Y) * GlobalWidth}; 
    SDL_SetRenderDrawColor(GlobalRenderer, (Colour >> 16) & 0xFF, (Colour >> 8) & 0xFF, Colour & 0xFF, (Colour >> 24) & 0xFF);
    SDL_RenderDrawLineF(GlobalRenderer, Start.X, Start.Y, End.X, End.Y);
}
void SDLDrawText(string String, v2 Position, v2 Size, u32 Color, bool Centered)
{
}
void SDLDebugOut(string String)
{
    SDL_Log("%s", String.Text);
}
void SDLSleep(int Milliseconds)
{
    SDL_Delay(Milliseconds);
}
memory_arena SDLCreateMemoryArena(u64 Size, memory_arena_type Type)
{
    memory_arena Arena = {};
    
    Arena.Buffer = (u8*)SDL_malloc(Size);
    Arena.Size = Size;
    Arena.Type = Type;
    
    return Arena;
}
span<u8> SDLLoadFile(memory_arena* Arena, char* Path)
{
    span<u8> Result = {};
    
    SDL_RWops* File = SDL_RWFromFile(Path, "rb");
    if (File)
    {
        i64 FileSize = File->seek(File, 0, RW_SEEK_END);
        File->seek(File, 0, RW_SEEK_SET);
        
        if (FileSize > 0)
        {
            Result.Memory = Alloc(Arena, (u32)FileSize);
            File->read(File, Result.Memory, 1, (u32)FileSize);
            Result.Count = FileSize;
        }
        
        File->close(File);
    }
    return Result;
}
void SDLSaveFile(char* Path, span<u8> Data)
{
}
