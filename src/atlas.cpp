#include "internal.hpp"
#include <fstream>

namespace foliage {
Atlas readAtlas(const std::filesystem::path& path) {
    require(std::filesystem::is_regular_file(path),"missing atlas manifest: "+path.string());
    require(std::filesystem::file_size(path)<=1024*1024,"atlas manifest exceeds 1 MiB");
    std::ifstream file(path); require(bool(file),"cannot read atlas manifest"); Json doc; file>>doc;
    require(doc.is_object()&&doc.value("format",std::string{})=="texutil-spritesheet"&&doc.value("uv_origin",std::string{})=="top-left","unsupported atlas format or UV origin");
    auto integer=[&](const Json& value, int lo, int hi) {
        require(value.is_number_integer()&&value.get<double>()>=lo&&value.get<double>()<=hi,"invalid atlas integer or bounds"); return value.get<int>();
    };
    integer(doc.at("version"),1,1);
    int columns=integer(doc.at("columns"),1,64),rows=integer(doc.at("rows"),1,64),count=integer(doc.at("count"),1,256),padding=integer(doc.at("padding"),0,64);
    require(count<=columns*rows,"atlas count exceeds grid capacity");
    auto dimensions=[&](const char* key) {
        const auto& value=doc.at(key); require(value.is_array()&&value.size()==2,"invalid atlas dimensions");
        return std::array<int,2>{integer(value[0],1,16384),integer(value[1],1,16384)};
    };
    auto size=dimensions("image_size"),cell=dimensions("cell_size"),content=dimensions("content_size");
    require(cell[0]*columns==size[0]&&cell[1]*rows==size[1]&&content[0]==cell[0]-2*padding&&content[1]==cell[1]-2*padding,"atlas grid, padding and image dimensions disagree");
    require(doc.at("cells").is_array()&&doc["cells"].size()==size_t(count),"atlas cells disagree with count");
    Atlas atlas; atlas.files.push_back(std::filesystem::weakly_canonical(path));
    for(int i=0;i<count;++i) {
        const auto& entry=doc["cells"][i]; int column=i%columns,row=i/columns;
        require(integer(entry.at("index"),0,count-1)==i&&integer(entry.at("column"),0,columns-1)==column&&integer(entry.at("row"),0,rows-1)==row,"atlas cells must be in row-major order");
        std::array<int,4> pixels{column*cell[0]+padding,row*cell[1]+padding,(column+1)*cell[0]-padding,(row+1)*cell[1]-padding};
        require(entry.at("pixels").is_array()&&entry["pixels"].size()==4&&entry.at("uv_rect").is_array()&&entry["uv_rect"].size()==4,"invalid atlas cell rectangle");
        std::array<float,4> uv;
        for(int k=0;k<4;++k) {
            require(integer(entry["pixels"][k],0,16384)==pixels[k],"atlas pixel rectangle disagrees with grid");
            uv[k]=float(pixels[k])/size[k%2]; const auto& value=entry["uv_rect"][k];
            require(value.is_number()&&std::isfinite(value.get<double>())&&std::abs(value.get<double>()-uv[k])<1e-6,"atlas UV rectangle disagrees with pixel bounds");
        }
        // TexUtil PNG rectangles use top-left origin. Foliage's internal UV V
        // points upward; GLB export flips it once for image-space glTF UVs.
        atlas.regions.push_back({uv[0],1-uv[3],uv[2],1-uv[1]});
    }
    require(doc.at("images").is_array()&&!doc["images"].empty()&&doc["images"].size()<=16,"atlas needs 1..16 images");
    for(const auto& image:doc["images"]) {
        require(image.at("file").is_string(),"atlas image path must be a string");
        auto name=image["file"].get<std::string>(); std::filesystem::path relative(name);
        require(!name.empty()&&name.find('\0')==std::string::npos&&!relative.is_absolute()&&relative.has_filename(),"atlas image path must be relative");
        for(const auto& part:relative) require(part!="..","atlas image path cannot contain '..'");
        auto texture=std::filesystem::weakly_canonical(path.parent_path()/relative);
        require(std::filesystem::is_regular_file(texture)&&std::filesystem::file_size(texture)<=64*1024*1024,"missing atlas image or image exceeds 64 MiB");
        std::ifstream png(texture,std::ios::binary); unsigned char header[24]{}; png.read(reinterpret_cast<char*>(header),24);
        const unsigned char signature[8]={137,80,78,71,13,10,26,10};
        require(png.gcount()==24&&std::equal(signature,signature+8,header)&&header[12]=='I'&&header[13]=='H'&&header[14]=='D'&&header[15]=='R',"atlas image must be a PNG");
        auto read=[&](int offset) { return (uint32_t(header[offset])<<24)|(uint32_t(header[offset+1])<<16)|(uint32_t(header[offset+2])<<8)|header[offset+3]; };
        require(read(16)==uint32_t(size[0])&&read(20)==uint32_t(size[1]),"atlas PNG dimensions disagree with manifest");
        atlas.files.push_back(texture);
    }
    return atlas;
}
}
