!!ARBfp1.0

# simple.fp
#
# copy primary color

ATTRIB iPrC = fragment.color.primary;# input primary color

OUTPUT oPrC = result.color;          # output color

MOV oPrC, iPrC;                      # copy primary color in to out

END
