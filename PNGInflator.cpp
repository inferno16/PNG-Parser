#include "PNGInflator.h"

uint32_t LengthsOrder[19] = { 16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15 };

PNGInflator::PNGInflator()
	:m_uWindowSize(0)
{}


PNGInflator::~PNGInflator()
{}

Binary PNGInflator::Decompress(Binary compressedData)
{
	m_oData = compressedData;
	ReadHeaders();
	return DecompressData();
}

Binary PNGInflator::DecompressData()
{
	// Read the chunk header
	bool BFINAL = (m_oData.GetBits(1) == 1);
	BType BTYPE = (BType)m_oData.GetBits(2);


	switch (BTYPE)
	{
	case BType::UNCOMPRESSED:
	{
		std::cout << "Data is not compressed!\n";
		m_oData.FlushBits(); // Discarding the remaining unused bits in the byte

		// Reading the LEN and NLEN fields
		uint16_t LEN, NLEN;
		m_oData.ReadData((byte_t*)&LEN, sizeof(LEN));
		m_oData.ReadData((byte_t*)&NLEN, sizeof(NLEN));

		if (LEN != (uint16_t)~NLEN) {
			throw "LEN field doesn't match the copliment of NLEN!";
		}

		// Extracting the data
		binary_t vec(LEN);
		m_oData.ReadData(vec.data(), LEN);
		return Binary(vec);
	}
	case BType::STATIC:
		std::cout << "Data is compressed using static Huffman codes!\n";
		break;
	case BType::DYNAMIC:
		std::cout << "Data is compressed using dynamic Huffman codes!\n";
		DecodeHuffmanCodes();
		break;
	default:
		std::cerr << "Unsupported BTYPE of " << (uint32_t)BTYPE << " found!\n";
		exit(1);
	}
	return Binary();
}

void PNGInflator::ReadHeaders()
{
	ZLHeader header;
	m_oData.ReadData((byte_t*)&header, sizeof(header));
	FillCMF(header);
	FillFLG(header);
}

void PNGInflator::FillCMF(const ZLHeader &header)
{
	// CM
	m_stCompressionInfo.CM = (header.CMF & (uint32_t)CompressionMethod::DEFLATE) ?
		CompressionMethod::DEFLATE :
		CompressionMethod::UNKNOWN;

	// CINFO
	m_stCompressionInfo.CINFO = (uint32_t)(header.CMF & CINFO_MASK) >> 4;
	if (m_stCompressionInfo.CINFO > 7) {
		std::cout << "CINFO is " << m_stCompressionInfo.CINFO << ", while the maximum allowed value is 7!\n";
		return;
	}
	m_uWindowSize = std::pow(2, m_stCompressionInfo.CINFO + 8);
}

void PNGInflator::FillFLG(const ZLHeader &header)
{
	// FCHECK
	m_stFlags.FCHECK = FCheckResult(header);

	// FDICT
	m_stFlags.FDICT = (bool)(header.FLG & FDICT_MASK);

	// FLEVEL
	m_stFlags.FLEVEL = GetCompressionLevel(header);
}

bool PNGInflator::FCheckResult(const ZLHeader &header)
{
	uint16_t check = (uint16_t)header.CMF * 256 + header.FLG;
	return (check % 31 == 0);
}

CompressionLevel PNGInflator::GetCompressionLevel(const ZLHeader &header)
{
	uint32_t level = header.FLG & FLEVEL_MASK;
	return (CompressionLevel)level;
}

void PNGInflator::DecodeHuffmanCodes()
{
	// Reading the number of literal, distance and code length codes
	uint32_t HLIT = m_oData.GetBits(5);
	uint32_t HDIST = m_oData.GetBits(5);
	uint32_t HCLEN = m_oData.GetBits(4);
	HLIT += HLIT_OFFSET;
	HDIST += HDIST_OFFSET;
	HCLEN += HCLEN_OFFSET;

	// Filling the code lengths for the code length alphabet
	LengthsSet clenLengths;
	for (size_t i = 0; i < HCLEN; i++)
	{
		uint32_t length = m_oData.GetBits(3);
		if (length > 0)
			clenLengths.insert(std::make_pair(length, new Node(LengthsOrder[i])));
	}

	Node *lenTree = CreateHuffmanTree(clenLengths);
	std::vector<uint32_t> lit_dist = ReadLiteralsAndDistances(lenTree, HLIT + HDIST);
	FreeHuffmanTree(lenTree);
	std::cout << "Read " << lit_dist.size() << " out of the " << HLIT + HDIST << " literal and distance symbols.\n";
	
	// Creating two separate vectors for the literal lengths and distance lengths
	LengthsSet litLengths;
	LengthsSet distLengths;
	//std::vector<std::pair<uint32_t, Node*>> litLengths(HLIT);
	//std::vector<uint32_t> distLengths(lit_dist.begin() + HLIT, lit_dist.end());
	size_t index = 0;
	std::vector<uint32_t>::iterator litEnd = lit_dist.begin() + HLIT;
	std::transform(lit_dist.begin(), litEnd, std::inserter(litLengths, litLengths.begin()), [&index](const uint32_t& len) {
		return std::make_pair(len, new Node(index++));
	});
	index = 0;
	std::transform(litEnd, lit_dist.end(), std::inserter(distLengths, distLengths.begin()), [&index](const uint32_t &len) {
		return std::make_pair(len, new Node(index++));
	});
	Node *litLenTree = CreateHuffmanTree(litLengths);
}

Node* PNGInflator::CreateHuffmanTree(LengthsSet values)
{
	// Creating the Huffman tree using a multimap with key - the level of the node and value - the node data  
	uint32_t currLevel = 0;
	std::vector<Node*> tempNodes;

	while (values.size() != 1 || !tempNodes.empty()) {
		if (tempNodes.size() != 0 && currLevel != values.begin()->first)
			throw "Can't construct Huffman tree from the given map!\n";
		currLevel = values.begin()->first;

		// Popping the first element from the map and putting it into the vector
		tempNodes.push_back(values.begin()->second);
		values.erase(values.begin());

		if (tempNodes.size() == 2) {
			// Creating new parent node from the 2 elements of the vector and putting it back in the map with lower level
			values.insert(std::make_pair(currLevel - 1, new Node(tempNodes.at(0), tempNodes.at(1))));
			tempNodes.erase(tempNodes.begin(), tempNodes.end());
		}
	}

	return values.begin()->second;
}

void PNGInflator::FreeHuffmanTree(Node * treeRoot)
{
	if (treeRoot->left != nullptr)
		FreeHuffmanTree(treeRoot->left);
	if (treeRoot->right != nullptr)
		FreeHuffmanTree(treeRoot->right);

	delete treeRoot;
	treeRoot = nullptr;
}

std::vector<uint32_t> PNGInflator::ReadLiteralsAndDistances(const Node* codeTree, uint32_t count)
{
	uint32_t repeatCount = 0;
	uint32_t lastVal = UINT_MAX;
	std::vector<uint32_t> lit_dist;
	while (count)
	{
		if (repeatCount > 0) {
			if (lastVal == UINT_MAX)
				throw "Trying to repeat the last symbol while there is no symbols read!";
			lit_dist.push_back(lastVal);
			count--;
			repeatCount--;
			continue; // Skip the rest of the code
		}

		uint32_t symbol = DecodeSymbol(codeTree);
		if (symbol >= 0 && symbol < 15) {
			// This is a code length
			lit_dist.push_back(symbol);
			lastVal = symbol;
			count--;
		}
		else if (symbol == 16) {
			// Repeat the previous code length 3 - 6 times (size read from the next 2 bits)
			repeatCount = m_oData.GetBits(2) + 3;
		}
		else if (symbol == 17) {
			// Put 3 - 10 zeros (size read from the next 3 bits)
			lastVal = 0;
			repeatCount = m_oData.GetBits(3) + 3;
		}
		else if (symbol == 18) {
			// Put 11 - 138 zeros (size read from the next 7 bits)
			lastVal = 0;
			repeatCount = m_oData.GetBits(7) + 11;
		}
		else {
			throw "Unexpected symbol found!";
		}
	}
	if (repeatCount > 0)
		throw "Repeat count goes beyond the the provided size!";

	return lit_dist;
}

uint32_t PNGInflator::DecodeSymbol(const Node* codeTree)
{
	const Node* currNode = codeTree;
	while (currNode->left != nullptr && currNode->right != nullptr)
	{
		uint32_t bit = m_oData.GetBits(1);
		if (bit) {
			currNode = currNode->right;
		}
		else {
			currNode = currNode->left;
		}
	}
	return currNode->value;
}
