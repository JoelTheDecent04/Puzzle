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

void
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
        case MapElem_Rectangle: case MapElem_Receiver: case MapElem_Laser: case MapElem_Box: case MapElem_Goal: case MapElem_Window:
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
RunEditor(game_state* GameState, game_input* Input, allocator Allocator)
{
    memory_arena* TArena = Allocator.Transient;
    memory_arena* PArena = Allocator.Permanent;
    
    map_editor* Editor = &GameState->Editor;
    map_desc* Map = GameState->Map;
    
    if (!Input->Buttons.MouseLeft)
    {
        Editor->Dragging = false;
    }
    
    PlatformRectangle(V2(0.0f, 0.1f), V2(0.5f, 0.5f), 0x80808080);
    
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
        
        PlatformRectangle(Box, 0, 0xFFFF0000);
        
        if (Editor->Dragging)
        {
            v2 NewPosition = Input->Cursor + Editor->CursorToElement;
            
            if (Input->Controls.LShift)
            {
                v2 NewBottomLeft = NewPosition - 0.5f * SelectedElement->Shape.Size;
                v2 NewBottomLeftRounded = { Round(NewBottomLeft.X / TileSize.X) * TileSize.X, Round(NewBottomLeft.Y / TileSize.Y) * TileSize.Y };
                NewPosition = NewBottomLeftRounded + 0.5f * SelectedElement->Shape.Size;
            }
            
            EditShape->Position = NewPosition;
        }
    }
    
    BeginGUI(Input);
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
        Box.Color = 0xFFFF0000;
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
        Window.Color = 0x80808080;
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
    
    Layout.NextRow();
    
    if (SelectedElement && SelectedElement->ActivatedBy)
    {
        SelectedElement->Shape = Editor->EditingTheActivatedState ? SelectedElement->ActivatedShape : SelectedElement->UnactivatedShape;
        
        Layout.Label("Controller:");
        Layout.Label(ArenaPrint(TArena, "%u", SelectedElement->ActivatedBy));
        
        Layout.NextRow();
        char* CurrentStateText = Editor->EditingTheActivatedState ? "Active" : "Unactive";
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
    if (Input->Buttons.MouseDownLeft && !GUIInputIsBeingHandled())
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
    span<rigid_body> RigidBodies = AllocSpan(MapArena, rigid_body, ComponentMaxCount);
    span<box> Boxes = AllocSpan(MapArena, box, ComponentMaxCount);
    span<attachment> Attachments = AllocSpan(MapArena, attachment, ComponentMaxCount);
    
    u32 RigidBodyCount = 0;
    u32 BoxCount = 0;
    u32 AttachmentCount = 0;
    
    Map->Player = {};
    
    u32 PlayerRigidBodyIndex = RigidBodyCount++;
    rigid_body* PlayerRigidBody = RigidBodies + PlayerRigidBodyIndex;
    
    PlayerRigidBody->P = V2(0.5f, 0.3f);
    PlayerRigidBody->Size = V2(0.025f, 0.025f);
    PlayerRigidBody->InvMass = 1.0f;
    
    Map->Player.RigidBodyIndex = PlayerRigidBodyIndex;
    
    u32 EntityCount = 0;
    for (u32 MapElementIndex = 0; MapElementIndex < Map->Elements.Count; MapElementIndex++)
    {
        map_element* MapElement = Map->Elements + MapElementIndex;
        switch (MapElement->Type)
        {
            case MapElem_Box:
            {
                u32 RigidBodyIndex = RigidBodyCount++;
                rigid_body* RigidBody = RigidBodies + RigidBodyIndex;
                RigidBody->P = MapElement->Shape.Position;
                RigidBody->Size  = MapElement->Shape.Size;
                RigidBody->InvMass = 1.0f;
                
                u32 BoxIndex = BoxCount++;
                box* Box = Boxes + BoxIndex;
                Box->RigidBodyIndex = RigidBodyIndex;
                Box->Color = 0xFFFFFFFF;
                
                if (MapElement->AttachedTo)
                {
                    attachment Attachment = {};
                    Attachment.RigidBodyIndex = RigidBodyIndex;
                    Attachment.ElementIndex = MapElement->AttachedTo;
                    
                    map_element* AttachedTo = Map->Elements + MapElement->AttachedTo;
                    Attachment.Offset = AttachedTo->Shape.Position - MapElement->Shape.Position;
                    
                    Attachments[AttachmentCount++] = Attachment;
                }
                
            } break;
            case MapElem_Rectangle:
            {
                rigid_body* RigidBody = &RigidBodies[RigidBodyCount++];
                RigidBody->P = MapElement->Shape.Position;
                RigidBody->Size  = MapElement->Shape.Size;
                RigidBody->InvMass = 0.0f;
            } break;
            case MapElem_Window:
            {
                rigid_body* RigidBody = &RigidBodies[RigidBodyCount++];
                RigidBody->P = MapElement->Shape.Position;
                RigidBody->Size  = MapElement->Shape.Size;
                RigidBody->InvMass = 0.0f;
                RigidBody->Transparent = true;
            } break;
        }
    }
    
    Assert(RigidBodyCount <= ComponentMaxCount);
    RigidBodies.Count = RigidBodyCount;
    
    Assert(BoxCount <= ComponentMaxCount);
    Boxes.Count = BoxCount;
    
    Assert(AttachmentCount <= ComponentMaxCount);
    Attachments.Count = AttachmentCount;
    
    Map->RigidBodies = RigidBodies;
    Map->Boxes = Boxes;
    Map->Attachments = Attachments;
}

static void
OnEditorClose(game_state* GameState)
{
    CreateComponents(GameState->Map, &GameState->MapArena);
}