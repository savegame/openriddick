!!ARBfp1.0


#-----------------------------------------
OUTPUT oCol = result.color;

ATTRIB v0 = fragment.color;
ATTRIB tc0 = fragment.texcoord[0];

TEMP tex0;
# PARAM white = {1.0, 1.0, 1.0, 1.0};
#-----------------------------------------

TXP tex0, tc0, texture[0], 2D;
MUL oCol, tex0, v0;

END
