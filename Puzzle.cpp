//Bugs
//if modifications are made to the level and the file is reloaded, nothing can be selected

#include "Editor.cpp"
#include "Physics.cpp"
#include "Console.cpp"

static void
PhysicsUpdate(span<rigid_body> RigidBodies, f32 DeltaTime, v2 Movement, rigid_body* Controlling);

static inline line_segment
GetLine(map_element MapElem)
{
    Assert(MapElem.Type == MapElem_Line || MapElem.Type == MapElem_Reflector);
    
    line_segment Rect = {
        MapElem.Shape.Start,
        MapElem.Shape.Start + MapElem.Shape.Offset
    };
    return Rect;
}

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
CalculateReflections(laser_beam* Result, u32 MaxIter, game_state* GameState, map_element* Laser)
{
    Assert(Laser->Type == MapElem_Laser);
    
    v2 P = Laser->Shape.Position;
    f32 AngleRadians = Laser->Angle * 2 * 3.14159f;
    v2 Direction = { cosf(AngleRadians), sinf(AngleRadians) };
    
    bool Done = false;
    
    u32 Iter = 0;
    for (; Iter < MaxIter; Iter++)
    {
        f32 tMin = 100.0f;
        ray_collision NearestCollision = {};
        map_element* NearestMapElement = 0;
        bool WillReflect = 0;
        
        map_desc* Map = GameState->Map;
        
        for (map_element& MapElement : Map->Elements)
        {
            switch (MapElement.Type)
            {
                case MapElem_Reflector: case MapElem_Line:
                {
                    ray_collision Collision = TestRayIntersection(P, Direction, GetLine(MapElement));
                    
                    if (Collision.DidHit && Collision.t < tMin)
                    {
                        NearestCollision = Collision;
                        tMin = Collision.t;
                        WillReflect = (MapElement.Type == MapElem_Reflector);
                        NearestMapElement = &MapElement;
                    }
                } break;
                
                case MapElem_Rectangle: case MapElem_Receiver:
                {
                    /*
                    rect_edges BoxEdges = GetEdges(&MapElement);
                    for (int i = 0; i < 4; i++)
                    {
                        ray_collision Collision = TestRayIntersection(P, Direction, BoxEdges.Edges[i]);
                        
                        if (Collision.DidHit && Collision.t < tMin)
                        {
                            NearestCollision = Collision;
                            tMin = Collision.t;
                            WillReflect = false;
                            NearestMapElement = &MapElement;
                        }
                    }*/
                } break;
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
                    NearestMapElement = 0;
                }
            }
            
        }
        
        if (NearestCollision.DidHit)
        {
            laser_beam* LaserBeam = &Result[Iter];
            LaserBeam->Start = P;
            LaserBeam->End = NearestCollision.P;
            LaserBeam->Color = Laser->Color;
            
            if (NearestMapElement)
            {
                NearestMapElement->Activated = true;
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
    
    if (!Done && (Iter < MaxIter))
    {
        laser_beam* LaserBeam = &Result[Iter];
        LaserBeam->Start = P;
        LaserBeam->End = P + 10.0f * UnitV(Direction);
        LaserBeam->Color = Laser->Color;
    }
}

void LoadMaps(allocator Allocator, game_state* Game)
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
    
    for (map_element& MapElement : GameState->Map->Elements)
    {
        MapElement.WasActivated = MapElement.Activated;
        MapElement.Activated = false;
    }
    
    for (map_element& MapElement : GameState->Map->Elements)
    {
        if (MapElement.ActivatedBy)
        {
            bool Activated = GameState->Map->Elements[MapElement.ActivatedBy].WasActivated;
            
            shape Target;
            if (Activated)
            {
                Target = MapElement.ActivatedShape;
            }
            else
            {
                Target = MapElement.UnactivatedShape;
            }
            f32 Speed = 3.0f;
            
            MapElement.Shape.E1 = LinearInterpolate(MapElement.Shape.E1, Target.E1, DeltaTime * Speed);
            MapElement.Shape.E2 = LinearInterpolate(MapElement.Shape.E2, Target.E2, DeltaTime * Speed);
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
        rigid_body* RigidBody = GameState->Map->RigidBodies + Attachment.RigidBodyIndex;
        map_element* MapElement = GameState->Map->Elements + Attachment.ElementIndex;
        
        MapElement->Shape.Position = RigidBody->P + Attachment.Offset;
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

static void
DrawGame(game_state* GameState, memory_arena* Arena)
{
    //Background
    PlatformRectangle(V2(0, 0), V2(1.0f, ScreenTop), 0xFF4040C0);
    
    u32 BackgroundStripeColor = 0x20FFFFFF;
    for (f32 X = 0.0f; X < 1.0f; X += 0.04f)
    {
        PlatformLine(V2(X, 0.0f), V2(X, ScreenTop), BackgroundStripeColor, 0.002f);
    }
    for (f32 Y = 0.0f; Y < ScreenTop; Y += 0.04f)
    {
        PlatformLine(V2(0.0f, Y), V2(1.0f, Y), BackgroundStripeColor, 0.002f);
    }
    
    //Scene
    for (map_element& MapElement : GameState->Map->Elements)
    {
        switch (MapElement.Type)
        {
            case MapElem_Line: case MapElem_Reflector:
            {
                PlatformLine(MapElement.Shape.Start, MapElement.Shape.Start + MapElement.Shape.Offset, MapElement.Color, 0.01f);
            } break;
            case MapElem_Rectangle: 
            {
                v2 MinCorner = MapElement.Shape.Position - 0.5f * MapElement.Shape.Size;
                v2 MaxCorner = MapElement.Shape.Position + 0.5f * MapElement.Shape.Size;
                
                PlatformRectangle(MinCorner, MapElement.Shape.Size, MapElement.Color);
                
                u32 RectStripeColor = 0x20000000;
                for (f32 X = MinCorner.X; X <= MaxCorner.X + 0.001f; X += 0.04f)
                {
                    PlatformLine(V2(X, MinCorner.Y), V2(X, MaxCorner.Y), RectStripeColor, 0.002f);
                }
                for (f32 Y = MinCorner.Y; Y <= MaxCorner.Y + 0.001f; Y += 0.04f)
                {
                    PlatformLine(V2(MinCorner.X, Y), V2(MaxCorner.X, Y), RectStripeColor, 0.002f);
                }
            } break;
            case MapElem_Receiver: case MapElem_Laser: case MapElem_Goal:
            {
                PlatformRectangle(MapElement.Shape.Position - 0.5f * MapElement.Shape.Size, MapElement.Shape.Size, MapElement.Color);
            } break;
            case MapElem_Box:
            {
                if (GameState->Editing)
                {
                    PlatformRectangle(MapElement.Shape.Position - 0.5f * MapElement.Shape.Size, MapElement.Shape.Size, 0xFFFF0000);
                }
            } break;
            case MapElem_Circle:
            {
                if (GameState->Editing)
                {
                    PlatformCircle(MapElement.Shape.Position, 0.5f * MapElement.Shape.Size.X, 0xFFFF0000);
                }
            }
            case MapElem_Window:
            break; //Drawn later in transparent section
            default: Assert(MapElement.Type == MapElem_Null);
        }
    }
    
    //Player and boxes
    if (!GameState->Editing)
    {
        //Player
        u32 PlayerRigidBodyIndex = GameState->Map->Player.RigidBodyIndex;
        if (PlayerRigidBodyIndex < GameState->Map->RigidBodies.Count)
        {
            rigid_body* RigidBody = GameState->Map->RigidBodies + PlayerRigidBodyIndex;
            PlatformRectangle(RigidBody->P - 0.5f * RigidBody->Size, RigidBody->Size, 0xFFFFFFFF);
        }
        
        //Boxes
        for (entity& Entity : GameState->Map->Entities)
        {
            rigid_body* RigidBody = GameState->Map->RigidBodies + Entity.RigidBodyIndex;
            switch (RigidBody->Type)
            {
                case RigidBody_AABB:
                {
                    PlatformRectangle(RigidBody->P - 0.5f * RigidBody->Size, RigidBody->Size, 0xFFFFFFFF);
                } break;
                case RigidBody_Circle:
                {
                    PlatformCircle(RigidBody->P, 0.5f * RigidBody->Size.X, 0xFFFFFFFF);
                } break;
            }
        }
    }
    
    //Lasers
    for (map_element& MapElement : GameState->Map->Elements)
    {
        if (MapElement.Type == MapElem_Laser)
        {
            bool IsActive = (MapElement.ActivatedBy == 0) || GameState->Map->Elements[MapElement.ActivatedBy].WasActivated;
            if (IsActive)
            {
                laser_beam LaserBeams[10] = {};
                //TODO: Get count of laser beams of result
                CalculateReflections(LaserBeams, ArrayCount(LaserBeams), GameState, &MapElement);
                
                for (laser_beam LaserBeam : LaserBeams)
                {
                    u32 RGB = (LaserBeam.Color & 0xFFFFFF);
                    
                    v2 Direction = UnitV(LaserBeam.End - LaserBeam.Start);
                    
                    PlatformLine(LaserBeam.Start, LaserBeam.End, RGB | 0x40000000, 0.008f);
                    PlatformLine(LaserBeam.Start, LaserBeam.End, RGB | 0x80000000, 0.005f);
                    PlatformLine(LaserBeam.Start - 0.002f * Direction, LaserBeam.End + 0.002f * Direction, 
                                 RGB | 0xC0000000, 0.0025f);
                    PlatformLine(LaserBeam.Start - 0.002f * Direction, LaserBeam.End + 0.002f * Direction, 
                                 RGB | 0xFF000000, 0.001f);
                }
            }
        }
    }
    
    //Transparent (window)
    for (map_element& MapElement : GameState->Map->Elements)
    {
        if (MapElement.Type == MapElem_Window)
        {
            PlatformRectangle(MapElement.Shape.Position - 0.5f * MapElement.Shape.Size, MapElement.Shape.Size, MapElement.Color);
        }
    }
}

void GameUpdateAndRender(game_state* GameState, float DeltaTime, game_input* Input, allocator Allocator)
{
    UpdateConsole(&GameState->Console, Input, DeltaTime);
    
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
        DrawGame(GameState, Allocator.Transient);
        RunEditor(GameState, Input, Allocator);
    }
    else
    {
        Assert(GameState->Map);
        SimulateGame(GameState, Input, DeltaTime, Allocator.Permanent);
        DrawGame(GameState, Allocator.Transient);
    }
    
    PlatformDrawText(ArenaPrint(Allocator.Transient, "Map %u", GameState->MapIndex), V2(0, 0), V2(0.1f, 0.05f));
    
    string MemoryString = ArenaPrint(Allocator.Transient, "%u bytes Permanent, %u bytes Transient", 
                                     Allocator.Permanent->Used, Allocator.Transient->Used);
    PlatformDrawText(MemoryString, V2(0.35f, 0.0f), V2(0.3f, 0.02f), 0xFF000000);
    
    DrawConsole(&GameState->Console);
}
