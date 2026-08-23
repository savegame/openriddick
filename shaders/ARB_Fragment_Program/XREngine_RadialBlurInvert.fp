!!ARBfp1.0
OPTION ARB_precision_hint_fastest;

#-----------------------------------
OUTPUT oCol = result.color;

ATTRIB tc0 = fragment.texcoord[0];
ATTRIB tc1 = fragment.texcoord[1];

PARAM p0 = program.env[0];		# rU,rV,cU,cV
PARAM p1 = program.env[1];		# pU,pV,0,Power
PARAM p2 = program.env[2];		# Sampler weights 1-4
PARAM p3 = program.env[3];		# Sampler weights 5-8
PARAM p4 = program.env[4];		# Affection r,g,b, color intensity
PARAM p5 = program.env[5];		# Color scale r,g,b,1
PARAM p6 = program.env[6];		# eU,eV,normalized (eUV-cUV)
PARAM p7 = program.env[7];		# Extra parameter (ZoneMin,ZoneMax,0,Time)

PARAM c0 = { 0.125, 1, 0.5, 0 };	# Average,Invert,_,0

TEMP t0;
TEMP t1;
TEMP t2;
TEMP t3;
TEMP r0;
TEMP r1;
TEMP r2;
TEMP r3;
TEMP r4;
TEMP r5;
TEMP r6;
TEMP r7;
TEMP rScale;

# Get direction
SUB t0, p0, tc0;

# Tweak learp table depending on voice amount.
SUB t3, tc1, c0.zzww;
MUL t3, t3, p7.wwzz;

# Calculate zones
# OLD # MUL r1, t0, c0.yyww;
# OLD # DP3 r1.w, r1, r1;
# OLD # RSQ r1.z, r1.w;
# OLD # MUL rScale.w, r1.z, r1.w;

# Apply streak power
MUL t0, t0, p1.yyzz;

# Calculate fade scale zone and clamp within 0-1 range
# OLD # LRP rScale.x, rScale.w, p7.y, p7.x;
# OLD # MIN rScale.x, rScale.x, c0.y;
# OLD # MAX rScale.x, rScale.x, c0.w;

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
TEX t1, tc1, texture[1], 2D;	# Mask0
TEX t2, tc1, texture[2], 2D;	# Mask1
TEX t3, t3, texture[3], 2D;

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

# Average and invert
MUL r0.rgb, r0, c0.x;
SUB r0, c0.yyyy, r0;

# Gray scale
DP3 r1.rgb, r0, p4;
LRP r0, p4.w, r1, r0;

# Final scale
DP3 r1, c0.yyyy, p4;
SUB r0.rgb, r1, r0;

# Handle zone free area
# OLD # SUB r1.w, c0.y, p1.w;
# OLD # LRP r0, rScale.x, r0, c0.yyyy;
# OLD # ADD_SAT oCol, r0, r1.wwww;

SUB r1, c0.yyyw, p1.wwwz;
MAD r0, r0, p5, r1;

LRP r1, t3.x, t1, t2;
LRP oCol, r1.x, r0, c0.yyyy;
#MOV oCol, t2.xxxx;
#MOV oCol, t1;

END
