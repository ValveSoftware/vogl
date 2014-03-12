!!ARBfp1.0

# 3lights.fp
#
# 3 specular lights

ATTRIB iPrC = fragment.color.primary;# input primary color
ATTRIB iTC0 = fragment.texcoord[0];  # normal (N)
ATTRIB iTC1 = fragment.texcoord[1];  # light vector (L) 0
ATTRIB iTC2 = fragment.texcoord[2];  # light vector (L) 1
ATTRIB iTC3 = fragment.texcoord[3];  # light vector (L) 2

OUTPUT oPrC = result.color;          # output color

PARAM lightCol0 = { 1.0, 0.25, 0.25, 1.0 }; # light 0 color
PARAM lightCol1 = { 0.25, 1.0, 0.25, 1.0 }; # light 1 color
PARAM lightCol2 = { 0.25, 0.25, 1.0, 1.0 }; # light 2 color

TEMP N, L, H, NdotL, NdotH, finalColor;
ALIAS diffuse = NdotL;
ALIAS specular = NdotH;

DP3 N.w, iTC0, iTC0;                 # normalize normal
RSQ N.w, N.w;
MUL N, iTC0, N.w;

DP3 L.w, iTC1, iTC1;                 # normalize light vec 0
RSQ L.w, L.w;
MUL L, iTC1, L.w;

ADD H, L, {0, 0, 1};                 # half-angle vector 0
DP3 H.w, H, H;                       # normalize it
RSQ H.w, H.w;
MUL H, H, H.w;

DP3 NdotL, N, L;                     # N . L0
MAX NdotL, NdotL, 0.0;               # max(N . L, 0)
MUL diffuse, iPrC, NdotL;            # diffuse color
MUL finalColor, diffuse, lightCol0;

DP3 NdotH, N, H;                     # N . H0
MAX NdotH, NdotH, 0.0;               # max(N . H, 0)
POW specular, NdotH.x, 128.0.x;      # NdotH^128
MAD finalColor, specular, lightCol0, finalColor;

DP3 L.w, iTC2, iTC2;                 # normalize light vec 1
RSQ L.w, L.w;
MUL L, iTC2, L.w;

ADD H, L, {0, 0, 1};                 # half-angle vector 1
DP3 H.w, H, H;                       # normalize it
RSQ H.w, H.w;
MUL H, H, H.w;

DP3 NdotL, N, L;                     # N . L1
MAX NdotL, NdotL, 0.0;               # max(N . L, 0)
MUL diffuse, iPrC, NdotL;            # diffuse color
MAD finalColor, diffuse, lightCol1, finalColor;

DP3 NdotH, N, H;                     # N . H1
MAX NdotH, NdotH, 0.0;               # max(N . H, 0)
POW specular, NdotH.x, 128.0.x;      # NdotH^128
MAD finalColor, specular, lightCol1, finalColor;

DP3 L.w, iTC3, iTC3;                 # normalize light vec 2
RSQ L.w, L.w;
MUL L, iTC3, L.w;

ADD H, L, {0, 0, 1};                 # half-angle vector 2
DP3 H.w, H, H;                       # normalize it
RSQ H.w, H.w;
MUL H, H, H.w;

DP3 NdotL, N, L;                     # N . L2
MAX NdotL, NdotL, 0.0;               # max(N . L, 0)
MUL diffuse, iPrC, NdotL;            # diffuse color
MAD finalColor, diffuse, lightCol2, finalColor;

DP3 NdotH, N, H;                     # N . H2
MAX NdotH, NdotH, 0.0;               # max(N . H, 0)
POW specular, NdotH.x, 128.0.x;      # NdotH^128
MAD oPrC.rgb, specular, lightCol2, finalColor;
MOV oPrC.a, iPrC.a;                  # preserve alpha

END
