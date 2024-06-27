#include <vector>
#include <string>

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
    u32 EntityIndex;
    u32 Color;
    
    //TODO: Should this be here?
    bool Translucent;
    
    rigid_body_type Type;
    v2 P;
    v2 dP;
    v2 Size;
    f32 InvMass;
    
    u32 ActivatedByIndex;
    v2 ActivatedP, UnactivatedP;
    v2 ActivatedSize, UnactivatedSize;
};

enum entity_type
{
    Entity_Null,
    Entity_Player,
    Entity_Laser,
    Entity_Reflector,
    Entity_Receiver,
    Entity_Circle,
    Entity_Box
};

struct entity
{
    entity_type Type;
    
    bool WasActivated;
    bool IsActivated;
    
    u32 RigidBodyIndex;
    u32 LineIndex;
    u32 LaserIndex;
};

struct attachment
{
    u32 EntityIndex;
    u32 AttachedToEntityIndex;
    v2 Offset;
};

struct line
{
    u32 EntityIndex;
    
    u32 Color;
    v2 Start;
    v2 Offset;
    bool Reflective;
};

struct laser
{
    v2 Position;
    u32 Color;
    f32 Angle;
    u32 ActivatedByIndex;
};

struct map_desc
{
    dynamic_array<map_element> Elements;
    
    entity Player;
    static_array<entity> Entities;
    
    static_array<rigid_body> RigidBodies;
    static_array<attachment> Attachments;
    static_array<line> Lines;
    static_array<laser> Lasers;
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

struct console;
struct game_state;

typedef void (*console_command_callback)(int ArgCount, string* Args, console* Console, game_state* GameState, memory_arena* Arena);

struct console_command
{
    string Command;
    console_command_callback Callback;
};

struct console
{
    f32 Height;
    f32 TargetHeight;
    
    console_command Commands[64];
    u64 CommandCount;
    
    string History[16];
    
    char Input[256];
    u32 InputLength;
    u32 InputCursor;
    
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

struct input_state
{
    button_state Buttons;
    v2 Cursor;
    v2 Movement;
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

enum render_type
{
    Render_Rectangle,
    Render_Circle,
    Render_Line,
    Render_Text,
    Render_Background
};

struct render_shape
{
    render_type Type;
    u32 Color;
    union
    {
        struct
        {
            v2 Position;
            v2 Size;
        } Rectangle;
        struct
        {
            v2 Position;
            f32 Radius;
        } Circle;
        struct 
        {
            v2 Start;
            v2 End;
            f32 Thickness;
        } Line;
        struct
        {
            v2 Position;
            f32 Size;
            string String;
        } Text;
    };
};

struct render_group
{
    u32 ShapeCount;
    render_shape Shapes[2048];
};

