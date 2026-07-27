!!ARBfp1.0


#-----------------------------------------
OUTPUT oCol = result.color;

ATTRIB tc0 = fragment.texcoord[0];
ATTRIB pixelpos = fragment.texcoord[1];

PARAM pln = program.env[0];

TEMP t0;
TEMP r0;

#-----------------------------------------

# sample texture
@if dynmip
TEXDYN t0, tc0, texture[0], 2D;
@else
TEX t0, tc0, texture[0], 2D;
@endif

# create alpha by distance
DP3 r0, pln, pixelpos;
SUB r0, r0, pln.w;
ABS r0, r0;
SUB_SAT r0, 1.0, r0;
MUL t0.a, t0.a, r0;
             
MOV oCol, t0;

END
