static string
RunCommand(int ArgCount, string* Args, game_state* GameState, memory_arena* TArena)
{
    if (ArgCount == 0) return {};
    
    string Result = String("Unknown command");
    
    if (StringsAreEqual(Args[0], String("null_map_elem_count")))
    {
        u32 Count = 0;
        for (map_element& MapElem : GameState->Map->Elements)
        {
            if (MapElem.Type == MapElem_Null)
            {
                Count++;
            }
        }
        
        Result = ArenaPrint(TArena, "%u null elements", Count);
    }
    
    if (StringsAreEqual(Args[0], String("activated")) && ArgCount == 2)
    {
        u32 EntityIndex = StringToU32(Args[1]);
        
        if (EntityIndex < GameState->Map->Entities.Count)
        {
            bool Activated = GameState->Map->Entities[EntityIndex].Activated;
            Result = ArenaPrint(TArena, "%u activated: %u", EntityIndex, Activated);
        }
        else
        {
            Result = String("Out of range");
        }
    }
    
    return Result;
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
    
    string Result = RunCommand(ArgCount, Args, GameState, TArena);
    AddLine(Console, Command);
    AddLine(Console, Result);
    //TODO: This is probably not ideal
}

static void
UpdateConsole(game_state* GameState, console* Console, game_input* Input, memory_arena* TArena, f32 DeltaTime)
{
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
            Console->InputCursor = Min(Console->InputLength, Console->InputCursor + 1);
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
DrawConsole(console* Console)
{
    f32 ScreenTop = 0.5625;
    
    f32 X0 = 0.1f;
    f32 X1 = 0.9f;
    
    f32 InputTextHeight = 0.02f;
    
    f32 Y0 = ScreenTop - Console->Height;
    f32 Width = X1 - X0;
    
    PlatformRectangle(V2(X0, Y0 + InputTextHeight), V2(Width, Console->Height), 0xC0000000);
    PlatformRectangle(V2(X0, Y0), V2(Width, InputTextHeight), 0xFF000000);
    
    string InputA = {Console->Input, (u32)Console->InputCursor};
    string InputB = {Console->Input + Console->InputCursor, (u32)(Console->InputLength - Console->InputCursor)};
    
    f32 Pad = 0.002f;
    X0 += Pad;
    f32 WidthA = DrawString(V2(X0, Y0), InputA, InputTextHeight);
    
    //Draw cursor
    if (Console->CursorOn)
    {
        PlatformRectangle(V2(X0 + WidthA, Y0 + Pad), 
                          V2(Pad, InputTextHeight - 2.0f * Pad),
                          0xFFFFFFFF);
    }
    
    DrawString(V2(X0 + WidthA, Y0), InputB, InputTextHeight);
    
    for (string History : Console->History)
    {
        Y0 += InputTextHeight + Pad;
        DrawString(V2(X0, Y0), History, InputTextHeight);
    }
}