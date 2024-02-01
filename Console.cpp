static inline bool
IsOpen(console Console)
{
    return Console.TargetHeight > 0.0f;
}

static void
UpdateConsole(console* Console, f32 DeltaTime)
{
    f32 ConsoleSpeed = 5.0f;
    f32 dHeight = ConsoleSpeed * DeltaTime;
    
    if (Console->Height < Console->TargetHeight)
    {
        Console->Height = Min(Console->TargetHeight, Console->Height + dHeight);
    }
    
    if (Console->Height > Console->TargetHeight)
    {
        Console->Height = Max(Console->TargetHeight, Console->Height - dHeight);
    }
}

static void
DrawConsole(console Console)
{
    if (IsOpen(Console))
    {
        f32 ScreenTop = 0.5625;
        f32 X0 = 0.1f;
        f32 X1 = 0.9f;
        f32 Y0 = ScreenTop - Console.Height;
        f32 Width = X1 - X0;
        PlatformRectangle(V2(X0, Y0), V2(Width, Console.Height), 0xC0FFFFFF);
        PlatformRectangle(V2(X0, Y0 - 0.02f), V2(Width, 0.02f), 0xFFFFFFFF);
    }
}

static void ToggleConsole(console* Console)
{
    if (Console->TargetHeight == 0.0f)
    {
        Console->TargetHeight = 0.2f;
    }
    else
    {
        Console->TargetHeight = 0.0f;
    }
    
}