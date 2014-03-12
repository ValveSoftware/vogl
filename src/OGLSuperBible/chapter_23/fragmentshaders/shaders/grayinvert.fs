// grayinvert.fs
//
// invert like a B&W negative

void main(void)
{
    // Convert to grayscale
    float gray = dot(gl_Color.rgb, vec3(0.3, 0.59, 0.11));

    // invert
    gray = 1.0 - gray;

    // replicate grayscale to RGB components
    gl_FragColor = vec4(gray, gray, gray, 1.0);
}
