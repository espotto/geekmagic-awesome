// Desktop adapter for the exact firmware parser, package reader and compositor.
#include "ThemeEngine.h"
#include "ThemePackage.h"
#include <ArduinoJson.h>
#include <algorithm>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
class Disk : public gm::Source {
 public:
  explicit Disk(const char* path):file_(path,std::ios::binary) {
    if(!file_)throw std::runtime_error(std::string("Cannot open ")+path);
    file_.seekg(0,std::ios::end);
    auto size=file_.tellg();
    if(size<0||size>gm::MaxPackage)throw std::runtime_error("Package exceeds 3 MiB");
    length_=static_cast<uint32_t>(size);
  }
  uint32_t size() const override {return length_;}
  bool read(uint32_t offset,void* dest,size_t count) override {
    if(offset>length_||count>length_-offset)return false;
    file_.clear();file_.seekg(offset);file_.read(static_cast<char*>(dest),count);return bool(file_);
  }
 private:
  std::ifstream file_;uint32_t length_=0;
};
class Screen : public gm::Display {
 public:
  void row(int x,int y,const uint16_t* pixels,int count) override {
    std::copy(pixels,pixels+count,pixels_+y*240+x);
  }
  void write(std::ostream& out) const {
    char row[240*3];
    for(int y=0;y<240;++y) {
      for(int x=0;x<240;++x) {
        uint16_t c=pixels_[y*240+x];
        row[x*3]=((c>>11)*255)/31;row[x*3+1]=(((c>>5)&63)*255)/63;row[x*3+2]=((c&31)*255)/31;
      }
      out.write(row,sizeof(row));
    }
  }
 private:
  uint16_t pixels_[240*240]{}; // Desktop only; firmware remains scanline based.
};
void metadata(const gm::Theme& theme) {
  JsonDocument doc;doc["id"]=theme.id;doc["name"]=theme.name;doc["author"]=theme.author;doc["version"]=theme.version;
  serializeJson(doc,std::cout);std::cout<<'\n';
}
uint64_t integer(const char* text,uint64_t maximum) {
  std::string value(text);
  if(value.empty()||value.find_first_not_of("0123456789")!=std::string::npos)throw std::runtime_error("Expected a nonnegative integer");
  uint64_t n=std::stoull(value);if(n>maximum)throw std::runtime_error("Preview argument exceeds limit");return n;
}
void u16(std::ostream& out,unsigned n) {out.put(char(n));out.put(char(n>>8));}
constexpr size_t MaxInjectedValues=32;
bool declaredDataField(const gm::Theme& theme,const std::string& key) {
  size_t dot=key.find('.');
  if(dot==std::string::npos||dot==0||dot+1>=key.size())return false;
  std::string source=key.substr(0,dot),field=key.substr(dot+1);
  for(const auto& data:theme.data) if(data.id==source)
    for(const auto& f:data.fields) if(f.id==field)return true;
  return false;
}
// `device.<name>=VALUE` (spec 2) goes to `device`, as a firmware would supply it.
std::vector<gm::ThemeValue> injectedValues(const gm::Theme& theme,char** args,int count,std::vector<gm::ThemeValue>& device) {
  std::vector<gm::ThemeValue> values;
  for(int i=0;i<count;++i) {
    std::string arg=args[i];
    size_t eq=arg.find('=');
    if(eq==std::string::npos||eq==0)throw std::runtime_error("Expected KEY=VALUE: "+arg);
    std::string key=arg.substr(0,eq),value=arg.substr(eq+1);
    if(theme.spec>=2&&gm::builtInSource(key)==gm::BuiltIn::Device) {
      for(const auto& existing:device) if(existing.key==key)throw std::runtime_error("Duplicate data key: "+key);
      device.push_back({key,value});
      continue;
    }
    if(!declaredDataField(theme,key))throw std::runtime_error("Undeclared data key: "+key);
    for(const auto& existing:values) if(existing.key==key)throw std::runtime_error("Duplicate data key: "+key);
    if(values.size()==MaxInjectedValues)throw std::runtime_error("Expected at most 32 injected values");
    values.push_back({key,value});
  }
  return values;
}
}
int main(int argc,char** argv) {
  try {
    if(argc<3)throw std::runtime_error("Expected validate-manifest, validate, or preview and a file");
    std::string command=argv[1];Disk file(argv[2]);gm::Theme theme;std::string error;
    if(command=="validate-manifest"&&argc==3) {
      if(file.size()>gm::MaxManifest)throw std::runtime_error("theme.json: manifest exceeds 16 KiB");
      std::string json(file.size(),'\0');
      if(!file.read(0,&json[0],json.size())||!gm::parseTheme(json,theme,error))throw std::runtime_error(error.empty()?"Cannot read manifest":error);
      metadata(theme);return 0;
    }
    gm::Package package(file);
    if(!package.load(theme,error))throw std::runtime_error(error);
    if(command=="validate"&&argc==3) {metadata(theme);return 0;}
    if(command!="preview"||argc<7)throw std::runtime_error("Expected preview PACKAGE OUTPUT EPOCH FPS FRAMES [KEY=VALUE...]");
    uint64_t epoch=integer(argv[4],253402300739ULL);
    unsigned fps=integer(argv[5],15),frames=integer(argv[6],900);
    if(!fps||!frames||frames>60*fps)throw std::runtime_error("Preview is limited to 1..15 FPS and 60 seconds");
    // "@lang=CODE" sets the device language, as the settings do.
    uint8_t language=0;int first=7;
    if(argc>7&&std::string(argv[7]).compare(0,6,"@lang=")==0) {
      std::string code=std::string(argv[7]).substr(6);int index=gm::languageIndex(code.c_str(),code.size());
      if(index<0)throw std::runtime_error("Unknown language: "+code);
      language=uint8_t(index);first=8;
    }
    std::vector<gm::ThemeValue> device,values=injectedValues(theme,argv+first,argc-first,device);
    std::ofstream out(argv[3],std::ios::binary);
    if(!out)throw std::runtime_error("Cannot create preview frames");
    out.write("STP1",4);u16(out,240);u16(out,240);u16(out,fps);u16(out,frames);
    gm::Engine engine;engine.setTheme(std::move(theme),0);engine.setValues(std::move(values));engine.setDeviceValues(std::move(device));engine.setLanguage(language);Screen screen;
    for(unsigned i=0;i<frames;++i) {
      // Sample at the first whole millisecond on/after the frame deadline.
      // Flooring 1000/15 would repeatedly sample before a 15 FPS frame is due.
      uint32_t ms=(i*1000U+fps-1)/fps;time_t now=static_cast<time_t>(epoch+ms/1000);
      tm* time=std::gmtime(&now);if(!time)throw std::runtime_error("Preview time is out of range");
      if(!gm::render(engine,engine.update(ms,time),package,screen))throw std::runtime_error("Could not read theme asset");
      screen.write(out);
    }
    out.close();if(!out)throw std::runtime_error("Could not write preview frames");
    return 0;
  } catch(const std::exception& error) {
    std::cerr<<error.what()<<'\n';return 1;
  }
}
