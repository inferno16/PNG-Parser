#include "PNGInflator.h"

uint32_t LengthsOrder[19] = { 16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15 };

PNGInflator::PNGInflator()
	:m_uWindowSize(0), m_oLookback(32 * 1024)
{
	m_pLitDist.first = GenerateStaticLitLen();
	m_pLitDist.second = GenerateStaticDist();
}


PNGInflator::~PNGInflator()
{
	FreeHuffmanTree(m_pLitDist.first);
	FreeHuffmanTree(m_pLitDist.second);
}

Binary PNGInflator::Decompress(Binary compressedData)
{
	m_oData = compressedData;
	ReadHeaders();
	return DecompressData();
}

Binary PNGInflator::DecompressData()
{
	bool BFINAL;
	Binary data;

	do {
		// Read the chunk header
		int bf = m_oData.GetBits(1);
		BFINAL = (bf == 1);
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
			data.AppendData(vec);
			break;
		}
		case BType::STATIC:
			std::cout << "Data is compressed using static Huffman codes!\n";
			data.AppendData(DecodeBlock(m_pLitDist));
			break;
		case BType::DYNAMIC: {
			std::cout << "Data is compressed using dynamic Huffman codes!\n";
			TreePair codes = DecodeHuffmanCodes();
			data.AppendData(DecodeBlock(codes));
			FreeHuffmanTree(codes.first);
			FreeHuffmanTree(codes.second);
			break;
		}
		default:
			std::cerr << "Unsupported BTYPE of " << (uint32_t)BTYPE << " found!\n";
			exit(1);
		}
	} while (!BFINAL);
	return data;
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
	m_uWindowSize = (uint32_t)std::pow(2, m_stCompressionInfo.CINFO + 8);
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

TreePair PNGInflator::DecodeHuffmanCodes()
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

	// Filling the literals LengthSet from the lit_dist vector
	LenghtsSetFromRange(litLengths, lit_dist.begin(), lit_dist.begin() + HLIT);

	// Filling the distances LengthSet from the lit_dist vector
	LenghtsSetFromRange(distLengths, lit_dist.begin() + HLIT, lit_dist.end());
	
	// Creating the literal code tree
	Node *litTree = CreateHuffmanTree(litLengths);

	// Creating the distace code tree
	Node *distTree;
	if (distLengths.size() == 1) {
		if (distLengths.begin()->first == 0) {
			distTree = nullptr;
		}
		else if (distLengths.begin()->first == 1) {
			distTree = new Node(new Node(DUMMY_CODE_VALUE), new Node(0));
		}
		else {
			// ToDo: Handle cases where length > 1
		}
	}
	else if(distLengths.size() > 1) {
		distTree = CreateHuffmanTree(distLengths);
	}
	else {
		// ToDo: Handle cases where size == 0
	}

	return std::make_pair(litTree, distTree);
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

uint32_t PNGInflator::DecodeLength(const uint32_t &symbol)
{
	if (symbol <= 256 || symbol > 285) {
		std::cerr << "Invalid symbol for length(" << symbol << ")!\n";
		exit(1);
	}
	if (symbol < 265) {
		return symbol - 254; // This gives us the length
	}
	else if (symbol == 285) {
		return 285;
	}
	else {
		uint32_t extraBits = (symbol - (265 - 4)) / 4;
		return ((((symbol - 265) % 4) + 4) << extraBits) + 3 + m_oData.GetBits(extraBits);
	}
}

uint32_t PNGInflator::DecodeDistance(const uint32_t & symbol)
{
	if (symbol < 0 || symbol > 29) {
		std::cerr << "Invalid symbol for distance(" << symbol << ")!\n";
		exit(1);
	}
	if (symbol < 4) {
		return symbol + 1;
	}
	else {
		uint32_t extraBits = (symbol - (4 - 2)) / 2;
		return ((((symbol - 4) % 2) + 2) << extraBits) + 1 + m_oData.GetBits(extraBits);
	}
}

Binary PNGInflator::DecodeBlock(const TreePair& alphabets)
{
	Binary data;
	do
	{
		uint32_t sym = DecodeSymbol(alphabets.first); // Reading symbol from the literal/length tree
		if (sym == 256) { // End of block
			break;
		}
		else if(sym <= 255) { // Literal byte
			data.AppendData((byte_t)sym);
			m_oLookback.AppendByte((byte_t)sym);
		}
		else { // Offset distance and length
			if (alphabets.second == nullptr) {
				std::cerr << "Distance alphabet contains 0 entries, but symbol " << sym << " was found in the stream!\n";
				exit(1);
			}
			uint32_t len = DecodeLength(sym);
			uint32_t dist = DecodeDistance(DecodeSymbol(alphabets.second)); // Reading a symbol from the distance tree and parsing it
			m_oLookback.WriteToObject(dist, len, data); // Copying data from the lookback dictionary
		}
	} while (true);
	return data;
}

Node * PNGInflator::GenerateStaticLitLen()
{
	std::vector<uint32_t> temp(288);
	std::fill(temp.begin(), temp.begin() + 144, 8); // Elements 0-143 have value of 8
	std::fill(temp.begin() + 144, temp.begin() + 256, 9); // Elements 144-255 have value of 9
	std::fill(temp.begin() + 256, temp.begin() + 280, 7); // Elements 256-279 have value of 7
	std::fill(temp.begin() + 280, temp.end(), 8); // Elements 280-287 have value of 8
	LengthsSet litLen;
	LenghtsSetFromRange(litLen, temp.begin(), temp.end());
	return CreateHuffmanTree(litLen);
}

Node * PNGInflator::GenerateStaticDist()
{
	std::vector<uint32_t> temp(32, 5); // Vector of 32 elemnts with value 5
	LengthsSet distLen;
	LenghtsSetFromRange(distLen, temp.begin(), temp.end());
	return CreateHuffmanTree(distLen);
}

void PNGInflator::LenghtsSetFromRange(LengthsSet &set, const std::vector<uint32_t>::iterator &begin, const std::vector<uint32_t>::iterator &end)
{
	uint32_t index = 0;
	std::transform(begin, end, std::inserter(set, set.begin()), [&index](const uint32_t& len) {
		return std::make_pair(len, new Node(index++));
	});
}
