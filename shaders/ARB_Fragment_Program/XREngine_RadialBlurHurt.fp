!!ARBfp1.0
OPTION ARB_precision_hint_fastest;

#-----------------------------------
OUTPUT oCol = result.color;

ATTRIB tc0 = fragment.texcoord[0];

PARAM p0 = program.env[0];		# rU,rV,cU,cV
PARAM p1 = program.env[1];		# pU,pV,0,Power
PARAM p2 = program.env[2];		# sampler weights 1-4
PARAM p3 = program.env[3];		# sampler weights 5-8
PARAM p4 = program.env[4];		# Affection r,g,b, color intensity
PARAM p5 = program.env[5];		# color scale r,g,b,1
PARAM p6 = program.env[6];		# eU,eV,normalized (eUV-cUV)
PARAM p7 = program.env[7];		# Extra parameter (_,MinConeStr,MaxConeStr,ClampMax)
PARAM p8 = program.env[8];		# Extra parameter (ZoneMin,ZoneMax,_,_)

PARAM c0 = { 0.125, 1, 0, 0 };	# Average,1,0,_

TEMP t0;
TEMP r0;
TEMP r1;
TEMP r2;
TEMP r3;
TEMP r4;
TEMP r5;
TEMP r6;
TEMP r7;
TEMP rScale;
TEMP rDir;

# Calculate length
SUB r0, p0.zwzw, tc0;
MOV r0.z, c0.z;
DP3 r0.w, r0, r0;
RSQ r0.z, r0.w;
MUL rScale.w, r0.z, r0.w;

# Calculate fade scale zone and clamp within 0-1 range
LRP rScale.x, rScale.w, p8.y, p8.x;
MIN rScale.x, rScale.x, c0.y;
MAX rScale.x, rScale.x, c0.z;

# Normalize center to pixel direction
SUB rDir, tc0, p0.zwzw;
MUL rDir, rDir, c0.yyzz;
DP3 rDir.w, rDir, rDir;
RSQ rDir.w, rDir.w;
MUL rDir.xy, rDir, rDir.w;

# Dot direction with normalized center to target
DP3 rDir.w, rDir.xyzz, p6.zwzw;

# Adjust cone width and clamp it within 0-1 range
SUB rDir.w, rDir.w, p7.y;
MUL rDir.w, rDir.w, p7.z;
MAX rDir.w, c0.z, rDir.w;
MIN rDir.w, c0.y, rDir.w;

# Make cone respect clear zone
MUL rDir.w, rScale.xxxx, rDir.wwww;
MIN rDir.w, c0.y, rDir.w;

# Multiply with wanted strength
MUL rDir.w, rDir.w, p7.w;

# Get streak direction and apply streak power
SUB t0, p0, tc0;
MUL t0, t0, p1.yyzz;
#MUL t0, t0, rScale;

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

#DP3 rScale, rScale, rScale;
MUL r0, r0, rScale.xxxx;

# Gray scale
DP3 r1.rgb, r0, p4;
LRP r0, p4.w, r1, r0;

# Final scale
MAD oCol, r0, p5, rDir.wzzz;

END