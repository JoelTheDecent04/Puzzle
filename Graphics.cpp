static void
PushRectangle(render_group* Group, v2 Position, v2 Size, u32 Color)
{
    render_shape Shape = {Render_Rectangle};
    Shape.Rectangle.Position = Position;
    Shape.Rectangle.Size = Size;
    Shape.Color = Color;
    Group->Shapes[Group->ShapeCount++] = Shape;
    Assert(Group->ShapeCount <= ArrayCount(Group->Shapes));
}

static void
PushRectangle(render_group* Group, rect Rect, u32 Color)
{
    PushRectangle(Group, Rect.MinCorner, Rect.MaxCorner - Rect.MinCorner, Color);
}

static void
PushRectangleOutline(render_group* Group, rect Rect, u32 Color, f32 Thickness = 0.01f)
{
    v2 MinCorner = Rect.MinCorner;
    v2 MaxCorner = Rect.MaxCorner;
    v2 Size = MaxCorner - MinCorner;
    
    PushRectangle(Group, V2(MinCorner.X, MaxCorner.Y), V2(Size.X, Thickness), Color);
    PushRectangle(Group, V2(MinCorner.X, MinCorner.Y - Thickness), V2(Size.X, Thickness), Color);
    PushRectangle(Group, V2(MinCorner.X - Thickness, MinCorner.Y), V2(Thickness, Size.Y), Color);
    PushRectangle(Group, V2(MaxCorner.X, MinCorner.Y), V2(Thickness, Size.Y), Color);
}

static void
PushCircle(render_group* Group, v2 Position, f32 Radius, u32 Color)
{
    render_shape Shape = {Render_Circle};
    Shape.Circle.Position = Position;
    Shape.Circle.Radius = Radius;
    Shape.Color = Color;
    Group->Shapes[Group->ShapeCount++] = Shape;
    Assert(Group->ShapeCount <= ArrayCount(Group->Shapes));
}

static void
PushLine(render_group* Group, v2 Start, v2 End, u32 Color, f32 Thickness)
{
    render_shape Shape = {Render_Line};
    Shape.Line.Start = Start;
    Shape.Line.End = End;
    Shape.Line.Thickness = Thickness;
    Shape.Color = Color;
    Group->Shapes[Group->ShapeCount++] = Shape;
    Assert(Group->ShapeCount <= ArrayCount(Group->Shapes));
}

static void
PushText(render_group* Group, string String, v2 Position, u32 Color = 0xFFFFFFFF, f32 Size = 0.015f)
{
    render_shape Shape = {Render_Text};
    Shape.Text.String = String;
    Shape.Text.Position = Position;
    Shape.Text.Size = Size;
    Shape.Color = Color;
    Group->Shapes[Group->ShapeCount++] = Shape;
    Assert(Group->ShapeCount <= ArrayCount(Group->Shapes));
}
