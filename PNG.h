#pragma once
#include <vector>
#include <string>
#include <fstream>
#include <iostream>
#include <Binary.h>
#include "PNGInflator.h"

extern uint32_t PNG_Signature[2]; // The PNG signature in Network-byte-order (Big-Endian)


// The next structures are wrapped in #pragma pack in order to avoid padding and 
// with that perventing bugs when reading wrong number of bytes from the stream
#pragma pack(push, 1)
struct ChunkHeader {
	uint32_t dataLength;
	char type[4];
};

struct Chunk
{
	ChunkHeader header;
	Binary data;
	uint32_t CRC;
};

struct IHDRData {
	uint32_t width;
	uint32_t height;
	uint8_t bitDepth;
	uint8_t colorType;
	uint8_t compressionMethod;
	uint8_t filterMethod;
	uint8_t interlaceMethod;
};

struct Pixel {
	Pixel() {}
	Pixel(const size_t &size) { SetSize(size); }
	Pixel(const Pixel &pObj) : bytes(pObj.bytes) { }
	void SetSize(const size_t &size) { if (size != 3 && size != 4) { throw "Invalid pixel size provided!"; } bytes.resize(size); }
	uint8_t Red() const { return bytes.at(0); }
	uint8_t Green() const { return bytes.at(1); }
	uint8_t Blue() const { return bytes.at(2); }
	uint8_t Alpha() const { return (bytes.size() == 4) ? bytes.at(3) : UINT8_MAX; }
	uint32_t ToInteger() const {
		return (bytes.size() == 3) ?
			(uint32_t)((bytes.at(0) << 16) | (bytes.at(1) << 8) | bytes.at(2)) :
			(uint32_t)((bytes.at(0) << 24) | (bytes.at(1) << 16) | (bytes.at(2) << 8) | bytes.at(3));
	}
	// The commented code is no longer used, but I'm not sure whether I want to remove it not.
	/*void FromInteger(const uint32_t &color) {
		size_t size = bytes.size();
		bytes.at(0) = (uint8_t)((color >> (size - 1) * 8) & 0xFF);
		bytes.at(1) = (uint8_t)((color >> (size - 2) * 8) & 0xFF);
		bytes.at(2) = (uint8_t)((color >> (size - 3) * 8) & 0xFF);
		if (size == 4)
			bytes.at(3) = (uint8_t)(color & 0xFF);
	}
	friend Pixel operator + (const Pixel &left, const Pixel &right) {
		Pixel p(left.bytes.size());
		p.bytes.at(0) = (uint8_t)(left.Red() + right.Red());
		p.bytes.at(1) = (uint8_t)(left.Green() + right.Green());
		p.bytes.at(2) = (uint8_t)(left.Blue() + right.Blue());
		if (p.bytes.size() == 4)
			p.bytes.at(3) = (uint8_t)(left.Alpha() + right.Alpha());

		return p;
	}
	void operator = (const Pixel &right) { bytes = right.bytes; }
	friend void operator += (Pixel &left, const Pixel &right) { left = left + right; }*/
	binary_t bytes;
};

struct Scanline {
	byte_t filter;
	std::vector<Pixel> pixels;
};
#pragma pack(pop)

enum class ChunkType {
	UNKNOWN = -1,
	IHDR, // image header, which is the first chunk in a PNG datastream
	PLTE, // palette table associated with indexed PNG images
	IDAT, // image data chunks
	IEND, // image trailer, which is the last chunk in a PNG datastream
	tRNS, // Transparency information
	cHRM, gAMA, iCCP, sBIT, sRGB, // Color space information
	iTXt, tEXt, zTXt, // Textual information
	bKGD, hIST, pHYs, sPLT, // Miscellaneous information
	tIME // Time information
};

enum class BitDepth {
	UNKNOWN = -1,
	DEPTH1 = 1,
	DEPTH2 = 2,
	DEPTH4 = 4,
	DEPTH8 = 8,
	DEPTH16 = 16
};

enum class ColorType {
	UNKNOWN		= -1,// | Allowed bit depth | Description
	GRAYSCALE	= 0, // | 1, 2, 4, 8, 16	| Each pixel is a greyscale sample
	TRUECOLOR	= 2, // | 8, 16				| Each pixel is an R,G,B triple
	INDEXED		= 3, // | 1, 2, 4, 8		| Each pixel is a palette index; a PLTE chunk shall appear.
	GRAYSCALEA	= 4, // | 8, 16				| Each pixel is a greyscale sample followed by an alpha sample.
	TRUECOLORA	= 6  // | 8, 16				| Each pixel is an R,G,B triple followed by an alpha sample.
};

//extern Pixel *FilterFunction(const std::vector<Scanline>&, const size_t&, const size_t&);

class PNG
{
public:
	// Constuctors and Destructor
	PNG() {}
	PNG(const std::string &filepath) { Open(filepath); }
	PNG(const char *filepath, const size_t &size) { Open(filepath, size); }
	~PNG();

	// Public methods
	void Open(const std::string &filepath) { Open(filepath.c_str(), filepath.length() + 1); }
	void Open(const char *filepath, const size_t &size);
	void ReadFile();
	bool IsSupported();
	void PrintHeaderInfo(std::ostream &stream);
	void PrintHexPixels(const std::vector<Scanline> &scanlines, std::ostream &stream);

private: // Methods
	bool CheckSignature(const uint32_t bytes[2]);
	ChunkType GetChunkType(const Chunk &chunk);
	ChunkType GetChunkType(const ChunkHeader &header);
	Chunk ReadChunk(std::ifstream &file);
	void ParseHeaders(Chunk &IHDR);
	Chunk MergeDataChunks(std::vector<Chunk> &IDATs);
	const char *GetColorTypeString(const ColorType &colorType);
	std::vector<Scanline> ReadScanlines(Binary &data);
	void ApplyFilters(std::vector<Scanline> &scanlines);
	void ApplyFilterToScanline(std::vector<Scanline> &scanlines, const size_t &lineNum, byte_t(PNG::* fn)(const std::vector<Scanline>&, const size_t&, const size_t&, const size_t &));
	// Returns the value from the same channel(byte) in the left pixel(pixel "a") or 0 if the curent pixel is the leftmost 
	byte_t SubFilter(const std::vector<Scanline> &scanlines, const size_t &x, const size_t &y, const size_t &byte);
	// Returns the value from the same channel(byte) in the top pixel(pixel "c") or 0 if the curent pixel is the topmost 
	byte_t UpFilter(const std::vector<Scanline> &scanlines, const size_t &x, const size_t &y, const size_t &byte);
	// Returns the average value of the above 2 filters rounded down to the nearest integer value
	byte_t AverageFilter(const std::vector<Scanline> &scanlines, const size_t &x, const size_t &y, const size_t &byte);
	// After some linear computations returns either SubFilter, UpFilter or the value form the same channel(byte) in the top-left pixel(pixel c)
	byte_t PaethFilter(const std::vector<Scanline> &scanlines, const size_t &x, const size_t &y, const size_t &byte);

private: // Variables
	char *m_sFilePath;
	IHDRData m_stHeaders;
	Chunk m_stIDAT;
};

