!!ARBfp1.0
OPTION ARB_precision_hint_fastest;

#-----------------------------------
#
# Generate a grey scale image from the depth buffer in clamped 0-1 range from wanted
# range values
#
#-----------------------------------

OUTPUT oCol = result.color;

# Texture coordinates
ATTRIB tc0 = fragment.texcoord[0];

# Program parameters
PARAM p0 = program.env[0];							# { BackPlane-FrontPlane, BP*FP, BP, _ }
PARAM p1 = program.env[1];							# { StartRange, Range, 1/Range, _ }

# Constants
PARAM cv0 = { 0.99609375, 0.00390625, 1.0, 0 };		# Used (xyzw)

# Registers
TEMP t0;
TEMP r0;

#------------------------------------------------

# Sample depth texture
TEX t0, tc0, texture[0], 2D;

# Calculate raw depth value
@if platform_xenon
	SUB t0.a, cv0.z, t0.r;
@else
	MUL t0.rgb, t0, cv0.xxxx;
	MAD t0.a, t0.g, cv0.y, t0.r;
@endif

MAD r0.x, t0.w, p0.x, -p0.z;
RCP r0.y, -r0.x;
MUL r0.z, r0.y, p0.y;

# Clamp inside wanted range and convert to 0-1 range
SUB r0.w, r0.z, p1.x;
MIN r0.w, r0.w, p1.y;
MAX r0.w, r0.w, cv0.w;
MUL r0.w, r0.w, p1.z;

# Move result as grey scale
MOV oCol, r0.wwww;

END

#------------------------------------------------
