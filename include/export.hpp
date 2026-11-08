#pragma once
#include "animator.hpp"
#include <cstdint>
#include <functional>
namespace studio {
// Top-down, tightly packed RGBA8. Decoding belongs to the caller, not the engine.
struct ExportImage {int width=0,height=0;std::vector<std::uint8_t> rgba;};
using FrameDecoder=std::function<ExportImage(const std::string&)>;
// One nonlooping pass: frame starts plus the terminal boundary. Cues cover
// (previous boundary, boundary], including time zero on the first sample.
void ExportTrace(const Clip&,const std::string& path);
// Raw source frames, without pose transforms; unused grid cells are transparent.
// BMP uses a BITMAPV4HEADER with explicit RGBA masks to preserve transparency.
void ExportContactSheet(const Clip&,const FrameDecoder&,const std::string& bmpPath,
                        const std::string& jsonPath,bool onePerRow=false);
}
