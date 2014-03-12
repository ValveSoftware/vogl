!!ARBfp1.0

# grayinvert.fp
#
# invert like a B&W negative

ATTRIB iPrC = fragment.color.primary; # input primary color

OUTPUT oPrC = result.color;           # output color

TEMP gray;

DP3 gray.r, iPrC, {0.3, 0.59, 0.11};  # R,G,B each contribute diff.
SUB oPrC.rgb, 1.0, gray.r;            # invert scalar gray value
MOV oPrC.a, 1.0;                      # init alpha to 1

END
