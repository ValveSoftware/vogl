!!ARBfp1.0

# toyball.fp
#
# Based on shader by Bill Licea-Kane

ATTRIB N = fragment.texcoord[0];
ATTRIB L = fragment.texcoord[1];
ATTRIB V = fragment.texcoord[2];       # obj-space position

OUTPUT oPrC = result.color;            # output color

PARAM myRed = {0.6, 0.0, 0.0, 1.0};
PARAM myYellow = {0.6, 0.5, 0.0, 1.0};
PARAM myBlue = {0.0, 0.3, 0.6, 1.0};

PARAM myHalfSpace0 = {0.31, 0.95, 0.0};
PARAM myHalfSpace1 = {-0.81, 0.59, 0.0};
PARAM myHalfSpace2 = {-0.81, -0.59, 0.0};
PARAM myHalfSpace3 = {0.31, -0.95, 0.0};
PARAM myHalfSpace4 = {1.0, 0.0, 0.0};

# stripe thickness, star size & ambient lighting,
# specular exponent, specular intensity
PARAM misc = {0.4, 0.2, 60.0, 0.5};

TEMP NV, NN, NL, NH, NdotL, NdotH, surfColor, distance, myInOut;
ALIAS specular = NdotH;

DP3 NV.w, V, V;                        # normalize vertex pos
RSQ NV.w, NV.w;
MUL NV, V, NV.w;

# Each flat edge of the star defines a half-space.  The interior
# of the star is any point within at least 4 out of 5 of them.
# Start with -3 so that it takes adding 4 ins to equal 1.
MOV myInOut, -3.0;

# We need to perform 5 dot products, one for each edge of 
# the star.  Perform first 4 in vector, 5th in a second
# vector along with the blue stripe.
DP3 distance.x, NV, myHalfSpace0;
DP3 distance.y, NV, myHalfSpace1;
DP3 distance.z, NV, myHalfSpace2;
DP3 distance.w, NV, myHalfSpace3;

# The half-space planes all intersect the origin.  We must
# offset them in order to give the star some size.
ADD distance, distance, misc.y;

CMP distance, distance, 0.0, 1.0;
DP4 distance, distance, 1.0;
ADD myInOut, myInOut, distance;

# set up last star edge and blue stripe
DP3 distance.x, NV, myHalfSpace4;
ADD distance.x, distance.x, misc.y;
ABS distance.y, NV.z;
SUB distance.y, distance.y, misc.x;
CMP distance, distance, 0.0, 1.0;
ADD_SAT myInOut, myInOut, distance.x;

# red star on yellow background
LRP surfColor, myInOut, myRed, myYellow;

# blue stripe down middle
LRP surfColor, distance.y, surfColor, myBlue;

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
MAD oPrC, misc.w, specular, surfColor; # 50% specular intensity

END
