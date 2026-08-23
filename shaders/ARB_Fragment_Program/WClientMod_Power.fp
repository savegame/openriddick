!!ARBfp1.0
OPTION ARB_precision_hint_fastest;

#-----------------------------------
OUTPUT oCol = result.color;

ATTRIB tc0r = fragment.texcoord[0];

PARAM p0 = program.env[0];
PARAM p1 = program.env[1];
PARAM p2 = program.env[2];
PARAM p3 = program.env[3];

PARAM c0 = { 1, 2, 3, 4 };
PARAM c1 = { 5, 6, 7, 8 };
PARAM c2 = { 0.3, 0.5, 2, 0 };
PARAM c3 = { 0.125, 9, 0.25, 1.0 };
PARAM c4 = { 1.0, 0.875, 0.75, 0.625 };
PARAM c5 = { 0.5, 0.375, 0.25, 0.125 };

TEMP t0;
TEMP r0;
TEMP r1;
TEMP r2;
TEMP r3;
TEMP r4;
TEMP r5;
TEMP r6;
TEMP r7;

TEMP tc0;

# Quick fix for clamping issues
MAD tc0, p1.y, c2.y, tc0r;

SUB r0, c2.yyww, tc0;
DP3 r1, r0, r0;
SUB r1, r1, p0.z;
MAX r1, r1, c2.w;
MUL r1, r1, c3.w;
MIN r1, r1, c0.x;

# Get direction
SUB t0, p0, tc0;
MUL t0, t0, c2.z;
MUL t0, t0, p1;
MUL t0, t0, r1;
MUL t0, t0, 2;

# Clamp direction to uv min/max border
MUL r0, t0, c1.w;
ADD r1, r0, tc0;
MAX r2, r1, p3.xyxy;
MIN r2, r2, p3.zwzw;
SUB r3, r2, tc0;
MUL t0, r3, c3.x;

# Find sample positions
MAD r0, t0, c0.x, tc0;
MAD r1, t0, c0.y, tc0;
MAD r2, t0, c0.z, tc0;
MAD r3, t0, c0.w, tc0;
MAD r4, t0, c1.x, tc0;
MAD r5, t0, c1.y, tc0;
MAD r6, t0, c1.z, tc0;
MAD r7, t0, c1.w, tc0;

# Fetch
TEX r0, r0, texture[0], 2D;
TEX r1, r1, texture[0], 2D;
TEX r2, r2, texture[0], 2D;
TEX r3, r3, texture[0], 2D;
TEX r4, r4, texture[0], 2D;
TEX r5, r5, texture[0], 2D;
TEX r6, r6, texture[0], 2D;
TEX r7, r7, texture[0], 2D;

# Sum
MAD r0, r1, c4.y, r0;
MAD r0, r2, c4.z, r0;
MAD r0, r3, c4.w, r0;
MAD r0, r4, c5.x, r0;
MAD r0, r5, c5.y, r0;
MAD r0, r6, c5.z, r0;
MAD r0, r7, c5.w, r0;

# Average
MUL r0.rgb, r0, c2.x;

# Intensity multiplier
MUL oCol, r0, p1.w;

END
