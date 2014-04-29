#version 130

uniform bool leftHalf;

out vec3 myCoord;

void main()
{
    gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;
    gl_ClipVertex = gl_Vertex;

    myCoord.xyz = gl_Position.xyx;
    if (leftHalf) {
	myCoord.x = (myCoord.x + 0.5f) * 2.0f;
	myCoord.z = -(myCoord.x);
    } else {
	myCoord.x = (myCoord.x - 0.5f) * 2.0f;
	myCoord.z = -(myCoord.x);
    }
    gl_FrontColor.rgb = myCoord;
}
