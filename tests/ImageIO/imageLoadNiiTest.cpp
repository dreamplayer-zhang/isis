/*
 * ImageFormatNii.cpp
 *
 *  Created on: Dec 07, 2009
 *      Author: hellrung
 */

#include <boost/foreach.hpp>

#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include "CoreUtils/log.hpp"
#include "DataStorage/io_factory.hpp"
#include "DataStorage/image.hpp"
#include "ImageIO/common.hpp"

int main(int argc, char *argv[]){

	ENABLE_LOG(isis::data::DataDebug,isis::util::DefaultMsgPrint,isis::util::info);
	ENABLE_LOG(isis::data::DataLog,isis::util::DefaultMsgPrint,isis::util::info);
	ENABLE_LOG(isis::util::CoreDebug,isis::util::DefaultMsgPrint,isis::util::info);
	ENABLE_LOG(isis::util::CoreLog,isis::util::DefaultMsgPrint,isis::util::info);
	ENABLE_LOG(isis::image_io::ImageIoDebug,isis::util::DefaultMsgPrint,isis::util::info);
	ENABLE_LOG(isis::image_io::ImageIoLog,isis::util::DefaultMsgPrint,isis::util::info);

	isis::data::ImageList img = isis::data::IOFactory::load("/scr/feige1/tmp/data.nii", "");

	/*/SCR/Programming2/hellrung/isis/Debug/tests/ImageIO/*/
	isis::data::IOFactory::write(img, "/tmp/delme.nii", "");
	return EXIT_SUCCESS;
}
