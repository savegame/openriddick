sbzfp.1.4;

/*¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯*\
	File:			Program for CTextureContainer_Blur

	Author:			Magnus Högdahl

	Copyright:		Starbreeze AB 2004

\*____________________________________________________________________________________________*/

texld r0, t0, str;
sub r0_sat.rgb, r0, c0;
mul r0_x4.rgb, r0, c1;
dp3 r1.rgb, r0, c2;
mul r1.rgb, r1, c4;
mad r0.rgb, r0, c3, r1;
mul r0.rgb, r0, v0;
mov r0.a, c2.a;



