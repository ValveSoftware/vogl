// stretch.vs
//
// Generic vertex transformation,
// followed by squash/stretch

uniform vec3 lightPos0;
uniform vec3 squashStretch;

void main(void)
{
    // normal MVP transform, followed by squash/stretch
    vec4 stretchedCoord = gl_Vertex;
    stretchedCoord.xyz *= squashStretch;
    gl_Position = gl_ModelViewProjectionMatrix * stretchedCoord;

    vec3 stretchedNormal = gl_Normal;
    stretchedNormal *= squashStretch;
    vec3 N = normalize(gl_NormalMatrix * stretchedNormal);
    vec4 V = gl_ModelViewMatrix * stretchedCoord;
    vec3 L = normalize(lightPos0 - V.xyz);
    vec3 H = normalize(L + vec3(0.0, 0.0, 1.0));

    // put diffuse lighting result in primary color
    float NdotL = dot(N, L);
    gl_FrontColor = gl_Color * vec4(max(0.0, NdotL));

    // copy (N.H)*8-7 into texcoord
    float NdotH = max(0.0, dot(N, H) * 8.0 - 7.0);
    gl_TexCoord[0] = vec4(NdotH, 0.0, 0.0, 1.0);
}
