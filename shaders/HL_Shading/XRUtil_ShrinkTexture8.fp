/*¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯*\
	File:
					
	Author:			Anders Ekermo
					
	Copyright:		Starbreeze AB 2007
					
	History:

\*____________________________________________________________________________________________*/

*_head_
{
    *type  hls
    *flags nodebug //dopreparse
    *name XRUtil_ShrinkTexture8
}

*flags
{
}

*generate
{
    *gen 0
}

*param
{
    *envx WHDim
    *envx ClampMin
    *envx ClampMax
    *envx OutR;
    *envx OutG;
    *envx OutB;
    *envx OutA;
}

*texture
{
    *tex2D_0	Texture
}

*attrib
{
    *texcoord0 TexCoord
}

*output
{
    *color      oCol
}


*source
"
#ifdef NO_LINEAR_FP_SAMPLE
        vec4 SamplePxl(sampler2D _Tex,vec2 _TC,vec2 _HDim)
        {
            vec4 Clr = texture2D(_Tex,vec2(_TC.x-_HDim.x,_TC.y-_HDim.y));
            Clr += texture2D(_Tex,vec2(_TC.x+_HDim.x,_TC.y-_HDim.y));
            Clr += texture2D(_Tex,vec2(_TC.x+_HDim.x,_TC.y+_HDim.y));
            Clr += texture2D(_Tex,vec2(_TC.x-_HDim.x,_TC.y-_HDim.y));
            return Clr * 0.25;
        }
#else
        vec4 SamplePxl(sampler2D _Tex,vec2 _TC,vec2 _HDim)
        {
            return texture2D(_Tex,_TC);
        }
#endif
"

*main
{
	*do0
	"
                vec2 CCoord = TexCoord.xy - WHDim.xy*3.0;
                vec2 HalfDim = WHDim.xy * 0.5;
                vec2 DoubleDim = WHDim.xy * 2.0;
                vec4 Res = splat4(0.0);
                vec2 DoubleDim1 = vec2(0.0, DoubleDim.y);
                vec2 DoubleDim2 = vec2(0.0, DoubleDim.y * 2.0);
                vec2 DoubleDim3 = vec2(0.0, DoubleDim.y * 3.0);

                Res += SamplePxl(Texture, CCoord, HalfDim);
                Res += SamplePxl(Texture, CCoord + DoubleDim1, HalfDim);
                Res += SamplePxl(Texture, CCoord + DoubleDim2, HalfDim);
                Res += SamplePxl(Texture, CCoord + DoubleDim3, HalfDim);
                CCoord.x += DoubleDim.x;

                Res += SamplePxl(Texture, CCoord, HalfDim);
                Res += SamplePxl(Texture, CCoord + DoubleDim1, HalfDim);
                Res += SamplePxl(Texture, CCoord + DoubleDim2, HalfDim);
                Res += SamplePxl(Texture, CCoord + DoubleDim3, HalfDim);
                CCoord.x += DoubleDim.x;

                Res += SamplePxl(Texture, CCoord, HalfDim);
                Res += SamplePxl(Texture, CCoord + DoubleDim1, HalfDim);
                Res += SamplePxl(Texture, CCoord + DoubleDim2, HalfDim);
                Res += SamplePxl(Texture, CCoord + DoubleDim3, HalfDim);
                CCoord.x += DoubleDim.x;

                Res += SamplePxl(Texture, CCoord, HalfDim);
                Res += SamplePxl(Texture, CCoord + DoubleDim1, HalfDim);
                Res += SamplePxl(Texture, CCoord + DoubleDim2, HalfDim);
                Res += SamplePxl(Texture, CCoord + DoubleDim3, HalfDim);

                Res *= 0.0625;
                float4 result = float4(dot(Res, OutR), dot(Res, OutG), dot(Res, OutB), dot(Res, OutA));
                result = clamp(result, ClampMin, ClampMax);
                oCol = result;
	"
}
