#pragma once
#include <Binary.h>
#include <iostream>
#include <set>
#include <algorithm> // used for std::transform() and std::fill()
#include <iterator> // used for std::inserter()
#include "RingBuffer.h"

#define CM_MASK 0x0F
#define CINFO_MASK 0xF0
#define FCHECK_MASK 0x1F
#define FDICT_MASK 0x20
#define FLEVEL_MASK 0xC0

#define HLIT_OFFSET 257
#define HDIST_OFFSET 1
#define HCLEN_OFFSET 4

#define CLEN_LEN_COUNT 19

#define DUMMY_CODE_VALUE UINT32_MAX


extern uint32_t LengthsOrder[19];


struct Node {
	Node(uint32_t val) : value(val), depth(0), left(nullptr), right(nullptr) {}
	Node() : Node(0) {}
	Node(Node* l, Node* r) :value(l->value + r->value), depth(std::max(l->depth, r->depth) + 1), left(l), right(r) {}
	const uint32_t value;
	const uint32_t depth;
	Node* left;
	Node* right;
};

typedef std::pair<uint32_t, Node*> LengthPair;
typedef std::pair<Node*, Node*> TreePair;

struct greater_node {
	bool operator() (const LengthPair& lhs, const LengthPair& rhs) const
	{
		if (lhs.first > rhs.first)
			return true;
		else if (lhs.first == rhs.first)
		{
			// Assuming that both the left and the right branch are either nullptr or pointer to Node
			if (lhs.second->left == nullptr && rhs.second->left != nullptr) // Left is leaf and right is branch
				return true;
			else if (lhs.second->left != nullptr && rhs.second->left == nullptr) // Left is branch and right is leaf
				return false;
			else if (lhs.second->left != nullptr && rhs.second->left != nullptr) // Both are branches
				return (lhs.second->depth < rhs.second->depth);
			else if (lhs.second->value < rhs.second->value) // Both are leafs
				return true;
		}
		return false;
	}
};

typedef std::multiset<LengthPair, greater_node> LengthsSet;

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
struct ZLHeader {
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
	TreePair DecodeHuffmanCodes();
	Node* CreateHuffmanTree(LengthsSet values);
	void FreeHuffmanTree(Node* treeRoot);
	std::vector<uint32_t> ReadLiteralsAndDistances(const Node* codeTree, uint32_t count);
	uint32_t DecodeSymbol(const Node* codeTree);
	uint32_t DecodeLength(const uint32_t &symbol);
	uint32_t DecodeDistance(const uint32_t &symbol);
	Binary DecodeBlock(const TreePair &alphabets);
	Node* GenerateStaticLitLen();
	Node* GenerateStaticDist();
	void LenghtsSetFromRange(LengthsSet &set, const std::vector<uint32_t>::iterator &begin, const std::vector<uint32_t>::iterator &end);

private: // Variables
	ZLCMF m_stCompressionInfo;
	ZLFLG m_stFlags;
	uint32_t m_uWindowSize;
	Binary m_oData;
	TreePair m_pLitDist;
	RingBuffer m_oLookback;
};
