!!ARBfp1.0

# prewitt.fp
#
# Prewitt edge detection

ATTRIB iTC0 = fragment.texcoord[0];  # input texcoord

OUTPUT oPrC = result.color;          # output color

TEMP tc0, tc1, tc2, tc3, tc4, tc5, tc6, tc7, tc8;
TEMP horizEdge, vertEdge;

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

#    -1 -1 -1       1 0 -1 
# H = 0  0  0   V = 1 0 -1
#     1  1  1       1 0 -1
#
# result = sqrt(H^2 + V^2)

ADD vertEdge, tc0, tc1;
ADD vertEdge, vertEdge, tc2;
ADD vertEdge, vertEdge, -tc6;
ADD vertEdge, vertEdge, -tc7;
ADD vertEdge, vertEdge, -tc8;
MUL vertEdge, vertEdge, vertEdge;

ADD horizEdge, tc2, tc5;
ADD horizEdge, horizEdge, tc8;
ADD horizEdge, horizEdge, -tc0;
ADD horizEdge, horizEdge, -tc3;
ADD horizEdge, horizEdge, -tc6;
MUL horizEdge, horizEdge, horizEdge;

ADD horizEdge, horizEdge, vertEdge;
RSQ horizEdge.r, horizEdge.r;
RSQ horizEdge.g, horizEdge.g;
RSQ horizEdge.b, horizEdge.b;
RCP oPrC.r, horizEdge.r;
RCP oPrC.g, horizEdge.g;
RCP oPrC.b, horizEdge.b;
MOV oPrC.a, 1.0;

END
