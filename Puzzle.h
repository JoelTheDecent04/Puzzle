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
    MapElem_Line,
    MapElem_Circle
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

enum rigid_body_type
{
    RigidBody_AABB,
    RigidBody_Circle
};

struct rigid_body
{
    rigid_body_type Type;
    
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

struct entity
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
    
    span<entity> Entities;
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
    
    char Input[256];
    int InputLength;
    int InputCursor;
    
    bool CursorOn;
    f32  CursorCountdown;
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

typedef u32 button_state;
enum
{
    Button_Jump     = (1 << 1),
    Button_Interact = (1 << 2),
    Button_Menu     = (1 << 3),
    Button_LMouse   = (1 << 4),
    Button_LShift   = (1 << 5),
    Button_Console  = (1 << 6),
    Button_Left     = (1 << 7),
    Button_Right    = (1 << 8),
};

struct game_input
{
    button_state Button;
    button_state ButtonDown;
    button_state ButtonUp;
    
    char* TextInput;
    v2 Movement;
	v2 Cursor;
};
