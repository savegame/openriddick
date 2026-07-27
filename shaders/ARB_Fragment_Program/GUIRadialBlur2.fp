!!ARBfp1.0
OPTION ARB_precision_hint_fastest;

#-----------------------------------
OUTPUT oCol = result.color;

ATTRIB tc0 = fragment.texcoord[0];

PARAM p0 = program.env[0];
PARAM p1 = program.env[1];

#PARAM c0 = { 1.2, 2.4, 3.6, 4.8 };
#PARAM c1 = { 6.0, 7.2, 8.4, 9.6 };
#PARAM c2 = { 10.8, 12.0, 13.2, 14.4 };
#PARAM c3 = { 15.6, 16.8, 18.0, 18.2 };

PARAM c0 = { 0.5, 1.5, 2.0, 2.5 };
PARAM c1 = { 3.0, 3.5, 4.0, 4.5 };
PARAM c2 = { 5.0, 5.5, 6.0, 6.5 };
PARAM c3 = { 7.0, 7.5, 8.0, 90 };

PARAM c4 = { 0.0685, 0.4, 2, 0 };

TEMP t0;
TEMP r0;
TEMP r1;
TEMP r2;
TEMP r3;
TEMP r4;
TEMP r5;
TEMP r6;
TEMP r7;

TEMP r8;
TEMP r9;
TEMP r10;
TEMP r11;
TEMP r12;
TEMP r13;
TEMP r14;
TEMP r15;

# Get direction
SUB t0, p0, tc0;

# Normalize vector
DP3 t0.w, t0, t0;
RSQ t0.w, t0.w;
MUL t0, t0, t0.w;
MUL t0, t0, c4.y;
MUL t0, t0, p1;

# Find sample positions
MAD r0, t0, c0.x, tc0;
MAD r1, t0, c0.y, tc0;
MAD r2, t0, c0.z, tc0;
MAD r3, t0, c0.w, tc0;
MAD r4, t0, c1.x, tc0;
MAD r5, t0, c1.y, tc0;
MAD r6, t0, c1.z, tc0;
MAD r7, t0, c1.w, tc0;

MAD r8, t0, c2.x, tc0;
MAD r9, t0, c2.y, tc0;
MAD r10, t0, c2.z, tc0;
MAD r11, t0, c2.w, tc0;
MAD r12, t0, c3.x, tc0;
MAD r13, t0, c3.y, tc0;
MAD r14, t0, c3.z, tc0;
MAD r15, t0, c3.w, tc0;

# Fetch
TEX r0, r0, texture[0], 2D;
TEX r1, r1, texture[0], 2D;
TEX r2, r2, texture[0], 2D;
TEX r3, r3, texture[0], 2D;
TEX r4, r4, texture[0], 2D;
TEX r5, r5, texture[0], 2D;
TEX r6, r6, texture[0], 2D;
TEX r7, r7, texture[0], 2D;

TEX r8, r8, texture[0], 2D;
TEX r9, r9, texture[0], 2D;
TEX r10, r10, texture[0], 2D;
TEX r11, r11, texture[0], 2D;
TEX r12, r12, texture[0], 2D;
TEX r13, r13, texture[0], 2D;
TEX r14, r14, texture[0], 2D;
TEX r15, r15, texture[0], 2D;

# Fade sample points going outwards (because streak map is full intensity white)

#MUL r0, r0, 1.0;
MUL r1, r1, 0.95;
MUL r2, r2, 0.9;
MUL r3, r3, 0.85;
MUL r4, r4, 0.8;
MUL r5, r5, 0.75;
MUL r6, r6, 0.7;
MUL r7, r7, 0.65;

MUL r8, r8, 0.55;
MUL r9, r9, 0.45;
MUL r10, r10, 0.35;
MUL r11, r11, 0.25;
MUL r12, r12, 0.2;
MUL r13, r13, 0.15;
MUL r14, r14, 0.10;
MUL r15, r15, 0.05;

# Sum
MOV t0, r0;
ADD r0, r0, r1;
ADD r1, r2, r3;
ADD r2, r4, r5;
ADD r3, r6, r7;

ADD r0, r0, r1;
ADD r1, r2, r3;
ADD r0, r0, r1;

# hmm
ADD r8, r8, r9;
ADD r9, r10, r11;
ADD r10, r12, r13;
ADD r11, r14, r15;

ADD r8, r8, r9;
ADD r9, r10, r11;
ADD r10, r8, r9;
ADD r0, r0, r10;

# Average
MUL r0, r0, c4.x;
DP3 r0, r0, r0;
MAD r0, t0, r0, r0;
MUL oCol, r0, p1.w;

END
