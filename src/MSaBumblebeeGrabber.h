#pragma once

#ifdef USE_PTGREY

//=============================================================================
// PGR Includes
//=============================================================================
#include "triclops.h"
#include "pgrflycapture.h"
#include "pgrflycapturestereo.h"
#include "pnmutils.h"
#include "ofMain.h"
#include "ofxOpenCv.h"

class BumblebeeGrabber {
public:
	struct {
		bool initWithBumblebee;
		bool enabled;
		bool useDisparity;

		int minDisparity;
		int maxDisparity;
		int stereoMask;

		bool doEdge;
		int edgeMask;

		bool doSurfaceValidation;
		int surfaceValidationSize;
		int surfaceDifference;

		bool doUniquenessValidation;
		float uniquenessThreshold;

		bool doTextureValidation;
		float textureThreshold;

		bool doSubPixel;
		bool doSubPixelValidation;

		int minDisparityMap;
		int maxDisparityMap;

		bool doInvert;
		bool doThreshold;
		int threshold;
	} settings;


	BumblebeeGrabber();
	~BumblebeeGrabber();

	void setup();
	void update();
	void draw(float x, float y, float w, float h);

	ofxCvGrayscaleImage		disparityImage;
	ofxCvGrayscaleImage		realImage;

protected:
	TriclopsContext			_triclops;
	TriclopsImage			_disparityImage;
	TriclopsImage			_refImage;
	TriclopsInput			_triclopsInput;

	FlyCaptureContext		_flycapture;
	FlyCaptureImage			_flycaptureImage;
	FlyCaptureInfoEx		_pInfo;
	FlyCapturePixelFormat   _pixelFormat;

	TriclopsError     _te;
	FlyCaptureError   _fe;

	int _iMaxRows;
	int _iMaxCols;

	vector<unsigned char>_rowIntMono;

	bool bInited;

	template<typename T>
	bool handleError(string desc, T& error) {
		if( error != FLYCAPTURE_OK ) {
			printf("*** Flycapture Error '%s' at line %d :\n\t%s\n", flycaptureErrorToString( error ), __LINE__, desc );
			settings.enabled = false;
			return false;
		}
		return true;
	}

};

#endif