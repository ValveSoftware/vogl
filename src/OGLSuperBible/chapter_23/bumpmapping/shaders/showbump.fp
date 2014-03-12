!!ARBfp1.0

# showbump.fp
#
# show bumpmap

ATTRIB iTC3 = fragment.texcoord[3];  # input texcoord 3

OUTPUT oPrC = result.color;          # output color

TEX oPrC, iTC3, texture[0], 2D;      # lookup bumpmap

END
