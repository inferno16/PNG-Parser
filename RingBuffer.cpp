#include "RingBuffer.h"

RingBuffer::RingBuffer(const size_t & size)
	: m_vData(size), m_uPosition(0), m_bBufferFull(false), m_uBufferSize(size)
{}

RingBuffer::~RingBuffer()
{
}

void RingBuffer::AppendData(const byte_t &byte)
{
	m_vData.at(m_uPosition) = byte;
	AdvanceCursor(m_uPosition);
	if (m_uPosition == 0 && !m_bBufferFull) // The position is 0 after AdvanceCursor when the cursor wraps
		m_bBufferFull = true;
}

void RingBuffer::AppendData(const binary_t & data)
{
	size_t size = data.size();
	if (size == 0)
		return;
	size_t offset = 0;
	const byte_t *vecPtr = data.data();
	byte_t *dataPtr = m_vData.data();
	while ((size - offset) + m_uPosition >= m_uBufferSize) {
		size_t remainingSize = m_uBufferSize - m_uPosition;
		//std::copy(data.begin() + offset, data.begin() + offset + remainingSize, m_vData.begin() + m_uPosition);
		memcpy_s(dataPtr + m_uPosition, remainingSize, vecPtr + offset, remainingSize);
		offset += remainingSize;
		m_uPosition = 0;
		m_bBufferFull = true;
	}
	//std::copy(data.begin() + offset, data.end(), m_vData.begin() + m_uPosition);
	size_t remaining = size - offset;
	memcpy_s(dataPtr + m_uPosition, remaining, vecPtr + offset, remaining);
	AdvanceCursor(m_uPosition, remaining);
	if (m_uPosition == 0 && !m_bBufferFull)
		m_bBufferFull = true;
}

#if 0
void RingBuffer::WriteToObject(const uint32_t & distance, const uint32_t & length, Binary & bObj)
{
	size_t readIndex = (m_uBufferSize + m_uPosition - distance) % m_uBufferSize;
	if (!m_bBufferFull && readIndex >= m_uPosition)
		throw "Trying to read part of a RingBuffer that is not set!";


	if (readIndex + length < m_uPosition && m_uPosition + length < m_uBufferSize) {
		binary_t vec(m_vData.begin() + readIndex, m_vData.begin() + length + readIndex);
		bObj.AppendData(vec);
		std::copy(vec.begin(), vec.end(), m_vData.begin() + m_uPosition);
		AdvanceCursor(m_uPosition, length);
		return;
	}
	for (size_t i = 0; i < length; i++)
	{
		byte_t byte = ReadByte(&readIndex);
		AppendData(byte);
		bObj.AppendData(byte);
	}
}
#else
void RingBuffer::WriteToObject(const uint32_t & distance, const uint32_t & length, Binary & bObj)
{
	size_t readIndex = (m_uBufferSize + m_uPosition - distance) % m_uBufferSize;
	if (!m_bBufferFull && readIndex >= m_uPosition)
		throw "Trying to read part of a RingBuffer that is not set!";

	size_t rangeSize = std::min(distance, length);
	binary_t range;
	if (rangeSize == 1) {
		range.assign(length, m_vData.at(readIndex));
		rangeSize = length;
	}
	else {
		range = GetRange(readIndex, rangeSize);
	}
	size_t appendedData = 0;
	do {
		if (rangeSize + appendedData > length) {
			range.erase(range.begin() + (length - appendedData), range.end());
			rangeSize = range.size();
		}
		AppendData(range);
		bObj.AppendData(range);
		appendedData += rangeSize;
	} while (appendedData < length);
}
#endif

byte_t RingBuffer::ReadByte(size_t *index)
{
	if (index == nullptr)
		index = &m_uPosition;
	byte_t byte = m_vData.at(*index);
	AdvanceCursor(*index);
	return byte;
}

inline void RingBuffer::AdvanceCursor(size_t &cursor, const size_t &amount)
{
	cursor = (cursor + amount) % m_uBufferSize;
}

binary_t RingBuffer::GetRange(const size_t &start, const size_t & size)
{
	binary_t range(size);
	byte_t *rangePtr = range.data();
	byte_t *dataPtr = m_vData.data();
	if ((start + size < m_uBufferSize)) {
		//std::copy(m_vData.begin() + start, m_vData.begin() + start + size, range.begin());
		memcpy_s(rangePtr, size, dataPtr + start, size);
	}
	else {
		//std::copy(m_vData.begin() + start, m_vData.end(), range.begin());
		size_t offset = m_uBufferSize - start;
		memcpy_s(rangePtr, offset, dataPtr + start, offset);
		size_t remaining = size - offset;
		memcpy_s(rangePtr + offset, remaining, dataPtr, remaining);
		//std::copy(m_vData.begin(), m_vData.begin() + (size - offset), range.begin() + offset);
	}
	return range;
}
