#include "GLOrientationHandler.hpp"


namespace isis
{
namespace viewer
{

const unsigned short GLOrientationHandler::textureSize = 4;

util::fvector4 GLOrientationHandler::transformVectorWithImageOrientation( const isis::viewer::ImageHolder &image, util::fvector4 vector )
{
	boost::numeric::ublas::matrix<float> vectorAsMatrix = boost::numeric::ublas::zero_matrix<float>( textureSize, 1 );
	boost::numeric::ublas::matrix<float> retCoords = boost::numeric::ublas::zero_matrix<float>( textureSize, 1 );
	vectorAsMatrix( 0, 0 ) = vector[0];
	vectorAsMatrix( 1, 0 ) = vector[1];
	vectorAsMatrix( 2, 0 ) = vector[2];
	retCoords = boost::numeric::ublas::prod( image.getNormalizedImageOrientation(), vectorAsMatrix );
	util::fvector4 retVec;
	retVec[0] = retCoords( 0, 0 );
	retVec[1] = retCoords( 1, 0 );
	retVec[2] = retCoords( 2, 0 );
	return retVec;
}

util::fvector4 GLOrientationHandler::transformVectorWithImageAndPlaneOrientation( const isis::viewer::ImageHolder &image, util::fvector4 vector, GLOrientationHandler::PlaneOrientation orientation )
{
	//first we have to transform the voxels to our physical image space
	util::fvector4 imageOrientedVector = GLOrientationHandler::transformVectorWithImageOrientation( image, vector );
	GLOrientationHandler::MatrixType tmpVector = boost::numeric::ublas::zero_matrix<float>( textureSize, 1 );

	for ( size_t i = 0; i < 3; i++ ) {
		tmpVector( i, 0 ) = imageOrientedVector[i];
	}

	//now we have to transform these coords to our respective plane view
	GLOrientationHandler::MatrixType planeOrientationMatrix =
		GLOrientationHandler::transformToView( boost::numeric::ublas::identity_matrix<float>( textureSize, textureSize ), orientation );
	GLOrientationHandler::MatrixType transformedImageVoxels = boost::numeric::ublas::prod( planeOrientationMatrix, tmpVector );
	util::fvector4 retVec = util::fvector4( transformedImageVoxels( 0, 0 ), transformedImageVoxels( 1, 0 ), transformedImageVoxels( 2, 0 ) );
	return retVec;
}

util::FixedVector<float, 3> GLOrientationHandler::getNormalizedScaling( const ImageHolder &image )
{
	util::FixedVector<float, 3> retScaling;
	util::fvector4 size = image.getImageSize();
	util::fvector4 scaling = image.getPropMap().getPropertyAs<util::fvector4>( "voxelSize" );
	util::fvector4 voxelGap = image.getPropMap().getPropertyAs<util::fvector4>( "voxelGap" );
	util::fvector4 physicalSize;

	for ( size_t index = 0; index < 3; index++ ) {
		physicalSize[index] = size[index] * ( scaling[index] + voxelGap[index] );
	}

	size_t biggestExtent = getBiggestVecElem<float>( physicalSize );

	for ( size_t index = 0; index < 3; index++ ) {
		retScaling[index] = 1.0 / physicalSize[biggestExtent] * physicalSize[index];
	}

	return retScaling;
}


GLOrientationHandler::MatrixType GLOrientationHandler::getOrientationMatrix( const ImageHolder &image, PlaneOrientation orientation, bool scaling )
{
	MatrixType retMat = image.getNormalizedImageOrientation();

	if ( scaling ) {
		MatrixType scaleMatrix = boost::numeric::ublas::identity_matrix<float>( textureSize, textureSize );
		util::FixedVector<float, 3> scaling = getNormalizedScaling( image );
		scaleMatrix( 0, 0 ) = scaling[0];
		scaleMatrix( 1, 1 ) = scaling[1];
		scaleMatrix( 2, 2 ) = scaling[2];
		retMat = boost::numeric::ublas::prod( retMat, scaleMatrix );
	}

	retMat = transformToView( retMat, orientation );
	return retMat;
}


GLOrientationHandler::MatrixType GLOrientationHandler::transformToView( GLOrientationHandler::MatrixType origMatrix, PlaneOrientation orientation )
{
	MatrixType transformMatrix = boost::numeric::ublas::identity_matrix<float>( textureSize, textureSize );

	switch ( orientation ) {
	case axial:
		/*setup axial matrix
		*-1  0  0
		* 0 -1  0
		* 0  0  1
		*/
		transformMatrix( 0, 0 ) = -1;
		transformMatrix( 1, 1 ) = -1;
		return boost::numeric::ublas::prod( transformMatrix, origMatrix );
		break;
	case sagittal:
		/*setup sagittal matrix
		* 0  1  0
		* 0  0  1
		* 1  0  0
		*/
		transformMatrix( 0, 0 ) = 0;
		transformMatrix( 2, 0 ) = 1;
		transformMatrix( 0, 1 ) = 1;
		transformMatrix( 2, 2 ) = 0;
		transformMatrix( 1, 2 ) = 1;
		transformMatrix( 1, 1 ) = 0;
		return boost::numeric::ublas::prod( transformMatrix, origMatrix );
		break;
	case coronal:
		/*setup coronal matrix
		* -1  0  0
		*  0  0  1
		*  0  1  0
		*/

		transformMatrix( 0, 0 ) = -1;
		transformMatrix( 1, 1 ) = 0;
		transformMatrix( 2, 2 ) = 0;
		transformMatrix( 2, 1 ) = 1;
		transformMatrix( 1, 2 ) = 1;
		return boost::numeric::ublas::prod( transformMatrix, origMatrix );
		break;
	}
}

void GLOrientationHandler::boostMatrix2Pointer( MatrixType boostMatrix, float *ret )
{
	size_t index = 0;

	for ( size_t i = 0; i < textureSize; i++ ) {
		for ( size_t j = 0; j < textureSize; j++ ) {
			ret[index++] = boostMatrix( i, j );
		}
	}
}

GLOrientationHandler::MatrixType GLOrientationHandler::orientation2TextureMatrix( const GLOrientationHandler::MatrixType &origMatrix )
{
	MatrixType retMat = boost::numeric::ublas::zero_matrix<float>( 4, 4 );

	for ( size_t i = 0; i < textureSize; i++ ) {
		for ( size_t j = 0; j < textureSize; j++ ) {
			retMat( i, j ) = origMatrix( i, j ) ? 1.0 / origMatrix( i, j ) : 0;

		}
	}

	//add the offsets
	for ( size_t i = 0; i < 3; i++ ) {
		for ( size_t j = 0; j < 3; j++ ) {
			if( retMat( i, j ) < 0 ) {
				retMat( 3, j ) = 1 + ( fabs( retMat( i, j ) ) - 1 ) / 2;
			} else if ( retMat( i, j ) > 0 ) {
				retMat( 3, j ) = -1 * ( fabs( retMat( i, j ) ) - 1 ) / 2;
			}
		}
	}

	return retMat;
}



GLOrientationHandler::ViewPortCoords GLOrientationHandler::calculateViewPort( size_t w, size_t h )
{
	ViewPortCoords retCoords;
	float scaleH = ( w < h ) ? ( ( float )w / ( float )h ) : 1;
	float scaleW = ( h < w ) ? ( ( float )h / ( float )w ) : 1;

	retCoords.h = h * scaleH;
	retCoords.w = w * scaleW;
	retCoords.x = ( w - ( w * scaleW ) ) / 2;
	retCoords.y = ( h - ( h * scaleH ) ) / 2;
	return retCoords;
}


}
} // end namespace