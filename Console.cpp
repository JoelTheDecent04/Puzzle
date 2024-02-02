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
                string Command = {Console->InputBuffer, Console->InputBufferLength};
                RunCommand(Console, Command);
                
                Console->InputBufferLength = 0;
            }
            else
            {
                if (Console->InputBufferLength + 1 < ArrayCount(Console->InputBuffer))
                {
                    Console->InputBuffer[Console->InputBufferLength++] = C;
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
    
    string Input = {Console->InputBuffer, Console->InputBufferLength};
    f32 TextWidth = DrawString(V2(X0, Y0), Input, InputTextHeight);
    
    //Draw cursor
    if (Console->CursorOn)
    {
        f32 CursorPad = 0.002f;
        PlatformRectangle(V2(X0 + TextWidth, Y0 + CursorPad), 
                          V2(CursorPad, InputTextHeight - 2.0f * CursorPad),
                          0xFFFFFFFF);
    }
}