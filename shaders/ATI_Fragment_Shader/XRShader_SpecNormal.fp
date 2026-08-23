sbzfp.1.4;

/*¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯*\
	File:			Program for CXR_Shader::RenderShading_FP14

	Author:			Magnus Högdahl

	Copyright:		Starbreeze AB 2004

	Comments:

		tv0 = Mapping coordinates
		tv1 = Tangent space light vector
		tv2 = Projection map coordinates
		tv3 = Half angle space row 0
		tv4 = Half angle space row 1
		tv5 = Half angle space row 2

		t0 = Normal
		t1 = Diffuse
		t2 = Cube normalize
		t3 = Spec power
		t4 = Projection

\*____________________________________________________________________________________________*/

// ----------------------------------
// Phase 1
passtc r1, t3, str;
passtc r3, t4, str;
passtc r5, t5, str;
texld r0, t0, str;	// Load normal
texld r2, t1, str;	// Load normalized light vector

dp3 r1.r, r0_bias_x2, r1;// Transform normal to half angle space for (N*H)^k look-up
dp3 r1.g, r0_bias_x2, r3;
dp3 r1.b, r0_bias_x2, r5;
mov r3.rgb, r0_bias_x2;	// Texcoord for normal renormalization
mov r4.b, r0.a;		// Store specular for next phase
//mov r3.rgb, r0;	// Texcoord for normal renormalization

// ----------------------------------
// Phase 2

texld r1, t0, str;    	// Read diffuse
texld r2, r3, str;	// Read normal
//passtc r2, r3, str;

texld r3, r1, str;	// Read specular power
passtc r0, r2, str;	// Normalized light vector
passtc r5, t1, str;	// Light vector
passtc r4, r4, str;	// Pass proj map

dp3 r5_sat.rgb, r5, r5;	// |LightVec|^2

dp3 r2_sat.rgb, r2_bias_x2, r0_bias_x2;// Normal * Lightvector
mul r5.a, r5_comp.r, r5_comp.r;// Square

mul r2.rgb, r2, c0;	// Diff * DiffColor

mul r2_x2.rgb, r2, r1;	// Diff * DiffTex
mul r2.a, r3.r, r4.b;	// SpecPower * SpecTex

mad r0_x4.rgb, r2.a, c1, r2;// Result = Diff + Spec * SpecColor

mul r0.rgb, r0, r5.a;	// Result *= (Attn * ProjMap)
mov r0.a, r1.a;


 
