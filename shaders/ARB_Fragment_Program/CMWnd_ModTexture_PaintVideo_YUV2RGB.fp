#/*¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯*\
#	File:			CMWnd_ModTexture_PaintVideo_YUV2RGB
#					
#	Author:			Jim Kjellin
#					
#	Copyright:		Starbreeze AB 2004
#					
#	History:
#
#\*____________________________________________________________________________________________*/
*flags
{
	*MaskMap	0x0001
}

*generate
{
	*permute MaskMap
	{
		*gen 0
	}
}

*program
{
	*header
	"!!ARBfp1.0
	OPTION ARB_precision_hint_fastest;
	"
	
	*begindeclarations
	{
		*common
		"
		#-----------------------------------
		
		OUTPUT oCol = result.color;
		
		ATTRIB tc0 = fragment.texcoord[0];
		ATTRIB iCol = fragment.color;
		
		# Y component in texture0's rgb
		# U component in texture1's rgb
		# V component in texture1's a
		
		TEMP YComponent;
		TEMP UVComponents;
		
		TEMP YUV;#  YComponent;
		TEMP RGB;# UVComponents;
		TEMP RESULT;
		
		PARAM YScale = { 0.5, 1.164, -0.073035294117647058823529411764044, 0 };
		PARAM UScale = { 0, -0.391, 2.018, 0 };
		PARAM VScale = { 1.596, -0.813, 0, 0 };
		"
		
		*if_MaskMap
		"
		ATTRIB tc2 = fragment.texcoord[2];
		TEMP MaskTexel;
		"
	}
	
	*enddeclarations
	"
		#-----------------------------------
	"
	
	*if_MaskMap
	"
		TEX MaskTexel, tc2, texture[2], 2D;
	"
	
	*do
	"
		TEX YComponent, tc0, texture[0], 2D;
		TEX UVComponents, tc0, texture[1], 2D;
		MOV YUV, YComponent;
		MOV RGB, UVComponents;
		
		SUB YUV.ga, UVComponents, YScale.x;
		MAD RGB.rgb, YUV.r, YScale.y, YScale.z;
		MAD RGB.rg, VScale.rgba, YUV.a, RGB;
		MAD RGB.gb, UScale.rgba, YUV.g, RGB;
		
		MUL_SAT RESULT.rgb, RGB, iCol;
		MOV RESULT.a, UScale.x;
	"
	
	*if_MaskMap
	"
		MUL oCol, MaskTexel, RESULT;
	"
	
	*ifnot_MaskMap
	"
		MOV oCol, RESULT;
		#MOV_SAT oCol.rgb, RGB;
	"
	
	*endprogram
	"	
		END
	"
}