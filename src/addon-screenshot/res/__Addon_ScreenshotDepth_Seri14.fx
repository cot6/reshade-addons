#include "ReShade.fxh"

texture2D __Addon_Texture_ScreenshotDepth_Seri14
{
    Width = BUFFER_WIDTH; Height = BUFFER_HEIGHT;
    Format = R16;
};

float PS_Addon_ScreenshotDepth_Seri14(in float4 position : SV_Position, in float2 texcoord : TEXCOORD) : SV_Target
{
    return ReShade::GetLinearizedDepth(texcoord);
}

technique __Addon_Technique_ScreenshotDepth_Seri14 < hidden = true; ui_label = "ScreenshotDepth"; ui_tooltip = "This technique only handled by Add-ons."; >
{
    pass
    {
        VertexShader = PostProcessVS;
        PixelShader = PS_Addon_ScreenshotDepth_Seri14;
        RenderTarget = __Addon_Texture_ScreenshotDepth_Seri14;
    }
}
