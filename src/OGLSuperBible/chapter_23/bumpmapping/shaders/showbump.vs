// showbump.vs
//
// setup interpolants for bumpmap lighting

uniform vec3 lightPos0;

void main(void)
{
    // normal MVP transform
    gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;

    // bumpmap texcoords
    gl_TexCoord[0].xy = gl_MultiTexCoord0.xy;
}
