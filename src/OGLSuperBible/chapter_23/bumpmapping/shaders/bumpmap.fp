!!ARBfp1.0

# specular.fp
#
# per-pixel specular lighting

ATTRIB iPrC = fragment.color.primary;# input primary color
ATTRIB iTC0 = fragment.texcoord[0];  # input texcoord 0
ATTRIB iTC1 = fragment.texcoord[1];  # input texcoord 1

OUTPUT oPrC = result.color;          # output color

TEMP N, NdotL;

TEX N, iTC0, texture[0], 2D;         # lookup normal
MAD N, N, 2.0, -1.0;                 # map [0,1] to [-1,1]

DP3 NdotL, N, iTC1;                  # N . L
MAX NdotL, NdotL, 0.0;               # max(N . L, 0)
MUL oPrC, iPrC, NdotL;               # diffuse color

END
