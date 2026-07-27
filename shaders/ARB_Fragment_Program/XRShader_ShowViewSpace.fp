!!ARBfp1.0

#-----------------------------------------
OUTPUT oCol = result.color;

#ATTRIB v0 = fragment.color;
ATTRIB tc0 = fragment.texcoord[0];

#PARAM VSScale = { 0.00390625, 0.00390625, 0.0009765625, 0 };
PARAM VSScale = { 0.0009765625, 0.0009765625, 0.0009765625, 0 };
PARAM VSOfs = { 0.5, 0.5, 0, 1 };
#-----------------------------------------

MAD oCol, tc0, VSScale, VSOfs;
END
