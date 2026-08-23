/*¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯*\
	File:			Program for COR:EFBB post processing effects
					
	Author:			Magnus Högdahl
					
	Copyright:		Starbreeze AB 2004
					
	History:

\*____________________________________________________________________________________________*/

*flags
{
	*noblur	0x0001
}

*generate
{
	*permute noblur
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
	
	*declarations
	"
		#-----------------------------------------
		OUTPUT oCol = result.color;
		
		ATTRIB v0 = fragment.color;
		ATTRIB tc0 = fragment.texcoord[0];
		
		PARAM c0 = program.env[0];
		PARAM c1 = program.env[1];
		PARAM c2 = program.env[2];
		PARAM c3 = program.env[3];
		
		PARAM const_val = { 0.5, 1.0, 2.0, 4.0 };
		
		TEMP t0;
		
		TEMP r0;
		TEMP r1;
		TEMP r2;
		#-----------------------------------------
	"

	*ifnot_noblur
	"
		ATTRIB tc1 = fragment.texcoord[1];
		TEMP t1;
		
		TEX t1, tc1, texture[1], 2D;
	"
	
	*main
	"
		TEX t0, tc0, texture[0], 2D;
		
		
		DP3 r1.rgb, t0, c0;
		MUL r1.rgb, r1, c1;
		MAD r0.rgb, t0, c2, r1;
		
		#mul r1.rgb, r0, 1-c3.a
		ADD r2.a, const_val.g, -c3.a;
		MUL r1.rgb, r0, r2.a;
		
		MAD r0.rgb, c0, c0.a, r1;
	"
	
	*ifnot_noblur
	"
		ADD r0.rgb, r0, t1;
	"
	
	*end
	"
		MOV r0.a, c0.a;
		MOV oCol, r0;
		
		END
	"
}

