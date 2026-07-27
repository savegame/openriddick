/*¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯*\
	File:			Program for XREngine guassian blur
					
	Author:			Magnus Högdahl
					
	Copyright:		Starbreeze AB 2004
					
	History:

 		Cycles/64 pixel vector: ALU 82.7, vertex 0, texture 136, sequencer 32, interpolator 4
 		18 GPRs, 10 threads, Performance (if enough threads): ~136 cycles per vector
\*____________________________________________________________________________________________*/

*flags
{
	*kernelgrow1 0x0001
	*kernelgrow2 0x0002
	*kernelgrow3 0x0004
}

*generate
{
	*gen 0							// 9x9
	*gen kernelgrow1				// 17x17
	*gen kernelgrow1 + kernelgrow2	// 33x33
//	*gen kernelgrow1 + kernelgrow2 + kernelgrow3
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
		ATTRIB texcoord = fragment.texcoord[0];

		#-----------------------------------------
		PARAM ColorBias = program.env[0];	# r,g,b,
		PARAM ColorScale = program.env[1];	# r,g,b,a
		PARAM ColorGamma = program.env[2];	# r,g,b,

		PARAM UVMin = program.env[3];		# u,v,u,v
		PARAM UVMax = program.env[4];		# u,v,u,v

		PARAM Weight0 = program.env[5];		# UCenter, VCenter, 0, w0
		PARAM Weight1 = program.env[6];		# w1,w2,w3,w4
		PARAM Weight5 = program.env[7];		# w5,w6,w7,w8

		PARAM dUV12 = program.env[8];
		PARAM dUV34 = program.env[9];
		PARAM dUV56 = program.env[10];
		PARAM dUV78 = program.env[11];

		#-----------------------------------------
		TEMP tcbase;
		TEMP tc0;
		TEMP tc1;
		TEMP tc2;
		TEMP tc3;
		TEMP tc4;

		TEMP t0;
		TEMP t1;
		TEMP t2;
		TEMP t3;
		TEMP t4;

		TEMP r0;
		TEMP Result;

	"
	
	*do1
	"
		#-----------------------------------------
		TEX t0, texcoord, texture[0], 2D;
		#MOV Result, t0;
		MUL Result.rgba, t0, Weight0.w;
		SWZ tcbase, texcoord, x, y, x, y;

		ADD tc0, tcbase, dUV12;
		MIN tc0, tc0, UVMax;
		MAX tc0, tc0, UVMin;
		SWZ tc1, tc0, z,w,0,0;
		ADD tc2, tcbase, dUV34;
		MIN tc2, tc2, UVMax;
		MAX tc2, tc2, UVMin;
		SWZ tc3, tc2, z,w,0,0;
		TEX t0, tc0, texture[0], 2D;
		TEX t1, tc1, texture[0], 2D;
		MAD Result.rgba, t0, Weight1.x, Result;
		MAD Result.rgba, t1, Weight1.y, Result;
	"
	
	*if_kernelgrow1
	"
		TEX t2, tc2, texture[0], 2D;
		TEX t3, tc3, texture[0], 2D;
		MAD Result.rgba, t2, Weight1.z, Result;
		MAD Result.rgba, t3, Weight1.w, Result;
	"
	
	*do2
	"		
		SUB tc0, tcbase, dUV12;
		MIN tc0, tc0, UVMax;
		MAX tc0, tc0, UVMin;
		SWZ tc1, tc0, z,w,0,0;
		SUB tc2, tcbase, dUV34;
		MIN tc2, tc2, UVMax;
		MAX tc2, tc2, UVMin;
		SWZ tc3, tc2, z,w,0,0;
		TEX t0, tc0, texture[0], 2D;
		TEX t1, tc1, texture[0], 2D;
		MAD Result.rgba, t0, Weight1.x, Result;
		MAD Result.rgba, t1, Weight1.y, Result;
	"
	
	*if_kernelgrow1
	"
		TEX t2, tc2, texture[0], 2D;
		TEX t3, tc3, texture[0], 2D;
		MAD Result.rgba, t2, Weight1.z, Result;
		MAD Result.rgba, t3, Weight1.w, Result;
	"
	
	*if_kernelgrow2
	"
		ADD tc0, tcbase, dUV56;
		MIN tc0, tc0, UVMax;
		MAX tc0, tc0, UVMin;
		SWZ tc1, tc0, z,w,0,0;
		ADD tc2, tcbase, dUV78;
		MIN tc2, tc2, UVMax;
		MAX tc2, tc2, UVMin;
		SWZ tc3, tc2, z,w,0,0;
		TEX t0, tc0, texture[0], 2D;
		TEX t1, tc1, texture[0], 2D;
		TEX t2, tc2, texture[0], 2D;
		TEX t3, tc3, texture[0], 2D;
		MAD Result.rgba, t0, Weight5.x, Result;
		MAD Result.rgba, t1, Weight5.y, Result;
		MAD Result.rgba, t2, Weight5.z, Result;
		MAD Result.rgba, t3, Weight5.w, Result;

		SUB tc0, tcbase, dUV56;
		MIN tc0, tc0, UVMax;
		MAX tc0, tc0, UVMin;
		SWZ tc1, tc0, z,w,0,0;
		SUB tc2, tcbase, dUV78;
		MIN tc2, tc2, UVMax;
		MAX tc2, tc2, UVMin;
		SWZ tc3, tc2, z,w,0,0;
		TEX t0, tc0, texture[0], 2D;
		TEX t1, tc1, texture[0], 2D;
		TEX t2, tc2, texture[0], 2D;
		TEX t3, tc3, texture[0], 2D;
		MAD Result.rgba, t0, Weight5.x, Result;
		MAD Result.rgba, t1, Weight5.y, Result;
		MAD Result.rgba, t2, Weight5.z, Result;
		MAD Result.rgba, t3, Weight5.w, Result;
	"

	*dofinish
	"
		MUL Result.rgba, Result, ColorScale;
		ADD Result.rgb, Result, ColorBias;
		MAX Result.rgb, Result, { 0, 0, 0, 0 };
		POW Result.r, Result.r, ColorGamma.r;
		POW Result.g, Result.g, ColorGamma.g;
		POW Result.b, Result.b, ColorGamma.b;

		#TEX Result, texcoord, texture[0], 2D;

		MOV oCol, Result;
		END
	"
}
