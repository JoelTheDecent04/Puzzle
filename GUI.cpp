struct gui_state
{
	bool MouseWentDown, MouseWentUp;
	v2 CursorPosition;
    
	u32 Hot, Active;
	bool InputHandled;
};

//Hot = hovering
//Active = clicking

gui_state GlobalGUIState;
u32 GlobalGUIIdentifierCounter;
render_group* GlobalRenderGroup;

static void
BeginGUI(game_input* Input, render_group* RenderGroup)
{
	//Check if buttons no longer exist
	if (GlobalGUIState.Hot >= GlobalGUIIdentifierCounter)
		GlobalGUIState.Hot = 0;
	if (GlobalGUIState.Active >= GlobalGUIIdentifierCounter)
		GlobalGUIState.Active = 0;
    
	GlobalGUIState.MouseWentDown = (Input->ButtonDown & Button_LMouse);
	GlobalGUIState.MouseWentUp = (Input->ButtonUp & Button_LMouse);
	GlobalGUIState.CursorPosition = Input->Cursor;
	GlobalGUIState.InputHandled = false;
    
	GlobalGUIIdentifierCounter = 1;
    
    GlobalRenderGroup = RenderGroup;
}

static inline bool
GUIInputIsBeingHandled()
{
	return GlobalGUIState.Hot > 0;
}

static bool 
Button(v2 Position, v2 Size, string String)
{
	u32 Identifier = GlobalGUIIdentifierCounter++;
	bool Result = false;
	
	if (GlobalGUIState.Active == Identifier)
	{
		if (GlobalGUIState.MouseWentUp)
		{
			if (GlobalGUIState.Hot == Identifier)
			{
				Result = true;
				GlobalGUIState.InputHandled = true;
			}
			else
			{
				GlobalGUIState.Active = 0;
			}
		}
	}
	else if (GlobalGUIState.Hot == Identifier)
	{
		if (GlobalGUIState.MouseWentDown)
		{
			GlobalGUIState.Active = Identifier;
		}
	}
    
	if (PointInRect(Position, Position + Size, GlobalGUIState.CursorPosition))
	{
		GlobalGUIState.Hot = Identifier;
	}
	else if (GlobalGUIState.Hot == Identifier)
	{
		GlobalGUIState.Hot = 0;
	}
    
	u32 Color = 0xFF8080FF;
	if (GlobalGUIState.Hot == Identifier)
	{
		Color = 0xFF6060FF;
	}
    
	PushRectangle(GlobalRenderGroup, Position, Size, Color /*, 0xFF8080FF*/);
    
    f32 TextWidth = PlatformTextWidth(String, Size.Y);
	PushText(GlobalRenderGroup, String, V2(Position.X + 0.5f * Size.X - 0.5f * TextWidth, Position.Y), 0xFFFFFF, Size.Y);
    
	return Result;
}

struct gui_layout
{
	f32 X, Y;
	f32 XPad;
	f32 RowHeight;
    
	bool Button(const char* Text);
	bool Button(string String);
	void Label(const char* Text);
	void Label(string Text);
	void NextRow();
};

static gui_layout 
DefaultLayout(f32 X, f32 Y)
{
	gui_layout Result = {};
    
	Result.RowHeight = 0.03f;
	Result.XPad = 0.01f;
	Result.X = X + Result.XPad;
	Result.Y = Y - 0.01f - Result.RowHeight;
    
	return Result;
}

bool gui_layout::Button(const char* Text)
{
	return Button(String(Text));
}

bool gui_layout::Button(string Text)
{
	f32 ButtonWidth = 0.06f;
	f32 ButtonHeight = 0.02f;
	bool Result = ::Button(V2(X, Y), V2(ButtonWidth, ButtonHeight), Text);
	X += ButtonWidth + XPad;
	return Result;
}

void gui_layout::Label(const char* Text)
{
	Label(String(Text));
}

void gui_layout::Label(string Text)
{
    f32 LabelFontSize = 0.02f;
    
    f32 Width = PlatformTextWidth(Text, LabelFontSize);
    Width = RoundUpToMultipleOf(0.06f, Width + XPad);
    
    PushText(GlobalRenderGroup, Text, V2(X, Y), 0xFFFFFF, 0.02f);
    X += Width + XPad;
}

void gui_layout::NextRow()
{
	X = XPad;
	Y -= RowHeight;
}