/*¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯*\
	File:			Program for rendering non-deferred decals
					
	Author:			Anders Ekermo
					
	Copyright:		Starbreeze AB 2007

	History:

	Comments:

		-----------------------------------------
		 Texture0 = Normal map			// Default = 1, 0.5, 0.5
		 Texture1 = Diffuse Map			// Default = 1,1,1,1
		 Texture2 = Specular Map		// Default = 1,1,1,1
		 
		 Texture 3..5 = Projection Map

		-----------------------------------------
		 TexCoord0 = Mapping tex coord
		 TexCoord1 = World-space Pixel Position (WSPOS)
		 TexCoord4 = Interpolated tangent space eye vector (IPTSEV)
		 TexCoord5 = Tangentspace-2-world transform, row 0
		 TexCoord6 = Tangentspace-2-world transform, row 1
		 TexCoord7 = Tangentspace-2-world transform, row 2

		-----------------------------------------
\*____________________________________________________________________________________________*/

*flags
{
	*trimesh	0x0001
	*light0         0x0002
	*light1         0x0004
	*light2         0x0008
	*proj0          0x0010
	*proj1          0x0020
	*proj2          0x0040
}

*generate
{
        *permute trimesh
        {
                *gen 0

                *gen light0
                *gen light0+light1
                *gen light0+light1+light2

                *gen light0+proj0
                *gen light0+light1+proj0
                *gen light0+light1+light2+proj0

                *gen light0+light1+proj0+proj1
                *gen light0+light1+light2+proj0+proj1

                *gen light0+light1+light2+proj0+proj1+proj2
        }
}


*program
{
	*header 
	"!!ARBfp1.0
	"
	
	*declarations
	"
		OUTPUT oCol = result.color;

		ATTRIB MappingTexCoord = fragment.texcoord[0];
		ATTRIB WSPos = fragment.texcoord[1];
		ATTRIB IPTSEV = fragment.texcoord[4];
		ATTRIB TS2W_Mat_0 = fragment.texcoord[5];
		ATTRIB TS2W_Mat_1 = fragment.texcoord[6];
		ATTRIB TS2W_Mat_2 = fragment.texcoord[7];

		PARAM const_val = { 0.5, 1.0, 2.0, 4.0 };
		PARAM const_val2 = { 0, 0.25, 4, 0.03215 };
		
		PARAM LFWSLV = program.env[0];		# { X, Y, Z, 0 }
                PARAM LightColor = program.env[1];	# { R, G, B, 0 } (0-2 range)
		PARAM SpecularColor = program.env[2];	# { R, G, B, SpecPower } (0-2 range)
		PARAM LFM0Texel = program.env[3];
		PARAM LFM1Texel = program.env[4];
		PARAM LFM2Texel = program.env[5];
		PARAM LFM3Texel = program.env[6];
		PARAM LFM4Texel = program.env[7];
		PARAM LFM5Texel = program.env[8];

		TEMP DiffuseTexel;
		TEMP SpecularTexelOrg;
		TEMP SpecularTexel;
		TEMP NormalTexel;
		
		TEMP NormalTexelW;
		TEMP Result;
		TEMP LightRes;
		TEMP Diffuse;
		
		TEMP r0;
		TEMP r1;
		TEMP r2;
		TEMP r3;

		TEMP TSEV;		# Tangent space eye vector
		TEMP WSEV;		# World space eye vector
		TEMP WSLV;              # World space light vector
		TEMP WSH;
		TEMP v_nosat;
		TEMP RW;
	"

	*if_light0
	"
                PARAM L0Pos = program.env[9];
                PARAM L0Range = program.env[10];
                PARAM L0Diffuse = program.env[11];
                PARAM L0Specular = program.env[12];
	"

	*if_light1
	"
                PARAM L1Pos = program.env[13];
                PARAM L1Range = program.env[14];
                PARAM L1Diffuse = program.env[15];
                PARAM L1Specular = program.env[16];
	"

	*if_light2
	"
                PARAM L2Pos = program.env[17];
                PARAM L2Range = program.env[18];
                PARAM L2Diffuse = program.env[19];
                PARAM L2Specular = program.env[20];
	"
	
        //Nasty thing since defines aren't dependent on flags and param positions change when we stuff in more lights
	*if_proj0
	{
	        *ifnot_light1
      	        "
      	              PARAM P0row0 = program.env[13];
      	              PARAM P0row1 = program.env[14];
      	              PARAM P0row2 = program.env[15];
      	        "

      	        *if_light1
      	        {
      	              *ifnot_light2
      	              "
                            PARAM P0row0 = program.env[17];
      	                    PARAM P0row1 = program.env[18];
      	                    PARAM P0row2 = program.env[19];
      	              "
      	              
      	              *if_light2
      	              "
                            PARAM P0row0 = program.env[21];
      	                    PARAM P0row1 = program.env[22];
      	                    PARAM P0row2 = program.env[23];
      	              "
      	        }
        }

	*if_proj1
	{
	        *ifnot_light2
  	        "
  	              PARAM P1row0 = program.env[20];
  	              PARAM P1row1 = program.env[21];
  	              PARAM P1row2 = program.env[22];
  	        "

	        *if_light2
  	        "
  	              PARAM P1row0 = program.env[24];
  	              PARAM P1row1 = program.env[25];
  	              PARAM P1row2 = program.env[26];
  	        "
	}

	*if_proj2
	"
	        PARAM P2row0 = program.env[27];
	        PARAM P2row1 = program.env[28];
	        PARAM P2row2 = program.env[29];
	"

        //*really* nasty trimesh param thing
	*if_trimesh
	{
                *ifnot_light0
                "
                      PARAM tmCenter = program.env[9];
                      PARAM tmNormal = program.env[10];
                      PARAM tmTanU = program.env[11];
                      PARAM tmTanV = program.env[12];
                "

                *if_light0
                {
                      *ifnot_light1
                      {
                            *ifnot_proj0
                            "
                                  PARAM tmCenter = program.env[13];
                                  PARAM tmNormal = program.env[14];
                                  PARAM tmTanU = program.env[15];
                                  PARAM tmTanV = program.env[16];
                            "

                            *if_proj0
                            "
                                  PARAM tmCenter = program.env[16];
                                  PARAM tmNormal = program.env[17];
                                  PARAM tmTanU = program.env[18];
                                  PARAM tmTanV = program.env[19];
                            "
                      }

                      *if_light1
                      {
                            *ifnot_light2
                            {
                                  *ifnot_proj0
                                  "
                                        PARAM tmCenter = program.env[17];
                                        PARAM tmNormal = program.env[18];
                                        PARAM tmTanU = program.env[19];
                                        PARAM tmTanV = program.env[20];
                                  "

                                  *if_proj0
                                  {
                                        *ifnot_proj1
                                        "
                                              PARAM tmCenter = program.env[20];
                                              PARAM tmNormal = program.env[21];
                                              PARAM tmTanU = program.env[22];
                                              PARAM tmTanV = program.env[23];
                                        "

                                        *if_proj1
                                        "
                                              PARAM tmCenter = program.env[23];
                                              PARAM tmNormal = program.env[24];
                                              PARAM tmTanU = program.env[25];
                                              PARAM tmTanV = program.env[26];
                                        "
                                  }
                            }

                            *if_light2
                            {
                                  *ifnot_proj0
                                  "
                                        PARAM tmCenter = program.env[21];
                                        PARAM tmNormal = program.env[22];
                                        PARAM tmTanU = program.env[23];
                                        PARAM tmTanV = program.env[24];
                                  "

                                  *if_proj0
                                  {
                                        *ifnot_proj1
                                        "
                                              PARAM tmCenter = program.env[24];
                                              PARAM tmNormal = program.env[25];
                                              PARAM tmTanU = program.env[26];
                                              PARAM tmTanV = program.env[27];
                                        "

                                        *if_proj1
                                        {
                                              *ifnot_proj2
                                              "
                                                    PARAM tmCenter = program.env[27];
                                                    PARAM tmNormal = program.env[28];
                                                    PARAM tmTanU = program.env[29];
                                                    PARAM tmTanV = program.env[30];
                                              "

                                              *if_proj2
                                              "
                                                    PARAM tmCenter = program.env[30];
                                                    PARAM tmNormal = program.env[31];
                                                    PARAM tmTanU = program.env[32];
                                                    PARAM tmTanV = program.env[33];
                                              "
                                        }
                                  }
                            }
                      }
                }
	}

	*ifnot_trimesh
	"
                # Fetch textures
		@if dynmip
			TEXDYN NormalTexel, MappingTexCoord, texture[0], 2D;
			TEXDYN DiffuseTexel, MappingTexCoord, texture[1], 2D;
			TEXDYN SpecularTexelOrg, MappingTexCoord, texture[2], 2D;
		@else
			TEX NormalTexel, MappingTexCoord, texture[0], 2D;
			TEX DiffuseTexel, MappingTexCoord, texture[1], 2D;
			TEX SpecularTexelOrg, MappingTexCoord, texture[2], 2D;
		@endif
	"
	
	*if_trimesh
	"
	        # Plane/sphere projection
	        SUB r0, WSPos, tmCenter;
	        DP3 r1.x, r0, tmTanU;
	        DP3 r1.y, r0, tmTanV;

                 # Alternative normal projection. Looks funky, but works better in corners
	      #  DP3 r2, r0, tmNormal;
              #  ABS r2, r2;
	      #  SWZ r0, r1, x,y,0,0;
              #  @if support_normalize
              #  NRM r0, r0;
              #  @else
	      #  DP3 r0.w, r0,r0;
	      #  RSQ r0.w, r0.w;
	      #  MUL r0, r0,r0.w;
	      #  @endif
	      #  MAD r1.xy, r2, r0, r1;

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

                # Fetch textures
		@if dynmip
			TEXDYN NormalTexel, r1, texture[0], 2D;
			TEXDYN DiffuseTexel, r1, texture[1], 2D;
			TEXDYN SpecularTexelOrg, r1, texture[2], 2D;
		@else
			TEX NormalTexel, r1, texture[0], 2D;
			TEX DiffuseTexel, r1, texture[1], 2D;
			TEX SpecularTexelOrg, r1, texture[2], 2D;
		@endif
	"

	*do1
	"
		#-----------------------------------------
		# Reconstruct normal from g,b components
		MOV r0, NormalTexel.b;
                MAD NormalTexel.rgba, NormalTexel, const_val.b, -const_val.g;# Bias and scale the normalmap texel (only rgb) (from 0->1, -1->1)
		SWZ NormalTexel.rgb, NormalTexel, 0, a, g, 0;
		DP3 NormalTexel.r, NormalTexel, NormalTexel;	# r = g^2 + b^2
		SUB_SAT NormalTexel.r, const_val.y, NormalTexel.r;	# r = 1 - r;
		RSQ NormalTexel.r, NormalTexel.r;		# r = 1/sqrt(r)
		RCP NormalTexel.r, NormalTexel.r;		# r = 1/r = sqrt(1 - g^2 - b^2)
		MOV NormalTexel.a, r0;
	"
	
	//Rotate normal into worldspace
        *ifnot_trimesh
        "
		DP3 NormalTexelW.r, NormalTexel, TS2W_Mat_0;	# Transform reflection vector to world space
		DP3 NormalTexelW.g, NormalTexel, TS2W_Mat_1;
		DP3 NormalTexelW.b, NormalTexel, TS2W_Mat_2;
        "
        
        *if_trimesh
        "
		MUL NormalTexelW, NormalTexel.z, tmTanU;
		MAD NormalTexelW, NormalTexel.y, tmTanV, NormalTexelW;
		MAD NormalTexelW, NormalTexel.x, tmNormal, NormalTexelW;
        "

        *do2
        "
                #-----------------------------------------
		# Diffuse
		MOV_SAT r0.rgb, NormalTexelW;
		MOV_SAT r1.rgb, -NormalTexelW;
		MUL Result.rgb, r1.r, LFM0Texel;
		MAD Result.rgb, r1.g, LFM2Texel, Result;
		MAD Result.rgb, r1.b, LFM4Texel, Result;
		MAD Result.rgb, r0.r, LFM1Texel, Result;
		MAD Result.rgb, r0.g, LFM3Texel, Result;
		MAD Result.rgb, r0.b, LFM5Texel, Result;
		MOV Diffuse.rgb, Result;
		MUL r1.rgb, LightColor, DiffuseTexel;		# Multiply diffusemap with light color
		MUL Result.rgb, Result, r1;
		
		#-----------------------------------------
		# Normalize TSEV
		@if support_normalize
		NRM TSEV.xyz, IPTSEV.xyz;
		@else
		DP3 TSEV.a, IPTSEV, IPTSEV;
		RSQ TSEV.a, TSEV.a;
		MUL TSEV.xyz, IPTSEV, TSEV.a;
		@endif
        "

        *ifnot_trimesh
        "
		DP3 WSEV.r, TSEV, TS2W_Mat_0;			# Transform eye vector to world space
		DP3 WSEV.g, TSEV, TS2W_Mat_1;
		DP3 WSEV.b, TSEV, TS2W_Mat_2;
        "
        
        *if_trimesh
        "
		MUL WSEV, TSEV.z, tmTanU;
		MAD WSEV, TSEV.y, tmTanV, WSEV;
		MAD WSEV, TSEV.x, tmNormal, WSEV;
        "

        *do3
        "
		#-----------------------------------------
		# Calc world space eye-reflection vector
		DP3 v_nosat.a, WSEV, NormalTexelW;
		ADD r0.a, v_nosat.a, v_nosat.a;
		MAD RW.xyz, NormalTexelW, r0.a, -WSEV;

		#-----------------------------------------
		# Project RW on base axes and sum intensity for specular
		MOV_SAT r0.rgb, RW;
		MOV_SAT r1.rgb, -RW;
		MUL r2.rgb, r1.r, LFM0Texel;
		MAD r2.rgb, r1.g, LFM2Texel, r2;
		MAD r2.rgb, r1.b, LFM4Texel, r2;
		MAD r2.rgb, r0.r, LFM1Texel, r2;
		MAD r2.rgb, r0.g, LFM3Texel, r2;
		MAD r2.rgb, r0.b, LFM5Texel, r2;
		MUL r2.rgb, r2, const_val.z;

		#-----------------------------------------
		# Specular + EnvMap
		DP3_SAT r0.a, RW, -LFWSLV;
		MUL SpecularTexel.a, SpecularTexelOrg.a, SpecularColor.a;
                MAX SpecularTexel.a, SpecularTexel.a, const_val.y;
		MOV SpecularTexel.xyz, SpecularTexelOrg;
		POW r1.a, r0.a, SpecularTexel.a;
		MUL r1.rgb, SpecularColor, r1.a;
		MUL r1.rgb, r1, r2;
		MAD_SAT Result.rgb, SpecularTexel, r1, Result;	# Mul specular with specular color, add to final fragment
        "

        *if_light0
        {
                *dolight0
                "
                        #-----------------------------------------
                        # Calc WSLV & Attenuation
                        SUB WSLV, L0Pos, WSPos;
                        DP3 r1.w, WSLV, WSLV;
                        @if support_normalize
                        NRM WSLV.xyz, WSLV.xyz;
                        @else
                        RSQ WSLV.a, r1.w;
                        MUL WSLV.xyz, WSLV, WSLV.a;
                        @endif
                        MUL_SAT r1.w, r1.w, L0Range.z;
                        ADD r1.w, const_val.g, -r1.w;
                        MUL r1.w, r1.w, r1.w;

                        #-----------------------------------------
                        # Calc halfangle vector
                        ADD WSH.xyz, WSEV, WSLV;
                        @if support_normalize
                        NRM WSH.xyz, WSH.xyz;
                        @else
                        DP3 WSH.a, WSH, WSH;
                        RSQ WSH.a, WSH.a;
                        MUL WSH.xyz, WSH, WSH.a;
                        @endif

                        #-----------------------------------------
                        # Self shadowing
                        DP3 r0.a, WSLV, NormalTexelW;			# WSSurfNormal isn't normalized but that should be ok here
                        ADD r0.a, const_val2.y, r0.a;
                        MUL_SAT r0.a, r0.a, const_val2.z;
                        MUL r1.w, r1.w, r0.a;

                        #-----------------------------------------
                        # Diffuse
                        MUL r2, L0Diffuse, DiffuseTexel;		# Multiply diffusemap with light color
                        DP3_SAT r0.rgb, NormalTexelW, WSLV;
                        MUL LightRes.rgb, r0, r2;			# Diffuse dotprod * Diffuse color

                        #-----------------------------------------
                        # Specular
                        MUL SpecularTexel, SpecularTexelOrg, L0Specular;
                        MAX SpecularTexel.a, SpecularTexel.a, const_val.y;
                        DP3_SAT r1.x, NormalTexelW, WSH;
                        POW r1.x, r1.x, SpecularTexel.a;
                        MAD_SAT LightRes.rgb, SpecularTexel, r1.x, LightRes;
                "

                *ifnot_proj0
                "
                        MAD_SAT Result.rgb, LightRes, r1.w, Result;
                "

                *if_proj0
                "
                        DP4 r0.x, WSPos, P0row0;
                        DP4 r0.y, WSPos, P0row1;
                        DP4 r0.z, WSPos, P0row2;

				        TEX r0, r0, texture[3], CUBE;
                        MUL r1, r0, r1.w;                           # Projection multiplied by attenuation
                        MAD_SAT Result.rgb, LightRes, r1, Result;
                "
        }
        
        *if_light1
        {
                *dolight1
                "
                        #-----------------------------------------
                        # Calc WSLV & Attenuation
                        SUB WSLV, L1Pos, WSPos;
                        DP3 r1.w, WSLV, WSLV;
                        @if support_normalize
                        NRM WSLV.xyz, WSLV.xyz;
                        @else
                        RSQ WSLV.a, r1.w;
                        MUL WSLV.xyz, WSLV, WSLV.a;
                        @endif
                        MUL_SAT r1.w, r1.w, L1Range.z;
                        ADD r1.w, const_val.g, -r1.w;
                        MUL r1.w, r1.w, r1.w;
        
                        #-----------------------------------------
                        # Calc halfangle vector
                        ADD WSH.xyz, WSEV, WSLV;
                        @if support_normalize
                        NRM WSH.xyz, WSH.xyz;
                        @else
                        DP3 WSH.a, WSH, WSH;
                        RSQ WSH.a, WSH.a;
                        MUL WSH.xyz, WSH, WSH.a;
                        @endif
        
                        #-----------------------------------------
                        # Self shadowing
                        DP3 r0.a, WSLV, NormalTexelW;			# WSSurfNormal isn't normalized but that should be ok here
                        ADD r0.a, const_val2.y, r0.a;
                        MUL_SAT r0.a, r0.a, const_val2.z;
                        MUL r1.w, r1.w, r0.a;

                        #-----------------------------------------
                        # Diffuse
                        MUL r2, L1Diffuse, DiffuseTexel;		# Multiply diffusemap with light color
                        DP3_SAT r0.rgb, NormalTexelW, WSLV;
                        MUL LightRes.rgb, r0, r2;			# Diffuse dotprod * Diffuse color
        
                        #-----------------------------------------
                        # Specular
                        MUL SpecularTexel, SpecularTexelOrg, L1Specular;
                        MAX SpecularTexel.a, SpecularTexel.a, const_val.y;
                        DP3_SAT r1.x, NormalTexelW, WSH;
                        POW r1.x, r1.x, SpecularTexel.a;
                        MAD_SAT LightRes.rgb, SpecularTexel, r1.x, LightRes;
                "
                
                *ifnot_proj1
                "
                        MAD_SAT Result.rgb, LightRes, r1.w, Result;
                "
                
                *if_proj1
                "
                        DP4 r0.x, WSPos, P1row0;
                        DP4 r0.y, WSPos, P1row1;
                        DP4 r0.z, WSPos, P1row2;

                        TEX r0, r0, texture[4], CUBE;
                        MUL r1, r0, r1.w;                           # Projection multiplied by attenuation
                        MAD_SAT Result.rgb, LightRes, r1, Result;
                "
        }
        
        *if_light2
        {
                *dolight2
                "
                        #-----------------------------------------
                        # Calc WSLV & Attenuation
                        SUB WSLV, L2Pos, WSPos;
                        DP3 r1.w, WSLV, WSLV;
                        @if support_normalize
                        NRM WSLV.xyz, WSLV.xyz;
                        @else
                        RSQ WSLV.a, r1.w;
                        MUL WSLV.xyz, WSLV, WSLV.a;
                        @endif
                        MUL_SAT r1.w, r1.w, L2Range.z;
                        ADD r1.w, const_val.g, -r1.w;
                        MUL r1.w, r1.w, r1.w;
        
                        #-----------------------------------------
                        # Calc halfangle vector
                        ADD WSH.xyz, WSEV, WSLV;
                        @if support_normalize
                        NRM WSH.xyz, WSH.xyz;
                        @else
                        DP3 WSH.a, WSH, WSH;
                        RSQ WSH.a, WSH.a;
                        MUL WSH.xyz, WSH, WSH.a;
                        @endif
        
                        #-----------------------------------------
                        # Self shadowing
                        DP3 r0.a, WSLV, NormalTexelW;			# WSSurfNormal isn't normalized but that should be ok here
                        ADD r0.a, const_val2.y, r0.a;
                        MUL_SAT r0.a, r0.a, const_val2.z;
                        MUL r1.w, r1.w, r0.a;

                        #-----------------------------------------
                        # Diffuse
                        MUL r2, L2Diffuse, DiffuseTexel;		# Multiply diffusemap with light color
                        DP3_SAT r0.rgb, NormalTexelW, WSLV;
                        MUL LightRes.rgb, r0, r2;			# Diffuse dotprod * Diffuse color

                        #-----------------------------------------
                        # Specular
                        MUL SpecularTexel, SpecularTexelOrg, L2Specular;
                        MAX SpecularTexel.a, SpecularTexel.a, const_val.y;
                        DP3_SAT r1.x, NormalTexelW, WSH;
                        POW r1.x, r1.x, SpecularTexel.a;
                        MAD_SAT LightRes.rgb, SpecularTexel, r1.x, LightRes;
                "
                
                *ifnot_proj2
                "
                        MAD_SAT Result.rgb, LightRes, r1.w, Result;
                "
                
                *if_proj2
                "
                        DP4 r0.x, WSPos, P2row0;
                        DP4 r0.y, WSPos, P2row1;
                        DP4 r0.z, WSPos, P2row2;

                        TEX r0, r0, texture[5], CUBE;
                        MUL r1, r0, r1.w;                           # Projection multiplied by attenuation
                        MAD_SAT Result.rgb, LightRes, r1, Result;
                "
        }
        
        *output
        "
		# Alpha from diffuse texture
		MOV Result.a, DiffuseTexel.a;

		# Write result
		MOV oCol, Result;

		END
	"
}
