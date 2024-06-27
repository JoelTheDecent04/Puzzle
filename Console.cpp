void AddLine(console* Console, string String);
void ClearConsole(console* Console);

#define CONSOLE_COMMAND(Console, Command) \
AddCommand(Console, String(#Command), Command_ ## Command)

void AddCommand(console* Console, string Command, console_command_callback Callback)
{
    Console->Commands[Console->CommandCount].Command = Command;
    Console->Commands[Console->CommandCount].Callback = Callback;
    Console->CommandCount++;
    Assert(Console->CommandCount < ArrayCount(Console->Commands));
}

void Command_null_map_elem_count(int ArgCount, string* Args, console* Console, game_state* GameState, memory_arena* Arena)
{
    u32 Count = 0;
    for (map_element& MapElem : GameState->Map->Elements)
    {
        if (MapElem.Type == MapElem_Null)
        {
            Count++;
        }
    }
    
    string Result = ArenaPrint(Arena, "%u null elements", Count);
    AddLine(Console, Result);
}

void Command_clear(int ArgCount, string* Args, console* Console, game_state* GameState, memory_arena* Arena)
{
    ClearConsole(Console);
}

void Command_activated(int ArgCount, string* Args, console* Console, game_state* GameState, memory_arena* Arena)
{
    if (ArgCount == 2)
    {
        u32 EntityIndex = StringToU32(Args[1]);
        
        if (EntityIndex < GameState->Map->Entities.Count)
        {
            bool Activated = GameState->Map->Entities[EntityIndex].WasActivated;
            string Result = ArenaPrint(Arena, "%u activated: %u", EntityIndex, Activated);
            AddLine(Console, Result);
        }
        else
        {
            AddLine(Console, String("Out of range"));
        }
    }
}

void Command_color(int ArgCount, string* Args, console* Console, game_state* GameState, memory_arena* Arena)
{
    u32 Index = GameState->Editor.SelectedElementIndex;
    map_element* SelectedElement = GameState->Map->Elements + Index;
    u32 NewColor = SelectedElement->Color;
    
    if (ArgCount == 2)
    {
        NewColor = StringToU32(Args[1]);
        if (NewColor >> 24 == 0)
        {
            NewColor |= 0xFF000000;
        }
    }
    if (ArgCount == 5)
    {
        u32 A = StringToU32(Args[1]);
        u32 R = StringToU32(Args[2]);
        u32 G = StringToU32(Args[3]);
        u32 B = StringToU32(Args[4]);
        
        Assert(A < 256);
        Assert(R < 256);
        Assert(G < 256);
        Assert(B < 256);
        
        NewColor = (A << 24) | (R << 16) | (G << 8) | B;
    }
    
    string Result = ArenaPrint(Arena, "Map element %u has color 0x%08x", Index, NewColor);
    AddLine(Console, Result);
    
    SelectedElement->Color = NewColor;
}

static void
AddLine(console* Console, string String)
{
    u32 Bytes = String.Length;
    string NewLine = {};
    NewLine.Length = Bytes;
    NewLine.Text = (char*)malloc(Bytes);
    memcpy(NewLine.Text, String.Text, Bytes);
    
    u32 MaxHistory = ArrayCount(Console->History);
    free(Console->History[MaxHistory - 1].Text);
    memmove(Console->History + 1, Console->History, sizeof(string) * (MaxHistory - 1));
    Console->History[0] = NewLine;
}

static void
ClearConsole(console* Console)
{
    for (string& String : Console->History)
    {
        free(String.Text);
        String = {};
    }
}

static void
ParseAndRunCommand(console* Console, string Command, game_state* GameState, memory_arena* TArena)
{
    int const MaxArgs = 16;
    string Args[MaxArgs];
    int ArgCount = 0;
    
    bool InsideArg = false;
    u32 CharIndex = 0;
    while (ArgCount < MaxArgs &&
           CharIndex < Command.Length)
    {
        char C = Command.Text[CharIndex];
        if (C == ' ')
        {
            if (InsideArg)
            {
                ArgCount++;
                InsideArg = false;
            }
        }
        else
        {
            if (InsideArg)
            {
                Args[ArgCount].Length++;
            }
            else
            {
                Args[ArgCount].Text = Command.Text + CharIndex;
                Args[ArgCount].Length = 1;
                InsideArg = true;
            }
        }
        
        CharIndex++;
    }
    
    if (InsideArg)
    {
        ArgCount++;
    }
    
    string Input = ArenaPrint(TArena, "> %.*s", Console->InputLength, Console->Input);
    AddLine(Console, Input);
    
    if (ArgCount > 0)
    {
        if (StringsAreEqual(Args[0], String("help")))
        {
            for (u64 CommandIndex = 0; CommandIndex < Console->CommandCount; CommandIndex++)
            {
                AddLine(Console, Console->Commands[CommandIndex].Command);
            }
        }
        else
        {
            for (u64 CommandIndex = 0; CommandIndex < Console->CommandCount; CommandIndex++)
            {
                if (StringsAreEqual(Console->Commands[CommandIndex].Command, Args[0]))
                {
                    Console->Commands[CommandIndex].Callback(ArgCount, Args, Console, GameState, TArena);
                    return;
                }
            }
            
            AddLine(Console, String("Invalid Command"));
        }
    }
}

static void
UpdateConsole(game_state* GameState, console* Console, game_input* Input, memory_arena* TArena, f32 DeltaTime)
{
    if (Console->CommandCount == 0)
    {
        CONSOLE_COMMAND(Console, null_map_elem_count);
        CONSOLE_COMMAND(Console, clear);
        CONSOLE_COMMAND(Console, activated);
        CONSOLE_COMMAND(Console, color);
    }
    
    //Check if toggled
    bool IsOpen = (Console->TargetHeight > 0.0f);
    if (Input->ButtonDown & Button_Console)
    {
        if (IsOpen)
            Console->TargetHeight = 0.0f;
        else
            Console->TargetHeight = 0.2f;
    }
    
    //Update openness
    f32 ConsoleSpeed = 1.0f;
    f32 dHeight = ConsoleSpeed * DeltaTime;
    
    if (Console->Height < Console->TargetHeight)
    {
        Console->Height = Min(Console->TargetHeight, Console->Height + dHeight);
    }
    
    if (Console->Height > Console->TargetHeight)
    {
        Console->Height = Max(Console->TargetHeight, Console->Height - dHeight);
    }
    
    //Update cursor
    f32 CursorTime = 0.4f;
    
    Console->CursorCountdown -= DeltaTime;
    if (Console->CursorCountdown < 0.0f)
    {
        Console->CursorCountdown += CursorTime;
        Console->CursorOn = !Console->CursorOn;
    }
    
    //Update input
    if (IsOpen)
    {
        if (Input->ButtonDown & Button_Left)
        {
            Console->InputCursor = Max(0, Console->InputCursor - 1);
        }
        
        if (Input->ButtonDown & Button_Right)
        {
            Console->InputCursor = Min((i32)Console->InputLength, (i32)(Console->InputCursor + 1));
        }
        
        //Clear input to prevent other actions
        Input->Button = 0;
        Input->ButtonDown = 0;
        Input->ButtonUp = 0;
        Input->Movement = {};
        
        for (char* TextInput = Input->TextInput; *TextInput; TextInput++)
        {
            char C = *TextInput;
            switch (C)
            {
                case '\n':
                {
                    string Command = {Console->Input, (u32)Console->InputLength};
                    ParseAndRunCommand(Console, Command, GameState, TArena);
                    
                    Console->InputLength = 0;
                    Console->InputCursor = 0;
                } break;
                case '\b':
                {
                    if (Console->InputCursor > 0)
                    {
                        memmove(Console->Input + Console->InputCursor - 1, 
                                Console->Input + Console->InputCursor,
                                Console->InputLength - Console->InputCursor);
                        Console->InputLength--;
                        Console->InputCursor--;
                    }
                } break;
                default:
                {
                    if (Console->InputLength + 1 < ArrayCount(Console->Input))
                    {
                        memmove(Console->Input + Console->InputCursor + 1, 
                                Console->Input + Console->InputCursor,
                                Console->InputLength - Console->InputCursor);
                        
                        Console->Input[Console->InputCursor++] = C;
                        Console->InputLength++;
                    }
                } break;
            }
        }
    }
}
static void
DrawConsole(console* Console, render_group* RenderGroup, memory_arena* Arena)
{
    f32 ScreenTop = 0.5625;
    
    f32 X0 = 0.1f;
    f32 X1 = 0.9f;
    
    f32 InputTextHeight = 0.02f;
    
    f32 Y0 = ScreenTop - Console->Height;
    f32 Width = X1 - X0;
    
    PushRectangle(RenderGroup, V2(X0, Y0 + InputTextHeight), V2(Width, Console->Height), 0xC0000000);
    PushRectangle(RenderGroup, V2(X0, Y0), V2(Width, InputTextHeight), 0xFF000000);
    
    string Input = ArenaPrint(Arena, "> %.*s", Console->InputLength, Console->Input);
    
    //Find cursor position
    Assert(Console->InputCursor + 2 <= Input.Length);
    string InputA = {Input.Text, Console->InputCursor + 2}; //Adjust for "> "
    
    f32 Pad = 0.002f;
    X0 += Pad;
    f32 WidthA = PlatformTextWidth(InputA, InputTextHeight);
    
    //Draw cursor
    if (Console->CursorOn)
    {
        PushRectangle(RenderGroup, 
                      V2(X0 + WidthA, Y0 + Pad), 
                      V2(Pad, InputTextHeight - 2.0f * Pad),
                      0xFFFFFFFF);
    }
    
    u32 TextColor = 0xFFFFFF;
    
    
    
    PushText(RenderGroup, Input, V2(X0, Y0), TextColor, InputTextHeight);
    
    for (string History : Console->History)
    {
        Y0 += InputTextHeight + Pad;
        PushText(RenderGroup, History, V2(X0, Y0), TextColor, InputTextHeight);
    }
}