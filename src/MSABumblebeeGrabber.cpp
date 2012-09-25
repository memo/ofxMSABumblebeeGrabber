#ifdef USE_PTGREY


#include "BumblebeeGrabber.h"

//=============================================================================
// Copyright � 2004 Point Grey Research, Inc. All Rights Reserved.
// 
// This software is the confidential and proprietary information of Point
// Grey Research, Inc. ("Confidential Information").  You shall not
// disclose such Confidential Information and shall use it only in
// accordance with the terms of the license agreement you entered into
// with Point Grey Research, Inc. (PGR).
// 
// PGR MAKES NO REPRESENTATIONS OR WARRANTIES ABOUT THE SUITABILITY OF THE
// SOFTWARE, EITHER EXPRESS OR IMPLIED, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
// PURPOSE, OR NON-INFRINGEMENT. PGR SHALL NOT BE LIABLE FOR ANY DAMAGES
// SUFFERED BY LICENSEE AS A RESULT OF USING, MODIFYING OR DISTRIBUTING
// THIS SOFTWARE OR ITS DERIVATIVES.
//=============================================================================
//=============================================================================
// $Id: grabstereo.cpp,v 1.9 2009/03/20 15:59:31 soowei Exp $
//=============================================================================
//=============================================================================
// grabstereo
//
// Gets input from the Digiclops/Bumblebee, and performs stereo processing
// to create a disparity image. A rectified image from the reference camera
// and the disparity image are both written out.
//
//=============================================================================

//=============================================================================
// System Includes
//=============================================================================
#include <stdio.h>
#include <stdlib.h>

#include "ofxSimpleGuiToo/ofxSimpleGuiToo.h"

//
// Macro to check, report on, and handle Triclops API error codes.
//
#define _HANDLE_TRICLOPS_ERROR( description, error )	\
{ \
	if( error != TriclopsErrorOk ) \
   { \
   printf( \
   "*** Triclops Error '%s' at line %d :\n\t%s\n", \
   triclopsErrorToString( error ), \
   __LINE__, \
   description );	\
   printf( "Press any key to exit...\n" ); \
   getchar(); \
   exit( 1 ); \
   } \
} \

//
// Macro to check, report on, and handle Flycapture API error codes.
//

#define _HANDLE_FLYCAPTURE_ERROR(desc, error)	{ if(handleError(desc, error) == false) return; }

/*
\
{ \
	if( error != FLYCAPTURE_OK ) \
   { \
   printf( \
   "*** Flycapture Error '%s' at line %d :\n\t%s\n", \
   flycaptureErrorToString( error ), \
   __LINE__, \
   description );	\
   printf( "Press any key to exit...\n" ); \
   getchar(); \
   exit( 1 ); \
   } \
} \
*/

BumblebeeGrabber::BumblebeeGrabber() {
	int _iMaxCols = 0;
	int _iMaxRows = 0;
	settings.enabled = false;
	bInited = false;
}


BumblebeeGrabber::~BumblebeeGrabber() {
	if(bInited == false) return;

	// Close the camera
	_fe = flycaptureStop( _flycapture );
	_HANDLE_FLYCAPTURE_ERROR( "flycaptureStop()", _fe );

	// Delete the image buffer
	_rowIntMono.clear();
	//   redMono = NULL;
	//   greenMono = NULL;
	//   blueMono = NULL;

	_fe = flycaptureDestroyContext( _flycapture );
	_HANDLE_FLYCAPTURE_ERROR( "flycaptureDestroyContext()", _fe );

	// Destroy the Triclops context
	_te = triclopsDestroyContext( _triclops ) ;
	_HANDLE_TRICLOPS_ERROR( "triclopsDestroyContext()", _te );
}

void BumblebeeGrabber::setup() {
	gui.saveToXML();
	gui.addPage("BumblebeeGrabber").setXMLName("settings/BumblebeeGrabber.xml");
	gui.addToggle("initWithBumblebee", settings.initWithBumblebee);
	gui.addToggle("enabled", settings.enabled);
	gui.addToggle("useDisparity", settings.useDisparity);
//	gui.addToggle("useForInput", settings.useForInput);

	gui.addSlider("minDisparity", settings.minDisparity, 0, 640);
	gui.addSlider("maxDisparity", settings.maxDisparity, 0, 640);

	gui.addSlider("stereoMask", settings.stereoMask, 1, 15).setIncrement(2);

	gui.addToggle("doEdge", settings.doEdge);
	gui.addSlider("edgeMask", settings.edgeMask, 3, 11);

	gui.addToggle("doSurfaceValidation", settings.doSurfaceValidation);
	gui.addSlider("surfaceValidationSize", settings.surfaceValidationSize, 100, 500);
	gui.addSlider("surfaceDifference", settings.surfaceDifference, 0, 128);

	gui.addToggle("doUniquenessValidation", settings.doUniquenessValidation);
	gui.addSlider("uniquenessThreshold", settings.uniquenessThreshold, 0, 10);

	gui.addToggle("doTextureValidation", settings.doTextureValidation);
	gui.addSlider("textureThreshold", settings.textureThreshold, 0, 2);

	gui.addToggle("doSubPixel", settings.doSubPixel);
	gui.addToggle("doSubPixelValidation", settings.doSubPixelValidation);

	gui.addSlider("minDisparityMap", settings.minDisparityMap, 0, 255);
	gui.addSlider("maxDisparityMap", settings.maxDisparityMap, 0, 255);

	gui.addToggle("doInvert", settings.doInvert);
	gui.addToggle("doTreshold", settings.doThreshold);
	gui.addSlider("threshold", settings.threshold, 0, 255);
	gui.loadFromXML();

	if(settings.initWithBumblebee == false) {
		settings.enabled = false;
		return;
	}

	settings.enabled = true;

//	if(settings.enabled == false) return;


	char* szCalFile;

	// Open the camera
	_fe = flycaptureCreateContext( &_flycapture );
	_HANDLE_FLYCAPTURE_ERROR( "flycaptureCreateContext()", _fe );

	// Initialize the Flycapture context
	_fe = flycaptureInitialize( _flycapture, 0 );
	_HANDLE_FLYCAPTURE_ERROR( "flycaptureInitialize()", _fe );

	// Save the camera's calibration file, and return the path 
	_fe = flycaptureGetCalibrationFileFromCamera( _flycapture, &szCalFile );
	_HANDLE_FLYCAPTURE_ERROR( "flycaptureGetCalibrationFileFromCamera()", _fe );

	// Create a Triclops context from the cameras calibration file
	_te = triclopsGetDefaultContextFromFile( &_triclops, szCalFile );
	_HANDLE_TRICLOPS_ERROR( "triclopsGetDefaultContextFromFile()", _te );

	// Get camera information
	_fe = flycaptureGetCameraInfo( _flycapture, &_pInfo );
	_HANDLE_FLYCAPTURE_ERROR( "flycatpureGetCameraInfo()", _fe );  

	if (_pInfo.CameraType == FLYCAPTURE_COLOR) {
		_pixelFormat = FLYCAPTURE_RAW16;
	} else {
		_pixelFormat = FLYCAPTURE_MONO16;
	}

	switch (_pInfo.CameraModel) {
	case FLYCAPTURE_BUMBLEBEE2:
		{
			unsigned long ulValue;
			flycaptureGetCameraRegister( _flycapture, 0x1F28, &ulValue );

			if((ulValue & 0x2)==0) {
				// Hi-res BB2
				_iMaxCols = 1024; 
				_iMaxRows = 768;   
			} else {
				// Low-res BB2
				_iMaxCols = 640;
				_iMaxRows = 480;
			}
		}
		break;

	case FLYCAPTURE_BUMBLEBEEXB3:
		_iMaxCols = 1280;
		_iMaxRows = 960;
		break;

	default:
		_te = TriclopsErrorInvalidCamera;
		_HANDLE_TRICLOPS_ERROR( "triclopsCheckCameraModel()", _te );
		break;
	}

	// Start transferring images from the camera to the computer
	_fe = flycaptureStartCustomImage(_flycapture, 3, 0, 0, _iMaxCols, _iMaxRows, 100, _pixelFormat);
	_HANDLE_FLYCAPTURE_ERROR( "flycaptureStart()", _fe );

	// Grab an image from the camera
	_fe = flycaptureGrabImage2( _flycapture, &_flycaptureImage );
	_HANDLE_FLYCAPTURE_ERROR( "flycaptureGrabImage()", _fe );

	// Extract information from the FlycaptureImage
	int imageCols = _flycaptureImage.iCols;
	int imageRows = _flycaptureImage.iRows;
	int imageRowInc = _flycaptureImage.iRowInc;
	int iSideBySideImages = _flycaptureImage.iNumImages;
	unsigned long timeStampSeconds = _flycaptureImage.timeStamp.ulSeconds;
	unsigned long timeStampMicroSeconds = _flycaptureImage.timeStamp.ulMicroSeconds;

	// Create buffers for holding the mono images
	_rowIntMono.resize(_iMaxCols * _iMaxRows * iSideBySideImages);

	ofLog(OF_LOG_VERBOSE, "BumblebeeGrabber::setup - " + ofToString(_iMaxCols) + "x" + ofToString(_iMaxRows) + ", " + ofToString(imageCols) + "x" + ofToString(imageRows));

	triclopsSetDisparityMappingOn(_triclops, true);
	triclopsSetTextureValidationMapping(_triclops, 0);
	triclopsSetUniquenessValidationMapping(_triclops, 0);
	triclopsSetSurfaceValidationMapping(_triclops, 0);

	update();


	bInited = true;
}

void BumblebeeGrabber::update() {
	if(settings.enabled == false) return;

	if(settings.maxDisparity - settings.minDisparity > 240) {
		settings.minDisparity = settings.maxDisparity-240;
		if(settings.minDisparity < 0) settings.minDisparity = 0;
	}
	triclopsSetDisparity(_triclops, settings.minDisparity, settings.maxDisparity);	//0...1024

	triclopsSetStereoMask(_triclops, settings.stereoMask);	// 1....15

	triclopsSetEdgeCorrelation(_triclops, settings.doEdge);
	if(settings.doEdge) {
		triclopsSetEdgeMask(_triclops, settings.edgeMask);	// 3...11
	}

	triclopsSetSurfaceValidation(_triclops, settings.doSurfaceValidation );
	if(settings.doSurfaceValidation) {
		triclopsSetSurfaceValidationSize(_triclops, settings.surfaceValidationSize);	// 100-500
		triclopsSetSurfaceValidationDifference(_triclops, settings.surfaceDifference);// 0..128
	}
	
	triclopsSetUniquenessValidation(_triclops, settings.doUniquenessValidation );
	if(settings.doUniquenessValidation) {
		triclopsSetUniquenessValidationThreshold(_triclops, settings.uniquenessThreshold);	// 0...10
	}

	triclopsSetTextureValidation(_triclops, settings.doTextureValidation );
	if(settings.doTextureValidation) {
		triclopsSetTextureValidationThreshold(_triclops, settings.textureThreshold);	// 0 ..2
	}

//	triclopsSetSubpixelInterpolation(_triclops, settings.doSubPixel );
//	triclopsSetStrictSubpixelValidation(_triclops, settings.doSubPixelValidation);

	triclopsSetDisparityMapping(_triclops, settings.minDisparityMap, settings.maxDisparityMap);

	// Grab an image from the camera
	_fe = flycaptureGrabImage2( _flycapture, &_flycaptureImage );
	_HANDLE_FLYCAPTURE_ERROR( "flycaptureGrabImage()", _fe );

	// Extract information from the FlycaptureImage
	int imageCols = _flycaptureImage.iCols;
	int imageRows = _flycaptureImage.iRows;
	int imageRowInc = _flycaptureImage.iRowInc;
	int iSideBySideImages = _flycaptureImage.iNumImages;
	unsigned long timeStampSeconds = _flycaptureImage.timeStamp.ulSeconds;
	unsigned long timeStampMicroSeconds = _flycaptureImage.timeStamp.ulMicroSeconds;

	// Create a temporary FlyCaptureImage for preparing the stereo image
	FlyCaptureImage tempImage;
	tempImage.pData = &_rowIntMono[0];

	// Convert the pixel interleaved raw data to row interleaved format
	_fe = flycapturePrepareStereoImage( _flycapture, _flycaptureImage, &tempImage, NULL);
	_HANDLE_FLYCAPTURE_ERROR( "flycapturePrepareStereoImage()", _fe );

	// Pointers to positions in the mono buffer that correspond to the beginning
	// of the red, green and blue sections
	unsigned char* redMono = NULL;
	unsigned char* greenMono = NULL;
	unsigned char* blueMono = NULL;

	redMono = &_rowIntMono[0];
	if (_flycaptureImage.iNumImages == 2)
	{
		greenMono = redMono + imageCols;
		blueMono = redMono + imageCols;
	}
	if (_flycaptureImage.iNumImages == 3)
	{
		greenMono = redMono + imageCols;
		blueMono = redMono + ( 2 * imageCols );
	}

	// Use the row interleaved images to build up an RGB TriclopsInput.  
	// An RGB _triclops input will contain the 3 raw images (1 from each camera).
	_te = triclopsBuildRGBTriclopsInput(
		imageCols, 
		imageRows, 
		imageRowInc,  
		timeStampSeconds, 
		timeStampMicroSeconds, 
		redMono, 
		greenMono, 
		blueMono, 
		&_triclopsInput);
	_HANDLE_TRICLOPS_ERROR( "triclopsBuildRGBTriclopsInput()", _te );

	// Rectify the images
	_te = triclopsRectify( _triclops, &_triclopsInput );
	_HANDLE_TRICLOPS_ERROR( "triclopsRectify()", _te );

	// Do stereo processing
	_te = triclopsStereo( _triclops );
	_HANDLE_TRICLOPS_ERROR( "triclopsStereo()", _te );

	// Retrieve the disparity image from the _triclops context
	_te = triclopsGetImage( _triclops, TriImg_DISPARITY, TriCam_REFERENCE, &_disparityImage );
	_HANDLE_TRICLOPS_ERROR( "triclopsGetImage()", _te );

	_te = triclopsGetImage( _triclops, TriImg_RECTIFIED, TriCam_REFERENCE, &_refImage );
	_HANDLE_TRICLOPS_ERROR( "triclopsGetImage()", _te );


	if(disparityImage.bAllocated == false || disparityImage.getWidth() != _disparityImage.ncols || disparityImage.getHeight() != _disparityImage.nrows) {
		ofLog(OF_LOG_WARNING, "BumblebeeGrabber::grab - allocating disparityImage: " + ofToString(_disparityImage.ncols) + " x " + ofToString(_disparityImage.nrows));
		disparityImage.allocate(_disparityImage.ncols, _disparityImage.nrows);
		gui.page("BumblebeeGrabber").addContent("disparityImage", disparityImage);
	}
	disparityImage.setFromPixels(_disparityImage.data, _disparityImage.ncols, _disparityImage.nrows);
	if(settings.doInvert) disparityImage.invert();
	if(settings.doThreshold) disparityImage.threshold(255-settings.threshold);


	if(realImage.bAllocated == false || realImage.getWidth() != _refImage.ncols || realImage.getHeight() != _refImage.nrows) {
		ofLog(OF_LOG_WARNING, "BumblebeeGrabber::grab - allocating realImage: " + ofToString(_refImage.ncols) + " x " + ofToString(_refImage.nrows));
		realImage.allocate(_refImage.ncols, _refImage.nrows);
		gui.page("BumblebeeGrabber").addContent("realImage", realImage);
	}
	realImage.setFromPixels(_refImage.data, _refImage.ncols, _refImage.nrows);

	if(settings.useDisparity == false) disparityImage = realImage;

	// Retrieve the rectified image from the _triclops context

	// Save the disparity and reference images
	//   _te = triclopsSaveImage( &_disparityImage, "disparity.pgm" );
	//   _HANDLE_TRICLOPS_ERROR( "triclopsSaveImage()", _te );

	//   _te = triclopsSaveImage( &_refImage, "reference.pgm" );
	//   _HANDLE_TRICLOPS_ERROR( "triclopsSaveImage()", _te );
}

void BumblebeeGrabber::draw(float x, float y, float w, float h) {
	if(settings.enabled == false) return;

	disparityImage.draw(x, y);
	realImage.draw(x, y-realImage.getHeight());
}

#endif