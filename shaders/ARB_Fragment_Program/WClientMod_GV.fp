!!ARBfp1.0
OPTION ARB_precision_hint_fastest;

#-----------------------------------
OUTPUT oCol = result.color;

ATTRIB tc0 = fragment.texcoord[0];
ATTRIB tc1 = fragment.texcoord[1];
ATTRIB tc2 = fragment.texcoord[2];

PARAM const_val = { 0.5, 2.0, 0.0, 1.0 };
PARAM const_val2 = { 0.7071067, 0.45, 1.818181, 1.5 };

PARAM c0 = program.env[0];

TEMP t0;

TEMP r0;
TEMP r1;
TEMP r2;

#-----------------------------------
# Perturbation

SUB r0, tc0, const_val.x;
MUL r0, r0, const_val.y;
MOV r0.z, const_val.z;
DP3 r1.a, r0, r0;
RSQ r2.a, r1.a;
RCP r1.a, r2.a;
MUL r0, r0, r2.a;
MUL r1.a, r1.a, const_val2.x;
SUB r1.a, r1.a, const_val2.y;
MAX r1.a, r1.a, const_val.z;
MUL r1.a, r1.a, const_val2.z;
MUL r1.a, r1.a, r1.a;
MUL r1, r0, -r1.a;
MUL r1, r1, const_val2.w;
MOV r1.z, const_val.w;
DP3 r0.x, r1, tc1;
DP3 r0.y, r1, tc2;
MOV r0.w, const_val.w;

TEX t0, r0, texture[0], 2D;
MUL_SAT r0, t0, c0;
MOV oCol, r0;

END