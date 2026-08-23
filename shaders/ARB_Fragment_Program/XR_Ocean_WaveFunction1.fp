/*¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯*\
	File:			
					
	Author:			Mangus Högdahl
					
	Copyright:		Starbreeze AB 2007
					
	History:

\*____________________________________________________________________________________________*/

*flags
{
	*horizon 1
}

*generate
{
	*gen 0
	*gen horizon
}

*program
{
	*do0
	"!!ARBfp1.0
OPTION ARB_draw_buffers;

		
		#-----------------------------------------
		# TexCoord0 = Mapping tex coord
		
		#-----------------------------------------
		
		OUTPUT oCol0 = result.color[0];
	#	OUTPUT oCol1 = result.color[1];
	#	OUTPUT oCol2 = result.color[2];
		
		ATTRIB tc0 = fragment.texcoord[0];
		PARAM time = program.env[0];
		PARAM vofs = program.env[1];
		PARAM vscale = program.env[2];
		PARAM vtessbase = program.env[3];
		PARAM vtessdiru = program.env[4];
		PARAM vtessdirv = program.env[5];
		PARAM vtess = program.env[6];
		PARAM vtesspersp = program.env[7];

		PARAM wavedir_00 = program.env[8];
		PARAM wavedir_01 = program.env[9];
		PARAM wavedir_02 = program.env[10];
		PARAM wavedir_03 = program.env[11];
		PARAM waveper_00_03 = program.env[12];
		PARAM waveamp_00_03 = program.env[13];
		PARAM wavekmag_00_03 = program.env[14];

		PARAM wavedir_04 = program.env[16];
		PARAM wavedir_05 = program.env[17];
		PARAM wavedir_06 = program.env[18];
		PARAM wavedir_07 = program.env[19];
		PARAM waveper_04_07 = program.env[20];
		PARAM waveamp_04_07 = program.env[21];
		PARAM wavekmag_04_07 = program.env[22];
		
		PARAM wavedir_08 = program.env[24];
		PARAM wavedir_09 = program.env[25];
		PARAM wavedir_0a = program.env[26];
		PARAM wavedir_0b = program.env[27];
		PARAM waveper_08_0b = program.env[28];
		PARAM waveamp_08_0b = program.env[29];
		PARAM wavekmag_08_0b = program.env[30];
		
		PARAM wavedir_0c = program.env[32];
		PARAM wavedir_0d = program.env[33];
		PARAM wavedir_0e = program.env[34];
		PARAM wavedir_0f = program.env[35];
		PARAM waveper_0c_0f = program.env[36];
		PARAM waveamp_0c_0f = program.env[37];
		PARAM wavekmag_0c_0f = program.env[38];
		
		PARAM wavedir_10 = program.env[40];
		PARAM wavedir_11 = program.env[41];
		PARAM wavedir_12 = program.env[42];
		PARAM wavedir_13 = program.env[43];
		PARAM waveper_10_13 = program.env[44];
		PARAM waveamp_10_13 = program.env[45];
		PARAM wavekmag_10_13 = program.env[46];
		
		PARAM wavedir_14 = program.env[48];
		PARAM wavedir_15 = program.env[49];
		PARAM wavedir_16 = program.env[50];
		PARAM wavedir_17 = program.env[51];
		PARAM waveper_14_17 = program.env[52];
		PARAM waveamp_14_17 = program.env[53];
		PARAM wavekmag_14_17 = program.env[54];
		
		PARAM wavedir_18 = program.env[56];
		PARAM wavedir_19 = program.env[57];
		PARAM wavedir_1a = program.env[58];
		PARAM wavedir_1b = program.env[59];
		PARAM waveper_18_1b = program.env[60];
		PARAM waveamp_18_1b = program.env[61];
		PARAM wavekmag_18_1b = program.env[62];
		
		PARAM wavedir_1c = program.env[64];
		PARAM wavedir_1d = program.env[65];
		PARAM wavedir_1e = program.env[66];
		PARAM wavedir_1f = program.env[67];
		PARAM waveper_1c_1f = program.env[68];
		PARAM waveamp_1c_1f = program.env[69];
		PARAM wavekmag_1c_1f = program.env[70];
		
		PARAM w2v_r0 = program.env[72];
		PARAM w2v_r1 = program.env[73];
		PARAM w2v_r2 = program.env[74];
		PARAM w2v_r3 = program.env[75];
		
		PARAM meterconv = { 0.03125, 32.0, 0, 0 };
		
	#	PARAM wavedir_00 = { 0.707, 0.707, 0, 0 };
	#	PARAM wavedir_01 = { 1, 0, 0, 0 };
	#	PARAM wavedir_02 = { 0.707, -0.707, 0, 0 };
	#	PARAM wavedir_03 = { 0.907, 0.057, 0, 0 };
		
	#	PARAM wavedir_04 = { 0.807, 0.247, 0, 0 };
	#	PARAM wavedir_05 = { 1, -0.1, 0, 0 };
	#	PARAM wavedir_06 = { 0.867, -0.167, 0, 0 };
	#	PARAM wavedir_07 = { 0.907, 0.207, 0, 0 };
		
	#	PARAM waveper_00_03 = { -0.091, -0.031, -0.053, -0.035 };
	#	PARAM waveamp_00_03 = { 1, 10, 2, 10 };
	#	PARAM waveper_04_07 = { -0.132, -0.161, -0.137, -0.071 };
	#	PARAM waveamp_04_07 = { 1.5, 3, 1.5, 6 };
		
		TEMP r0;
		TEMP r1;
		TEMP r2;
		TEMP pos;
		TEMP restpos;
		TEMP ofs;
		TEMP timescaled;
		
		MUL timescaled, time, 1.0;

	#	PARAM vtessbase = program.env[3];
	#	PARAM vtessdiru = program.env[4];
	#	PARAM vtessdirv = program.env[5];
	#	PARAM vtess = program.env[6];
		
		MOV r2, tc0;
	#	SUB r2.y, 1.0, r2.y;
	
		LRP r2.y, r2.y, vtesspersp.x, vtesspersp.y;
		RCP r2.y, r2.y;
		MAD r2.y, r2.y, vtesspersp.z, vtesspersp.w;

	#	LRP r2.y, r2.y, 1.0, 8.0;
	#	RCP r2.y, r2.y;
	#	SUB r2.y, r2.y, 0.125;
	#	MUL r2.y, r2.y, 1.142857142;
			
		MAD r2.x, r2.x, 2.0, -1.0;
		LRP r0.x, r2.y, vtess.y, vtess.x;
	#	MUL r0.x, r0.x, 1.5;
	#	MOV r0.x, 1024;
		MUL r0.x, r0.x, r2.x;
		MUL r0.y, r2.y, vtess.z;
	#	MOV r0.z, 0;
	#	MUL r0, r2, 1024;
		MUL pos, r0.x, vtessdiru;
		MAD pos, r0.y, vtessdirv, pos;
		ADD pos, pos, vtessbase;
		MOV pos.w, 1.0;
	"

	*if_horizon
	"
		ADD pos.z, pos.z, -14000.0;
	"
	
	*ifnot_horizon
	"		
	#	MOV r2, tc0;
	#	MAD pos, vscale, tc0, vofs;
		MOV ofs, 0.0;
		MUL restpos, meterconv.x, pos;
		
		# Wave pack 1
		DP3 r0.x, restpos, wavedir_00;
		DP3 r0.y, restpos, wavedir_01;
		DP3 r0.z, restpos, wavedir_02;
		DP3 r0.w, restpos, wavedir_03;
		MUL r0, r0, wavekmag_00_03;
		MAD r0, waveper_00_03, timescaled.x, r0;
		SIN r1.x, r0.x;
		SIN r1.y, r0.y;
		SIN r1.z, r0.z;
		SIN r1.w, r0.w;
		COS r2.x, r0.x;
		COS r2.y, r0.y;
		COS r2.z, r0.z;
		COS r2.w, r0.w;
		MUL r1, r1, waveamp_00_03;
		MUL r2, -r2, waveamp_00_03;
		MAD ofs.xy, r1.x, wavedir_00, ofs;
		MAD ofs.xy, r1.x, wavedir_01, ofs;
		MAD ofs.xy, r1.x, wavedir_02, ofs;
		MAD ofs.xy, r1.x, wavedir_03, ofs;
		ADD ofs.z, ofs, r2.x;
		ADD ofs.z, ofs, r2.y;
		ADD ofs.z, ofs, r2.z;
		ADD ofs.z, ofs, r2.w;
	
		# Wave pack 2
		DP3 r0.x, restpos, wavedir_04;
		DP3 r0.y, restpos, wavedir_05;
		DP3 r0.z, restpos, wavedir_06;
		DP3 r0.w, restpos, wavedir_07;
		MUL r0, r0, wavekmag_04_07;
		MAD r0, waveper_04_07, timescaled.x, r0;
		SIN r1.x, r0.x;
		SIN r1.y, r0.y;
		SIN r1.z, r0.z;
		SIN r1.w, r0.w;
		COS r2.x, r0.x;
		COS r2.y, r0.y;
		COS r2.z, r0.z;
		COS r2.w, r0.w;
		MUL r1, r1, waveamp_04_07;
		MUL r2, -r2, waveamp_04_07;
		MAD ofs.xy, r1.x, wavedir_04, ofs;
		MAD ofs.xy, r1.x, wavedir_05, ofs;
		MAD ofs.xy, r1.x, wavedir_06, ofs;
		MAD ofs.xy, r1.x, wavedir_07, ofs;
		ADD ofs.z, ofs, r2.x;
		ADD ofs.z, ofs, r2.y;
		ADD ofs.z, ofs, r2.z;
		ADD ofs.z, ofs, r2.w;
	
		# Wave pack 3
		DP3 r0.x, restpos, wavedir_08;
		DP3 r0.y, restpos, wavedir_09;
		DP3 r0.z, restpos, wavedir_0a;
		DP3 r0.w, restpos, wavedir_0b;
		MUL r0, r0, wavekmag_08_0b;
		MAD r0, waveper_08_0b, timescaled.x, r0;
		SIN r1.x, r0.x;
		SIN r1.y, r0.y;
		SIN r1.z, r0.z;
		SIN r1.w, r0.w;
		COS r2.x, r0.x;
		COS r2.y, r0.y;
		COS r2.z, r0.z;
		COS r2.w, r0.w;
		MUL r1, r1, waveamp_08_0b;
		MUL r2, -r2, waveamp_08_0b;
		MAD ofs.xy, r1.x, wavedir_08, ofs;
		MAD ofs.xy, r1.x, wavedir_09, ofs;
		MAD ofs.xy, r1.x, wavedir_0a, ofs;
		MAD ofs.xy, r1.x, wavedir_0b, ofs;
		ADD ofs.z, ofs, r2.x;
		ADD ofs.z, ofs, r2.y;
		ADD ofs.z, ofs, r2.z;
		ADD ofs.z, ofs, r2.w;
	
		# Wave pack 4
		DP3 r0.x, restpos, wavedir_0c;
		DP3 r0.y, restpos, wavedir_0d;
		DP3 r0.z, restpos, wavedir_0e;
		DP3 r0.w, restpos, wavedir_0f;
		MUL r0, r0, wavekmag_0c_0f;
		MAD r0, waveper_0c_0f, timescaled.x, r0;
		SIN r1.x, r0.x;
		SIN r1.y, r0.y;
		SIN r1.z, r0.z;
		SIN r1.w, r0.w;
		COS r2.x, r0.x;
		COS r2.y, r0.y;
		COS r2.z, r0.z;
		COS r2.w, r0.w;
		MUL r1, r1, waveamp_0c_0f;
		MUL r2, -r2, waveamp_0c_0f;
		MAD ofs.xy, r1.x, wavedir_0c, ofs;
		MAD ofs.xy, r1.x, wavedir_0d, ofs;
		MAD ofs.xy, r1.x, wavedir_0e, ofs;
		MAD ofs.xy, r1.x, wavedir_0f, ofs;
		ADD ofs.z, ofs, r2.x;
		ADD ofs.z, ofs, r2.y;
		ADD ofs.z, ofs, r2.z;
		ADD ofs.z, ofs, r2.w;


		# Wave pack 5
		DP3 r0.x, restpos, wavedir_10;
		DP3 r0.y, restpos, wavedir_11;
		DP3 r0.z, restpos, wavedir_12;
		DP3 r0.w, restpos, wavedir_13;
		MUL r0, r0, wavekmag_10_13;
		MAD r0, waveper_10_13, timescaled.x, r0;
		SIN r1.x, r0.x;
		SIN r1.y, r0.y;
		SIN r1.z, r0.z;
		SIN r1.w, r0.w;
		COS r2.x, r0.x;
		COS r2.y, r0.y;
		COS r2.z, r0.z;
		COS r2.w, r0.w;
		MUL r1, r1, waveamp_10_13;
		MUL r2, -r2, waveamp_10_13;
		MAD ofs.xy, r1.x, wavedir_10, ofs;
		MAD ofs.xy, r1.x, wavedir_11, ofs;
		MAD ofs.xy, r1.x, wavedir_12, ofs;
		MAD ofs.xy, r1.x, wavedir_13, ofs;
		ADD ofs.z, ofs, r2.x;
		ADD ofs.z, ofs, r2.y;
		ADD ofs.z, ofs, r2.z;
		ADD ofs.z, ofs, r2.w;
	
		# Wave pack 6
		DP3 r0.x, restpos, wavedir_14;
		DP3 r0.y, restpos, wavedir_15;
		DP3 r0.z, restpos, wavedir_16;
		DP3 r0.w, restpos, wavedir_17;
		MUL r0, r0, wavekmag_14_17;
		MAD r0, waveper_14_17, timescaled.x, r0;
		SIN r1.x, r0.x;
		SIN r1.y, r0.y;
		SIN r1.z, r0.z;
		SIN r1.w, r0.w;
		COS r2.x, r0.x;
		COS r2.y, r0.y;
		COS r2.z, r0.z;
		COS r2.w, r0.w;
		MUL r1, r1, waveamp_14_17;
		MUL r2, -r2, waveamp_14_17;
		MAD ofs.xy, r1.x, wavedir_14, ofs;
		MAD ofs.xy, r1.x, wavedir_15, ofs;
		MAD ofs.xy, r1.x, wavedir_16, ofs;
		MAD ofs.xy, r1.x, wavedir_17, ofs;
		ADD ofs.z, ofs, r2.x;
		ADD ofs.z, ofs, r2.y;
		ADD ofs.z, ofs, r2.z;
		ADD ofs.z, ofs, r2.w;
	
		# Wave pack 7
		DP3 r0.x, restpos, wavedir_18;
		DP3 r0.y, restpos, wavedir_19;
		DP3 r0.z, restpos, wavedir_1a;
		DP3 r0.w, restpos, wavedir_1b;
		MUL r0, r0, wavekmag_18_1b;
		MAD r0, waveper_18_1b, timescaled.x, r0;
		SIN r1.x, r0.x;
		SIN r1.y, r0.y;
		SIN r1.z, r0.z;
		SIN r1.w, r0.w;
		COS r2.x, r0.x;
		COS r2.y, r0.y;
		COS r2.z, r0.z;
		COS r2.w, r0.w;
		MUL r1, r1, waveamp_18_1b;
		MUL r2, -r2, waveamp_18_1b;
		MAD ofs.xy, r1.x, wavedir_18, ofs;
		MAD ofs.xy, r1.x, wavedir_19, ofs;
		MAD ofs.xy, r1.x, wavedir_1a, ofs;
		MAD ofs.xy, r1.x, wavedir_1b, ofs;
		ADD ofs.z, ofs, r2.x;
		ADD ofs.z, ofs, r2.y;
		ADD ofs.z, ofs, r2.z;
		ADD ofs.z, ofs, r2.w;
	
		# Wave pack 8
		DP3 r0.x, restpos, wavedir_1c;
		DP3 r0.y, restpos, wavedir_1d;
		DP3 r0.z, restpos, wavedir_1e;
		DP3 r0.w, restpos, wavedir_1f;
		MUL r0, r0, wavekmag_1c_1f;
		MAD r0, waveper_1c_1f, timescaled.x, r0;
		SIN r1.x, r0.x;
		SIN r1.y, r0.y;
		SIN r1.z, r0.z;
		SIN r1.w, r0.w;
		COS r2.x, r0.x;
		COS r2.y, r0.y;
		COS r2.z, r0.z;
		COS r2.w, r0.w;
		MUL r1, r1, waveamp_1c_1f;
		MUL r2, -r2, waveamp_1c_1f;
		MAD ofs.xy, r1.x, wavedir_1c, ofs;
		MAD ofs.xy, r1.x, wavedir_1d, ofs;
		MAD ofs.xy, r1.x, wavedir_1e, ofs;
		MAD ofs.xy, r1.x, wavedir_1f, ofs;
		ADD ofs.z, ofs, r2.x;
		ADD ofs.z, ofs, r2.y;
		ADD ofs.z, ofs, r2.z;
		ADD ofs.z, ofs, r2.w;
	
	
		MAD r0.xyz, ofs, meterconv.y, pos;
		MOV r0.w, 1;
		DP4 pos.x, r0, w2v_r0;
		DP4 pos.y, r0, w2v_r1;
		DP4 pos.z, r0, w2v_r2;
		DP4 pos.w, r0, w2v_r3;
	"
	
	*dofinish
	"
		MOV oCol0, pos;

	#	MOV oCol0, pos;
	#	MOV oCol1, r1;
		
		END
	"
}
