!!ARBfp1.0

# specular.fp
#
# per-pixel specular lighting

ATTRIB iPrC = fragment.color.primary;# input primary color
ATTRIB iTC0 = fragment.texcoord[0];  # normal (N)
ATTRIB iTC1 = fragment.texcoord[1];  # light vector (L)

OUTPUT oPrC = result.color;          # output color

TEMP N, L, H, NdotL, NdotH;
ALIAS diffuse = NdotL;
ALIAS specular = NdotH;

DP3 N.w, iTC0, iTC0;                 # normalize normal
RSQ N.w, N.w;
MUL N, iTC0, N.w;

DP3 L.w, iTC1, iTC1;                 # normalize light vec
RSQ L.w, L.w;
MUL L, iTC1, L.w;

ADD H, L, {0, 0, 1};                 # half-angle vector
DP3 H.w, H, H;                       # normalize it
RSQ H.w, H.w;
MUL H, H, H.w;

DP3 NdotL, N, L;                     # N . L
MAX NdotL, NdotL, 0.0;               # max(N . L, 0)
MUL diffuse, iPrC, NdotL;            # diffuse color

DP3 NdotH, N, H;                     # N . H
MAX NdotH, NdotH, 0.0;               # max(N . H, 0)
POW specular, NdotH.x, 128.0.x;      # NdotH^128

ADD oPrC.rgb, diffuse, specular;     # diffuse + specular
MOV oPrC.a, iPrC.a;                  # preserve alpha

END
