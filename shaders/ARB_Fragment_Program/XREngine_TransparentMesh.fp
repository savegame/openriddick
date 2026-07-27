!!ARBfp1.0
OPTION ARB_precision_hint_fastest;

#-----------------------------------
OUTPUT oCol = result.color;

ATTRIB tc0 = fragment.texcoord[0];

PARAM p0 = program.env[0];		# t0Scale,t1Scale,_,_
TEMP  t0;						# Before mesh
TEMP  t1;						# After mesh
TEMP  r0;

# Fetch color
TEX t0, tc0, texture[0], 2D;
#TEX t1, tc0, texture[1], 2D;

# Blend togheter textures
#MUL r0, t0, p0.x;
#MAD oCol, t1, p0.y, r0;
MOV t0.a, p0.x;
MOV oCol, t0;

END
