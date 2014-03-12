// 3lights.vs
//
// setup interpolants for 3 specular lights

uniform vec3 lightPos0;
uniform vec3 lightPos1;
uniform vec3 lightPos2;
varying vec3 N, L[3];

void main(void)
{
    // vertex MVP transform
    gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;

    vec4 V = gl_ModelViewMatrix * gl_Vertex;

    // eye-space normal
    N = gl_NormalMatrix * gl_Normal;

    // Light vectors
    L[0] = lightPos0 - V.xyz;
    L[1] = lightPos1 - V.xyz;
    L[2] = lightPos2 - V.xyz;

    // Copy the primary color
    gl_FrontColor = gl_Color;
}
