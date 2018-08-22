#include "RingBuffer.h"

RingBuffer::RingBuffer(const size_t & size)
	: m_vData(size), m_uPosition(0)
{}

RingBuffer::~RingBuffer()
{
}

void RingBuffer::AppendByte(const byte_t &byte)
{
	m_vData.at(m_uPosition) = byte;
	AdvanceCursor(m_uPosition);
}

void RingBuffer::WriteToObject(const uint32_t & distance, const uint32_t & length, Binary & bObj)
{
	size_t readIndex = (m_vData.size() + m_uPosition - distance) % m_vData.size();
	for (size_t i = 0; i < length; i++)
	{
		byte_t byte = ReadByte(&readIndex);
		AppendByte(byte);
		bObj.AppendData(byte);
	}
}

byte_t RingBuffer::ReadByte(size_t *index)
{
	if (index == nullptr)
		index = &m_uPosition;
	byte_t byte = m_vData.at(*index);
	AdvanceCursor(*index);
	return byte;
}

inline void RingBuffer::AdvanceCursor(size_t &cursor)
{
	cursor = (cursor + 1) % m_vData.size();
}
