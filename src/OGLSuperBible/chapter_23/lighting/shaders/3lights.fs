// 3lights.fs
//
// 3 specular lights

varying vec3 N, L[3];

void main(void)
{
    const float specularExp = 128.0;

    vec3 NN = normalize(N);

    // Light colors
    vec3 lightCol[3];
    lightCol[0] = vec3(1.0, 0.25, 0.25);
    lightCol[1] = vec3(0.25, 1.0, 0.25);
    lightCol[2] = vec3(0.25, 0.25, 1.0);

    gl_FragColor = vec4(0.0);

    for (int i = 0; i < 3; i++)
    {
        vec3 NL = normalize(L[i]);
        vec3 NH = normalize(NL + vec3(0.0, 0.0, 1.0));

        // Accumulate the diffuse contributions
        gl_FragColor.rgb += gl_Color.rgb * lightCol[i] * 
            max(0.0, dot(NN, NL));

        // Accumulate the specular contributions
        gl_FragColor.rgb += lightCol[i] * 
            pow(max(0.0, dot(NN, NH)), specularExp);
    }

    gl_FragColor.a = gl_Color.a;
}
