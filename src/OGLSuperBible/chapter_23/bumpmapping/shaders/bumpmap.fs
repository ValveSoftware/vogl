// bumpmap.fs
//
// per-pixel bumpmapped diffuse lighting

uniform sampler2D sampler0;
varying vec3 L;

void main(void)
{
    vec3 N = texture2D(sampler0, gl_TexCoord[0].xy).rgb;
    // map texel from [0,1] to [-1,1]
    N *= 2.0;
    N -= 1.0;

    // calculate diffuse lighting
    float NdotL = dot(N, L);
    gl_FragColor = gl_Color * vec4(max(0.0, NdotL));
}
