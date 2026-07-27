sbzfp.1.4;

/*¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯*\
	File:			Program for COR:EFBB post processing effects

	Author:			Magnus Högdahl

	Copyright:		Starbreeze AB 2004

\*____________________________________________________________________________________________*/

// ----------------------------------
// Phase 1
texld r0, t0, str;
texld r1, t1, str;
add r2.rgb, r0, r1;

// ----------------------------------
// Phase 2
passtc r2, r2, str;
texld r0, t2, str;
texld r1, t3, str;
add r0.rgb, r0, r1;
add r0.rgb, r0, r2;
mul r0.rgb, r0, c0;
mul r0.rgb, r0, v0;
mov r0.a, c0.a;
