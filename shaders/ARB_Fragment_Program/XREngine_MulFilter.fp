!!ARBfp1.0
OPTION ARB_precision_hint_fastest;

#-----------------------------------
OUTPUT oCol = result.color;

ATTRIB tc0 = fragment.texcoord[0];

PARAM p0 = program.env[0];		# min,max,cU,cV
PARAM p1 = program.env[1];		# r,g,b,Original Color

PARAM c0 = { 0, 1, 0, 0 };	# _,1,0,_

TEMP t0;
TEMP r0;
TEMP rScale;

# Calculate length
SUB r0, p0.zwzw, tc0;
MOV r0.z, c0.z;
DP3 r0.w, r0, r0;
RSQ r0.z, r0.w;
MUL rScale.w, r0.z, r0.w;

# Calculate fade scale zone and clamp within 0-1 range
LRP rScale.x, rScale.w, p0.y, p0.x;
MIN rScale.x, rScale.x, c0.y;
MAX rScale.x, rScale.x, c0.z;

LRP r0, rScale.x, c0.yyyy, p1;
LRP oCol, p1.a, r0, c0.yyyy;

END
