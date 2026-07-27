/*¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯*\
	File:			Program for XREngine color correction fusion
					
	Author:			Magnus Högdahl
					
	Copyright:		Starbreeze AB 2007
					
	History:

\*____________________________________________________________________________________________*/


*flags
{
	*lerp	0x0001
	*append	0x0002
	*gamma	0x0004
}

*generate
{
	*gen 0
	*gen lerp
	*gen append
	*gen gamma
}

*program
{
	*header 
	"!!ARBfp1.0
	"

	*declarations
	"
		#-----------------------------------
		OUTPUT oCol = result.color;

		ATTRIB tc0 = fragment.texcoord[0];

		PARAM RGBMapColor = program.env[0];
		PARAM Const1 = { 2.7182818284590, 1, 2, 0.5 };
		PARAM RGBMapUV = { 0.003086419753, 0.0555555555, 0.00154321, 0.027777778 };
		PARAM RGBMapUVHalf = { 0.00154321, 0.027777778, 0.00154321, 0.027777778 };
		PARAM RGBMapScale = { 17, 17, 18, 0.05555555 };

		TEMP t0;
		TEMP t1;
		TEMP t2;
		TEMP t3;
		TEMP r0;
		TEMP r1;
		TEMP r2;
	"

	*if_gammatest
	"
		PARAM Gamma = program.env[1];
		PARAM BlackLevel = program.env[2];
#		PARAM GammaR = program.env[1];
#		PARAM GammaY = program.env[2];
#		PARAM GammaG = program.env[3];
#		PARAM GammaC = program.env[4];
#		PARAM GammaB = program.env[5];
#		PARAM GammaM = program.env[6];
#		PARAM GammaK = program.env[7];
#		PARAM GammaR = { 0.5, 2.0, 2.0, 1 };
#		PARAM GammaY = { 0.5, 0.5, 2.0, 1 };
#		PARAM GammaG = { 2.0, 0.5, 2.0, 1 };
#		PARAM GammaC = { 2.0, 0.5, 0.5, 1 };
#		PARAM GammaB = { 2.0, 2.0, 0.5, 1 };
#		PARAM GammaM = { 0.5, 2.0, 0.5, 1 };
		PARAM GammaR = { 1.0, 1.0, 1.0, 1 };
		PARAM GammaY = { 1.0, 1.0, 1.0, 1 };
		PARAM GammaG = { 1.0, 1.0, 1.0, 1 };
		PARAM GammaC = { 1.0, 1.0, 1.0, 1 };
		PARAM GammaB = { 0.5, 2.5, 0.5, 1 };
		PARAM GammaM = { 1.0, 1.0, 1.0, 1 };
		PARAM GammaK = { 1,1,1,1 };
		TEMP Dot;
		TEMP AbsDotComp;
		TEMP DotSat;
		TEMP DotSatNeg;
		TEMP f03;
		TEMP f47;
		TEMP GammaW;
#		PARAM Plane0 = { 0.0, -0.5, 0.5, 0 };
#		PARAM Plane1 = { 0.5, -0.5, 0.0, 0 };
#		PARAM Plane2 = { 0.5, 0.0, -0.5, 0 };
		PARAM Plane0 = { 0.0, -0.7071, 0.7071, 0 };
		PARAM Plane1 = { 0.7071, -0.7071, 0.0, 0 };
		PARAM Plane2 = { 0.7071, 0.0, -0.7071, 0 };

		TEX t0, tc0, texture[0], 2D;
		DP3 r0.w, t0, t0;
		RCP r0.w, r0.w;
		MUL r0, t0, r0.w;
		
		MOV_SAT t0, t0;
		DP3 Dot.x, r0, Plane0;
		DP3 Dot.y, r0, Plane1;
		DP3 Dot.z, r0, Plane2;
		MOV_SAT DotSat.xyz, Dot;
		MOV_SAT DotSatNeg.xyz, -Dot;
		MAX_SAT AbsDotComp.xyz, Dot, -Dot;
		SUB_SAT AbsDotComp.xyz, 1, AbsDotComp;

		MUL f03.x, AbsDotComp.x, DotSat.y;
		MUL f03.x, f03.x, DotSat.z;
		MUL f03.y, AbsDotComp.y, DotSatNeg.x;
		MUL f03.y, f03.y, DotSat.z;
		MUL f03.z, AbsDotComp.z, DotSatNeg.x;
		MUL f03.z, f03.z, DotSatNeg.y;
		MOV f03.w, 0;
		
		MUL f47.x, AbsDotComp.x, DotSatNeg.y;
		MUL f47.x, f47.x, DotSatNeg.z;
		MUL f47.y, AbsDotComp.y, DotSat.x;
		MUL f47.y, f47.y, DotSatNeg.z;
		MUL f47.z, AbsDotComp.z, DotSat.x;
		MUL f47.z, f47.z, DotSat.y;

		SUB r0.xyz, 1, f03;
		SUB r1.xyz, 1, f47;
		MUL r0.xyz, r0, r1;
		MUL f47.w, r0.x, r0.y;
		MUL_SAT f47.w, f47.w, r0.z;
		MUL f47.w, f47.w, f47.w;
		MUL f47.w, f47.w, f47.w;

		MUL f47.w, AbsDotComp.x, AbsDotComp.y;
		MUL f47.w, f47.w, AbsDotComp.z;
		ADD f47.w, f47.w, 0.0001;
	#	MOV f03, 0;
	#	MOV f47, 0;
	#	MOV f47.w, 1;

		# sum weights and get rcp
		DP3 r0.x, f03, 1;
		DP4 r0.y, f47, 1;
		ADD r0.x, r0.x, r0.y;
		RCP r0.x, r0.x;

		# Normalize weights
		MUL f03.xyz, f03, r0.x;
		MUL f47.xyzw, f47, r0.x;

		MUL GammaW.rgb, f03.x, GammaR;
		MAD GammaW.rgb, f03.y, GammaY, GammaW;
		MAD GammaW.rgb, f03.z, GammaG, GammaW;
		MAD GammaW.rgb, f47.x, GammaC, GammaW;
		MAD GammaW.rgb, f47.y, GammaB, GammaW;
		MAD GammaW.rgb, f47.z, GammaM, GammaW;
		MAD GammaW.rgb, f47.w, GammaK, GammaW;

		ADD t0, t0, 0.000001;
		POW r0.r, t0.r, GammaW.r;
		POW r0.g, t0.g, GammaW.g;
		POW r0.b, t0.b, GammaW.b;
#	MOV r0, f47.y;
#	MOV r0, GammaW;
		ADD r0.rgb, r0, BlackLevel;
		MOV r0.a, 1;
		MOV oCol, r0;
	"

	*if_gamma
	"
		PARAM Gamma = program.env[1];
		PARAM BlackLevel = program.env[2];

		TEX t0, tc0, texture[0], 2D;
		ADD t0, t0, 0.0000001;
		POW r0.r, t0.r, Gamma.r;
		POW r0.g, t0.g, Gamma.g;
		POW r0.b, t0.b, Gamma.b;
		ADD r0.rgb, r0, BlackLevel;
		MOV r0.a, 1;
		MOV oCol, r0;
	"
	
	*if_append
	"
		TEX t0, tc0, texture[0], 2D;
		MOV r0, t0;

		MUL r2.xy, r0, RGBMapScale;

		MUL r1.x, r0.z, RGBMapScale.x;
		FRC r1.y, r1.x;
		SUB r1.x, r1.x, r1.y;

		MAD r2.x, r1.x, RGBMapScale.z, r2.x;

		MAD r2.xy, r2, RGBMapUV, RGBMapUVHalf;
		TEX t2, r2, texture[1], 2D;
		ADD r2.x, RGBMapScale.w, r2.x;
		TEX t3, r2, texture[1], 2D;
		LRP r0.rgb, r1.y, t3, t2;

		MUL r0.rgb, r0, RGBMapColor;
		LRP r0.rgb, RGBMapColor.a, r0, t0;
		MOV oCol, r0;
	"
	*if_lerp
	"
		TEX t0, tc0, texture[0], 2D;
		TEX t1, tc0, texture[1], 2D;
		MOV r1, RGBMapColor;
		LRP r0.rgba, r1.a, t1, t0;
		MUL r0.rgb, r0, r1;
		MOV oCol, r0;
	"
	*ifnot_lerp { *ifnot_gamma { *ifnot_append
	{
		*simplecolormodulate
		"
			TEX t0, tc0, texture[1], 2D;
			MUL r0, t0, RGBMapColor;
			MOV oCol, r0;
		"
	}}}
	
	*end
	"END
	"
}
