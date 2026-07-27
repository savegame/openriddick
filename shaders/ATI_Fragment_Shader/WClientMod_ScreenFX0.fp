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

dp3 r2.rgb, r0, c0;
mul r2.rgb, r2, c1;
mad r0.rgb, r0, c2, r2;
//lrp r0.rgb, c3.a_comp, r0, c3;
add r0.rgb, r0, r1;



