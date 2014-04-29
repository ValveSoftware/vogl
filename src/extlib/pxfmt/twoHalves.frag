#version 130

uniform bool passThru;
uniform bool leftHalf;

in vec3 myCoord;

void main()
{
    vec4 color = vec4(0.0f, 0.0f, 0.0f, 1.0f);

    if (passThru) {
	color = gl_Color;
    } else {
	if (leftHalf) {
	    color.rgb = myCoord;
	} else {
	    //// Simple test of assigning the value:
	    color.rgb = myCoord;

	    //// I deleted more fancy/practice ways of determining the color.
	}
    }

    gl_FragColor = color;

    return;
}
