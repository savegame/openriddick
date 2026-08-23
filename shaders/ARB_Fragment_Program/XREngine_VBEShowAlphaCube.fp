!!ARBfp1.0

#-----------------------------------------
OUTPUT oCol = result.color;

ATTRIB v0 = fragment.color;
ATTRIB tc0 = fragment.texcoord[0];

TEMP tex0;
#-----------------------------------------

TEX tex0, tc0, texture[0], CUBE;
MOV tex0.rgb, tex0.a;
MUL oCol, tex0, v0;
END
