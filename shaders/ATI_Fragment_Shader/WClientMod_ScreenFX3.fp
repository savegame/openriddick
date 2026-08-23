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

add r0.rgb, r0, r1;
mul r0.rgb, r0, c0;
mov r0.a, one;


