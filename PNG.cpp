#include "PNG.h"
#include <iomanip>

// The PNG signature in Network-byte-order (Big-Endian)
uint32_t PNG_Signature[2] = { 0x474E5089, 0x0A1A0A0D };

//typedef Pixel(PNG::* FilterFunction)(const std::vector<Scanline>&, const size_t&, const size_t&);

PNG::~PNG()
{
	delete[] m_sFilePath;
}

void PNG::Open(const char * filepath, const size_t &size)
{
	m_sFilePath = new char[size];
	strcpy_s(m_sFilePath, size, filepath);
}

void PNG::ReadFile()
{
	std::cout << "Reading file: " << m_sFilePath << std::endl;
	std::ifstream file(m_sFilePath, std::ios::binary);

	// Checking file signature
	uint32_t sig[2];
	file.read((char*)&sig, sizeof(sig));
	if (!CheckSignature(sig)) {
		std::cerr << "File signature mismatch!\n";
		return;
	}

	// Reading the IHDR chunk
	Chunk IHDR = ReadChunk(file);

	if (GetChunkType(IHDR.header) != ChunkType::IHDR) {
		std::cerr << "IHDR chunk not found!\n";
		return;
	}

	ParseHeaders(IHDR);
	// ToDo: The next 2 rows are for debugging purposes and I might want to remove them in future
	PrintHeaderInfo(std::cout);
	std::cout << std::endl;

	// Reading the IDAT chunk(s)
	std::vector<Chunk> IDATChunks;
	Chunk chunk;
	bool dataFinished = false;
	do {
		chunk = ReadChunk(file);
		if (GetChunkType(chunk) == ChunkType::IDAT) {
			if (dataFinished) {
				std::cerr << "IDAT Chunks are not consecutive!\n";
				return;
			}
			IDATChunks.push_back(chunk);
		}
		else if (IDATChunks.size() > 0 && !dataFinished) {
			dataFinished = true;
		}
	} while (GetChunkType(chunk) != ChunkType::IEND);

	m_stIDAT = MergeDataChunks(IDATChunks);

	// ToDo: The code below is not part of the "file reading" so it might as well be in a separate method
	PNGInflator inf;
	Binary decompressedData;
	{
		decompressedData = inf.Decompress(m_stIDAT.data);
	}
	if (decompressedData.GetSize() == 0) {
		std::cout << "Couldn't decompress the stream!\n";
		return;
	}
	std::ofstream f("testImg.bin", std::ios::binary);
	decompressedData.WriteToStream(f);
	std::vector<Scanline> slv;
	ReadScanlines(slv, decompressedData);
	return;
	decompressedData.~Binary();
	ApplyFilters(slv);
	std::cout << "\nRaw pixel data:\n";
	PrintHexPixels(slv, std::cout);
}

bool PNG::IsSupported()
{
	return ((m_stHeaders.colorType == (uint8_t)ColorType::TRUECOLORA ||
		m_stHeaders.colorType == (uint8_t)ColorType::TRUECOLOR) &&
		m_stHeaders.bitDepth == (uint8_t)BitDepth::DEPTH8 &&
		m_stHeaders.filterMethod == 0 &&
		m_stHeaders.interlaceMethod == 0 &&
		m_stHeaders.compressionMethod == 0
		);
}

void PNG::PrintHeaderInfo(std::ostream & stream)
{
	stream << "PNG Headers:\n\n";
	stream << "Width: " << m_stHeaders.width << std::endl;
	stream << "Height: " << m_stHeaders.height << std::endl;
	stream << "Bit depth: " << (uint32_t)m_stHeaders.bitDepth << std::endl;
	stream << "Color type: " << GetColorTypeString((ColorType)m_stHeaders.colorType) << std::endl;
	stream << "Interlaced: " << (m_stHeaders.interlaceMethod ? "Yes" : "No") << std::endl;
	stream << "Filtering method: " << (m_stHeaders.filterMethod ? "Unknown" : "Adaptive filtering") << std::endl;
	stream << "Compression method: " << (m_stHeaders.compressionMethod ? "Unknown" : "LZ77 DEFLATE Algorithm") << std::endl;
}

void PNG::PrintHexPixels(const std::vector<Scanline>& scanlines, std::ostream &stream)
{
	if (scanlines.empty() || scanlines.begin()->pixels.empty())
		return;
	
	std::ios_base::fmtflags flags(stream.flags()); // Save the current flags

	size_t paddingSize = scanlines.begin()->pixels.begin()->bytes.size() * 2;
	stream << std::hex << std::uppercase << std::setfill('0'); // Alter the stream flags

	for (size_t i = 0; i < scanlines.size(); i++) {
		for (size_t j = 0; j < scanlines.at(i).pixels.size(); j++) {
			// Print the color in hex format e.g. opaque blue in RGBA is represented as bytes[4] = {0, 255, 0, 255}
			// by the struct Pixel and after the formating is displayed in hex as #00FF00FF
			stream << "#" << std::setw(paddingSize) << scanlines[i].pixels[j].ToInteger() << ((j == scanlines[i].pixels.size() - 1) ? "\n" : ", ");
		}
	}

	stream.flags(flags); // Restore the initial flags
}

bool PNG::CheckSignature(const uint32_t bytes[2])
{
	return ((PNG_Signature[0] & bytes[0]) && (PNG_Signature[1] & bytes[1]));
}

ChunkType PNG::GetChunkType(const Chunk & chunk)
{
	return GetChunkType(chunk.header);
}

ChunkType PNG::GetChunkType(const ChunkHeader & header)
{
	if (strncmp(header.type, "IHDR", 4) == 0) {
		return ChunkType::IHDR;
	}
	else if (strncmp(header.type, "PLTE", 4) == 0) {
		return ChunkType::PLTE;
	}
	else if (strncmp(header.type, "IDAT", 4) == 0) {
		return ChunkType::IDAT;
	}
	else if (strncmp(header.type, "IEND", 4) == 0) {
		return ChunkType::IEND;
	}
	else if (strncmp(header.type, "tRNS", 4) == 0) {
		return ChunkType::tRNS;
	}
	else if (strncmp(header.type, "cHRM", 4) == 0) {
		return ChunkType::cHRM;
	}
	else if (strncmp(header.type, "gAMA", 4) == 0) {
		return ChunkType::gAMA;
	}
	else if (strncmp(header.type, "iCCP", 4) == 0) {
		return ChunkType::iCCP;
	}
	else if (strncmp(header.type, "sBIT", 4) == 0) {
		return ChunkType::sBIT;
	}
	else if (strncmp(header.type, "sRGB", 4) == 0) {
		return ChunkType::sRGB;
	}
	else if (strncmp(header.type, "iTXt", 4) == 0) {
		return ChunkType::iTXt;
	}
	else if (strncmp(header.type, "tEXt", 4) == 0) {
		return ChunkType::tEXt;
	}
	else if (strncmp(header.type, "zTXt", 4) == 0) {
		return ChunkType::zTXt;
	}
	else if (strncmp(header.type, "bKGD", 4) == 0) {
		return ChunkType::bKGD;
	}
	else if (strncmp(header.type, "hIST", 4) == 0) {
		return ChunkType::hIST;
	}
	else if (strncmp(header.type, "pHYs", 4) == 0) {
		return ChunkType::pHYs;
	}
	else if (strncmp(header.type, "sPLT", 4) == 0) {
		return ChunkType::sPLT;
	}
	else if (strncmp(header.type, "tIME", 4) == 0) {
		return ChunkType::tIME;
	}
	std::cerr << "Unknown chunk encountered! Type: " << header.type << "(" <<
		(uint32_t)header.type[0] << ", " <<
		(uint32_t)header.type[1] << ", " <<
		(uint32_t)header.type[2] << ", " <<
		(uint32_t)header.type[3] << ")" << std::endl;
	return ChunkType::UNKNOWN;
}

Chunk PNG::ReadChunk(std::ifstream & file)
{
	Chunk chunk;
	file.read((char*)&chunk.header, sizeof(chunk.header));
	chunk.header.dataLength = Binary::ByteSwap(chunk.header.dataLength); // Convert to Little-Endian
	chunk.data.ReadFromStream(file, chunk.header.dataLength);
	file.read((char*)&chunk.CRC, sizeof(chunk.CRC));
	chunk.CRC = Binary::ByteSwap(chunk.CRC);
	return chunk;
}

void PNG::ParseHeaders(Chunk &IHDR)
{
	IHDR.data.ReadData((byte_t*)&m_stHeaders, sizeof(m_stHeaders));
	m_stHeaders.width = Binary::ByteSwap(m_stHeaders.width);
	m_stHeaders.height = Binary::ByteSwap(m_stHeaders.height);
}

Chunk PNG::MergeDataChunks(std::vector<Chunk> &IDATs)
{
	if (IDATs.size() == 1)
		return IDATs.at(0);
	Binary data;
	for (size_t i = 0; i < IDATs.size(); i++)
	{
		data.AppendData(IDATs.at(i).data);
	}

	return { { data.GetSize(), { 'I','D','A','T' } }, data, 0 }; // ToDo: Might want to calculate CRC in future
}

const char * PNG::GetColorTypeString(const ColorType &colorType)
{
	switch (colorType)
	{
	case ColorType::GRAYSCALE:
		return "Grayscale";
	case ColorType::TRUECOLOR:
		return "Truecolor";
	case ColorType::INDEXED:
		return "Indexed-color";
	case ColorType::GRAYSCALEA:
		return "Grayscale with alpha";
	case ColorType::TRUECOLORA:
		return "Truecolor with alpha";
	}
	return nullptr;
}

void PNG::ReadScanlines(std::vector<Scanline> &slv, Binary & data)
{
	std::cout << "Reading scanlines from stream...\n";
	slv.resize(m_stHeaders.height);
	size_t pixelSize = (m_stHeaders.colorType == (uint8_t)ColorType::TRUECOLOR) ? 3 : 4;
	size_t slDataSize = pixelSize * m_stHeaders.width;
	binary_t tempPixels(slDataSize);
	Scanline sl;
	for (size_t i = 0; i < m_stHeaders.height; i++) {
		data.ReadData((byte_t*)&sl.filter, sizeof(sl.filter));
		data.ReadData(tempPixels.data(), slDataSize);
		sl.FillPixels(tempPixels, pixelSize);
		slv[i] = sl;
		//tempPixels.erase(tempPixels.begin(), tempPixels.end());
		binary_t(slDataSize).swap(tempPixels);
		sl.pixels.erase(sl.pixels.begin(), sl.pixels.end());
	}
	//return slv;
}

void PNG::ApplyFilters(std::vector<Scanline>& scanlines)
{
	std::cout << "Applying filters to the scanlines...\n";
	for (size_t y = 0; y < scanlines.size(); y++) {
		switch (scanlines.at(y).filter)
		{
		case 0:
			continue;
		case 1:
			ApplyFilterToScanline(scanlines, y, &PNG::SubFilter);
			break;
		case 2:
			ApplyFilterToScanline(scanlines, y, &PNG::UpFilter);
			break;
		case 3:
			ApplyFilterToScanline(scanlines, y, &PNG::AverageFilter);
			break;
		case 4:
			ApplyFilterToScanline(scanlines, y, &PNG::PaethFilter);
			break;
		default:
			throw "Invalid filter found!";
			break;
		}
	}
}

void PNG::ApplyFilterToScanline(std::vector<Scanline>& scanlines, const size_t & lineNum, byte_t(PNG::* fn)(const std::vector<Scanline>&, const size_t&, const size_t&, const size_t&))
{
	for (size_t px = 0; px < scanlines.at(lineNum).pixels.size(); px++) {
		for (size_t byte = 0; byte < scanlines.at(lineNum).pixels.at(px).bytes.size(); byte++) {
			// Call the function from the provided "fn" pointer(using the current context) and add it's result to the current channel
			scanlines.at(lineNum).pixels.at(px).bytes[byte] += (this->*fn)(scanlines, px, lineNum, byte);
		}
	}
}

byte_t PNG::SubFilter(const std::vector<Scanline> &scanlines, const size_t &x, const size_t &y, const size_t &byte)
{
	return (x == 0) ? 0 : scanlines.at(y).pixels.at(x - 1).bytes.at(byte);
}

byte_t PNG::UpFilter(const std::vector<Scanline> &scanlines, const size_t &x, const size_t &y, const size_t &byte)
{
	return (y == 0) ? 0 : scanlines.at(y - 1).pixels.at(x).bytes.at(byte);
}

byte_t PNG::AverageFilter(const std::vector<Scanline> &scanlines, const size_t &x, const size_t &y, const size_t &byte)
{
	uint32_t sum = (uint32_t)SubFilter(scanlines, x, y, byte) + (uint32_t)UpFilter(scanlines, x, y, byte); // Make sure to prevent integer overflow
	return (byte_t)(sum / 2);
}

byte_t PNG::PaethFilter(const std::vector<Scanline> &scanlines, const size_t &x, const size_t &y, const size_t &byte)
{
	//  For more info: https://www.w3.org/TR/2003/REC-PNG-20031110/#9Filter-type-4-Paeth
	int a, b, c, p, pa, pb, pc; // Using signed int to prevent under or overflow
	a = SubFilter(scanlines, x, y, byte);
	b = UpFilter(scanlines, x, y, byte);
	c = (y == 0 || x == 0) ? 0 : scanlines.at(y - 1).pixels.at(x - 1).bytes.at(byte);
	p = a + b - c;
	pa = abs(p - a);
	pb = abs(p - b);
	pc = abs(p - c);
	return (pa <= pb && pa <= pc) ? a : (pb <= pc) ? b : c;
}
