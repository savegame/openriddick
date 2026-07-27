sbzfp.1.4;

/*¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯*\
	File:			Bump mapped environment mapping (GenEnv operator)

	Author:			Magnus Högdahl

	Copyright:		Starbreeze AB 2004

	Comments:		It's almost funny how complicated a texm3x3vspec
				operation becomes on a 8500...

\*____________________________________________________________________________________________*/


// ----------------------------------
// Phase 1

texld r0, t0, str;
passtc r1, t1, str;
passtc r2, t2, str;
passtc r3, t3, str;
passtc r4, t4, str;

dp3 r5.r, r0_bias_x2, r1;
dp3 r5.g, r0_bias_x2, r2;
dp3 r5.b, r0_bias_x2, r3;
dp3 r3.r, r5, r4;
dp3 r0.g, r5, r5;
mul r3_x2.rgb, r5, r3.r;
mul r4.rgb, r4, r0.g;
sub r3.rgb, r3, r4;

// ----------------------------------
// Phase 2

texld r3, r3, str;
mul r0.rgb, r3, v0;
mov r0.a, v0.a;


