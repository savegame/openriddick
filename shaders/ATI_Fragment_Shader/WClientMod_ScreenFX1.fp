sbzfp.1.4;

/*¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯*\
	File:			Program for COR:EFBB post processing effects

	Author:			Magnus Högdahl

	Copyright:		Starbreeze AB 2004

\*____________________________________________________________________________________________*/

// ----------------------------------
// Phase 1
texld r0,t0,str;
texld r1,t1,str;
mov r2, r0;
dp3 r0.rgb, r0, c0;
mul r0_x2.rgb, r0, c2;
add r0.rgb, r0, r2;
lrp r0.rgb, c3, r0, r2;
mul r1.rgb, r1, c4;
add r0.rgb, r0, r1;
//mad r0.rgb, r0, two, r2;
//add r1.rgb, t1, t2;
//add r1.rgb, r1, t3;
//mad r0, r1, c4, r0;

// ----------------------------------
// Phase 2
passtc r0, r0, str;
texld r1, t2, str;
texld r2, t3, str;

add r1.rgb, r1, r2;
mul r1.rgb, r1, c4;
add r0.rgb, r0, r1;

