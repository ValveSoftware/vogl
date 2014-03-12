// showbump.fs
//
// show bumpmap

uniform sampler2D sampler0;

void main(void)
{
    // show bumpmap as colors
    gl_FragColor = texture2D(sampler0, gl_TexCoord[0].xy);
}
