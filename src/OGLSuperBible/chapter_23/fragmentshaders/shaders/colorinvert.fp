!!ARBfp1.0

# colorinvert.fp
#
# invert like a color negative

ATTRIB iPrC = fragment.color.primary; # input primary color

OUTPUT oPrC = result.color;           # output color

SUB oPrC.rgb, 1.0, iPrC;              # invert RGB colors
MOV oPrC.a, 1.0;                      # init alpha to 1

END
