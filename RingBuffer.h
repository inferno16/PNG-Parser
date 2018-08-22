#pragma once
#include <vector>
#include <Binary.h>

class RingBuffer
{
public:
	RingBuffer(const size_t &size);
	~RingBuffer();
	void AppendByte(const byte_t &byte);
	void WriteToObject(const uint32_t &distance, const uint32_t &length, Binary &bObj);

private: // Methods
	byte_t ReadByte(size_t *index = nullptr);
	inline void AdvanceCursor(size_t &cursor);

private: // Variables
	binary_t m_vData;
	size_t m_uPosition;
};

