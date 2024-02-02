void RunCommand(console* Console, string Command)
{
    if (StringsAreEqual(Command, String("quit")))
    {
        Console->TargetHeight = 0.0f;
    }
}

static void
UpdateConsole(console* Console, game_input* Input, f32 DeltaTime)
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
        
        for (char* TextInput = Input->TextInput;
             *TextInput;
             TextInput++)
        {
            char C = *TextInput;
            if (C == '\n')
            {
                string Command = {Console->Input, (u32)Console->InputLength};
                RunCommand(Console, Command);
                
                Console->InputLength = 0;
                Console->InputCursor = 0;
            }
            else if (C == '\b')
            {
                if (Console->InputCursor > 0)
                {
                    memmove(Console->Input + Console->InputCursor - 1, 
                            Console->Input + Console->InputCursor,
                            Console->InputLength - Console->InputCursor);
                    Console->InputLength--;
                    Console->InputCursor--;
                }
            }
            else
            {
                if (Console->InputLength + 1 < ArrayCount(Console->Input))
                {
                    memmove(Console->Input + Console->InputCursor + 1, 
                            Console->Input + Console->InputCursor,
                            Console->InputLength - Console->InputCursor);
                    
                    Console->Input[Console->InputCursor++] = C;
                    Console->InputLength++;
                }
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
    
    PlatformRectangle(V2(X0, Y0 + InputTextHeight), V2(Width, Console->Height), 0xC0FFFFFF);
    PlatformRectangle(V2(X0, Y0), V2(Width, InputTextHeight), 0xFF000000);
    
    string InputA = {Console->Input, (u32)Console->InputCursor};
    string InputB = {Console->Input + Console->InputCursor, (u32)(Console->InputLength - Console->InputCursor)};
    
    f32 WidthA = DrawString(V2(X0, Y0), InputA, InputTextHeight);
    
    //Draw cursor
    if (Console->CursorOn)
    {
        f32 CursorPad = 0.002f;
        PlatformRectangle(V2(X0 + WidthA, Y0 + CursorPad), 
                          V2(CursorPad, InputTextHeight - 2.0f * CursorPad),
                          0xFFFFFFFF);
    }
    
    DrawString(V2(X0 + WidthA, Y0), InputB, InputTextHeight);
}