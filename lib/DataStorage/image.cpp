// kate: show-indent on; indent-mode tab; indent-width 4; tab-width 4; replace-tabs off; auto-insert-doxygen on

//
// C++ Implementation: image
//
// Description:
//
//
// Author: Enrico Reimer<reimer@cbs.mpg.de>, (C) 2009
//
// Copyright: See COPYING file that comes with this distribution
//
//

#include "image.hpp"
#include "CoreUtils/vector.hpp"
#include <boost/foreach.hpp>

namespace isis{ namespace data{

namespace _internal{
	
bool image_chunk_order::operator() ( const data::Chunk& a, const data::Chunk& b )const
{
	MAKE_LOG(DataDebug);

	//@todo exception ??
	if(!(a.hasProperty("indexOrigin") && a.hasProperty("indexOrigin"))){
		LOG(DataDebug,util::error) << "The chunk has no position, it can not be sorted into the image." << std::endl;
		return false;
	}

	const util::fvector4 &posA=a.getProperty<util::fvector4>("indexOrigin");
	const util::fvector4 &posB=b.getProperty<util::fvector4>("indexOrigin");
	
	return posA.lexical_less_reverse(posB);
}

}
	
Image::Image (_internal::image_chunk_order lt ) :set ( lt ),PropertyObject(needed),clean(true)
{
	const size_t idx[]={0,0,0,0};
	init(idx);
}


bool Image::insertChunk ( const Chunk &chunk ) {
	MAKE_LOG(DataLog);
	if(not set.empty() && set.begin()->volume() != chunk.volume()){
		LOG(DataLog,util::error)
			<< "Cannot insert chunk, because its volume doesn't fit with the volume of the chunks allready in this image." << std::endl;
		return false;
	}
	if(not chunk.sufficient()){
		const util::PropMap::key_list missing=chunk.missing();
		LOG(DataLog,util::error)
			<< "Cannot insert chunk. Missing properties: " << util::list2string(missing.begin(),missing.end(),", ","<",">") << std::endl;
		return false;
	}
	if(set.insert(chunk).second){
		clean=false;
		return true;
	} else return false;
}


bool Image::reIndex() {
	MAKE_LOG(DataLog);
	MAKE_LOG(DataDebug);
	if(set.empty()){
		clean=true;
		LOG(DataLog,util::warning) << "Reindexing an empty image." << std::endl;
		return true;
	}
	
	//redo lookup table
	lookup.resize(set.size());
	size_t idx=0;
	for(std::set<Chunk,_internal::image_chunk_order>::iterator it=set.begin();it!=set.end();it++,idx++)
		lookup[idx]=it;
	
	//get primary attributes from first chunk
	const unsigned short chunk_dims=chunksBegin()->dimRange().second+1;
	chunkVolume = chunksBegin()->volume();

	//copy sizes of the chunks to to the first chunk_dims sizes of the image
	size_t size[Chunk::n_dims];
	for(unsigned short i=0;i<chunk_dims;i++)
		size[i]=chunksBegin()->dimSize(i);

	//if there are many chunks, they must leave at least on dimension to the image to sort them in
	if(chunk_dims>=Image::n_dims){
		if(lookup.size()>1){
			LOG(DataLog,util::error)
			<< "Cannot handle multiple Chunks, if they have more than "
			<< Chunk::n_dims-1 << " dimensions" << std::endl;
			return false;
		}
		//if there is only one chunk, its ok - the image will consist only of this one, 
		//and commonGet will allways return <0,set.begin()->dim2Index()>
		//thus in this case voxel() equals set.begin()->voxel()
	} else {// OK there is at least one dimension to sort in the chunks 
		// check the chunks for at least one dimensional break - use that for the size of that dimension
		size[chunk_dims]= getChunkStride() ?:1;
		for(unsigned short i=chunk_dims+1;i<Image::n_dims;i++)//if there are dimensions left figure out their size
			size[i]= getChunkStride(size[i-1])/size[i-1] ?:1;
	}

	//Clean up the properties 
	//@todo might fail if the image contains a prop that differs to that in the Chunks (which is equal in the chunks)
	util::PropMap common= set.begin()->propMap();
	std::set<util::PropMap::key_type> uniques;
	for(ChunkIterator i= ++chunksBegin();i!=chunksEnd();i++){
		const util::PropMap::diff_map difference=common.diff(i->propMap());
		BOOST_FOREACH(const util::PropMap::diff_map::value_type &ref,difference)
			uniques.insert(ref.first);
	}
	LOG(DataDebug,util::info) << uniques.size() << " Chunk-unique properties found in the Image" << std::endl;
	LOG(DataDebug,util::verbose_info) << util::list2string(uniques.begin(),uniques.end(),", ") << std::endl;

	BOOST_FOREACH(const util::PropMap::key_type &ref,uniques)
		common.erase(ref);

	properties.join(common);
	LOG(DataDebug,util::info) << common.size() << " common properties saved into the image" << std::endl;
	LOG(DataDebug,util::verbose_info) << util::list2string(common.begin(),common.end(),", ") << std::endl;
	
	LOG(DataDebug,util::verbose_info) << "It now has: " << util::list2string(properties.begin(),properties.end(),", ") << std::endl;

	//get indexOrigin from the first chunk
	setProperty("indexOrigin",chunksBegin()->getPropertyValue("indexOrigin"));
	
	init(size);
	clean=true;
	return true;
}

std::pair<size_t,size_t> Image::commonGet (size_t first,size_t second,size_t third,size_t fourth) const
{
	MAKE_LOG(DataDebug);
	if(not clean){
		LOG(DataDebug,util::error)
		<< "Getting data from a non indexed image will result in undefined behavior. Run reIndex first." << std::endl;
	}
	if(set.empty()){
		LOG(DataDebug,util::error)
		<< "Getting data from a empty image will result in undefined behavior." << std::endl;
	}
	
	const size_t idx[]={first,second,third,fourth};
	if(!rangeCheck(idx)){
		LOG(DataDebug,isis::util::error)
		<< "Index " << util::list2string(idx,idx+4,"|") << " is out of range (" << sizeToString() << ")" 	<< std::endl;
	}
	
	const size_t index=dim2Index(idx);
	return std::make_pair(index/chunkVolume,index%chunkVolume);
}
	
const Chunk& Image::getChunkAt(size_t at)const
{
	return *(lookup[at]);
}
Chunk& Image::getChunkAt(size_t at)
{
	//@todo we must cast away the const here because std::set has no non-const iterators
	Chunk &ret=const_cast<Chunk&>(*(lookup[at]));
	return ret;
}
	
Chunk& Image::getChunk (size_t first,size_t second,size_t third,size_t fourth) {
	MAKE_LOG(DataDebug);
	if(not clean){
		LOG(DataDebug,util::info)
		<< "Image is not clean. Running reIndex ..." << std::endl;
		reIndex();
	}
	const size_t index=commonGet(first,second,third,fourth).first;
	return getChunkAt(index);
}
	
const Chunk& Image::getChunk (size_t first,size_t second,size_t third,size_t fourth) const {
	const size_t index=commonGet(first,second,third,fourth).first;
	return getChunkAt(index);
}

size_t Image::getChunkStride ( size_t base_stride )
{
	MAKE_LOG(DataLog);
	MAKE_LOG(DataDebug);
	size_t ret;
	if(set.empty()){
		LOG(DataLog,util::error)
		<< "Trying to get chunk stride in an empty image" << std::endl;
		return 0;
	} else if (lookup.empty()) {
		LOG(DataDebug,util::error)
		<< "Lookup table for chunks is empty. Do reIndex() first!" << std::endl;
		return 0;
	} else if(lookup.size()>3*base_stride) {
		/* there can't be any stride with less than 3*base_stride chunks (which would actually be an invalid image)
		 * _____
		 * |c c| has no stride/dimensional break
		 * _____
		 * |c c|
		 * |c  | has a dimensional break, but is invalid
		 * _____
		 * |c c|
		 * |c c| is the first reasonable case
		 */
		
		// get the distance between first and second chunk for comparision
		util::fvector4 thisV=lookup[0]->getProperty<util::fvector4>("indexOrigin");
		util::fvector4 nextV=lookup[base_stride]->getProperty<util::fvector4>("indexOrigin");
		const util::fvector4 dist1 =nextV-thisV;
		
		// compare every follwing distance to that
		for(size_t i=base_stride;i<lookup.size()-base_stride;i+=base_stride){
			util::fvector4 thisV=lookup[i]->getProperty<util::fvector4>("indexOrigin");
			util::fvector4 nextV=lookup[i+base_stride]->getProperty<util::fvector4>("indexOrigin");
			const util::fvector4 dist =nextV-thisV;

			LOG(DataDebug,util::verbose_info)
			<< "Distance between chunk " << util::MSubject(i) << " and " << util::MSubject(i+base_stride)
			<< " is " << dist.len() << std::endl;
			
			if(dist.sqlen() > dist1.sqlen()*4){// found an dimensional break - leave
				ret=i+1;
				LOG(DataDebug,util::info)
				<< "Distance between chunk " << util::MSubject(i) << " and " << util::MSubject(i+base_stride)
				<< " is more then twice the first distance, assuming dimensional break at " << ret << std::endl;
				return ret; 
			}
		}
	}
	//we didn't find any break, so we assume its a linear image |c c ... c|
	ret=lookup.size();
	LOG(DataDebug,util::info)
	<< "No dimensional break found, assuming it to be at the end (" << ret << ")" << std::endl;
	return ret;
}

std::list<util::PropMap::mapped_type> Image::getChunksProperties(const util::PropMap::key_type& key, bool unique)const
{
	std::list<util::PropertyValue > ret;
	for(ChunkSet::const_iterator i=set.begin();i!=set.end();i++){
		const util::PropertyValue &prop=i->getPropertyValue(key);
		if(unique && prop.empty()) //if unique is requested and the property is empty
			continue; //skip it
		else if(unique && !(ret.empty() ||  prop == ret.back()))
			//if unique is requested and the property is equal to the one added before
			continue;//skip it
		else
			ret.push_back(prop);
	}
	return ret;
}


Image::ChunkIterator Image::chunksBegin(){return set.begin();}
Image::ChunkIterator Image::chunksEnd(){return set.end();}


util::fvector4 Image::size()const
{
	return util::fvector4(dimSize(0),dimSize(1),dimSize(2),dimSize(3));
}

ImageList::ImageList(ChunkList src)
{
	while(!src.empty()){
		value_type buff(new Image);
		for(ChunkList::iterator i=src.begin();i!=src.end();){
			if(buff->insertChunk(*i))
				src.erase(i++);
			else
				i++;
		}
		if(buff->chunksBegin()!=buff->chunksEnd()){
			buff->reIndex();
			push_back(buff);
		}
	}
}


}}