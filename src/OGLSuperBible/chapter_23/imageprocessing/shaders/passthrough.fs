// passthrough.fs
//
// pass through a single texel value

uniform sampler2D sampler0;

void main(void)
{
    gl_FragColor = texture2D(sampler0, gl_TexCoord[0].st);
}
