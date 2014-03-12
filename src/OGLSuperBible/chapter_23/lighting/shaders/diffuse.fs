// diffuse.fs
//
// per-pixel diffuse lighting

varying vec3 N, L;

void main(void)
{
    // output the diffuse color
    float intensity = max(0.0, 
        dot(normalize(N), normalize(L)));

    gl_FragColor = gl_Color;
    gl_FragColor.rgb *= intensity;
}
