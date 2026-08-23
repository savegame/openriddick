!!ARBfp1.0
OPTION ARB_precision_hint_fastest;

OUTPUT oCol = result.color;

# Textures
#   texture[0]											# Diffuse
#   texture[1]											# Depth

# Texture coordinates
ATTRIB tc0 = fragment.texcoord[0];						# Diffuse texcoords
ATTRIB tc1 = fragment.texcoord[1];						# Generated coords for screen space calc.

# Predefines
PARAM c0 = program.env[0];								# { 1/ScrW, 1/ScrH, (ScrW-1)/ScrTexW, (ScrH-1)/ScrTexH }
PARAM c1 = program.env[1];								# { FrontPlane, BackPlane, Back - Front, Back * Front }

# Program parameters
PARAM p1 = program.env[2];								# { 1/MaxFogDist, _, _, _ }

# Constants
PARAM const_val0 = { 0, 0.5, 1, 2 };					# Used (xyzw)
PARAM const_val1 = { 0.99609380937181768, 0.0038909914428586628, 0.000015199185323666652, 0};	# Used (xyzw)

# Registers
TEMP t0;
TEMP t2;
TEMP t3;

TEMP r0;
TEMP r1;

TEMP r2;
TEMP r3;
TEMP r4;

TEMP r5;

#------------------------------------------------

# Calculate screen texcoord in 0-1 range
RCP r0.w, tc1.w;
MUL r0.xyz, tc1, r0.w;
@if platform_xenon
	MAD r0.xyz, r0.xyzw, c0.xyzw, const_val0.yyxx;
@else
	MAD r0, r0.xyzw, c0.xyzw, const_val0.yyxx;
@endif

TEX r4, tc0,  texture[0], 2D;		# Diffuse
TEX t0, r0,  texture[1], 2D;		# Depth mask texel

# Apply depth mask
@if platform_xenon
@else
	# Calc raw depth value
	DP4 t0, t0, const_val1;
@endif

# Convert depth buffer to z
@if platform_xenon
	SUB t0.w, const_val0.y, t0.x;
@endif
MAD r2.x, t0.a, c1.z, -c1.y;
RCP r2.y, -r2.x;
MUL r2.z, r2.y, c1.w;

# Convert fragment to z
RCP r5.y, tc1.w;
MUL r5.y, r5.y, tc1.z;
@if platform_xenon
  MAD r5.z, r5.y, c1.z, -c1.y;
  RCP r5.z, -r5.z;
  MUL r5.w, r5.z, c1.w;
@else
  MAD r5.y, r5.y, const_val0.y, const_val0.y;
  MAD r5.y, r5.y, c1.z, -c1.y;
  RCP r5.z, -r5.y;
  MAD r5.w, r5.z, c1.w, -const_val1.y;
@endif

# Calculate z difference
SUB r2.w, r2.z, r5.w;
MUL_SAT r2.w, r2.w, p1.x;
POW r2.w, r2.w, const_val0.w;
	
MUL oCol, r4, r2.w;

END
