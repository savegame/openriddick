!!ARBfp1.0
OPTION ARB_precision_hint_fastest;

#-----------------------------------
OUTPUT oCol = result.color;

ATTRIB tc0 = fragment.texcoord[0];

PARAM p0 = program.env[0];		# rU,rV,cU,cV
PARAM p1 = program.env[1];		# pU,pV,0,Power
PARAM p2 = program.env[2];		# Sampler weights 1-4
PARAM p3 = program.env[3];		# Sampler weights 5-8
PARAM p4 = program.env[4];		# Affection r,g,b, color intensity
PARAM p5 = program.env[5];		# Color scale r,g,b,1
PARAM p6 = program.env[6];		# eU,eV,normalized (eUV-cUV)

PARAM c0 = { 0.125, 0.5, 1, 0 };	# Average,_,_,_

TEMP t0;
TEMP r0;
TEMP r1;
TEMP r2;
TEMP r3;
TEMP r4;
TEMP r5;
TEMP r6;
TEMP r7;

# Get direction and apply streak power
SUB t0, p0, tc0;
MUL t0, t0, p1.yyzz;


# Find sample positions
MAD r0, t0, p2.x, tc0;
MAD r1, t0, p2.y, tc0;
MAD r2, t0, p2.z, tc0;
MAD r3, t0, p2.w, tc0;
MAD r4, t0, p3.x, tc0;
MAD r5, t0, p3.y, tc0;
MAD r6, t0, p3.z, tc0;
MAD r7, t0, p3.w, tc0;

# Fetch
TEX r0, r0, texture[0], 2D;
TEX r1, r1, texture[0], 2D;
TEX r2, r2, texture[0], 2D;
TEX r3, r3, texture[0], 2D;
TEX r4, r4, texture[0], 2D;
TEX r5, r5, texture[0], 2D;
TEX r6, r6, texture[0], 2D;
TEX r7, r7, texture[0], 2D;

MUL r0.rgb, r0, r0;
MUL r1.rgb, r1, r1;
MUL r2.rgb, r2, r2;
MUL r3.rgb, r3, r3;
MUL r4.rgb, r4, r4;
MUL r5.rgb, r5, r5;
MUL r6.rgb, r6, r6;
MUL r7.rgb, r7, r7;

# Sum
MOV t0.rgb, r0;
ADD r0.rgb, r0, r1;
ADD r1.rgb, r2, r3;
ADD r2.rgb, r4, r5;
ADD r3.rgb, r6, r7;
ADD r0.rgb, r0, r1;
ADD r1.rgb, r2, r3;
ADD r0.rgb, r0, r1;

# Average
MUL r0.rgb, r0, c0.x;

# Gray scale
DP3 r1.rgb, r0, p4;
LRP r0, p4.w, r1, r0;

# Final scale
MUL oCol, r0, p5;

END
