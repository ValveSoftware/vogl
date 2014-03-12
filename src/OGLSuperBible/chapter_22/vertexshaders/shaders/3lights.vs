// 3lights.vs
//
// Generic vertex transformation,
// 3 colored lights

uniform vec3 lightPos0;
uniform vec3 lightPos1;
uniform vec3 lightPos2;

varying vec4 gl_TexCoord[4];

void main(void)
{
    // normal MVP transform
    gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;

    vec3 N = normalize(gl_NormalMatrix * gl_Normal);
    vec4 V = gl_ModelViewMatrix * gl_Vertex;

    // Light colors
    vec4 lightCol[3];
    lightCol[0] = vec4(1.0, 0.25, 0.25, 1.0);
    lightCol[1] = vec4(0.25, 1.0, 0.25, 1.0);
    lightCol[2] = vec4(0.25, 0.25, 1.0, 1.0);

    // Light vectors
    vec3 L[3], H[3];
    L[0] = normalize(lightPos0 - V.xyz);
    L[1] = normalize(lightPos1 - V.xyz);
    L[2] = normalize(lightPos2 - V.xyz);

    gl_FrontColor = vec4(0.0);

    for (int i = 0; i < 3; i++)
    {
        // Half-angles
        H[i] = normalize(L[i] + vec3(0.0, 0.0, 1.0));

        // Accumulate the diffuse contributions
        gl_FrontColor += gl_Color * lightCol[i] * 
                         vec4(max(0.0, dot(N, L[i])));

        // Put N.H specular coefficients into texcoords
        gl_TexCoord[1+i] = vec4(max(0.0, dot(N, H[i]) * 8.0 - 7.0), 
                                0.0, 0.0, 1.0);
    }
}
