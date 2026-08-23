sbzfp.1.4;

/*¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯*\
	File:			Program for COR:EFBB post processing effects

	Author:			Magnus Högdahl

	Copyright:		Starbreeze AB 2004

\*____________________________________________________________________________________________*/

// ----------------------------------
// Phase 1
texld r0,t0,str;
passtc r3, t1, str;
passtc r4, t2, str;
dp3 r2.r, r0_bias_x2, r3;
dp3 r2.g, r0_bias_x2, r4;
sub r2.rg, r2, c0;
mov r2.b, zero;

// ----------------------------------
// Phase 2
texld r2, r2, str;
mov r0, r2;
