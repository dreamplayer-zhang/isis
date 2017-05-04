#ifdef _WINDOWS
#define ZLIB_WINAPI
#endif

#include <isis/data/io_interface.h>
#include <isis/data/io_factory.hpp>
#include <fstream>

namespace isis
{
namespace image_io
{

class ImageFormat_FListProxy: public FileFormat
{
private:

protected:
	util::istring suffixes( io_modes /*modes*/ )const override {return "flist";}
public:
	std::string getName()const override {return "filelist proxy (gets filenames from files or stdin)";}

	std::list<data::Chunk> doLoad( std::istream &in, std::list<util::istring> formatstack ) {
		static const std::regex linebreak( "[[.newline.][.carriage-return.]]" );
		std::string fnames;
		size_t fcnt = 0;

		std::list<data::Chunk> ret;

		while( !in.eof() ) {
			in >> fnames ;
			for( const std::string fname : util::stringToList<std::string>( fnames, linebreak ) ) {
				LOG( Runtime, info ) << "loading " << fname;
				try{
					fcnt++;
					ret.splice(ret.end(), data::IOFactory::loadChunks(fname, formatstack ));
				} catch(data::IOFactory::io_error &e){
					LOG(Runtime,error) << "Loading of " << fname << "(#" << fcnt << " in list) failed with " << e.what() << "(the plugin used was:" << e.which() << ")";
				}
			}
		}

		LOG_IF( fcnt == 0, Runtime, warning ) << "didn't get any filename from the input list";

		return ret;
	}

	std::list<data::Chunk> load(std::basic_streambuf<char> *source, std::list<util::istring> formatstack, const util::istring &, std::shared_ptr<util::ProgressFeedback> feedback )throw( std::runtime_error & ) override {
		std::istream stream(source);
		assert(formatstack.back()=="flist");
		formatstack.pop_back();
		return doLoad( stream, formatstack );
	}

	void write( const data::Image &/*image*/, const std::string &/*filename*/, const util::istring &/*dialect*/, std::shared_ptr<util::ProgressFeedback> /*progress*/ )throw( std::runtime_error & ) {
		throw( std::runtime_error( "not yet implemented" ) );
	}
};
}
}
isis::image_io::FileFormat *factory()
{
	return new isis::image_io::ImageFormat_FListProxy();
}
