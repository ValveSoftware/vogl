!!ARBfp1.0

# passthrough.fp
#
# pass through a single texel value

ATTRIB iTC0 = fragment.texcoord[0];  # input texcoord

OUTPUT oPrC = result.color;          # output color

TEX oPrC, iTC0, texture[0], 2D;      # lookup texel

END
