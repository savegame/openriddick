/*¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯*\
	File:			Bump mapped environment mapping (GenEnv surface operator)
					
	Author:			Magnus Högdahl
					
	Copyright:		Starbreeze AB 2004
					
	History:
		2007-01-18:	Converted to registry format and added fog support.

\*____________________________________________________________________________________________*/

*flags
{
	*fog		0x0001
	*fogcube 	0x0002
	*fogsky 	0x0004
}

*generate
{
	*gen 0
	*permute fogcube+fogsky
	{
		*gen fog
	}
}

*defines
{
	*FOGCONST0 0
	*FOGCONST1 1
	*FOGCONST2 2
	*FOGCONST3 3
	*FOGTEXTURE0 6
	*FOGTEXTURE1 7
	*FOGTEXCOORD0 6
	*FOGTEXCOORD1 7
}

*program
{
	*do
	"!!ARBfp1.0

		#-----------------------------------
		OUTPUT oCol = result.color;

		ATTRIB v0 = fragment.color;

		ATTRIB tc0 = fragment.texcoord[0];
		ATTRIB tc1 = fragment.texcoord[1];
		ATTRIB tc2 = fragment.texcoord[2];
		ATTRIB tc3 = fragment.texcoord[3];

		PARAM const1 = {0.0, 1.0, 2.0, 1.0};

		TEMP tc4;

		TEMP NormalMapTexel;
		TEMP t3;
		TEMP u;
		TEMP e;

		TEMP r0;

		#-----------------------------------

		TEX NormalMapTexel, tc0, texture[0], 2D;

		MAD NormalMapTexel.rgba, NormalMapTexel, const1.z, -const1.y;# Bias and scale the normalmap texel (only rgb) (from 0->1, -1->1)
		SWZ NormalMapTexel.rgb, NormalMapTexel, 0, a, g, 0;
		DP3 NormalMapTexel.r, NormalMapTexel, NormalMapTexel;	# r = g^2 + b^2
		SUB NormalMapTexel.r, const1.y, NormalMapTexel.r;	# r = 1 - r;
		RSQ NormalMapTexel.r, NormalMapTexel.r;			# r = 1/sqrt(r)
		RCP NormalMapTexel.r, NormalMapTexel.r;			# r = 1/r = sqrt(1 - g^2 - b^2)

		DP3 u.x, tc1, NormalMapTexel;
		DP3 u.y, tc2, NormalMapTexel;
		DP3 u.z, tc3, NormalMapTexel;
		MOV e.x, tc1.w;
		MOV e.y, tc2.w;
		MOV e.z, tc3.w;
		DP3 r0.a, e, u;
		DP3 r0.x, u, u;
		RCP r0.x, r0.x;
		MUL r0.x, r0.x, r0.a;
		MUL r0.x, r0.x, const1.z;
		MAD tc4.xyz, u, r0.x, -e;

		TEX t3, tc4, texture[1], CUBE;

		MUL r0.rgb, t3, v0;
		MOV r0.a, v0.a;
	"
	
	*if_fog
	{
		*INCLUDE "Include_XREngine_Fog.fph"
		*do
		"
			LRP r0.rgb, FogResult.a, FogResult, r0;
		"		
	}

	*end	
	"
		MOV oCol, r0;
		END
	"
}
