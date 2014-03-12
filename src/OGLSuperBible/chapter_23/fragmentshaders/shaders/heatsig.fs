// heatsig.fs
//
// map grayscale to heat signature

uniform sampler1D sampler0;

void main(void)
{
    // Convert to grayscale
    float gray = dot(gl_Color.rgb, vec3(0.3, 0.59, 0.11));

    // lookup heatsig value
    gl_FragColor = texture1D(sampler0, gray);
}
