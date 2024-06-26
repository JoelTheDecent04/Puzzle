#include <string.h>

static void
ChangeMap(game_state* GameState, u32 NewMapIndex, memory_arena* Arena)
{
    Assert(Arena->Type == PERMANENT);
    
    Assert(NewMapIndex < GameState->Maps.Count);
    
    map_desc* NewMap = GameState->Maps[NewMapIndex];
    
    if (!NewMap)
    {
        NewMap = AllocStruct(Arena, map_desc);
    }
    
    GameState->Map = NewMap;
    GameState->MapIndex = NewMapIndex;
}

static inline void
DeleteElement(u32 Index, map_desc* Map)
{
    Map->Elements[Index] = {MapElem_Null};
}

static span<u8>
SerialiseMap(memory_arena* Arena, map_desc* Map)
{
    u32 Bytes = sizeof(saved_map_header) + Map->Elements.Count * sizeof(map_element);
    span<u8> Data = AllocSpan(Arena, u8, Bytes);
    
    saved_map_header* Header = (saved_map_header*)Data.Memory;
    
    Header->ElementCount = Map->Elements.Count;
    Header->ElementSize = sizeof(map_element);
    
    map_element* Elements = (map_element*)(Data.Memory + sizeof(saved_map_header));
    
    memcpy(Elements, Map->Elements.Memory, Header->ElementCount * sizeof(map_element));
    
    return Data;
}

static map_desc*
DeserialiseMap(memory_arena* Arena, span<u8> Data)
{
    saved_map_header* Header = (saved_map_header*)Data.Memory;
    map_desc* Map = 0;
    
    if (Header)
    {
        if (Header->ElementSize != sizeof(map_element))
        {
            PlatformDebugOut(String("Warning: Map element has different size\n"));
        }
        
        Map = AllocStruct(Arena, map_desc);
        
        span<map_element> Elements = AllocSpan(Arena, map_element, Header->ElementCount);
        
        u8* Buffer = (u8*)(Header + 1);
        for (u32 ElementIndex = 0; ElementIndex < Header->ElementCount; ElementIndex++)
        {
            memcpy(&Elements[ElementIndex], Buffer, Header->ElementSize);
            Buffer += Header->ElementSize;
        }
        
        Map->Elements = Array(Elements);
    }
    return Map;
}

static rect
BoundingBox(map_element* MapElem)
{
    rect Rect = {};
    switch (MapElem->Type)
    {
        case MapElem_Rectangle: case MapElem_Receiver: case MapElem_Laser: case MapElem_Box: case MapElem_Goal: case MapElem_Window: case MapElem_Circle:
        {
            Rect = {
                MapElem->Shape.Position - 0.5f * MapElem->Shape.Size,
                MapElem->Shape.Position + 0.5f * MapElem->Shape.Size
            };
        } break;
        
        case MapElem_Reflector: case MapElem_Line:
        {
            f32 Padding = 0.01f;
            
            f32 StartX = MapElem->Shape.Start.X;
            f32 StartY = MapElem->Shape.Start.Y;
            f32 OffsetX = MapElem->Shape.Offset.X;
            f32 OffsetY = MapElem->Shape.Offset.Y;
            
            f32 MinX = Min(StartX, StartX + OffsetX) - Padding;
            f32 MinY = Min(StartY, StartY + OffsetY) - Padding;
            f32 MaxX = Max(StartX, StartX + OffsetX) + Padding;
            f32 MaxY = Max(StartY, StartY + OffsetY) + Padding;
            
            Rect = {
                { MinX, MinY },
                { MaxX, MaxY }
            };
            
        } break;
        default: Assert(MapElem->Type == MapElem_Null);
    }
    
    return Rect;
}

static inline map_element*
GetSelectedElement(map_editor* Editor, map_desc* Map)
{
    map_element* Result = 0;
    if (Editor->SelectedElementIndex < Map->Elements.Count)
    {
        Result = &Map->Elements[Editor->SelectedElementIndex];
    }
    return Result;
}

static inline shape*
GetShapeToEdit(map_editor* Editor, map_desc* Map)
{
    map_element* Element = GetSelectedElement(Editor, Map);
    shape* Shape = 0;
    
    if (Element)
    {
        if (Element->ActivatedBy)
        {
            if (Editor->EditingTheActivatedState)
            {
                Shape = &Element->ActivatedShape;
            }
            else
            {
                Shape = &Element->UnactivatedShape;
            }
        }
        else
        {
            Shape = &Element->Shape;
        }
    }
    
    return Shape;
}

static void
SaveMapToDisk(game_state* GameState, memory_arena* Arena)
{
    string Path = ArenaPrint(Arena, "maps/map%u.bin", GameState->MapIndex);
    span<u8> MapData = SerialiseMap(Arena, GameState->Map);
    PlatformSaveFile(Path.Text, MapData);
}

static void
RunEditor(render_group* Group, game_state* GameState, game_input* Input, allocator Allocator)
{
    memory_arena* TArena = Allocator.Transient;
    memory_arena* PArena = Allocator.Permanent;
    
    map_editor* Editor = &GameState->Editor;
    map_desc* Map = GameState->Map;
    
    if (!(Input->Button & Button_LMouse))
    {
        Editor->Dragging = false;
    }
    
    PushRectangle(Group, V2(0.0f, 0.1f), V2(0.5f, 0.5f), 0x80808080);
    
    i32 NextElementSelection = 0;
    
    v2 TileSize = { 0.04f, 0.04f };
    v2 BoxSize = { 0.03f, 0.03f };
    
    map_element* SelectedElement = GetSelectedElement(Editor, GameState->Map);
    shape* EditShape = GetShapeToEdit(Editor, Map);
    
    if (SelectedElement)
    {
        rect Box = BoundingBox(SelectedElement);
        
        v2 Gap = { 0.01f, 0.01f };
        Box.MinCorner -= Gap;
        Box.MaxCorner += Gap;
        
        PushRectangleOutline(Group, Box, 0xFFFF0000, 0.003f);
        
        if (Editor->Dragging)
        {
            v2 NewPosition = Input->Cursor + Editor->CursorToElement;
            
            if (Input->Button & Button_LShift)
            {
                v2 NewBottomLeft = NewPosition - 0.5f * SelectedElement->Shape.Size;
                v2 NewBottomLeftRounded = { Round(NewBottomLeft.X / TileSize.X) * TileSize.X, Round(NewBottomLeft.Y / TileSize.Y) * TileSize.Y };
                NewPosition = NewBottomLeftRounded + 0.5f * SelectedElement->Shape.Size;
            }
            
            EditShape->Position = NewPosition;
        }
    }
    
    BeginGUI(Input, Group);
    gui_layout Layout = DefaultLayout(0.0f, ScreenTop);
    
    string Path = ArenaPrint(TArena, "maps/map%u.bin", GameState->MapIndex);
    
    if (Layout.Button("Load"))
    {
        span<u8> MapData = PlatformLoadFile(TArena, Path.Text);
        GameState->Map = DeserialiseMap(PArena, MapData);
        PlatformDebugOut(String("Warning: Does not update map array\n"));
        return;
    }
    
    if (Layout.Button("Save"))
    {
        SaveMapToDisk(GameState, TArena);
    }
    
    if (Layout.Button("Prev Map"))
    {
        int NewMapIndex = GameState->MapIndex - 1;
        if (NewMapIndex == -1)
        {
            NewMapIndex = GameState->Maps.Count - 1;
        }
        
        ChangeMap(GameState, NewMapIndex, Allocator.Permanent);
        return;
    }
    
    if (Layout.Button("Next Map"))
    {
        int NewMapIndex = (GameState->MapIndex + 1) % GameState->Maps.Count;
        
        SaveMapToDisk(GameState, TArena);
        ChangeMap(GameState, NewMapIndex, PArena);
        
        return;
    }
    
    Layout.NextRow();
    
    Layout.Label("Add");
    Layout.NextRow();
    
    v2 ScreenCenter = V2(0.5f, 0.5f * ScreenTop);
    
    if (Layout.Button("Rect"))
    {
        map_element Rectangle = { MapElem_Rectangle };
        Rectangle.Shape.Position = ScreenCenter;
        Rectangle.Shape.Size = TileSize;
        Rectangle.Color = 0xFFFFFFFF;
        Add(&Map->Elements, &Rectangle, PArena);
    }
    
    if (Layout.Button("Sensor"))
    {
        map_element Sensor = { MapElem_Receiver };
        Sensor.Shape.Position = ScreenCenter;
        Sensor.Shape.Size = V2(0.01f, 0.01f);
        Sensor.Color = 0xFFFFFFFF;
        Add(&Map->Elements, &Sensor, PArena);
    }
    
    if (Layout.Button("Laser"))
    {
        map_element Laser = { MapElem_Laser };
        Laser.Shape.Position = ScreenCenter;
        Laser.Shape.Size = V2(0.01f, 0.01f);
        Laser.Angle = 0.0f;
        Laser.Color = 0xFF00FF00;
        Add(&Map->Elements, &Laser, PArena);
    }
    
    if (Layout.Button("Reflector"))
    {
        map_element Reflector = { MapElem_Reflector };
        Reflector.Shape.Start = ScreenCenter;
        Reflector.Shape.Offset = TileSize;
        Reflector.Color = 0xFF808080;
        Add(&Map->Elements, &Reflector, PArena);
    }
    
    if (Layout.Button("Box"))
    {
        map_element Box = { MapElem_Box };
        Box.Shape.Position = ScreenCenter;
        Box.Shape.Size = BoxSize;
        Box.Color = 0xFFFFFFFF;
        Add(&Map->Elements, &Box, PArena);
    }
    
    Layout.NextRow();
    
    if (Layout.Button("Goal"))
    {
        map_element Goal = { MapElem_Goal };
        Goal.Shape.Position = ScreenCenter;
        Goal.Shape.Size = TileSize;
        Goal.Color = 0xFF0000FF;
        Add(&Map->Elements, &Goal, PArena);
    }
    
    if (Layout.Button("Window"))
    {
        map_element Window = { MapElem_Window };
        Window.Shape.Position = ScreenCenter;
        Window.Shape.Size = TileSize;
        Window.Color = 0x80FFFFFF;
        Add(&Map->Elements, &Window, PArena);
    }
    
    if (Layout.Button("Line"))
    {
        map_element Line = { MapElem_Line };
        Line.Shape.Position = ScreenCenter;
        Line.Shape.Size = TileSize;
        Line.Color = 0xFFFFFFFF;
        Add(&Map->Elements, &Line, PArena);
    }
    
    if (Layout.Button("Circle"))
    {
        map_element Line = { MapElem_Circle };
        Line.Shape.Position = ScreenCenter;
        Line.Shape.Size = BoxSize;
        Line.Color = 0xFFFFFFFF;
        Add(&Map->Elements, &Line, PArena);
    }
    
    Layout.NextRow();
    
    if (SelectedElement && SelectedElement->ActivatedBy)
    {
        SelectedElement->Shape = Editor->EditingTheActivatedState ? SelectedElement->ActivatedShape : SelectedElement->UnactivatedShape;
        
        Layout.Label("Controller:");
        Layout.Label(ArenaPrint(TArena, "%u", SelectedElement->ActivatedBy));
        
        Layout.NextRow();
        const char* CurrentStateText = Editor->EditingTheActivatedState ? "Active" : "Unactive";
        if (Layout.Button(CurrentStateText))
        {
            Editor->EditingTheActivatedState = !Editor->EditingTheActivatedState;
        }
    }
    
    Layout.NextRow();
    
    if (SelectedElement)
    {
        string ElemIDString = ArenaPrint(TArena, "ID: %u", Editor->SelectedElementIndex);
        Layout.Label(ElemIDString);
        Layout.NextRow();
    }
    
    if (EditShape)
    {
        string PosText = ArenaPrint(TArena, "(%.0f, %.0f)", EditShape->Position.X * 1000, EditShape->Position.Y * 1000);
        Layout.Label("(X, Y)");
        Layout.Label(PosText);
        
        Layout.NextRow();
        
        string SizeText = ArenaPrint(TArena, "(%.0f, %.0f)", EditShape->Size.X * 1000, EditShape->Size.Y * 1000);
        Layout.Label("(W, H)");
        Layout.Label(SizeText);
        
        Layout.NextRow();
        
        Layout.Label("Width");
        
        if (Layout.Button("<"))
        {
            EditShape->Size.X = EditShape->Size.X - TileSize.X;
            //EditShape->Size.X = Max(TileSize.X, EditShape->Size.X - TileSize.X);
        }
        
        if (Layout.Button(">"))
        {
            EditShape->Size.X += TileSize.X;
        }
        
        Layout.NextRow();
        
        Layout.Label("Height");
        
        if (Layout.Button("<"))
        {
            EditShape->Size.Y = EditShape->Size.Y - TileSize.Y;
            //EditShape->Size.Y = Max(TileSize.Y, EditShape->Size.Y - TileSize.Y);
        }
        
        if (Layout.Button(">"))
        {
            EditShape->Size.Y += TileSize.Y;
        }
        
        if (SelectedElement->Type == MapElem_Laser)
        {
            Layout.NextRow();
            Layout.Label("Angle");
            
            if (Layout.Button("<"))
            {
                SelectedElement->Angle -= 0.125f;
            }
            
            if (Layout.Button(">"))
            {
                SelectedElement->Angle += 0.125f;
            }
        }
        
        Layout.NextRow();
        
        Layout.Label("Color");
        PushRectangle(Group, V2(Layout.X, Layout.Y), V2(0.02f, 0.02f), SelectedElement->Color);
        Layout.X += 0.025f;
        Layout.Label(ArenaPrint(TArena, "0x%x", SelectedElement->Color));
        
        Layout.NextRow();
    }
    
    if (SelectedElement)
    {
        Layout.NextRow();
        
        if (Layout.Button("Delete"))
        {
            
            Map->Elements[Editor->SelectedElementIndex] = {MapElem_Null};
			Editor->SelectedElementIndex = 0;
        }
        
        if (Layout.Button("Copy"))
        {
            map_element* Element = GetSelectedElement(Editor, Map);
            Add(&Map->Elements, Element, PArena);
        }
        
        if (Layout.Button("Interact"))
        {
            Editor->State = MapEditor_InteractSelection;
        }
        if (Layout.Button("Attach"))
        {
            Editor->State = MapEditor_AttachSelection;
        }
        
        Layout.NextRow();
        
        if (SelectedElement->AttachedTo)
        {
            string AttachedToString = ArenaPrint(TArena, "Attached to %u, offset (%.3f, %.3f)", 
                                                 SelectedElement->AttachedTo,
                                                 SelectedElement->AttachmentOffset.X, 
                                                 SelectedElement->AttachmentOffset.Y);
            Layout.Label(AttachedToString);
            
            if (Layout.Button("Detach"))
            {
                SelectedElement->AttachedTo = 0;
                SelectedElement->AttachmentOffset = {};
            }
        }
        
    }
    
    //Clicking another element
    if ((Input->ButtonDown & Button_LMouse) && !GUIInputIsBeingHandled())
    {
        for (u32 MapElementIndex = 0; MapElementIndex < Map->Elements.Count; MapElementIndex++)
        {
            map_element* MapElement = Map->Elements + MapElementIndex;
            if (PointInRect(BoundingBox(MapElement), Input->Cursor))
            {
                map_element* NewSelectedElement = MapElement;
                bool ShouldChangeElementSelection = true;
                
                if (Editor->State == MapEditor_InteractSelection)
                {
                    map_element* SelectedElement = GetSelectedElement(Editor, Map);
                    Assert(SelectedElement);
                    SelectedElement->ActivatedBy = MapElementIndex;
                    SelectedElement->ActivatedShape = SelectedElement->Shape;
                    SelectedElement->UnactivatedShape = SelectedElement->Shape;
                    ShouldChangeElementSelection= false;
                }
                
                if (Editor->State == MapEditor_AttachSelection)
                {
                    map_element* SelectedElement = GetSelectedElement(Editor, Map);
                    SelectedElement->AttachedTo = MapElementIndex;
                    SelectedElement->AttachmentOffset = NewSelectedElement->Shape.Position - SelectedElement->Shape.Position;
                }
                
                Editor->State = MapEditor_Default;
                if (ShouldChangeElementSelection)
                {
                    Editor->SelectedElementIndex = MapElementIndex;
                    Editor->Dragging = true;
                    Editor->CursorToElement = MapElement->Shape.Position - Input->Cursor;
                    break;
                }
            }
        }
    }
}

static void
CreateComponents(map_desc* Map, memory_arena* MapArena)
{
    ResetArena(MapArena);
    
    u32 ComponentMaxCount = Map->Elements.Count + 1; //One for the player
    static_array<rigid_body> RigidBodies = AllocStaticArray(MapArena, rigid_body, ComponentMaxCount);
    static_array<entity> Entities =        AllocStaticArray(MapArena, entity, ComponentMaxCount);
    static_array<attachment> Attachments = AllocStaticArray(MapArena, attachment, ComponentMaxCount);
    static_array<line> Lines =             AllocStaticArray(MapArena, line, ComponentMaxCount);
    static_array<laser> Lasers =           AllocStaticArray(MapArena, laser, ComponentMaxCount);
    
    //TODO: I hate this
    Add(&Lasers, {});
    
    rigid_body PlayerRigidBody = {};
    PlayerRigidBody.P = V2(0.5f, 0.3f);
    PlayerRigidBody.Size = V2(0.025f, 0.025f);
    PlayerRigidBody.InvMass = 1.0f;
    PlayerRigidBody.Color = 0xFFC0C0C0;
    
    entity* Player = &Map->Player;
    *Player = {Entity_Player};
    Player->RigidBodyIndex = Add(&RigidBodies, PlayerRigidBody);
    
    for (u32 MapElementIndex = 0; MapElementIndex < Map->Elements.Count; MapElementIndex++)
    {
        map_element MapElem = Map->Elements[MapElementIndex];
        
        u32 RigidBodyIndex = 0;
        u32 LineIndex = 0;
        u32 LaserIndex = 0;
        
        switch (MapElem.Type)
        {
            case MapElem_Box:
            {
                rigid_body RigidBody = {};
                RigidBody.Type = RigidBody_AABB;
                RigidBody.P = MapElem.Shape.Position;
                RigidBody.Size  = MapElem.Shape.Size;
                RigidBody.InvMass = 1.0f;
                RigidBody.Color = MapElem.Color;
                
                RigidBodyIndex = Add(&RigidBodies, RigidBody);
            } break;
            case MapElem_Circle:
            {
                rigid_body RigidBody = {};
                RigidBody.Type = RigidBody_Circle;
                RigidBody.P = MapElem.Shape.Position;
                RigidBody.Size  = MapElem.Shape.Size;
                RigidBody.InvMass = 1.0f;
                RigidBody.Color = MapElem.Color;
                
                RigidBodyIndex = Add(&RigidBodies, RigidBody);
                
            } break;
            case MapElem_Rectangle: case MapElem_Window: case MapElem_Receiver:
            {
                rigid_body RigidBody = {};
                RigidBody.P = MapElem.Shape.Position;
                RigidBody.Size  = MapElem.Shape.Size;
                RigidBody.InvMass = 0.0f;
                RigidBody.Translucent = (MapElem.Type == MapElem_Window || MapElem.Type == MapElem_Laser);
                RigidBody.Color = MapElem.Color;
                RigidBody.ActivatedByIndex = MapElem.ActivatedBy;
                RigidBody.ActivatedP = MapElem.ActivatedShape.Position;
                RigidBody.ActivatedSize = MapElem.ActivatedShape.Size;
                RigidBody.UnactivatedP = MapElem.UnactivatedShape.Position;
                RigidBody.UnactivatedSize = MapElem.UnactivatedShape.Size;
                
                RigidBodyIndex = Add(&RigidBodies, RigidBody);
            } break;
            case MapElem_Reflector: case MapElem_Line:
            {
                line Line = {};
                Line.Color = MapElem.Color;
                Line.Start = MapElem.Shape.Start;
                Line.Offset = MapElem.Shape.Offset;
                Line.Reflective = (MapElem.Type == MapElem_Reflector);
                
                LineIndex = Add(&Lines, Line);
            } break;
            case MapElem_Laser:
            {
                laser Laser = {};
                Laser.Color = MapElem.Color;
                Laser.Angle = MapElem.Angle;
                Laser.Position = MapElem.Shape.Position;
                Laser.ActivatedByIndex = MapElem.ActivatedBy;
                
                LaserIndex = Add(&Lasers, Laser);
            }
            default:
            {
            }
        }
        
        if (MapElem.AttachedTo)
        {
            Assert(RigidBodyIndex || LineIndex || LaserIndex);
            
            map_element* AttachedTo = Map->Elements + MapElem.AttachedTo;
            v2 Offset = MapElem.Shape.Position - AttachedTo->Shape.Position;
            
            attachment Attachment = {};
            Attachment.EntityIndex = MapElementIndex;
            Attachment.AttachedToEntityIndex = MapElem.AttachedTo;
            Attachment.Offset = Offset;
            
            Add(&Attachments, Attachment);
        }
        
        entity Entity = {};
        Entity.RigidBodyIndex = RigidBodyIndex;
        Entity.LineIndex = LineIndex;
        Entity.LaserIndex = LaserIndex;
        
        u32 EntityIndex = Add(&Entities, Entity);
        
        if (RigidBodyIndex)
        {
            RigidBodies[RigidBodyIndex].EntityIndex = EntityIndex;
        }
        if (LineIndex)
        {
            Lines[LineIndex].EntityIndex = EntityIndex;
        }
    }
    
    
    Map->Entities = Entities;
    
    Map->RigidBodies = RigidBodies;
    Map->Attachments = Attachments;
    Map->Lines = Lines;
    Map->Lasers = Lasers;
    Map->Attachments = Attachments;
}

static void
OnEditorClose(game_state* GameState)
{
    CreateComponents(GameState->Map, &GameState->MapArena);
}