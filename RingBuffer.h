#pragma once
#include <vector>
#include <Binary.h>

class RingBuffer
{
public:
	RingBuffer(const size_t &size);
	~RingBuffer();
	void AppendData(const byte_t &byte);
	void AppendData(const binary_t &data);
	void WriteToObject(const uint32_t &distance, const uint32_t &length, Binary &bObj);

private: // Methods
	byte_t ReadByte(size_t *index = nullptr);
	inline void AdvanceCursor(size_t &cursor, const size_t &amount = 1);
	binary_t GetRange(const size_t &start, const size_t &size);

private: // Variables
	binary_t m_vData;
	size_t m_uPosition;
	size_t m_uBufferSize;
	bool m_bBufferFull;
};

