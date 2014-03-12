!!ARBfp1.0

# sepia.fp
#
# convert RGB to sepia tone

ATTRIB iPrC = fragment.color.primary; # input primary color

OUTPUT oPrC = result.color;           # output color

TEMP gray;

DP3 gray, iPrC, {0.3, 0.59, 0.11};    # convert to grayscale

MUL oPrC.rgb, gray, {1.2, 1.0, 0.8};  # convert to sepia
MOV oPrC.a, 1.0;                      # init alpha to 1

END
