!!ARBfp1.0

# checkerboard.fp
#
# 3D solid checker grid

ATTRIB N = fragment.texcoord[0];
ATTRIB L = fragment.texcoord[1];
ATTRIB V = fragment.texcoord[2];       # obj-space position

OUTPUT oPrC = result.color;            # output color

PARAM onColor = {1.0, 1.0, 1.0, 1.0};
PARAM offColor = {0.0, 0.0, 0.0, 1.0};

# 0.25 * squares per side, ambient lighting, 
# specular exponent, specular intensity
PARAM misc = {2.0, 0.2, 60.0, 0.75};

TEMP NV, NN, NL, NH, NdotL, NdotH, surfColor, onOrOff;
ALIAS specular = NdotH;

DP3 NV.w, V, V;                        # normalize vertex pos
RSQ NV.w, NV.w;
MUL NV, V, NV.w;

# Map position from -1,1 to 0,numSquaresPerSide/2
MAD onOrOff, NV, misc.x, misc.x;
# mod2 by doubling FRC, then subtract 1 for >= 1 compare
FRC onOrOff, onOrOff;
MAD onOrOff, onOrOff, 2.0, -1.0;
CMP onOrOff, onOrOff, 0.0, 1.0;

# perform xor by adding all 3 axes' onoroff values,
# then mod2 again
DP3 onOrOff, onOrOff, 1.0;
MUL onOrOff, onOrOff, 0.5;
FRC onOrOff, onOrOff;
MAD onOrOff, onOrOff, 2.0, -1.0;
CMP onOrOff, onOrOff, 0.0, 1.0;

# checkerboard grid
LRP surfColor, onOrOff, onColor, offColor;

DP3 NN.w, N, N;                        # normalize normal
RSQ NN.w, NN.w;
MUL NN, N, NN.w;

DP3 NL.w, L, L;                        # normalize light vec
RSQ NL.w, NL.w;
MUL NL, L, NL.w;

ADD NH, NL, {0, 0, 1};                 # half-angle vector
DP3 NH.w, NH, NH;                      # normalize it
RSQ NH.w, NH.w;
MUL NH, NH, NH.w;

# diffuse lighting
DP3 NdotL, NN, NL;                     # N . L
MAX NdotL, NdotL, 0.0;                 # max(N . L, 0)
ADD NdotL, NdotL, misc.y;              # 20% ambient
MUL surfColor, surfColor, NdotL;       # factor in diffuse color

# specular lighting
DP3 NdotH, NN, NH;                     # N . H
MAX NdotH, NdotH, 0.0;                 # max(N . H, 0)
POW specular, NdotH.x, misc.z;         # NdotH^60
MAD oPrC, misc.w, specular, surfColor; # 75% specular intensity

END
