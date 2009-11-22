/*
 * Filename      : trig.cpp
 * Author(s)     : Chris Thielen (chris@luethy.net)
 * Date Created  : Unknown (2006?)
 * Last Modified : Saturday, January 5, 2008
 * Purpose       : 
 * Notes         :
 */

#include "includes.h"
#include <math.h> // not in includes.h because of conflicts with Log
#include "Utilities/trig.h"

Trig *Trig::pInstance = 0;

Trig::Trig( void ) {
	int i;

	convdr = M_PI / 180.;
	convrd = 180. / M_PI;

	for( i = 0; i < 360; i++ ) {
		cosTable[i] = cos( (double)i * convdr );
		sinTable[i] = sin( (double)i * convdr );
		tanTable[i] = tan( (double)i * convdr );
	}
}

Trig *Trig::Instance( void ) {

	if( pInstance == 0 ) {
		pInstance = new Trig;
	}

	return( pInstance );
}

double Trig::DegToRad( int i ) {
	return( i * convdr );
}

double Trig::DegToRad( double i ) {
	return( i * convdr );
}

int Trig::RadToDeg( double i ) {
	return( (int)(i * convrd) );
}

double Trig::GetCos( int ang ) {
	return( cosTable[ang] );
}

double Trig::GetCos( double ang ) {
	return( cos( ang ) );
}

double Trig::GetSin( int ang ) {
	return( sinTable[ang] );
}

double Trig::GetSin( double ang ) {
	return( sin(ang) );
}

// rotates point (x, y) about point (ax, ay) and sets nx, ny to new point
void Trig::RotatePoint( float x, float y, float ax, float ay, float *nx, float *ny, float ang ) {
	float theta = atan2( y - ay, x - ax );
	float dist = sqrt( ((x - ax)*(x-ax)) + ((y - ay)*(y-ay)) );
	float ntheta = theta + ang;

	*nx = ax + (dist * cos( ntheta ) );
	*ny = ay - (dist * sin( ntheta ) );
}

// Turns an arbitrary angle into an angle between -180 and 180 degrees
float normalizeAngle(float angle){
	while( angle < -180. ) angle += 360.;
	while( angle > 180. ) angle -= 360.;
	return angle;
}
