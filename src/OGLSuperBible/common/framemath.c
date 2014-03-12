// FrameMath.c
// Prototypes in gltools.h
// OpenGL SuperBible
// Richard S. Wright Jr.
// Frame of reference manipulation routines

#include "gltools.h"
#include <string.h>

// Initialize a frame of reference. 
// Uses default OpenGL viewing position and orientation
void gltInitFrame(GLTFrame *pFrame)
    {
    pFrame->vLocation[0] = 0.0f;
    pFrame->vLocation[1] = 0.0f;
    pFrame->vLocation[2] = 0.0f;
    
    pFrame->vUp[0] = 0.0f;
    pFrame->vUp[1] = 1.0f;
    pFrame->vUp[2] = 0.0f;
    
    pFrame->vForward[0] = 0.0f;
    pFrame->vForward[1] = 0.0f;
    pFrame->vForward[2] = -1.0f;
    }
    
///////////////////////////////////////////////////////////////////
// Derives a 4x4 transformation matrix from a frame of reference
void gltGetMatrixFromFrame(GLTFrame *pFrame, GLTMatrix mMatrix)
    {
    GLTVector3 vXAxis;       // Derived X Axis
    
    // Calculate X Axis
    gltVectorCrossProduct(pFrame->vUp, pFrame->vForward, vXAxis);
    
    // Just populate the matrix
    // X column vector
    memcpy(mMatrix, vXAxis, sizeof(GLTVector3));
    mMatrix[3] = 0.0f;
    
    // y column vector
    memcpy(mMatrix+4, pFrame->vUp, sizeof(GLTVector3));
    mMatrix[7] = 0.0f;
    
    // z column vector
    memcpy(mMatrix+8, pFrame->vForward, sizeof(GLTVector3));
    mMatrix[11] = 0.0f;
    
    // Translation/Location vector
    memcpy(mMatrix+12, pFrame->vLocation, sizeof(GLTVector3));
    mMatrix[15] = 1.0f;
    }
        
////////////////////////////////////////////////////////////////////
// Apply an actors transform given it's frame of reference
void gltApplyActorTransform(GLTFrame *pFrame)
    {
    GLTMatrix mTransform;
    gltGetMatrixFromFrame(pFrame, mTransform);
    glMultMatrixf(mTransform);
    }
    
//////////////////////////////////////////////////////////////////
// Apply a camera transform given a frame of reference. This is
// pretty much just an alternate implementation of gluLookAt using
// floats instead of doubles and having the forward vector specified
// instead of a point out in front of me. 
void gltApplyCameraTransform(GLTFrame *pCamera)
    {
    GLTMatrix mMatrix;
    GLTVector3 vAxisX;
    GLTVector3 zFlipped;
    
    zFlipped[0] = -pCamera->vForward[0];
    zFlipped[1] = -pCamera->vForward[1];
    zFlipped[2] = -pCamera->vForward[2];
    
    // Derive X vector
    gltVectorCrossProduct(pCamera->vUp, zFlipped, vAxisX);
    
    // Populate matrix, note this is just the rotation and is transposed
    mMatrix[0] = vAxisX[0];
    mMatrix[4] = vAxisX[1];
    mMatrix[8] = vAxisX[2];
    mMatrix[12] = 0.0f;
    
    mMatrix[1] = pCamera->vUp[0];
    mMatrix[5] = pCamera->vUp[1];
    mMatrix[9] = pCamera->vUp[2];
    mMatrix[13] = 0.0f;
    
    mMatrix[2] = zFlipped[0];
    mMatrix[6] = zFlipped[1];
    mMatrix[10] = zFlipped[2];
    mMatrix[14] = 0.0f;
    
    mMatrix[3] = 0.0f;
    mMatrix[7] = 0.0f;
    mMatrix[11] = 0.0f;
    mMatrix[15] = 1.0f;
    
    // Do the rotation first
    glMultMatrixf(mMatrix);
    
    // Now, translate backwards
    glTranslatef(-pCamera->vLocation[0], -pCamera->vLocation[1], -pCamera->vLocation[2]);
    }
     
/////////////////////////////////////////////////////////
// March a frame of reference forward. This simply moves
// the location forward along the forward vector.
void gltMoveFrameForward(GLTFrame *pFrame, GLfloat fStep)
    {
    pFrame->vLocation[0] += pFrame->vForward[0] * fStep;
    pFrame->vLocation[1] += pFrame->vForward[1] * fStep;
    pFrame->vLocation[2] += pFrame->vForward[2] * fStep;
    }
    
/////////////////////////////////////////////////////////
// Move a frame of reference up it's local Y axis
void gltMoveFrameUp(GLTFrame *pFrame, GLfloat fStep)
    {
    pFrame->vLocation[0] += pFrame->vUp[0] * fStep;
    pFrame->vLocation[1] += pFrame->vUp[1] * fStep;
    pFrame->vLocation[2] += pFrame->vUp[2] * fStep;
    }

////////////////////////////////////////////////////////
// Move a frame of reference along it's local X axis
void gltMoveFrameRight(GLTFrame *pFrame, GLfloat fStep)
    {
    GLTVector3 vCross;
    
    gltVectorCrossProduct(pFrame->vUp, pFrame->vForward, vCross);
    pFrame->vLocation[0] += vCross[0] * fStep;
    pFrame->vLocation[1] += vCross[1] * fStep;
    pFrame->vLocation[2] += vCross[2] * fStep;
    }
        
/////////////////////////////////////////////////////////
// Translate a frame in world coordinates
void gltTranslateFrameWorld(GLTFrame *pFrame, GLfloat x, GLfloat y, GLfloat z)
    { pFrame->vLocation[0] += x; pFrame->vLocation[1] += y; pFrame->vLocation[2] += z; }
    
/////////////////////////////////////////////////////////
// Translate a frame in local coordinates
void gltTranslateFrameLocal(GLTFrame *pFrame, GLfloat x, GLfloat y, GLfloat z)
    { 
    gltMoveFrameRight(pFrame, x);
    gltMoveFrameUp(pFrame, y);
    gltMoveFrameForward(pFrame, z);
    }

/////////////////////////////////////////////////////////
// Rotate a frame around it's local Y axis
void gltRotateFrameLocalY(GLTFrame *pFrame, GLfloat fAngle)
    {
    GLTMatrix mRotation;
    GLTVector3 vNewForward;
    
    gltRotationMatrix((float)gltDegToRad(fAngle), 0.0f, 1.0f, 0.0f, mRotation);
    gltRotationMatrix(fAngle, pFrame->vUp[0], pFrame->vUp[1], pFrame->vUp[2], mRotation);

    gltRotateVector(pFrame->vForward, mRotation, vNewForward);
    memcpy(pFrame->vForward, vNewForward, sizeof(GLTVector3));
    }

//////////////////////////////////////////////////////////
//  Rotate a frame around it's local X axis
void gltRotateFrameLocalX(GLTFrame *pFrame, GLfloat fAngle)
    {
    GLTMatrix mRotation;
    GLTVector3 vCross;
    
    gltVectorCrossProduct(vCross, pFrame->vUp, pFrame->vForward);
    gltRotationMatrix(fAngle, vCross[0], vCross[1], vCross[2], mRotation);

    GLTVector3 vNewVect;
    // Inline 3x3 matrix multiply for rotation only
    vNewVect[0] = mRotation[0] * pFrame->vForward[0] + mRotation[4] * pFrame->vForward[1] + mRotation[8] *  pFrame->vForward[2];	
    vNewVect[1] = mRotation[1] * pFrame->vForward[0] + mRotation[5] * pFrame->vForward[1] + mRotation[9] *  pFrame->vForward[2];	
    vNewVect[2] = mRotation[2] * pFrame->vForward[0] + mRotation[6] * pFrame->vForward[1] + mRotation[10] * pFrame->vForward[2];	
    memcpy(pFrame->vForward, vNewVect, sizeof(GLfloat)*3);

    // Update pointing up vector
    vNewVect[0] = mRotation[0] * pFrame->vUp[0] + mRotation[4] * pFrame->vUp[1] + mRotation[8] *  pFrame->vUp[2];	
    vNewVect[1] = mRotation[1] * pFrame->vUp[0] + mRotation[5] * pFrame->vUp[1] + mRotation[9] *  pFrame->vUp[2];	
    vNewVect[2] = mRotation[2] * pFrame->vUp[0] + mRotation[6] * pFrame->vUp[1] + mRotation[10] * pFrame->vUp[2];	
    memcpy(pFrame->vUp, vNewVect, sizeof(GLfloat) * 3);
    }

/////////////////////////////////////////////////////////////
// Rotate a frame around it's local Z axis
void gltRotateFrameLocalZ(GLTFrame *pFrame, GLfloat fAngle)
    {
    GLTMatrix mRotation;

    // Only the up vector needs to be rotated
    gltRotationMatrix(fAngle, pFrame->vForward[0], pFrame->vForward[1], pFrame->vForward[2], mRotation);

    GLTVector3 vNewVect;
    vNewVect[0] = mRotation[0] * pFrame->vUp[0] + mRotation[4] * pFrame->vUp[1] + mRotation[8] *  pFrame->vUp[2];	
    vNewVect[1] = mRotation[1] * pFrame->vUp[0] + mRotation[5] * pFrame->vUp[1] + mRotation[9] *  pFrame->vUp[2];	
    vNewVect[2] = mRotation[2] * pFrame->vUp[0] + mRotation[6] * pFrame->vUp[1] + mRotation[10] * pFrame->vUp[2];	
    memcpy(pFrame->vUp, vNewVect, sizeof(GLfloat) * 3);
    }

