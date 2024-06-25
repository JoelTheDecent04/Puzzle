#pragma comment(lib, "user32.lib")
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")

#define UNICODE
#include <Windows.h>
#include <d3d11_1.h>
#include <d3dcompiler.h>

#include <string>

#include "Utilities.cpp"
#include "Maths.cpp"

#include "Puzzle.h"

#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_STATIC
#include "stb_truetype.h"

#define DEBUG 1

memory_arena GlobalDebugArena;
#define LOG(...) \
OutputDebugStringA(ArenaPrint(&GlobalDebugArena, __VA_ARGS__).Text)

std::string GlobalTextInput;

LRESULT CALLBACK WindowProc(HWND Window, UINT Message, WPARAM WParam, LPARAM LParam);

struct font_texture
{
    stbtt_bakedchar* BakedChars;
    ID3D11SamplerState* SamplerState;
    ID3D11ShaderResourceView* TextureView;
    
    f32 TextureWidth;
    f32 TextureHeight;
    f32 RasterisedSize;
};

font_texture* DefaultFont;

struct d3d11_device
{
    ID3D11Device1* Device;
    ID3D11DeviceContext1* DeviceContext;
};


struct d3d11_shader
{
    ID3D11VertexShader* VertexShader;
    ID3D11PixelShader* PixelShader;
    ID3D11InputLayout* InputLayout;
};


//Platform Functions
void Win32DebugOut(string String);
void Win32Sleep(int Milliseconds);
span<u8> Win32LoadFile(memory_arena* Arena, char* Path);
void Win32SaveFile(char* Path, span<u8> Data);
f32 Win32TextWidth(string String, f32 FontSize);

#define PlatformDebugOut    Win32DebugOut
#define PlatformSleep       Win32Sleep
#define PlatformLoadFile    Win32LoadFile
#define PlatformSaveFile    Win32SaveFile
#define PlatformTextWidth   Win32TextWidth

void KeyboardAndMouseInputState(input_state* InputState, HWND Window);
memory_arena Win32CreateMemoryArena(u64 Size, memory_arena_type Type);
font_texture CreateFontTexture(allocator Allocator, d3d11_device D3D11, char* Path);
void DrawText(allocator Allocator, d3d11_device D3D11, font_texture Font, string Text, v2 Position, v4 Color);

#include "Puzzle.cpp"

static bool GlobalWindowDidResize;

void DirectX11Render(render_group* Group, d3d11_device D3D11, allocator Allocator, d3d11_shader QuadShader, d3d11_shader TextShader, font_texture Font);

d3d11_device CreateD3D11Device()
{
    d3d11_device Result = {};
    
    ID3D11Device* BaseDevice;
    ID3D11DeviceContext* BaseDeviceContext;
    
    D3D_FEATURE_LEVEL FeatureLevels[] = {D3D_FEATURE_LEVEL_11_0};
    UINT CreationFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
    
#ifdef DEBUG
    CreationFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
    HRESULT HResult = D3D11CreateDevice(0, D3D_DRIVER_TYPE_HARDWARE,
                                        0, CreationFlags,
                                        FeatureLevels, ArrayCount(FeatureLevels),
                                        D3D11_SDK_VERSION, &BaseDevice,
                                        0, &BaseDeviceContext);
    Assert(SUCCEEDED(HResult));
    
    //Get 1.1 interface of device and context
    HResult = BaseDevice->QueryInterface(IID_PPV_ARGS(&Result.Device));
    Assert(SUCCEEDED(HResult));
    BaseDevice->Release();
    
    HResult = BaseDeviceContext->QueryInterface(IID_PPV_ARGS(&Result.DeviceContext));
    Assert(SUCCEEDED(HResult));
    BaseDeviceContext->Release();
    
#ifdef DEBUG
    //Setup debug to break on D3D11 error
    ID3D11Debug* D3DDebug;
    Result.Device->QueryInterface(IID_PPV_ARGS(&D3DDebug));
    if (D3DDebug)
    {
        ID3D11InfoQueue* D3DInfoQueue;
        HResult = D3DDebug->QueryInterface(IID_PPV_ARGS(&D3DInfoQueue));
        if (SUCCEEDED(HResult))
        {
            D3DInfoQueue->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_CORRUPTION, true);
            D3DInfoQueue->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_ERROR, true);
            D3DInfoQueue->Release();
        }
        D3DDebug->Release();
    }
#endif
    
    return Result;
}

IDXGISwapChain1* CreateD3D11SwapChain(ID3D11Device1* Device, HWND Window)
{
    //Get DXGI Adapter
    IDXGIDevice1* DXGIDevice;
    
    HRESULT HResult = Device->QueryInterface(IID_PPV_ARGS(&DXGIDevice));
    Assert(SUCCEEDED(HResult));
    
    IDXGIAdapter* DXGIAdapter;
    HResult = DXGIDevice->GetAdapter(&DXGIAdapter);
    Assert(SUCCEEDED(HResult));
    
    DXGIDevice->Release();
    
    DXGI_ADAPTER_DESC AdapterDesc;
    DXGIAdapter->GetDesc(&AdapterDesc);
    
    OutputDebugStringA("Graphics Device: ");
    OutputDebugStringW(AdapterDesc.Description);
    
    //Get DXGI Factory
    IDXGIFactory2* DXGIFactory;
    HResult = DXGIAdapter->GetParent(IID_PPV_ARGS(&DXGIFactory));
    DXGIAdapter->Release();
    
    DXGI_SWAP_CHAIN_DESC1 SwapChainDesc = {};
    SwapChainDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
    SwapChainDesc.SampleDesc.Count = 2;
    SwapChainDesc.SampleDesc.Quality = 0;
    SwapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    SwapChainDesc.BufferCount = 2;
    SwapChainDesc.Scaling = DXGI_SCALING_STRETCH;
    SwapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    SwapChainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
    SwapChainDesc.Flags = 0;
    
    IDXGISwapChain1* SwapChain;
    HResult = DXGIFactory->CreateSwapChainForHwnd(Device, Window, &SwapChainDesc, 0, 0, &SwapChain);
    Assert(SUCCEEDED(HResult));
    
    DXGIFactory->Release();
    
    return SwapChain;
}

ID3D11RenderTargetView* CreateRenderTarget(ID3D11Device1* Device, IDXGISwapChain1* SwapChain)
{
    ID3D11Texture2D* FrameBuffer;
    HRESULT HResult = SwapChain->GetBuffer(0, IID_PPV_ARGS(&FrameBuffer));
    Assert(SUCCEEDED(HResult));
    
    ID3D11RenderTargetView* FrameBufferView;
    HResult = Device->CreateRenderTargetView(FrameBuffer, 0, &FrameBufferView);
    Assert(SUCCEEDED(HResult));
    FrameBuffer->Release();
    
    return FrameBufferView;
}

d3d11_shader CreateShader(wchar_t* Path, ID3D11Device* Device, D3D11_INPUT_ELEMENT_DESC* InputElementDesc, u32 InputElementDescCount)
{
    d3d11_shader Result = {};
    
    //Create Vertex Shader
    ID3DBlob* VertexShaderBlob;
    ID3DBlob* CompileErrorsBlob;
    HRESULT HResult = D3DCompileFromFile(Path, 0, 0, "vs_main", "vs_5_0", 0, 0, &VertexShaderBlob, &CompileErrorsBlob);
    
    if (FAILED(HResult))
    {
        if (HResult == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND))
        {
            OutputDebugStringA("Failed to compile shader, file not found");
        }
        else if (CompileErrorsBlob)
        {
            OutputDebugStringA("Failed to compile shader:");
            OutputDebugStringA((char*)CompileErrorsBlob->GetBufferPointer());
            CompileErrorsBlob->Release();
        }
    }
    
    HResult = Device->CreateVertexShader(VertexShaderBlob->GetBufferPointer(), VertexShaderBlob->GetBufferSize(), 0, &Result.VertexShader);
    Assert(SUCCEEDED(HResult));
    
    //Create Pixel Shader
    ID3DBlob* PixelShaderBlob;
    HResult = D3DCompileFromFile(Path, 0, 0, "ps_main", "ps_5_0", 0, 0, &PixelShaderBlob, &CompileErrorsBlob);
    
    if (FAILED(HResult))
    {
        if (HResult == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND))
        {
            OutputDebugStringA("Failed to compile shader, file not found");
        }
        else if (CompileErrorsBlob)
        {
            OutputDebugStringA("Failed to compile shader:");
            OutputDebugStringA((char*)CompileErrorsBlob->GetBufferPointer());
            CompileErrorsBlob->Release();
        }
    }
    
    HResult = Device->CreatePixelShader(PixelShaderBlob->GetBufferPointer(), PixelShaderBlob->GetBufferSize(), 0, &Result.PixelShader);
    Assert(SUCCEEDED(HResult));
    PixelShaderBlob->Release();
    
    HResult = Device->CreateInputLayout(InputElementDesc, InputElementDescCount, 
                                        VertexShaderBlob->GetBufferPointer(), VertexShaderBlob->GetBufferSize(), 
                                        &Result.InputLayout);
    Assert(SUCCEEDED(HResult));
    VertexShaderBlob->Release();
    
    return Result;
}

void DrawQuad(d3d11_device D3D11, v2 A, v2 B, v2 C, v2 D, v4 Color)
{
    //Create Vertex Buffer
    
    f32 VertexData[] = {
        A.X, A.Y, Color.R, Color.G, Color.B, Color.A,
        B.X, B.Y, Color.R, Color.G, Color.B, Color.A,
        C.X, C.Y, Color.R, Color.G, Color.B, Color.A,
        D.X, D.Y, Color.R, Color.G, Color.B, Color.A
    };
    
    u32 Stride = 6 * sizeof(f32);
    u32 VertexCount = sizeof(VertexData) / Stride;
    u32 Offset = 0;
    
    D3D11_BUFFER_DESC VertexBufferDesc = {};
    VertexBufferDesc.ByteWidth = sizeof(VertexData);
    VertexBufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
    VertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    
    D3D11_SUBRESOURCE_DATA VertexSubresourceData = { VertexData };
    
    ID3D11Buffer* VertexBuffer;
    HRESULT HResult = D3D11.Device->CreateBuffer(&VertexBufferDesc, &VertexSubresourceData, &VertexBuffer);
    Assert(SUCCEEDED(HResult));
    
    //Draw
    D3D11.DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    D3D11.DeviceContext->IASetVertexBuffers(0, 1, &VertexBuffer, &Stride, &Offset);
    D3D11.DeviceContext->Draw(VertexCount, 0);
    
    VertexBuffer->Release();
}

int WINAPI wWinMain(HINSTANCE Instance, HINSTANCE, LPWSTR CommandLine, int ShowCode)
{
    WNDCLASS WindowClass = {};
    WindowClass.lpfnWndProc = WindowProc;
    WindowClass.hInstance = Instance;
    WindowClass.hCursor = LoadCursor(0, IDC_ARROW);
    WindowClass.lpszClassName = L"MainWindow";
    
    if (!RegisterClass(&WindowClass))
    {
        return -1;
    }
    
    int DefaultWidth = 1280, DefaultHeight = 720;
    
    RECT ClientRect = {0, 0, DefaultWidth, DefaultHeight};
    AdjustWindowRect(&ClientRect, WS_OVERLAPPEDWINDOW, FALSE);
    
    HWND Window = CreateWindow(WindowClass.lpszClassName,
                               L"Puzzle Game",
                               WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                               CW_USEDEFAULT, CW_USEDEFAULT,
                               ClientRect.right - ClientRect.left,
                               ClientRect.bottom - ClientRect.top,
                               0, 0, Instance, 0);
    
    if (!Window)
    {
        return -1;
    }
    
    d3d11_device D3D11 = CreateD3D11Device();
    IDXGISwapChain1* SwapChain = CreateD3D11SwapChain(D3D11.Device, Window);
    ID3D11RenderTargetView* FrameBufferView = CreateRenderTarget(D3D11.Device, SwapChain);
    
    D3D11_INPUT_ELEMENT_DESC InputElementDesc[] = 
    {
        {"POS", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"COL", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0}
    };
    
    d3d11_shader Shader = CreateShader(L"assets/shaders.hlsl", D3D11.Device, InputElementDesc, ArrayCount(InputElementDesc));
    
    D3D11_INPUT_ELEMENT_DESC FontShaderInputElementDesc[] = 
    {
        {"POS", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TEX", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"COL", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0}
    };
    d3d11_shader FontShader = CreateShader(L"assets/fontshaders.hlsl", D3D11.Device, FontShaderInputElementDesc, ArrayCount(FontShaderInputElementDesc));
    
    D3D11_BLEND_DESC BlendDesc = {};
    BlendDesc.AlphaToCoverageEnable = false;
    BlendDesc.IndependentBlendEnable = false;
    BlendDesc.RenderTarget[0].BlendEnable = true;
    BlendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
    BlendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    BlendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    
    BlendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    BlendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
    BlendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    BlendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    
    ID3D11BlendState* BlendState;
    HRESULT HResult = D3D11.Device->CreateBlendState(&BlendDesc, &BlendState);
    Assert(SUCCEEDED(HResult));
    D3D11.DeviceContext->OMSetBlendState(BlendState, NULL, 0xFFFFFFFF);
    
    memory_arena TransientArena = Win32CreateMemoryArena(Megabytes(16), TRANSIENT);
    memory_arena PermanentArena = Win32CreateMemoryArena(Megabytes(16), PERMANENT);
    GlobalDebugArena = Win32CreateMemoryArena(Megabytes(1), PERMANENT);
    
    allocator Allocator = {};
    Allocator.Transient = &TransientArena;
    Allocator.Permanent = &PermanentArena;
    
    LARGE_INTEGER CounterFrequency;
    QueryPerformanceFrequency(&CounterFrequency); //Counts per second
    
    int TargetFrameRate = 60;
    float SecondsPerFrame = 1.0f / (float)TargetFrameRate;
    int CountsPerFrame = (int)(CounterFrequency.QuadPart / TargetFrameRate);
    
    game_input PreviousInput = {};
    
    render_group RenderGroup;
    RenderGroup.ShapeCount = 0;
    
    game_state* GameState = GameInitialise(Allocator);
    
    font_texture FontTexture = CreateFontTexture(Allocator, D3D11, "assets/LiberationMono-Regular.ttf");
    DefaultFont = &FontTexture;
    
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
        
        if (GlobalWindowDidResize)
        {
            D3D11.DeviceContext->OMSetRenderTargets(0, 0, 0);
            FrameBufferView->Release();
            
            HRESULT Result = SwapChain->ResizeBuffers(0, 0, 0, DXGI_FORMAT_UNKNOWN, 0);
            Assert(SUCCEEDED(Result));
            
            ID3D11Texture2D* FrameBuffer;
            Result = SwapChain->GetBuffer(0, IID_PPV_ARGS(&FrameBuffer));
            Assert(SUCCEEDED(Result));
            
            Result = D3D11.Device->CreateRenderTargetView(FrameBuffer, 0, &FrameBufferView);
            Assert(SUCCEEDED(Result));
            FrameBuffer->Release();
            
            GlobalWindowDidResize = false;
        }
        
        //-------------------------
        
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
        
        ResetArena(&TransientArena);
        GameUpdateAndRender(&RenderGroup, GameState, SecondsPerFrame, &Input, Allocator);
        
        //..........................
        FLOAT Color[4] = {0.1f, 0.2f, 0.3f, 1.0f};
        D3D11.DeviceContext->ClearRenderTargetView(FrameBufferView, Color);
        
        RECT WindowRect;
        GetClientRect(Window, &WindowRect);
        D3D11_VIEWPORT Viewport = { 0.0f, 0.0f, (FLOAT)(WindowRect.right - WindowRect.left), (FLOAT)(WindowRect.bottom - WindowRect.top), 0.0f, 1.0f };
        D3D11.DeviceContext->RSSetViewports(1, &Viewport);
        
        D3D11.DeviceContext->OMSetRenderTargets(1, &FrameBufferView, 0);
        
        D3D11.DeviceContext->IASetInputLayout(Shader.InputLayout);
        D3D11.DeviceContext->VSSetShader(Shader.VertexShader, 0, 0);
        D3D11.DeviceContext->PSSetShader(Shader.PixelShader, 0, 0);
        
        DirectX11Render(&RenderGroup, D3D11, Allocator, Shader, FontShader, FontTexture);
        
        SwapChain->Present(1, 0);
        //---------------------------
        
        RenderGroup.ShapeCount = 0;
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

void DirectX11Render(render_group* Group, d3d11_device D3D11, allocator Allocator, d3d11_shader QuadShader, d3d11_shader TextShader, font_texture Font)
{
    for (u32 Index = 0; Index < Group->ShapeCount; Index++)
    {
        render_shape Shape = Group->Shapes[Index];
        
        f32 A = (Shape.Color >> 24) / 255.0f;
        f32 R = ((Shape.Color >> 16) & 0xFF) / 255.0f;
        f32 G = ((Shape.Color >> 8) & 0xFF)  / 255.0f;
        f32 B = (Shape.Color & 0xFF) / 255.0f;
        
        v4 Color = V4(R, G, B, A);
        
        switch (Shape.Type)
        {
            case Render_Rectangle:
            {
                v2 Origin = Shape.Rectangle.Position;
                v2 XAxis = V2(Shape.Rectangle.Size.X, 0.0f);
                v2 YAxis = V2(0.0f, Shape.Rectangle.Size.Y);
                
                D3D11.DeviceContext->IASetInputLayout(QuadShader.InputLayout);
                D3D11.DeviceContext->VSSetShader(QuadShader.VertexShader, 0, 0);
                D3D11.DeviceContext->PSSetShader(QuadShader.PixelShader, 0, 0);
                DrawQuad(D3D11, Origin + YAxis, Origin + YAxis + XAxis, Origin, Origin + XAxis, Color);
            } break;
            case Render_Circle:
            {
            } break;
            case Render_Line:
            {
                v2 XAxis = Shape.Line.End - Shape.Line.Start;
                v2 YAxis = UnitV(Perp(XAxis)) * Shape.Line.Thickness;
                v2 Origin = Shape.Line.Start - 0.5f * YAxis;
                
                D3D11.DeviceContext->IASetInputLayout(QuadShader.InputLayout);
                D3D11.DeviceContext->VSSetShader(QuadShader.VertexShader, 0, 0);
                D3D11.DeviceContext->PSSetShader(QuadShader.PixelShader, 0, 0);
                DrawQuad(D3D11, Origin + YAxis, Origin + YAxis + XAxis, Origin, Origin + XAxis, Color);
            } break;
            case Render_Text:
            {
                if (Shape.Text.String.Length == 0)
                {
                    break;
                }
                
                D3D11.DeviceContext->IASetInputLayout(TextShader.InputLayout);
                D3D11.DeviceContext->VSSetShader(TextShader.VertexShader, 0, 0);
                D3D11.DeviceContext->PSSetShader(TextShader.PixelShader, 0, 0);
                DrawText(Allocator, D3D11, Font, Shape.Text.String, Shape.Text.Position, Color);
            } break;
            default: Assert(0);
        }
        
    }
}
LRESULT CALLBACK WindowProc(HWND Window, UINT Message, WPARAM WParam, LPARAM LParam)
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
            Assert(WParam < 128);
            char Char = (char)WParam;
            
            if (Char == '\r')
            {
                Char = '\n';
            }
            
            GlobalTextInput.append(1, Char);
            return 0;
        }
        
        case WM_SIZE:
        {
            GlobalWindowDidResize = true;
            return 0;
        }
        
        default:
        {
            return DefWindowProc(Window, Message, WParam, LParam);
        }
    }
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
    //TODO: Call OpenInputDesktop() to determine whether the current desktop is the input desktop
    POINT CursorPos = {};
    GetCursorPos(&CursorPos);
    ScreenToClient(Window, &CursorPos);
    
    RECT ClientRect;
    GetClientRect(Window, &ClientRect);
    
    u32 WindowWidth = ClientRect.right - ClientRect.left;
    u32 WindowHeight = ClientRect.bottom - ClientRect.top;
    
    if (ClientRect.right > 0 && ClientRect.bottom > 0)
    {
        InputState->Cursor.X += (f32)CursorPos.x / WindowWidth;
        InputState->Cursor.Y += 0.5625f * (f32)(WindowHeight - CursorPos.y) / WindowHeight;
    }
}

stbtt_bakedchar BakedChars[128];

font_texture CreateFontTexture(allocator Allocator, d3d11_device D3D11, char* Path)
{
    f32 RasterisedSize = 64.0f;
    
    font_texture Result = {};
    Result.RasterisedSize = RasterisedSize;
    
    //Bake characters
    u32 TextureWidth = 512;
    u32 TextureHeight = 512;
    
    Result.TextureWidth = (f32)TextureWidth;
    Result.TextureHeight = (f32)TextureHeight;
    
    span<u8> TrueTypeFile = Win32LoadFile(Allocator.Transient, Path);
    
    u8* TempBuffer = Alloc(Allocator.Transient, TextureWidth * TextureHeight * sizeof(u8));
    
    stbtt_BakeFontBitmap(TrueTypeFile.Memory, 0, RasterisedSize, TempBuffer, TextureWidth, TextureHeight, 0, 128, BakedChars);
    
    //Create Sampler
    D3D11_SAMPLER_DESC SamplerDesc = {};
    SamplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    SamplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_BORDER;
    SamplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_BORDER;
    SamplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_BORDER;
    SamplerDesc.BorderColor[0] = 1.0f;
    SamplerDesc.BorderColor[1] = 1.0f;
    SamplerDesc.BorderColor[2] = 1.0f;
    SamplerDesc.BorderColor[3] = 1.0f;
    SamplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    
    D3D11.Device->CreateSamplerState(&SamplerDesc, &Result.SamplerState);
    
    D3D11_TEXTURE2D_DESC TextureDesc = {};
    TextureDesc.Width = TextureWidth;
    TextureDesc.Height = TextureHeight;
    TextureDesc.MipLevels = 1;
    TextureDesc.ArraySize = 1;
    TextureDesc.Format = DXGI_FORMAT_R8_UNORM;
    TextureDesc.SampleDesc.Count = 1;
    TextureDesc.Usage = D3D11_USAGE_IMMUTABLE;
    TextureDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    
    D3D11_SUBRESOURCE_DATA TextureSubresourceData = {};
    TextureSubresourceData.pSysMem = TempBuffer;
    TextureSubresourceData.SysMemPitch = TextureWidth * sizeof(u8);
    
    ID3D11Texture2D* Texture;
    D3D11.Device->CreateTexture2D(&TextureDesc, &TextureSubresourceData, &Texture);
    
    D3D11.Device->CreateShaderResourceView(Texture, 0, &Result.TextureView);
    
    Result.BakedChars = BakedChars;
    return Result;
}

struct char_vertex
{
    v2 Position;
    v2 UV;
    v4 Color;
};

void DrawText(allocator Allocator, d3d11_device D3D11, font_texture Font, string Text, v2 Position, v4 Color)
{
    f32 FontTexturePixelsToScreen = (6.0f) / Font.RasterisedSize / Font.TextureHeight;
    
    f32 X = Position.X;
    f32 Y = Position.Y;
    
    u32 Stride = sizeof(char_vertex);
    u32 Offset = 0;
    u32 VertexCount = 6 * Text.Length;
    
    char_vertex* VertexData = AllocArray(Allocator.Transient, char_vertex, VertexCount);
    
    for (u32 I = 0; I < Text.Length; I++)
    {
        uint8_t Char = (uint8_t)Text.Text[I];
        Assert(Char < 128);
        stbtt_bakedchar BakedChar = Font.BakedChars[Char];
        
        f32 X0 = X + BakedChar.xoff * FontTexturePixelsToScreen;
        f32 Y1 = Y - BakedChar.yoff * FontTexturePixelsToScreen + 0.5f * Font.RasterisedSize * FontTexturePixelsToScreen;
        
        f32 Width = FontTexturePixelsToScreen * (f32)(BakedChar.x1 - BakedChar.x0);
        f32 Height = FontTexturePixelsToScreen * (f32)(BakedChar.y1 - BakedChar.y0);
        
        VertexData[6 * I + 0].Position = {X0, Y1 - Height};
        VertexData[6 * I + 1].Position = {X0, Y1};
        VertexData[6 * I + 2].Position = {X0 + Width, Y1};
        VertexData[6 * I + 3].Position = {X0 + Width, Y1};
        VertexData[6 * I + 4].Position = {X0 + Width, Y1 - Height};
        VertexData[6 * I + 5].Position = {X0, Y1 - Height};
        
        VertexData[6 * I + 0].UV = {BakedChar.x0 / Font.TextureWidth, BakedChar.y1 / Font.TextureHeight};
        VertexData[6 * I + 1].UV = {BakedChar.x0 / Font.TextureWidth, BakedChar.y0 / Font.TextureHeight};
        VertexData[6 * I + 2].UV = {BakedChar.x1 / Font.TextureWidth, BakedChar.y0 / Font.TextureHeight};
        VertexData[6 * I + 3].UV = {BakedChar.x1 / Font.TextureWidth, BakedChar.y0 / Font.TextureHeight};
        VertexData[6 * I + 4].UV = {BakedChar.x1 / Font.TextureWidth, BakedChar.y1 / Font.TextureHeight};
        VertexData[6 * I + 5].UV = {BakedChar.x0 / Font.TextureWidth, BakedChar.y1 / Font.TextureHeight};
        
        VertexData[6 * I + 0].Color = Color;
        VertexData[6 * I + 1].Color = Color;
        VertexData[6 * I + 2].Color = Color;
        VertexData[6 * I + 3].Color = Color;
        VertexData[6 * I + 4].Color = Color;
        VertexData[6 * I + 5].Color = Color;
        
        X += BakedChar.xadvance * FontTexturePixelsToScreen;
    }
    
    D3D11_BUFFER_DESC VertexBufferDesc = {};
    VertexBufferDesc.ByteWidth = VertexCount * sizeof(char_vertex);
    VertexBufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
    VertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    
    D3D11_SUBRESOURCE_DATA VertexSubresourceData = { VertexData };
    
    ID3D11Buffer* VertexBuffer;
    HRESULT HResult = D3D11.Device->CreateBuffer(&VertexBufferDesc, &VertexSubresourceData, &VertexBuffer);
    Assert(SUCCEEDED(HResult));
    
    D3D11.DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    D3D11.DeviceContext->IASetVertexBuffers(0, 1, &VertexBuffer, &Stride, &Offset);
    
    D3D11.DeviceContext->PSSetShaderResources(0, 1, &Font.TextureView);
    D3D11.DeviceContext->PSSetSamplers(0, 1, &Font.SamplerState);
    
    D3D11.DeviceContext->Draw(VertexCount, 0);
    
    VertexBuffer->Release();
}

f32 Win32TextWidth(string String, f32 FontSize)
{
    f32 Result = 0.0f;
    f32 FontTexturePixelsToScreen = (6.0f) / DefaultFont->RasterisedSize / DefaultFont->TextureHeight;
    for (u32 I = 0; I < String.Length; I++)
    {
        stbtt_bakedchar* BakedChar = DefaultFont->BakedChars + String.Text[I];
        Result += BakedChar->xadvance * FontTexturePixelsToScreen;
    }
    return Result;
}