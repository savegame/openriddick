!!ARBfp1.0

#-----------------------------------------
OUTPUT oCol = result.color;

ATTRIB v0 = fragment.color;
ATTRIB tc0 = fragment.texcoord[0];

TEMP tex0;
#-----------------------------------------

TEX tex0, tc0, texture[0], 2D;
MAD tex0.rgb, tex0.a, 16, -7.5;	# (a-0.5)*16 + 0.5 = 16a - 8 + 0.5 = 16a - 7.5
MUL oCol, tex0, v0;
END
