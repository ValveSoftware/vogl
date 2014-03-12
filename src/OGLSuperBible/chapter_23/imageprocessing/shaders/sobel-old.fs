// sobel.fs
//
// Sobel edge detection

uniform sampler2D sampler0;
uniform vec2 tc_offset[9];

void main(void)
{
    vec4 sample00, sample01, sample02;
    vec4 sample10, sample11, sample12;
    vec4 sample20, sample21, sample22;

    vec2 tc00 = gl_TexCoord[0].st + tc_offset[0];
    vec2 tc01 = gl_TexCoord[0].st + tc_offset[1];
    vec2 tc02 = gl_TexCoord[0].st + tc_offset[2];
    vec2 tc10 = gl_TexCoord[0].st + tc_offset[3];
    vec2 tc11 = gl_TexCoord[0].st + tc_offset[4];
    vec2 tc12 = gl_TexCoord[0].st + tc_offset[5];
    vec2 tc20 = gl_TexCoord[0].st + tc_offset[6];
    vec2 tc21 = gl_TexCoord[0].st + tc_offset[7];
    vec2 tc22 = gl_TexCoord[0].st + tc_offset[8];
    sample00 = texture2D(sampler0, tc00);
    sample01 = texture2D(sampler0, tc01);
    sample02 = texture2D(sampler0, tc02);
    sample10 = texture2D(sampler0, tc10);
    sample11 = texture2D(sampler0, tc11);
    sample12 = texture2D(sampler0, tc12);
    sample20 = texture2D(sampler0, tc20);
    sample21 = texture2D(sampler0, tc21);
    sample22 = texture2D(sampler0, tc22);

//    -1 -2 -1       1 0 -1 
// H = 0  0  0   V = 2 0 -2
//     1  2  1       1 0 -1
//
// result = sqrt(H^2 + V^2)

    vec4 horizEdge = sample02 + (2.0*sample12) + sample22 -
                     (sample00 + (2.0*sample10) + sample20);

    vec4 vertEdge = sample00 + (2.0*sample01) + sample02 -
                    (sample20 + (2.0*sample21) + sample22);

    gl_FragColor.rgb = sqrt((horizEdge.rgb * horizEdge.rgb) + 
                            (vertEdge.rgb * vertEdge.rgb));
    gl_FragColor.a = 1.0;
}
