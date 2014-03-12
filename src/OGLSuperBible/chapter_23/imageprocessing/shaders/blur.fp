!!ARBfp1.0

# blur.fp
#
# blur (low-pass) 3x3 kernel

ATTRIB iTC0 = fragment.texcoord[0];  # input texcoord

OUTPUT oPrC = result.color;          # output color

TEMP tc0, tc1, tc2, tc3, tc4, tc5, tc6, tc7, tc8;

ADD tc0, iTC0, program.local[0];
ADD tc1, iTC0, program.local[1];
ADD tc2, iTC0, program.local[2];
ADD tc3, iTC0, program.local[3];
ADD tc4, iTC0, program.local[4];
ADD tc5, iTC0, program.local[5];
ADD tc6, iTC0, program.local[6];
ADD tc7, iTC0, program.local[7];
ADD tc8, iTC0, program.local[8];
TEX tc0, tc0, texture[0], 2D;
TEX tc1, tc1, texture[0], 2D;
TEX tc2, tc2, texture[0], 2D;
TEX tc3, tc3, texture[0], 2D;
TEX tc4, tc4, texture[0], 2D;
TEX tc5, tc5, texture[0], 2D;
TEX tc6, tc6, texture[0], 2D;
TEX tc7, tc7, texture[0], 2D;
TEX tc8, tc8, texture[0], 2D;

#   1 2 1
#   2 1 2   / 13
#   1 2 1

ADD tc0, tc0, tc2;
ADD tc2, tc4, tc6;
ADD tc0, tc0, tc2;
ADD tc0, tc0, tc8;

ADD tc1, tc1, tc3;
ADD tc3, tc5, tc7;
ADD tc1, tc1, tc3;

MAD tc0, tc1, 2.0, tc0;

MUL oPrC, tc0, 0.076923; # 1/13

END
