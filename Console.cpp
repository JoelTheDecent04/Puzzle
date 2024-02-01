static void
UpdateConsole(console* Console, game_input* Input, f32 DeltaTime)
{
    //Check if toggled
    if (Input->ButtonDown & Button_Menu)
    {
        bool IsOpen = Console->TargetHeight > 0.0f;
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
    for (char* TextInput = Input->TextInput;
         *TextInput;
         TextInput++)
    {
        char C = *TextInput;
        if (C == '\n')
        {
            //RunCommand();
            
            Console->InputBufferLength = 0;
        }
        
        if (Console->InputBufferLength + 1 < ArrayCount(Console->InputBuffer))
        {
            Console->InputBuffer[Console->InputBufferLength++] = C;
        }
    }
}

static void
DrawConsole(console* Console)
{
    f32 ScreenTop = 0.5625;
    
    f32 X0 = 0.1f;
    f32 X1 = 0.9f;
    
    f32 InputHeight = 0.02f;
    
    f32 Y0 = ScreenTop - Console->Height;
    f32 Width = X1 - X0;
    
    PlatformRectangle(V2(X0, Y0 + InputHeight), V2(Width, Console->Height), 0xC0FFFFFF);
    PlatformRectangle(V2(X0, Y0), V2(Width, InputHeight), 0xFFFFFFFF);
    
    string Input = {Console->InputBuffer, Console->InputBufferLength};
    PlatformDrawText(Input, V2(X0, Y0), V2(Width, InputHeight));
}