// specular.fs
//
// per-pixel specular lighting

varying vec3 N, L;

void main(void)
{
    const float specularExp = 128.0;
    vec3 NN = normalize(N);
    vec3 NL = normalize(L);
    vec3 NH = normalize(NL + vec3(0.0, 0.0, 1.0));

    // calculate diffuse lighting
    float intensity = max(0.0, dot(NN, NL));
    vec3 diffuse = gl_Color.rgb * intensity;

    // calculate specular lighting
    intensity = max(0.0, dot(NN, NH));
    vec3 specular = vec3(pow(intensity, specularExp));

    // sum the diffuse and specular components
    gl_FragColor.rgb = diffuse + specular;
    gl_FragColor.a = gl_Color.a;
}
