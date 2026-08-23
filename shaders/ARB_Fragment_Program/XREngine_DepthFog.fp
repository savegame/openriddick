/*¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯*\
	File:			Wrapper program for multipass fog
					
	Author:			Magnus Högdahl
					
	Copyright:		Starbreeze AB 2007
					
	History:

\*____________________________________________________________________________________________*/

*flags
{
	*fogcube 	0x0001
	*fogsky 	0x0002
}

*generate
{
	*permute fogcube+fogsky
	{
		*gen 0
	}
}

*defines
{
	*FOGCONST0 0
	*FOGCONST1 1
	*FOGCONST2 2
	*FOGCONST3 3
	*FOGTEXTURE0 0
	*FOGTEXTURE1 1
	*FOGTEXCOORD0 0
	*FOGTEXCOORD1 1
}

*program
{
	*do
	"!!ARBfp1.0
		OUTPUT oCol = result.color;
	"
	
	*INCLUDE "Include_XREngine_Fog.fph"

	*do2
	"
		MOV oCol, FogResult;

		END
	"
}
