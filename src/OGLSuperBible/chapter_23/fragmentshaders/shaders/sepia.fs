// sepia.fs
//
// convert RGB to sepia tone

void main(void)
{
    // Convert RGB to grayscale
    float gray = dot(gl_Color.rgb, vec3(0.3, 0.59, 0.11));

    // convert grayscale to sepia
    gl_FragColor = vec4(gray * vec3(1.2, 1.0, 0.8), 1.0);
}
