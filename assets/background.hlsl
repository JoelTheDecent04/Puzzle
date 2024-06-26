struct VS_Input
{
    float2 pos : POS;
    float4 color : COL;
};

struct VS_Output
{
    float4 position : SV_POSITION;
    float4 color : COL;
    float2 uv : POS;
};

VS_Output vs_main(VS_Input input)
{
    VS_Output output;
    output.position = float4(input.pos.x * 2.0f - 1.0f, 2.0f / 0.5625f * input.pos.y - 1.0f, 0.0f, 1.0f);
    output.color = input.color;    
    output.uv = input.pos;

    return output;
}

float4 ps_main(VS_Output input) : SV_TARGET
{
	float cell_width = 1.0f / 25.0f;
	float cell_height = 1.0f / 25.0f;
	float line_thickness = 0.0025f;

	float2 uv = input.uv;
	
	float x = fmod(uv.x, cell_width);
	float y = fmod(uv.y, cell_height);
	
	float4 result = input.color;

	if (x < 0.5f * line_thickness || x > cell_width - 0.5f * line_thickness ||
            y < 0.5f * line_thickness || y > cell_height - 0.5f * line_thickness)
	{
		float3 white = float3(1.0f, 1.0f, 1.0f);
		result = float4(0.8f * input.color.rgb + 0.2f * white, 1.0f);
	}
	

	return result;   
}