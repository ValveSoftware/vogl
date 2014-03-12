!!ARBfp1.0

# heatsig.fp
#
# map grayscale to heat signature

ATTRIB iPrC = fragment.color.primary; # input primary color

OUTPUT oPrC = result.color;           # output color

TEMP gray;

DP3 gray, iPrC, {0.3, 0.59, 0.11};    # R,G,B -> gray

TEX oPrC, gray, texture[0], 1D;       # lookup heatsig value

END
