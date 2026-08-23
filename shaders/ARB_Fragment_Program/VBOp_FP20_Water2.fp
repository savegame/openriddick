/*¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯*\
	File:			FP20_Water
					
	Author:			Magnus Högdahl
					
	Copyright:		Starbreeze AB 2006
					
	History:

\*____________________________________________________________________________________________*/

*flags
{
	*fog 		0x0001
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
	*FOGCONST0 9
	*FOGCONST1 10
	*FOGCONST2 11
	*FOGCONST3 12
	*FOGTEXTURE0 6
	*FOGTEXTURE1 7
	*FOGTEXCOORD0 6
	*FOGTEXCOORD1 7
}

*program
{
	*do
	"!!ARBfp1.0

		OUTPUT oCol = result.color;

		ATTRIB vcol = fragment.color;
		ATTRIB tc0 = fragment.texcoord[0];
		ATTRIB tc1 = fragment.texcoord[1];
		#ATTRIB IPTSEV = fragment.texcoord[3];
		#ATTRIB tcproj = fragment.texcoord[4];
		ATTRIB MSPOS = fragment.texcoord[2];
		ATTRIB TSN = fragment.texcoord[3];
		ATTRIB TSU = fragment.texcoord[4];
		ATTRIB TSV = fragment.texcoord[5];

		PARAM c0 = program.env[0];
		PARAM c1 = program.env[1];
		PARAM ReflectionRefY = program.env[2];
		PARAM ReflectionRefX = program.env[3];
		
		PARAM Const_TSEV = program.env[4];

		PARAM Const_Proj0 = program.env[5];
		PARAM Const_Proj1 = program.env[6];
		PARAM Const_Proj2 = program.env[7];
		PARAM Const_Proj3 = program.env[8];

		PARAM Const0 = {0.0, 0.5, 1.0, 2.0};
		PARAM RefN = {0,0,1,0};

		PARAM Const1a = { 0.01, 0.01, 0, 0 };
		PARAM Const1b = { 3, 3, 0, 0 };
		PARAM Const2a = { 0.00, 0.03, 0, 0 };
		PARAM Const2b = { 9, 9, 0, 0 };
		PARAM Const3a = { 0.00, -0.035, 0, 0 };
		PARAM Const3b = { 10.99, 10.99, 0, 0 };
		PARAM Const4a = { -0.01, -0.005, 0, 0 };
		PARAM Const4b = { 8.99, 8.99, 0, 0 };
		PARAM Const5a = { 0.007, 0.005, 0, 0 };
		PARAM Const5b = { 2.5, 2.5, 0, 0 };
		PARAM Const6a = { 0.005, -0.007, 0, 0 };
		PARAM Const6b = { 2.4, 2.4, 0, 0 };
		PARAM Const5 = { -5, 30, 0, 0 };
		PARAM Const6 = { 9, 33, 0, 0 };
		PARAM Const7 = { -37, -333, 0, 0 };
		PARAM Const8 = { -5, -337, 0, 0 };
		PARAM Const9 = { 17, -233, 0, 0 };
		PARAM Const10 = { -15, -237, 0, 0 };
		#PARAM Const11 = { 0.5,1,1.0,1 };
		PARAM Const11 = { 1,1,1,1 };
		PARAM Const_DistScale = { 0.02, 0.1, 0, 0 };
		PARAM Const_TSRefNormal = { 1,0,0,0 };


		TEMP dN;
		TEMP NormalMapTexel;
		TEMP ReflectionTexel;
		TEMP RefractionTexel;
		TEMP t1;
		TEMP TSEV;
		TEMP TSRefY;
		TEMP TSRefX;
		TEMP TSRefN;
		TEMP IPTSEV;
		TEMP tcproj;

		TEMP r0;
		TEMP r1;
		TEMP r2;
		TEMP Result;

		#-----------------------------------
		SUB r0, Const_TSEV, MSPOS;
		DP3 IPTSEV.x, r0, TSN;
		DP3 IPTSEV.y, r0, TSV;
		DP3 IPTSEV.z, r0, TSU;
		
		DP4 tcproj.x, MSPOS, Const_Proj0;
		DP4 tcproj.y, MSPOS, Const_Proj1;
		DP4 tcproj.z, MSPOS, Const_Proj2;
		DP4 tcproj.w, MSPOS, Const_Proj3;

		DP3 TSEV.w, IPTSEV, IPTSEV;
		RSQ TSEV.w, TSEV.w;
		MUL TSEV.xyz, IPTSEV, TSEV.w;

		MUL r2, c0.w, 3;

		MUL r0.xy, r2.w, Const1a;
		MAD r0, tc0, Const1b, r0;
		TEX r1, r0, texture[2], 2D;
		MAD r1.rgb, r1, Const0.w, -Const0.z;	# (0..1 -> -1..1)
		MOV NormalMapTexel, r1;

		MUL r0.xy, r2.w, Const2a;
		MAD r0, tc0, Const2b, r0;
		TEX r1, r0, texture[2], 2D;
		MAD r1.rgb, r1, Const0.w, -Const0.z;	# (0..1 -> -1..1)
		ADD NormalMapTexel, NormalMapTexel, r1;

		MUL r0.xy, r2.w, Const3a;
		MAD r0, tc0, Const3b, r0;
		TEX r1, r0, texture[1], 2D;
		MAD r1.rgb, r1, Const0.w, -Const0.z;	# (0..1 -> -1..1)
		ADD NormalMapTexel, NormalMapTexel, r1;

		MUL r0.xy, r2.w, Const4a;
		MAD r0, tc0, Const4b, r0;
		TEX r1, r0, texture[1], 2D;
		MAD r1.rgb, r1, Const0.w, -Const0.z;	# (0..1 -> -1..1)
		ADD NormalMapTexel, NormalMapTexel, r1;

		MUL r0.xy, r2.w, Const5a;
		MAD r0, tc0, Const5b, r0;
		TEX r1, r0, texture[1], 2D;
		MAD r1.rgb, r1, Const0.w, -Const0.z;	# (0..1 -> -1..1)
		ADD NormalMapTexel, NormalMapTexel, r1;

		MUL r0.xy, r2.w, Const6a;
		MAD r0, tc0, Const6b, r0;
		TEX r1, r0, texture[2], 2D;
		MAD r1.rgb, r1, Const0.w, -Const0.z;	# (0..1 -> -1..1)
		ADD NormalMapTexel, NormalMapTexel, r1;

		MUL NormalMapTexel, NormalMapTexel, 0.1666;
		#MAD NormalMapTexel.rgba, NormalMapTexel, Const0.w, -Const0.z;	# (0..1 -> -1..1)
		#MOV NormalMapTexel, 0.0;
		MUL NormalMapTexel, NormalMapTexel, 1.0;

		DP3 TSRefY.x, ReflectionRefY, TSN;
		DP3 TSRefY.y, ReflectionRefY, TSV;
		DP3 TSRefY.z, ReflectionRefY, TSU;
		DP3 TSRefX.x, ReflectionRefX, TSN;
		DP3 TSRefX.y, ReflectionRefX, TSV;
		DP3 TSRefX.z, ReflectionRefX, TSU;
		DP3 TSRefN.x, RefN, TSN;
		DP3 TSRefN.y, RefN, TSV;
		DP3 TSRefN.z, RefN, TSU;
#MOV TSRefY, ReflectionRefY;
#MOV TSRefX, ReflectionRefX;


#		MOV NormalMapTexel.b, 0;
		MOV NormalMapTexel.r, 0;
		DP3 NormalMapTexel.r, NormalMapTexel, NormalMapTexel;		# r = g^2 + b^2
		SUB NormalMapTexel.r, Const0.z, NormalMapTexel.r;		# r = 1 - r;
		RSQ NormalMapTexel.r, NormalMapTexel.r;				# r = 1/sqrt(r)
		RCP NormalMapTexel.r, NormalMapTexel.r;				# r = 1/r = sqrt(1 - g^2 - b^2)

#		MOV NormalMapTexel, { 1,0,0,0 };

		DP3 r0.x, NormalMapTexel, TSEV;
		DP3 r0.y, Const_TSRefNormal, TSEV;
		MUL r0.xy, r0, 2;
		MAD r1, NormalMapTexel, r0.x, -TSEV;
		MAD r2, Const_TSRefNormal, r0.y, -TSEV;
		SUB dN, r1, r2;
#MAD Result, dN, 0.25, 0.5;

#		SUB dN, NormalMapTexel, TSRefN;
		DP3 r0.x, TSRefX, dN;
		DP3 r0.y, TSRefY, dN;
		MUL r1.xy, r0, 0.02;
		MUL r0.xy, r0, Const_DistScale;

		MOV r0.zw,0;
		RCP r0.w, tc1.w;
		MAD r0.xy, tc1, r0.w, r0;

		TEX ReflectionTexel, r0, texture[0], 2D;

		RCP r0.w, tcproj.w;
		MUL r0.xy, tcproj, r0.w;
		SUB r0.xy, r0, r1;
		TEX RefractionTexel, r0, texture[4], 2D;

		DP3_SAT r0.a, NormalMapTexel, TSEV;
	#	MUL r0.a, r0.a, r0.a;
		ADD r0.a, 1, -r0.a;
		MAX r0.a, 0.1, r0.a;
		MUL r1.rgb, RefractionTexel, vcol;
		MUL r0.a, r0.a, vcol.a;
		LRP Result.rgb, r0.a, ReflectionTexel, r1;
		MOV Result.a, 1;
#DP3 dN, dN, TSRefY;
#MAD Result, dN, 0.25, 0.5;
	"

	*if_fog
	{
		*INCLUDE "Include_XREngine_Fog.fph"
		*do
		"
			LRP Result.rgb, FogResult.a, FogResult, Result;
		"
	}

	*end
	"
		MOV oCol, Result;
		END
	"
}

