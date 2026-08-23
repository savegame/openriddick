!!ARBfp1.0

#-----------------------------------------
OUTPUT oCol = result.color;

#ATTRIB v0 = fragment.color;
ATTRIB tc0 = fragment.texcoord[0];
ATTRIB tcdepth = fragment.texcoord[1];

PARAM Mtx0 = program.env[0];
PARAM Mtx1 = program.env[1];
PARAM Mtx2 = program.env[2];
PARAM Mtx3 = program.env[3];

PARAM VPParam = program.env[4]; # { F, B, 1/B, ... }
PARAM VPConst = program.env[5];	# { 2B, B+F, 2BF, 2(B-F) }
PARAM VPParam2 = program.env[6]; # { scale }

#PARAM VSScale = { 0.00390625, 0.00390625, 0.0009765625, 0 };
#PARAM VSScale = { 0.0009765625, 0.0009765625, 0.0009765625, 0 };
PARAM VSScale = { 0,0,2, 0 };
#PARAM VSScale = { 0, 0, 0.0009765625, 0 };
#PARAM VSScale = { 0, 0, 0.0009765625, 0 };
#PARAM VSOfs = { 0.5, 0.5, 0, 1 };
PARAM VSOfs = { 0, 0.0, 0, 1 };

PARAM const_depthdot = { 0.0039062502328306575316582639013686, 0.000015258789971994755983040093364721, 0.000000059604648328104515558750364705942, 0 };

	@if platform_pc
		PARAM const_vpmul = { 2, -2, 0, 0 };
		PARAM const_vpadd = { -1, 1, 0, 0 };
	@else
		PARAM const_vpmul = { 2, 2, 0, 0 };
		PARAM const_vpadd = { -1, -1, 0, 0 };
	@endif

TEMP tdepth;
TEMP tcdepthproj;
TEMP pixelwindow;
TEMP pixelview;
TEMP pixelview2;
TEMP pixelworld;
TEMP r0;
TEMP r1;
TEMP r2;
#-----------------------------------------

		MOV pixelview, 0;
		MOV pixelview2, tc0;
		
		RCP tcdepthproj.w, tcdepth.w;
		MUL tcdepthproj, tcdepth, tcdepthproj.w;
		TEX tdepth, tcdepthproj, texture[0], 2D;

		@if platform_xenon
			SUB r2.x, 1, tdepth.r;
		@else
			MAD tdepth, tdepth, 255, 0.5;
			FLR tdepth, tdepth;
			DP3 tdepth.a, tdepth, const_depthdot;
			MAD r2.x, tdepth.a, 2, -1;
		@endif
		
		MAD r2.y, r2.x, VPConst.w, -VPConst.x;
		RCP r2.y, r2.y;
		MUL pixelview.z, r2.y, -VPConst.z;
	#	MUL pixelview.z, pixelview.z, 2;
		
		MAD pixelview.xy, tcdepthproj, const_vpmul, const_vpadd;
		MUL pixelview.x, pixelview, VPParam2.z;
		MUL pixelview.y, pixelview, VPParam2.w;
		MUL pixelview.xy, pixelview, pixelview.z;
		MOV pixelview.w, 1;
	
		DP4 pixelworld.x, pixelview, Mtx0;
		DP4 pixelworld.y, pixelview, Mtx1;
		DP4 pixelworld.z, pixelview, Mtx2;
		DP4 pixelworld.w, pixelview, Mtx3;

		SUB pixelview, pixelview, pixelview2;
		ABS pixelview, pixelview;
		MAD oCol, pixelview, VSScale, VSOfs;
		
		MUL r0, pixelworld, 0.00390625;
		TEX r0, r0, texture[1], 2D;
		MOV oCol, r0;
	#		MAD oCol, tc0, VSScale, VSOfs;
END
