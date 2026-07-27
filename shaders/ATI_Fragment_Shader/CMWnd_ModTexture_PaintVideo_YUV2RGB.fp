sbzfp.1.4;

/*¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯*\
	File:			CMWnd_ModTexture_PaintVideo_YUV2RGB

	Author:			Jim Kjellin

	Copyright:		Starbreeze AB 2004

	Comments:

		tv0 = Mapping coordinates
		tv1 = Mapping coordinates

		t0 = Y component in RGB
		t1 = U component in RGB, V component in A

		c0 = Custom Y values for "proper" YUV conversion
		c1 = Custom U values for "proper" YUV conversion
		c2 = Custom V values for "proper" YUV conversion

\*____________________________________________________________________________________________*/


texld r0, t0, str;
texld r1, t1, str;


mad r2.rgb, r0, c0_x2.g, c0_neg.b;

mad r2.g, c1_neg, r1_bias.r, r2;
mad r2.b, c1_x2, r1_bias.r, r2;

mad r2.r, c2_x2, r1_bias.a, r2;
mad r2.g, c2_neg, r1_bias.a, r2;

mul r0_sat, r2, v0;
