/*¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯*\
	File:			Program for CXR_Util::RenderSurface vertex lighting with
				per-pixel projection mapping - High-level version

	Author:			Anders Ekermo; Original by Magnus Högdahl

	Copyright:		Starbreeze AB 2008

	History:

\*____________________________________________________________________________________________*/

*_head_
{
    *type  hls
    *flags nodebug //dopreparse
}

*flags
{
	*proj0 		0x0001
	*proj1 		0x0002
	*proj2 		0x0004
	*fog 		0x0008
	*fogcube 	0x0010
	*fogsky 	0x0020
	*lighting	0x0040
	*texture1	0x0080
	*fognocolor	0x0100
	*depthblend     0x0200
}

*generate
{
	// 4 FPs
	*gen 0
	*gen lighting
	{
		*gen proj0
		*gen proj0+proj1
		*gen proj0+proj1+proj2
	}

	// 20 FPs
	*gen fog
	{
		*permute fogcube+fogsky
		{
			*gen lighting
			{
				*gen proj0
				*gen proj0+proj1
				*gen proj0+proj1+proj2
			}
			*gen 0
			*gen texture1
		}

		*gen fognocolor
		*gen lighting+fognocolor
	}
	*gen texture1

	*gen depthblend
        {
        	*gen lighting
        	{
        		*gen proj0
        		*gen proj0+proj1
        		*gen proj0+proj1+proj2
        	}
                *gen fog
                {
			*permute fogcube+fogsky
                        {
                              *gen 0
                              *gen lighting
                              {
                      	           *gen proj0
                      	           *gen proj0+proj1
                      		   *gen proj0+proj1+proj2
                      	      }
			}
          		*gen fognocolor
			*gen lighting+fognocolor
                }
        }
}

*output
{
    *color oCol
}

*attrib
{
    *color      vCol
    *texcoord0  TCMapping0
    *texcoord1  TCMapping1
    *texcoord2  TCMSPos
    *texcoord3  Light0
    *texcoord4  Light1
    *texcoord5  Light2
    *if_fog
    {
        *texcoord6    ScreenCoord
        *texcoord7    Fog1
    }
    *ifnot_fog
    {
        *if_depthblend
        {
            *texcoord6    ScreenCoord
        }
    }
}

*param
{
    *env       Color
    *if_fog
    {
        *env   FogConst0
        *env   FogConst1
        *env   FogConst2
        *env   FogConst3
    }
    *if_depthblend
    {
        *env       VPParam
        *env       VPConst
        *env       VPScale
        *env       DepthBlendParam
        *env       DepthBlendAdd
        *env       ScreenCenter
    }

    *env       ProjMap0x
    *env       ProjMap0y
    *env       ProjMap0z
    *env       ProjMap1x
    *env       ProjMap1y
    *env       ProjMap1z
    *env       ProjMap2x
    *env       ProjMap2y
    *env       ProjMap2z
}

*texture
{
     *tex2D_0     DiffuseTex0
     *tex2D_1     DiffuseTex1
     *texCUBE_2   ProjMap0
     *texCUBE_3   ProjMap1
     *texCUBE_4   ProjMap2
     *tex2D_5     DepthScreen
     *if_fog
     {
         *tex2D_6 FogTex        //Can't rename these
         *texCUBE_7 FogCubeTex
     }
}

*source
{
        *INCLUDE "XR_FPDepth.fph"
        *if_fog
        {
		*INCLUDE "Include_XREngine_Fog_HL.fph"
        }
}

*main
{
	*temp
	"
		vec4 Result;
		vec4 Tmp;
		vec4 DiffTexel;
	"

	*if_lighting
	{
		*if_proj0
		"
			Tmp.x = dot(TCMSPos, ProjMap0x);
			Tmp.y = dot(TCMSPos, ProjMap0y);
			Tmp.z = dot(TCMSPos, ProjMap0z);
			Result = texCUBE(ProjMap0,Tmp.xyz) * Light0;
		"
		*ifnot_proj0
		"
			Result = Light0;
		"
		*if_proj1
		"
			Tmp.x = dot(TCMSPos, ProjMap1x);
			Tmp.y = dot(TCMSPos, ProjMap1y);
			Tmp.z = dot(TCMSPos, ProjMap1z);
			Result += texCUBE(ProjMap1,Tmp.xyz) * Light1;
		"
		*ifnot_proj1
		"
	                Result += Light1;
		"
		*if_proj2
		"
			Tmp.x = dot(TCMSPos, ProjMap2x);
			Tmp.y = dot(TCMSPos, ProjMap2y);
			Tmp.z = dot(TCMSPos, ProjMap2z);
			Result += texCUBE(ProjMap2,Tmp.xyz) * Light2;
		"
		*ifnot_proj2
		"
		        Result += Light2;
		"
	}
	*ifnot_lighting
	"
	        Result = splat4(1.0);
	"

	*do_0
	"
		DiffTexel = texDyn2D(DiffuseTex0,TCMapping0.xy);
	"

	*if_texture1
	"
                Tmp = texDyn2D(DiffuseTex1,TCMapping1.xy);
                DiffTexel *= Tmp;
	"

	*do_1
	"
	        DiffTexel *= (vCol * Color);
	        Result.a = 1.0;
	        Result *= DiffTexel;
	"

	*if_depthblend
	"
                vec4 depth = tex2D(DepthScreen,(ScreenCoord.xy / ScreenCoord.w) * DepthBlendParam.yz + ScreenCenter.xy);
                float d_res = ConvertDepth(depth,VPConst);
                /*
                depth.rgb *= 0.99609375;
                float d_res = (depth.g*0.00390625) + depth.r;

		d_res = (d_res * Diff) - BackPlane;
                d_res = -(Prod / d_res);
                */

                /*
		float BackPlane = VPParam.y;
		float FrontPlane = VPParam.x;
		float Diff = BackPlane - FrontPlane;
		float Prod = BackPlane * FrontPlane;

                float Tmp2 = TCMSPos.z/TCMSPos.w;
                Tmp2 = (Tmp2 * 0.5) + 0.5;
                Tmp2 = (Tmp2 * Diff) - BackPlane;
                Tmp2 = -(Prod / Tmp2) - 0.00390625;
		Result.rgba = splat4(Tmp2);
                */

                float Tmp2 = ScreenCoord.z;//((TCMSPos.z / TCMSPos.w)*0.5) + 0.5;
                Tmp2 = max(d_res - Tmp2 + DepthBlendParam.w,0.0);
		Result *= saturate(splat4(Tmp2 * DepthBlendParam.x) + DepthBlendAdd);
	"

	*if_fog
	{
	        *do
	        "
                        vec4 FogResult;
                        float FogAlphaScale;
                        DoFog(FogConst0,FogConst1,FogConst2,FogConst3,ScreenCoord,Fog1,FogResult,FogAlphaScale);
	        "
		*ifnot_fognocolor
		"
                        Result.rgb = lerp(Result.rgb,FogResult.rgb,FogResult.a);
		"
		*do2
		"
                        Result.a *= FogAlphaScale;
		"
	}

	*out
	"
                oCol = Result;
	"
}
