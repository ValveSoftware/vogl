!!ARBfp1.0

# diffuse.fp
#
# per-pixel diffuse lighting

ATTRIB iPrC = fragment.color.primary;# input primary color
ATTRIB iTC0 = fragment.texcoord[0];  # normal (N)
ATTRIB iTC1 = fragment.texcoord[1];  # light vector (L)

OUTPUT oPrC = result.color;          # output color

TEMP N, L, NdotL;

DP3 N.w, iTC0, iTC0;                 # normalize normal
RSQ N.w, N.w;
MUL N, iTC0, N.w;

DP3 L.w, iTC1, iTC1;                 # normalize light vec
RSQ L.w, L.w;
MUL L, iTC1, L.w;

DP3 NdotL, N, L;                     # N . L
MAX NdotL, NdotL, 0.0;               # max(N . L, 0)
MUL oPrC.rgb, iPrC, NdotL;           # diffuse color
MOV oPrC.a, iPrC.a;                  # preserve alpha

END
