#include "AssetImportPipeline.h"

#include "AssetWorkspace.h"
#include "ResourceManager.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iomanip>
#include <limits>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <system_error>
#include <vector>

namespace {
std::string hashString(const std::string& text) {
    std::uint64_t hash = 14695981039346656037ULL;
    for(unsigned char c:text) {
        hash^=c;
        hash*=1099511628211ULL;
    }
    std::ostringstream output;
    output<<std::hex<<std::setw(16)<<std::setfill('0')<<hash;
    return output.str();
}
bool validPlatform(const std::string& platform) {
    if(platform.empty() || platform.size()>48) return false;
    return std::all_of(platform.begin(),platform.end(),[](unsigned char c) {
        return std::isalnum(c) || c=='-' || c=='_';
    });
}
bool validGuid(const std::string& guid) {
    return guid.size()==32 && std::all_of(guid.begin(),guid.end(),
           [](unsigned char c){return std::isxdigit(c)!=0;});
}
bool within(const std::filesystem::path& root,
            const std::filesystem::path& file) {
    const auto relative=file.lexically_relative(root);
    if(relative.empty() || relative.is_absolute()) return false;
    for(const auto& part:relative)
        if(part==".." || part==".") return false;
    return true;
}
void u32(std::ostream& output,std::uint32_t v) {
    for(unsigned shift=0;shift!=32;shift+=8)
        output.put(static_cast<char>((v>>shift)&0xffU));
}
// Binary texture artifact (little endian):
// "SEIMG001" (8 bytes) + width u32 + height u32 + RGBA8 pixels.
bool convertTexture(const AssetManifestEntry& entry,
                    const std::filesystem::path& source,
                    const std::filesystem::path& outputPath,
                    ImportedArtifact& artifact, std::string& error) {
    ResourceManager resources;
    try {
        auto id=resources.loadTextureFromFile(entry.id,source.string());
        const auto& tex=resources.getTexture(id);
        if(tex.width<1 || tex.height<1 || tex.width>16384 || tex.height>16384 ||
           tex.pixels.size()!=static_cast<std::size_t>(tex.width)*tex.height*4) {
            error="Invalid or oversized imported texture: "+entry.path;
            return false;
        }
        const auto& settings=entry.textureImport;
        if(settings.maxDimension<1 || settings.maxDimension>8192) {
            error="Invalid maxDimension for "+entry.id;return false;
        }
        const int maxSide=std::max(tex.width,tex.height);
        const int w=maxSide>settings.maxDimension ?
            std::max(1,static_cast<int>((static_cast<std::int64_t>(tex.width)*
                                       settings.maxDimension)/maxSide)) : tex.width;
        const int h=maxSide>settings.maxDimension ?
            std::max(1,static_cast<int>((static_cast<std::int64_t>(tex.height)*
                                       settings.maxDimension)/maxSide)) : tex.height;
        std::vector<unsigned char> pixels(static_cast<std::size_t>(w)*h*4);
        for(int y=0;y<h;++y) {
            const int sourceY=std::min(tex.height-1,
                static_cast<int>((static_cast<std::int64_t>(y)*tex.height)/h));
            const int sy=settings.flipY ? tex.height-1-sourceY:sourceY;
            for(int x=0;x<w;++x) {
                const int sx=std::min(tex.width-1,
                    static_cast<int>((static_cast<std::int64_t>(x)*tex.width)/w));
                const auto offset=(static_cast<std::size_t>(sy)*tex.width+sx)*4;
                const auto dest=(static_cast<std::size_t>(y)*w+x)*4;
                for(int component=0;component<4;++component)
                    pixels[dest+component]=tex.pixels[offset+component];
                if(settings.premultiplyAlpha) {
                    const auto alpha=pixels[dest+3];
                    for(int channel=0;channel<3;++channel)
                        pixels[dest+channel]=static_cast<unsigned char>(
                            (static_cast<unsigned>(pixels[dest+channel])*alpha+127)/255);
                }
            }
        }
        std::ofstream out(outputPath,std::ios::binary|std::ios::trunc);
        if(!out) {error="Cannot write imported texture: "+outputPath.string();return false;}
        out.write("SEIMG001",8);
        u32(out,static_cast<std::uint32_t>(w));
        u32(out,static_cast<std::uint32_t>(h));
        out.write(reinterpret_cast<const char*>(pixels.data()),
                  static_cast<std::streamsize>(pixels.size()));
        if(!out) {error="Failed to write texture bytes: "+outputPath.string();return false;}
        artifact.width=w;artifact.height=h;
        return true;
    } catch(const std::exception& e) {
        error="Texture import failed for "+entry.id+": "+e.what();
        return false;
    }
}
}

std::filesystem::path AssetImportPipeline::cacheRoot(
    const std::filesystem::path& projectRoot,const std::string& platform) {
    return projectRoot / ".simpleengine" / "cache" / platform;
}

ImportBuildReport AssetImportPipeline::build(
    const std::filesystem::path& projectRoot,const std::string& platform) {
    ImportBuildReport result;
    if(!validPlatform(platform)) {
        result.errors.push_back("Invalid target platform token.");
        return result;
    }
    const auto sourceRoot=projectRoot/"Assets";
    AssetWorkspace workspace(sourceRoot);
    std::string error;
    if(!workspace.open(error)) {
        result.errors.push_back(error);return result;
    }
    std::size_t imported=0;
    if(!workspace.sync(imported,error)) {
        result.errors.push_back(error);return result;
    }
    const auto audit=workspace.audit();
    if(!audit.valid()) {
        result.errors=audit.errors;return result;
    }
    const auto cache=cacheRoot(projectRoot,platform);
    std::error_code ec;
    if(std::filesystem::is_symlink(cache) ||
       std::filesystem::is_symlink(cache.parent_path())) {
        result.errors.push_back("Import cache directory cannot be a symlink.");
        return result;
    }
    std::filesystem::create_directories(cache,ec);
    if(ec) {result.errors.push_back("Cannot create import cache: "+ec.message());return result;}
    const auto root=std::filesystem::weakly_canonical(sourceRoot,ec);
    if(ec) {result.errors.push_back("Cannot resolve asset directory.");return result;}
    std::map<std::string,int> states;
    std::map<std::string,std::string> keys;
    std::function<void(const AssetManifestEntry&)> visit;
    visit=[&](const AssetManifestEntry& asset) {
        if(!result.errors.empty()) return;
        const int state=states[asset.id];
        if(state==2) return;
        if(state==1) {
            result.errors.push_back("Asset dependency cycle at "+asset.id);
            return;
        }
        states[asset.id]=1;
        std::vector<std::string> dependencyKeys;
        for(const auto& name:asset.dependencies) {
            const auto* dependency=workspace.index().findById(name);
            if(!dependency) {
                result.errors.push_back(asset.id+" missing dependency "+name);
                return;
            }
            visit(*dependency);
            if(!result.errors.empty()) return;
            dependencyKeys.push_back(name+"="+keys[name]);
        }
        std::sort(dependencyKeys.begin(),dependencyKeys.end());
        std::ostringstream descriptor;
        descriptor << ImporterVersion << '|' << platform << '|' << asset.guid
                   << '|' << asset.contentHash << '|' << asset.sourceBytes
                   << '|' << AssetManifest::assetTypeToString(asset.type)
                   << '|' << asset.textureImport.maxDimension
                   << '|' << asset.textureImport.flipY
                   << '|' << asset.textureImport.premultiplyAlpha;
        for(const auto& dep:dependencyKeys) descriptor << '|' << dep;
        const std::string key=hashString(descriptor.str());
        keys[asset.id]=key;
        if(!validGuid(asset.guid)) {
            result.errors.push_back("Invalid GUID for "+asset.id);return;
        }
        const bool texture=asset.type==AssetType::Texture;
        const auto artifactPath=cache/(asset.guid+"-"+key+(texture?".seimg":".seblob"));
        ImportedArtifact artifact;
        artifact.id=asset.id;artifact.guid=asset.guid;artifact.cacheKey=key;
        artifact.file=artifactPath;
        artifact.sourceBytes=static_cast<std::size_t>(asset.sourceBytes);
        const auto source=std::filesystem::weakly_canonical(sourceRoot/asset.path,ec);
        if(ec || !within(root,source) || !std::filesystem::is_regular_file(source)) {
            result.errors.push_back("Unsafe or missing source file: "+asset.path);
            return;
        }
        if(asset.sourceBytes>256ULL*1024*1024) {
            result.errors.push_back("Asset exceeds 256 MiB import limit: "+asset.id);
            return;
        }
        if(std::filesystem::is_regular_file(artifactPath)) {
            artifact.cacheHit=true;++result.cached;
        } else {
            const auto temp=artifactPath.string()+".tmp";
            std::filesystem::remove(temp,ec);
            bool produced=false;
            if(texture) produced=convertTexture(asset,source,temp,artifact,error);
            else {
                std::filesystem::copy_file(source,temp,
                    std::filesystem::copy_options::overwrite_existing,ec);
                produced=!ec;
                if(!produced) error="Could not copy source artifact: "+ec.message();
            }
            if(!produced) {
                std::filesystem::remove(temp,ec);
                result.errors.push_back(error);
                return;
            }
            std::filesystem::rename(temp,artifactPath,ec);
            if(ec) {
                std::filesystem::remove(temp);
                result.errors.push_back("Cannot commit artifact: "+ec.message());
                return;
            }
            ++result.built;
        }
        artifact.artifactBytes=std::filesystem::file_size(artifactPath,ec);
        if(ec) {
            result.errors.push_back("Cannot read artifact size: "+artifactPath.string());
            return;
        }
        result.outputBytes+=artifact.artifactBytes;
        result.artifacts.push_back(std::move(artifact));
        states[asset.id]=2;
    };
    for(const auto& asset:workspace.index().getEntries()) {
        visit(asset);
        if(!result.errors.empty()) break;
    }
    return result;
}
bool AssetImportPipeline::clean(const std::filesystem::path& projectRoot,
                                const std::string& platform,std::string& error) {
    if(!validPlatform(platform)) {error="Invalid target platform token.";return false;}
    const auto cache=cacheRoot(projectRoot,platform);
    if(std::filesystem::is_symlink(cache) ||
       std::filesystem::is_symlink(cache.parent_path())) {
        error="Cannot clean a symlinked cache directory.";return false;
    }
    std::error_code ec;
    std::filesystem::remove_all(cache,ec);
    if(ec) {error="Cannot clean import cache: "+ec.message();return false;}
    return true;
}
