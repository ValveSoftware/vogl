// fog.vs
//
// Generic vertex transformation,
// diffuse and specular lighting,
// per-vertex exponential fog

uniform vec3 lightPos0;
uniform float density;

void main(void)
{
    // normal MVP transform
    gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;

    vec3 N = normalize(gl_NormalMatrix * gl_Normal);
    vec4 V = gl_ModelViewMatrix * gl_Vertex;
    vec3 L = normalize(lightPos0 - V.xyz);
    vec3 H = normalize(L + vec3(0.0, 0.0, 1.0));
    const float specularExp = 128.0;

    // calculate diffuse lighting
    float NdotL = dot(N, L);
    vec4 diffuse = gl_Color * vec4(max(0.0, NdotL));

    // calculate specular lighting
    float NdotH = dot(N, H);
    vec4 specular = vec4(pow(max(0.0, NdotH), specularExp));

    // calculate 2nd order exponential fog factor
    const float e = 2.71828;
    float fogFactor = (density * length(V));
    fogFactor *= fogFactor;
    fogFactor = clamp(pow(e, -fogFactor), 0.0, 1.0);

    // sum the diffuse and specular components, then
    // blend with the fog color based on fog factor
    const vec4 fogColor = vec4(0.5, 0.8, 0.5, 1.0);
    gl_FrontColor = mix(fogColor, clamp(diffuse + specular, 0.0, 1.0), 
                        fogFactor);
}
