// grayscale.fs
//
// convert RGB to grayscale

void main(void)
{
    // Convert to grayscale
    float gray = dot(gl_Color.rgb, vec3(0.3, 0.59, 0.11));

    // replicate grayscale to RGB components
    gl_FragColor = vec4(gray, gray, gray, 1.0);
}
