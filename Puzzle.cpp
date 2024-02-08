//Bugs
//if modifications are made to the level and the file is reloaded, nothing can be selected

#include "Graphics.cpp"
#include "GUI.cpp"
#include "Editor.cpp"
#include "Physics.cpp"
#include "Console.cpp"

void PhysicsUpdate(span<rigid_body> RigidBodies, f32 DeltaTime, v2 Movement, rigid_body* Controlling);

struct ray_collision
{
    bool DidHit;
    v2 P;
    v2 Normal;
    f32 t;
};

static ray_collision
TestRayIntersection(v2 P, v2 Direction, line_segment Wall)
{
    f32 Epsilon = 0.00001f;
    v2 WallDirection = Wall.End - Wall.Start;
    
    v2 st = Inverse(M2x2(WallDirection, -1.0f * Direction)) * (P - Wall.Start);
    f32 s = st.X;
    f32 t = st.Y;
    
    ray_collision Result = {};
    Result.DidHit = (s >= 0.0f && s <= 1.0f) && (t > 0.0f);
    if (Result.DidHit)
    {
        Result.P = P + t * Direction - Epsilon * UnitV(Direction);
        Result.Normal = Direction - DotProduct(Direction, UnitV(WallDirection)) * UnitV(WallDirection); //TODO : Make a projection function
        Result.t = t;
    }
    return Result;
}

struct laser_beam
{
    v2 Start;
    v2 End;
    u32 Color;
};

static void
CalculateReflections(laser_beam* Result, u32 MaxIter, map_desc* Map, laser* Laser)
{
    v2 P = Laser->Position;
    f32 AngleRadians = Laser->Angle * 2 * 3.14159f;
    v2 Direction = { cosf(AngleRadians), sinf(AngleRadians) };
    
    bool Done = false;
    
    u32 Iter = 0;
    for (; Iter < MaxIter; Iter++)
    {
        f32 tMin = 100.0f;
        ray_collision NearestCollision = {};
        u32 NearestCollisionEntityIndex = 0;
        bool WillReflect = 0;
        
        for (line Line : Map->Lines)
        {
            line_segment LineSegment = {Line.Start, Line.Start + Line.Offset};
            ray_collision Collision = TestRayIntersection(P, Direction, LineSegment);
            
            if (Collision.DidHit && Collision.t < tMin)
            {
                NearestCollision = Collision;
                tMin = Collision.t;
                WillReflect = Line.Reflective;
                NearestCollisionEntityIndex = Line.EntityIndex;
            }
        }
        
        for (rigid_body& RigidBody : Map->RigidBodies)
        {
            if (RigidBody.Transparent)
            {
                continue;
            }
            
            v2 MinCorner = RigidBody.P - 0.5f * RigidBody.Size;
            v2 MaxCorner = RigidBody.P + 0.5f * RigidBody.Size;
            
            line_segment Edges[4] = {
                {MinCorner, V2(MaxCorner.X, MinCorner.Y)},
                {MinCorner, V2(MinCorner.X, MaxCorner.Y)},
                {MaxCorner, V2(MinCorner.X, MaxCorner.Y)},
                {MaxCorner, V2(MaxCorner.X, MinCorner.Y)},
            }; 
            
            for (line_segment Edge : Edges)
            {
                ray_collision Collision = TestRayIntersection(P, Direction, Edge);
                
                if (Collision.DidHit && Collision.t < tMin)
                {
                    NearestCollision = Collision;
                    tMin = Collision.t;
                    WillReflect = false;
                    NearestCollisionEntityIndex = RigidBody.EntityIndex;
                }
            }
        }
        
        if (NearestCollision.DidHit)
        {
            laser_beam* LaserBeam = &Result[Iter];
            LaserBeam->Start = P;
            LaserBeam->End = NearestCollision.P;
            LaserBeam->Color = Laser->Color;
            
            if (NearestCollisionEntityIndex)
            {
                Map->Entities[NearestCollisionEntityIndex].IsActivated = true;
            }
            
            if (WillReflect)
            {
                P = NearestCollision.P;
                Direction = Direction - 2 * DotProduct(Direction, UnitV(NearestCollision.Normal)) * UnitV(NearestCollision.Normal);
            }
            else
            {
                Done = true;
                break;
            }
        }
        else
        {
            break;
        }
    }
    
    if (!Done && Iter < MaxIter)
    {
        laser_beam* LaserBeam = &Result[Iter];
        LaserBeam->Start = P;
        LaserBeam->End = P + 10.0f * UnitV(Direction);
        LaserBeam->Color = Laser->Color;
    }
}

static void 
LoadMaps(allocator Allocator, game_state* Game)
{
    u32 MaxMapCount = 100;
    
    span<map_desc*> Maps = AllocSpan(Allocator.Permanent, map_desc*, MaxMapCount);
    
    int MapsLoaded = 0;
    for (u32 MapIndex = 0; MapIndex < MaxMapCount; MapIndex++)
    {
        string Path = ArenaPrint(Allocator.Transient, "maps/map%u.bin", MapIndex);
        span<u8> MapData = PlatformLoadFile(Allocator.Transient, Path.Text); //This only works because Path is printed with ArenaPrint() which is null terminated
        map_desc* Map = DeserialiseMap(Allocator.Permanent, MapData);
        
        if (Map)
        {
            MapsLoaded++;
        }
        
        Maps[MapIndex] = Map;
    }
    
    if (MapsLoaded == 0)
    {
        map_desc* EmptyMap = AllocStruct(Allocator.Permanent, map_desc);
        Maps[0] = EmptyMap;
    }
    
    Game->Maps = Maps;
}

static game_state* 
GameInitialise(allocator Allocator)
{
    game_state* GameState = AllocStruct(Allocator.Permanent, game_state);
    
    LoadMaps(Allocator, GameState);
    
    GameState->MapArena = CreateSubArena(Allocator.Permanent, Kilobytes(16));
    
    GameState->Map = GameState->Maps[0];
    Assert(GameState->Map);
    
    return GameState;
}

static void
SimulateGame(game_state* GameState, game_input* Input, f32 DeltaTime, memory_arena* Arena)
{
    v2 Movement = Input->Movement;
    
    for (entity& Entity : GameState->Map->Entities)
    {
        Entity.WasActivated = Entity.IsActivated;
        Entity.IsActivated = false;
    }
    
    for (rigid_body& RigidBody : GameState->Map->RigidBodies)
    {
        if (RigidBody.ActivatedByIndex)
        {
            bool Activated = GameState->Map->Entities[RigidBody.ActivatedByIndex].WasActivated;
            
            v2 TargetP, TargetSize;
            if (Activated)
            {
                TargetP = RigidBody.ActivatedP;
                TargetSize = RigidBody.ActivatedSize;
            }
            else
            {
                TargetP = RigidBody.UnactivatedP;
                TargetSize = RigidBody.UnactivatedSize;
            }
            
            f32 Speed = 3.0f;
            RigidBody.P = LinearInterpolate(RigidBody.P, TargetP, DeltaTime * Speed);
            RigidBody.Size = LinearInterpolate(RigidBody.Size, TargetSize, DeltaTime * Speed);
        }
    }
    
    if (GameState->Map->RigidBodies.Count > 0)
    {
        rigid_body* Controlling = GameState->Map->RigidBodies + GameState->Map->Player.RigidBodyIndex;
        
        if (Input->ButtonDown & Button_Jump)
        {
            Controlling->dP.Y = 1.5f;
        }
        
        PhysicsUpdate(ToSpan(GameState->Map->RigidBodies), DeltaTime, Movement, Controlling);
    }
    
    for (attachment Attachment : GameState->Map->Attachments)
    {
        Assert(Attachment.EntityIndex);
        Assert(Attachment.AttachedToEntityIndex);
        
        entity* Entity = GameState->Map->Entities + Attachment.EntityIndex;
        entity* AttachedTo = GameState->Map->Entities + Attachment.AttachedToEntityIndex;
        
        Assert(AttachedTo->RigidBodyIndex);
        rigid_body* AttachedToRigidBody = GameState->Map->RigidBodies + AttachedTo->RigidBodyIndex;
        v2 P = AttachedToRigidBody->P + Attachment.Offset;
        
        if (Entity->RigidBodyIndex)
        {
            rigid_body* RigidBody = GameState->Map->RigidBodies + Entity->RigidBodyIndex;
            RigidBody->P = P;
        }
        if (Entity->LineIndex)
        {
            line* Line = GameState->Map->Lines + Entity->LineIndex;
            Line->Start = P;
        }
        if (Entity->LaserIndex)
        {
            laser* Laser = GameState->Map->Lasers + Entity->LaserIndex;
            Laser->Position = P;
        }
    }
    
    /*
    bool LevelCompleted = false;
    for (map_element& MapElement : GameState->Map->Elements)
    {
        if (MapElement.Type == MapElem_Goal)
        {
            v2 SizeSum = MapElement.Shape.Size + GameState->Player.Size;
            v2 MinCorner = MapElement.Shape.Position - 0.5f * SizeSum;
            v2 MaxCorner = MapElement.Shape.Position + 0.5f * SizeSum;
            if (PointInRect(MinCorner, MaxCorner, GameState->Player.P))
            {
                LevelCompleted = true;
            }
        }
    }

    
    if (LevelCompleted)
    {
        PlatformDebugOut(String("Level Completed\n"));
        ChangeMap(GameState, GameState->MapIndex + 1, Arena);
    }
*/
}

static void DrawMapForEditor(render_group* Group, map_desc* Map)
{
    for (map_element& MapElement : Map->Elements)
    {
        switch (MapElement.Type)
        {
            case MapElem_Line: case MapElem_Reflector:
            {
                PushLine(Group, MapElement.Shape.Start, MapElement.Shape.Start + MapElement.Shape.Offset, MapElement.Color, 0.01f);
            } break;
            case MapElem_Rectangle: 
            {
                v2 MinCorner = MapElement.Shape.Position - 0.5f * MapElement.Shape.Size;
                v2 MaxCorner = MapElement.Shape.Position + 0.5f * MapElement.Shape.Size;
                
                PushRectangle(Group, MinCorner, MapElement.Shape.Size, MapElement.Color);
                
                u32 RectStripeColor = 0x20000000;
                for (f32 X = MinCorner.X; X <= MaxCorner.X + 0.001f; X += 0.04f)
                {
                    PushLine(Group, V2(X, MinCorner.Y), V2(X, MaxCorner.Y), RectStripeColor, 0.002f);
                }
                for (f32 Y = MinCorner.Y; Y <= MaxCorner.Y + 0.001f; Y += 0.04f)
                {
                    PushLine(Group, V2(MinCorner.X, Y), V2(MaxCorner.X, Y), RectStripeColor, 0.002f);
                }
            } break;
            case MapElem_Receiver: case MapElem_Laser: case MapElem_Goal:
            {
                PushRectangle(Group, MapElement.Shape.Position - 0.5f * MapElement.Shape.Size, MapElement.Shape.Size, MapElement.Color);
            } break;
            case MapElem_Box:
            {
                PushRectangle(Group, MapElement.Shape.Position - 0.5f * MapElement.Shape.Size, MapElement.Shape.Size, 0xFFFF0000);
            } break;
            case MapElem_Circle:
            {
                
                PushCircle(Group, MapElement.Shape.Position, 0.5f * MapElement.Shape.Size.X, 0xFFFF0000);
            }
            case MapElem_Window:
            break; //Drawn later in transparent section
            default: Assert(MapElement.Type == MapElem_Null);
        }
    }
    
    
    //Transparent (window)
    for (map_element& MapElement : Map->Elements)
    {
        if (MapElement.Type == MapElem_Window)
        {
            PushRectangle(Group, MapElement.Shape.Position - 0.5f * MapElement.Shape.Size, MapElement.Shape.Size, MapElement.Color);
        }
    }
}

static void DrawMap(render_group* Group, map_desc* Map)
{
    for (rigid_body RigidBody : Map->RigidBodies)
    {
        PushRectangle(Group, RectOf(RigidBody), RigidBody.Color);
    }
    
    for (line Line : Map->Lines)
    {
        PushLine(Group, Line.Start, Line.Start + Line.Offset, Line.Color, 0.01f);
    }
    
    for (laser& Laser : Map->Lasers)
    {
        v2 LaserSize = V2(0.01f, 0.01f);
        PushRectangle(Group, Laser.Position - 0.5f * LaserSize, LaserSize, Laser.Color);
        
        bool IsActive = (Laser.ActivatedByIndex == 0) || Map->Entities[Laser.ActivatedByIndex].WasActivated;
        if (IsActive)
        {
            laser_beam LaserBeams[10] = {};
            //TODO: Get count of laser beams of result
            CalculateReflections(LaserBeams, ArrayCount(LaserBeams), Map, &Laser);
            
            for (laser_beam LaserBeam : LaserBeams)
            {
                u32 RGB = (LaserBeam.Color & 0xFFFFFF);
                
                v2 Direction = UnitV(LaserBeam.End - LaserBeam.Start);
                
                PushLine(Group, LaserBeam.Start, LaserBeam.End, RGB | 0x40000000, 0.008f);
                PushLine(Group, LaserBeam.Start, LaserBeam.End, RGB | 0x80000000, 0.005f);
                PushLine(Group, LaserBeam.Start - 0.002f * Direction, LaserBeam.End + 0.002f * Direction, 
                         RGB | 0xC0000000, 0.0025f);
                PushLine(Group, LaserBeam.Start - 0.002f * Direction, LaserBeam.End + 0.002f * Direction, 
                         RGB | 0xFF000000, 0.001f);
            }
        }
    }
    
    //Transparent (window)
    for (map_element& MapElement : Map->Elements)
    {
        if (MapElement.Type == MapElem_Window)
        {
            PushRectangle(Group, MapElement.Shape.Position - 0.5f * MapElement.Shape.Size, MapElement.Shape.Size, MapElement.Color);
        }
    }
}

static void
DrawGame(render_group* Group, game_state* GameState, memory_arena* Arena)
{
    //Background
    PushRectangle(Group, V2(0, 0), V2(1.0f, ScreenTop), 0xFF4040C0);
    
    u32 BackgroundStripeColor = 0x20FFFFFF;
    f32 LineWidth = 0.002f;
    for (f32 X = 0.0f; X < 1.0f; X += 0.04f)
    {
        PushRectangle(Group, V2(X - 0.5f * LineWidth, 0.0f), V2(LineWidth, ScreenTop), BackgroundStripeColor);
    }
    for (f32 Y = 0.0f; Y < ScreenTop; Y += 0.04f)
    {
        PushRectangle(Group, V2(0.0f, Y - LineWidth), V2(1.0f, LineWidth), BackgroundStripeColor);
    }
    
    if (GameState->Editing)
    {
        DrawMapForEditor(Group, GameState->Map);
    }
    else
    {
        DrawMap(Group, GameState->Map);
    }
}

void GameUpdateAndRender(render_group* RenderGroup, game_state* GameState, float DeltaTime, game_input* Input, allocator Allocator)
{
    UpdateConsole(GameState, &GameState->Console, Input, Allocator.Transient, DeltaTime);
    
    if (!GameState->Editing && (Input->ButtonDown & Button_Interact))
    {
        //OnEditorOpen();
        GameState->Editing = true;
    }
    else if (GameState->Editing && (Input->ButtonDown & Button_Interact))
    {
        OnEditorClose(GameState);
        GameState->Editing = false;
    }
    
    if (GameState->Editing)
    {
        Assert(GameState->Map);
        DrawGame(RenderGroup, GameState, Allocator.Transient);
        RunEditor(RenderGroup, GameState, Input, Allocator);
    }
    else
    {
        Assert(GameState->Map);
        SimulateGame(GameState, Input, DeltaTime, Allocator.Permanent);
        DrawGame(RenderGroup, GameState, Allocator.Transient);
    }
    
    PushText(RenderGroup, ArenaPrint(Allocator.Transient, "Map %u", GameState->MapIndex), V2(0, 0));
    
    string MemoryString = ArenaPrint(Allocator.Transient, "%u bytes Permanent, %u bytes Transient", 
                                     Allocator.Permanent->Used, Allocator.Transient->Used);
    PushText(RenderGroup, MemoryString, V2(0.35f, 0.0f));
    
    DrawConsole(&GameState->Console, RenderGroup);
}
