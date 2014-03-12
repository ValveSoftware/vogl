// fogcoord.vs
//
// Generic vertex transformation,
// diffuse and specular lighting,
// per-vertex fogcoord

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

    // calculate diffuse lighting
    float NdotL = dot(N, L);
    vec4 diffuse = gl_Color * vec4(max(0.0, NdotL));

    // calculate specular lighting
    float NdotH = dot(N, H);
    vec4 specular = vec4(pow(max(0.0, NdotH), specularExp));

    // calculate fog coordinate: distance from eye
    gl_FogFragCoord = length(V);

    // sum the diffuse and specular components
    gl_FrontColor = diffuse + specular;
}
