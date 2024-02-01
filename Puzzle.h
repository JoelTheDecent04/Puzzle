enum map_elem_type
{
    MapElem_Null,
    MapElem_Rectangle,
    MapElem_Reflector,
    MapElem_Receiver,
    MapElem_Laser,
    MapElem_Box,
    MapElem_Goal,
    MapElem_Window,
    MapElem_Line
};

f32 const ScreenTop = 0.5625f;

union shape
{
    struct
    {
        v2 Start, Offset;
    };
    struct
    {
        v2 Position, Size;
    };
    struct
    {
        v2 E1, E2;
    };
};

struct map_element
{
    map_elem_type Type;
    shape Shape;
    
    u32 ActivatedBy;
    shape ActivatedShape;
    shape UnactivatedShape;
    
    u32 Color;
    f32 Angle;
    
    bool WasActivated;
    bool Activated;
    
    u32 AttachedTo;
    v2 AttachmentOffset;
};

struct rigid_body
{
    v2 P;
    v2 dP;
    v2 Size;
    f32 InvMass;
    
    //TODO: Should this be here?
    bool Transparent;
};

/*
struct entity
{
    v2 P, dP;
    bool HasUpdated;
    v2 Size;
    
    u32 MapElementAttachmentIndex;
    v2 MapElementAttachmentOffset;
};*/

struct box
{
    u32 RigidBodyIndex;
    u32 Color;
};

struct player
{
    u32 RigidBodyIndex;
};

struct attachment
{
    u32 RigidBodyIndex;
    u32 ElementIndex;
    v2 Offset;
};

struct map_desc
{
    dynamic_array<map_element> Elements;
    
    span<rigid_body> RigidBodies;
    span<attachment> Attachments;
    
    span<box> Boxes;
    player Player;
};

struct saved_map_header
{
    u32 ElementCount;
    u32 Padding;
    u32 ElementSize;
};

enum map_editor_state
{
    MapEditor_Default,
    MapEditor_InteractSelection,
    MapEditor_AttachSelection
};

struct map_editor
{
    u32 SelectedElementIndex;
    bool Dragging;
    v2 CursorToElement;
    bool EditingTheActivatedState;
    
    map_editor_state State;
};

struct console
{
    f32 Height;
    f32 TargetHeight;
};

struct game_state
{
    span<map_desc*> Maps;
    map_desc* Map;
    memory_arena MapArena;
    u32 MapIndex;
    
    bool Editing;
    map_editor Editor;
    
    console Console;
};
