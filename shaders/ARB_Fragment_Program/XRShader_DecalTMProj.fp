!!ARBfp1.0

#-----------------------------------------
OUTPUT oCol = result.color;

ATTRIB WSPos = fragment.texcoord[1];

PARAM tmCenter = program.env[0];
PARAM tmNormal = program.env[1];
PARAM tmTanU = program.env[2];
PARAM tmTanV = program.env[3];
PARAM tmColor = program.env[4];

TEMP r0;
TEMP r1;
TEMP r2;

PARAM const_val = { 0.5, 1.0, 2.0, 4.0 };
PARAM const_val2 = {0.1,0.01,0,0.2};

#-----------------------------------------

# Plane/sphere projection
SUB r0, WSPos, tmCenter;
DP3 r1.x, r0, tmTanU;
DP3 r1.y, r0, tmTanV;

# Add normal projection to clamp around corners
DP3 r2, r0, tmNormal;
ABS r2.x, r2.x;
SUB r2.xyzw, tmNormal.w, r2.x;
KIL r2;
RCP r2.x, r2.x;
MUL r2.x, r2.x, tmNormal.w;
MUL r2.x, r2.x,r2.x;
MUL r1.xy, r1, r2.x;

# Set wallmark size
MAD r1.xy, r1, tmTanU.w, const_val.x;

SWZ r1, r1, x,y,x,y;
SUB r0, const_val.y, r1;
KIL r0;
KIL r1;

# sample texture
@if dynmip
TEXDYN r0, r1, texture[0], 2D;
@else
TEX r0, r1, texture[0], 2D;
@endif
MUL r0, r0, tmColor;
MOV oCol, r0;

END
