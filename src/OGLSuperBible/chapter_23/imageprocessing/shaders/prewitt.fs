// prewitt.fs
//
// Prewitt edge detection

uniform sampler2D sampler0;
uniform vec2 tc_offset[9];

void main(void)
{
    vec4 sample[9];

    for (int i = 0; i < 9; i++)
    {
        sample[i] = texture2D(sampler0, 
                              gl_TexCoord[0].st + tc_offset[i]);
    }

//    -1 -1 -1       1 0 -1 
// H = 0  0  0   V = 1 0 -1
//     1  1  1       1 0 -1
//
// result = sqrt(H^2 + V^2)

    vec4 horizEdge = sample[2] + sample[5] + sample[8] -
                     (sample[0] + sample[3] + sample[6]);

    vec4 vertEdge = sample[0] + sample[1] + sample[2] -
                    (sample[6] + sample[7] + sample[8]);

    gl_FragColor.rgb = sqrt((horizEdge.rgb * horizEdge.rgb) + 
                            (vertEdge.rgb * vertEdge.rgb));
    gl_FragColor.a = 1.0;
}
