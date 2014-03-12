// sepspec.vs
//
// Generic vertex transformation,
// diffuse and specular lighting
// interpolated separately

uniform vec3 lightPos0;

void main(void)
{
    // normal MVP transform
    gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;

    vec3 N = normalize(gl_NormalMatrix * gl_Normal);
    vec4 V = gl_ModelViewMatrix * gl_Vertex;
    vec3 L = normalize(lightPos0 - V.xyz);
    vec3 H = normalize(L + vec3(0.0, 0.0, 1.0));
    const float specularExp = 128.0;

    // put diffuse into primary color
    float NdotL = dot(N, L);
    gl_FrontColor = gl_Color * vec4(max(0.0, NdotL));

    // put specular into secondary color
    float NdotH = dot(N, H);
    gl_FrontSecondaryColor = vec4(pow(max(0.0, NdotH), specularExp));
}
