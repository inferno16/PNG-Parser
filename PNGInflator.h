#pragma once
#include <Binary.h>
#include <math.h>
#include <iostream>
#include <map>
#include <functional>

#define CM_MASK 0x0F
#define CINFO_MASK 0xF0
#define FCHECK_MASK 0x1F
#define FDICT_MASK 0x20
#define FLEVEL_MASK 0xC0

#define HLIT_OFFSET 257
#define HDIST_OFFSET 1
#define HCLEN_OFFSET 4

#define CLEN_LEN_COUNT 19


extern uint32_t LengthsOrder[19];

struct Node {
	Node(uint32_t val) : value(val), left(nullptr), right(nullptr) {}
	Node() : Node(0) {}
	Node(Node* l, Node* r) :value(l->value + r->value), left(l), right(r) {}
	const uint32_t value;
	Node* left;
	Node* right;
};

typedef std::multimap<uint32_t, Node*, std::greater<uint32_t>> LengthsMap;

enum class CompressionMethod {
	UNKNOWN = -1,
	DEFLATE = 0x08,
	RESERVED = 0x0F
};

enum class CompressionLevel {
	FASTEST = 0,
	FAST = 0x40,
	DEFAULT = 0x80,
	SLOWEST = 0xC0
};

enum class BType {
	UNCOMPRESSED = 0,
	STATIC = 1,
	DYNAMIC = 2,
	RESERVED = 3
};

#pragma pack(push, 1)
struct ZLHeader{
	uint8_t CMF; // bits 0-3 is CM, 4-7 is CINFO
	uint8_t FLG; // bits 0-4 is FCHECK, 5 is FDICT and 6-7 is FLEVEL
};

struct ZLCMF {
	CompressionMethod CM;
	uint32_t CINFO; // Compression info. CINFO = log2(WindowSize) - 8, to find WindowSize use: 2^(CINFO+8)
};

struct ZLFLG {
	bool FCHECK;
	bool FDICT;
	CompressionLevel FLEVEL;
};
#pragma pack(pop)


class PNGInflator
{
public:
	PNGInflator();
	~PNGInflator();

	Binary Decompress(Binary compressedData);
	Binary DecompressData();

private: // Methods
	void ReadHeaders();
	void FillCMF(const ZLHeader &header);
	void FillFLG(const ZLHeader &header);
	bool FCheckResult(const ZLHeader &header);
	CompressionLevel GetCompressionLevel(const ZLHeader &header);
	void DecodeHuffmanCodes();
	Node* CreateHuffmanTree(LengthsMap values);
	void FreeHuffmanTree(Node* treeRoot);
	std::vector<uint32_t> ReadLiteralsAndDistances(const Node* codeTree, uint32_t count);
	uint32_t DecodeSymbol(const Node* codeTree);
	

private: // Variables
	ZLCMF m_stCompressionInfo;
	ZLFLG m_stFlags;
	uint32_t m_uWindowSize;
	Binary m_oData;
};