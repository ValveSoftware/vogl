// skinning.vs
//
// Perform vertex skinning by
// blending between two MV
// matrices

uniform vec3 lightPos;
uniform mat4 mv2;
uniform mat3 mv2IT;
attribute float weight;

void main(void)
{
    // compute each vertex influence
    vec4 V1 = gl_ModelViewMatrix * gl_Vertex;
    vec4 V2 = mv2 * gl_Vertex;
    vec4 V = (V1 * weight) + (V2 * (1.0 - weight));
    gl_Position = gl_ProjectionMatrix * V;

    // compute each normal influence
    vec3 N1 = gl_NormalMatrix * gl_Normal;
    vec3 N2 = mv2IT * gl_Normal;

    vec3 N = normalize((N1 * weight) + (N2 * (1.0 - weight)));
    vec3 L = normalize(lightPos - V.xyz);
    vec3 H = normalize(L + vec3(0.0, 0.0, 1.0));

    // put diffuse lighting result in primary color
    float NdotL = dot(N, L);
    gl_FrontColor = 0.1 + gl_Color * vec4(max(0.0, NdotL));

    // copy (N.H)*8-7 into texcoord
    float NdotH = max(0.0, dot(N, H) * 8.0 - 7.0);
    gl_TexCoord[0] = vec4(NdotH, 0.0, 0.0, 1.0);
}
