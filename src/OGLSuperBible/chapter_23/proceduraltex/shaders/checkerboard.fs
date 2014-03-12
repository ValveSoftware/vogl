// checkerboard.fs
//
// 3D solid checker grid

varying vec3 V; // object-space position
varying vec3 N; // eye-space normal
varying vec3 L; // eye-space light vector

const vec3 onColor = vec3(1.0, 1.0, 1.0);
const vec3 offColor = vec3(0.0, 0.0, 0.0);

const float ambientLighting = 0.2;
const float specularExp = 60.0;
const float specularIntensity = 0.75;

const int numSquaresPerSide = 8;


void main (void)
{
    // Normalize vectors
    vec3 NN = normalize(N);
    vec3 NL = normalize(L);
    vec3 NV = normalize(V);
    vec3 NH = normalize(NL + vec3(0.0, 0.0, 1.0));

    // Map -1,1 to 0,numSquaresPerSide
    vec3 onOrOff = ((NV + 1.0) * float(numSquaresPerSide)) / 2.0;
    // mod 2 >= 1
    onOrOff = step(1.0, mod(onOrOff, 2.0));
    // 3-way xor
    onOrOff.x = step(0.5, 
        mod(onOrOff.x + onOrOff.y + onOrOff.z, 2.0));

    // checkerboard grid
    vec3 surfColor = mix(offColor, onColor, onOrOff.x);

    // calculate diffuse lighting + 20% ambient
    surfColor *= (ambientLighting + vec3(max(0.0, dot(NN, NL))));

    // calculate specular lighting w/ 75% intensity
    surfColor += (specularIntensity * 
        vec3(pow(max(0.0, dot(NN, NH)), specularExp)));

    gl_FragColor = vec4(surfColor, 1.0);
}