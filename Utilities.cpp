#include <stdint.h>
#include <cstdarg>
#include <stdio.h>

typedef uint64_t u64;
typedef int64_t i64;
typedef uint32_t u32;
typedef int32_t i32;
typedef uint16_t u16;
typedef int16_t i16;
typedef uint8_t u8;
typedef int8_t i8;
typedef float f32;
typedef double f64;

#define Kilobytes(n) (n * 1024)
#define Megabytes(n) (n * 1024 * 1024)

#define ArrayCount(x) (sizeof((x))/sizeof((x)[0]))

struct string
{
	char* Text;
	u32 Length;
};

//TODO: This does not seem to actually calculate the length at compile time :(
constexpr string
String(const char* Text)
{
	string Result = {};
	Result.Text = (char*)Text;
    
	u32 Length = 0;
	while (Text[Length])
		Length++;
    
	Result.Length = Length;
    
	return Result;
}

#if 0
#define String(x) \
string{x, sizeof(x) - 1}
#endif

static bool
StringsAreEqual(string A, string B)
{
    if (A.Length != B.Length)
        return false;
    
    for (u32 I = 0; I < A.Length; I++)
    {
        if (A.Text[I] != B.Text[I])
            return false;
    }
    
    return true;
}

#if DEBUG
#define Assert(x) DoAssert(x)
#else
#define Assert(x)
#endif

static inline void
DoAssert(bool Condition)
{
	if (!Condition)
	{
		__debugbreak();
	}
}

enum memory_arena_type
{
	NORMAL, PERMANENT, TRANSIENT
};

struct memory_arena
{
	u8* Buffer;
	u64 Used;
	u64 Size;
	memory_arena_type Type;
};

#define AllocStruct(Arena, Type) \
(Type*)Alloc(Arena, sizeof(Type))

#define AllocArray(Arena, Type, Count) \
(Type*)Alloc(Arena, Count * sizeof(Type))

static u8*
Alloc(memory_arena* Arena, u64 Size)
{
	u8* Result = Arena->Buffer + Arena->Used;
	Arena->Used += Size;
    
	for (int i = 0; i < Size; i++)
		Result[i] = 0;
    
	Assert(Arena->Used < Arena->Size);
    
    //Assert(((size_t)Result & 0b11) == 0);
    
    return Result;
}

static bool
WasAllocatedFrom(memory_arena* Arena, void* Memory)
{
    size_t ArenaStart = (size_t)Arena->Buffer;
    size_t ArenaEnd = (size_t)Arena->Buffer + Arena->Used;
    
    size_t Ptr = (size_t)Memory;
    
    bool Result = (Ptr >= ArenaStart && Ptr <= ArenaEnd);
    return Result;
}

static inline memory_arena
CreateSubArena(memory_arena* Arena, u64 Size)
{
    memory_arena Result = {};
    Result.Buffer = Alloc(Arena, Size);
    Result.Size = Size;
    return Result;
}

static inline void
ResetArena(memory_arena* Arena)
{
	Assert(Arena->Type != PERMANENT);
    
#if DEBUG
	for (int i = 0; i < Arena->Used; i++)
		Arena->Buffer[i] = 0xFF;
#endif
    
	Arena->Used = 0;
}

struct allocator
{
	memory_arena* Permanent;
	memory_arena* Transient;
};

static string
ArenaPrint(memory_arena* Arena, char* Format, ...)
{
	va_list Args;
	va_start(Args, Format);
    
	string Result = {};
	char* Buffer = (char*)(Arena->Buffer + Arena->Used);
    
    int MaxChars = 4096;
	int CharsWritten = vsprintf_s(Buffer, MaxChars, Format, Args);
	Arena->Used += CharsWritten + 1; //TODO: Arena used should be rounded up to a nice number
    
	Result.Text = Buffer;
	Result.Length = CharsWritten;
    
	va_end(Args);
    
	Assert(Arena->Used < Arena->Size);
    
	return Result;
}

template <typename type>
struct span
{
	type* Memory;
	u32 Count;
    
    type& operator[](u32 Index)
    {
#if DEBUG
        Assert(Index < Count);
#endif
        return Memory[Index];
    }
};

template <typename type>
type* begin(span<type> Span)
{
    return Span.Memory;
}

template <typename type>
type* end(span<type> Span)
{
    return Span.Memory + Span.Count;
}

#define AllocSpan(Arena, Type, Count) \
(span<Type> {AllocArray(Arena, Type, Count), Count})
/*
template <typename type>
span<type>
BeginSpan(memory_arena* Arena)
{
	span<type> Span = {};
	Span.Memory = (type*)(Arena->Buffer + Arena->Used);
    
	return Span;
}
*/
template <typename type>
void
EndSpan(span<type>& Span, memory_arena* Arena)
{
    Assert(WasAllocatedFrom(Arena, Span.Memory));
	Span.Count = (u32)((type*)(Arena->Buffer + Arena->Used) - Span.Memory);
}

template <typename type>
type*
operator+(span<type> Span, u32 Index)
{
	return &Span[Index];
}

template <typename type>
struct static_array
{
    type* Memory;
    u32 Count;
    u32 Capacity;
    
    type& operator[](u32 Index)
    {
        Assert(Index < Count);
        return Memory[Index];
    }
};

template<typename type>
static type* begin(static_array<type> Array)
{
    return Array.Memory;
}

template<typename type>
static type* end(static_array<type> Array)
{
    return Array.Memory + Array.Count;
}

template <typename type>
static inline u32 Add(static_array<type>* Array, type NewElement)
{
    Assert(Array->Count < Array->Capacity);
    Array->Memory[Array->Count] = NewElement;
    u32 Result = Array->Count;
    Array->Count++;
    return Result;
}

template <typename type>
type* operator+(static_array<type> Array, u32 Index)
{
	return &Array[Index];
}

#define AllocStaticArray(Arena, Type, Capacity) \
(static_array<Type> {AllocArray(Arena, Type, Capacity), 0, Capacity})

template <typename type>
static span<type> ToSpan(static_array<type> Array)
{
    span<type> Span = {};
    Span.Memory = Array.Memory;
    Span.Count = Array.Count;
    return Span;
}

template <typename type>
struct dynamic_array
{
	type* Memory;
	u32 Count;
    u32 Capacity;
	
    type* begin()
    {
        return Memory;
    }
    
    type* end()
    {
        return Memory + Count;
    }
    
    type& operator[] (u32 Index)
    {
#if DEBUG
        Assert(Index < Count);
#endif
        return Memory[Index];
    }
    
};

template <typename type>
type* operator+(dynamic_array<type> Array, u32 Index)
{
	return &Array[Index];
}

template <typename type>
void Add(dynamic_array<type>* Array, type* NewElement, memory_arena* Arena)
{
    if (Array->Count >= Array->Capacity)
    {
        //Note: Old memory is not freed
        u32 NewCapacity = Array->Capacity * 2;
        type* NewMemory = AllocArray(Arena, type, NewCapacity);
        memcpy(NewMemory, Array->Memory, Array->Count * sizeof(type));
        
        Array->Memory = NewMemory;
        Array->Capacity = NewCapacity;
    }
    Array->Memory[Array->Count++] = *NewElement;
}

template <typename type>
static inline dynamic_array<type>
Array(span<type> Span)
{
    dynamic_array<type> Array = {};
    Array.Memory = Span.Memory;
    Array.Count = Span.Count;
    Array.Capacity = Span.Count;
    return Array;
}
