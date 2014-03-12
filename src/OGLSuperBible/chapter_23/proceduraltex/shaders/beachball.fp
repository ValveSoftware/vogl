!!ARBfp1.0

# beachball.fp
#
# Longitudinal stripes, end caps

ATTRIB N = fragment.texcoord[0];
ATTRIB L = fragment.texcoord[1];
ATTRIB V = fragment.texcoord[2];       # obj-space position

OUTPUT oPrC = result.color;            # output color

PARAM myRed = {1.0, 0.0, 0.0, 1.0};
PARAM myYellow = {1.0, 1.0, 0.0, 1.0};
PARAM myGreen = {0.0, 1.0, 0.0, 1.0};
PARAM myBlue = {0.0, 0.0, 1.0, 1.0};
PARAM myWhite = {1.0, 1.0, 1.0, 1.0};
PARAM myBlack = {0.0, 0.0, 0.0, 1.0};

PARAM northHalfSpace = {0.0, 0.0, 1.0};
PARAM northeastHalfSpace = {0.707, 0.0, 0.707};
PARAM northwestHalfSpace = {-0.707, 0.0, 0.707};

# cap size minus one, ambient lighting, 
# specular exponent, specular intensity
PARAM misc = {-0.97, 0.2, 60.0, 0.75};

TEMP NV, NN, NL, NH, NdotL, NdotH, surfColor, distance, mirror;
ALIAS specular = NdotH;
ALIAS redColor = NV;

DP3 NV.w, V, V;                        # normalize vertex pos
RSQ NV.w, NV.w;
MUL NV, V, NV.w;

# Mirror half of ball across X and Z axes
CMP mirror, NV.x, -1.0, 1.0;
MUL NV.xz, NV, mirror;

# Check for north/south, east/west, 
# northeast/southwest, northwest/southeast
DP3 distance.x, NV, northHalfSpace;
DP3 distance.y, NV, northeastHalfSpace;
DP3 distance.z, NV, northwestHalfSpace;

# setup for white caps on top and bottom
ABS distance.w, NV.y;
ADD distance.w, distance.w, misc.x;

CMP distance, distance, 0.0, 1.0;

# red, green, red+green=yellow, and blue stripes
LRP redColor, distance.x, myRed, myBlack;
MAD distance.z, -distance.y, distance.z, distance.y;
LRP surfColor, distance.z, myGreen, myBlack;
ADD surfColor, surfColor, redColor;
SUB distance.y, 1.0, distance.y;
LRP surfColor, distance.y, myBlue, surfColor;

# white caps on top and bottom
LRP surfColor, distance.w, myWhite, surfColor;

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
