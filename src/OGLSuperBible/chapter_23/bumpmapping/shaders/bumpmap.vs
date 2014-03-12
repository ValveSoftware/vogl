// bumpmap.vs
//
// setup interpolants for bumpmap lighting

uniform vec3 lightPos0;
varying vec3 L;

void main(void)
{
    // normal MVP transform
    gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;

    // bumpmap texcoords
    gl_TexCoord[0].xy = gl_MultiTexCoord0.xy;

    // put tangent-space light vector into texcoord 1
    vec3 Ltan;
    vec3 Lobj = lightPos0 - gl_Vertex.xyz;
    Ltan.x = dot(Lobj, gl_MultiTexCoord1.xyz); // tangent
    Ltan.y = dot(Lobj, gl_MultiTexCoord2.xyz); // binormal
    Ltan.z = dot(Lobj, gl_MultiTexCoord3.xyz); // normal
    L = normalize(Ltan);

    // Copy the primary color
    gl_FrontColor = gl_Color;
}
