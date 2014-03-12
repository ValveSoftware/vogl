!!ARBfp1.0

# grayscale.fp
#
# convert RGB to grayscale

ATTRIB iPrC = fragment.color.primary; # input primary color

OUTPUT oPrC = result.color;           # output color

DP3 oPrC.rgb, iPrC, {0.3, 0.59, 0.11};# R,G,B each contribute diff.
MOV oPrC.a, 1.0;                      # init alpha to 1

END
