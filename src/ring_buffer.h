#pragma once

#include <string.h>
#include <algorithm>

template <typename T, int SIZE, bool ALLOW_OVERWRITE = true>
class ring_buffer
{
public:
	ring_buffer()
	    : m_oldest(0), m_count(0)
	{
		// Nothing to do.
	}

	void add(const T &item)
	{
		const size_t index = (m_oldest + m_count) % SIZE;
		if constexpr (ALLOW_OVERWRITE) {
			if (m_count < SIZE) {
				++m_count;
			} else {
				m_oldest = (m_oldest + 1) % SIZE;
			}
		} else {
			if (m_count >= SIZE) {
				return;
			}
			++m_count;
		}
		m_elems[index] = item;
	}

	const T &get_oldest() const
	{
		return m_elems[m_oldest];
	}

	const T &pop_oldest()
	{
		const T &value = m_elems[m_oldest];
		if (m_count > 0) {
			m_oldest = (m_oldest + 1) % SIZE;
			--m_count;
		}
		return value;
	}

	const T &get_newest() const
	{
		return m_elems[(m_oldest + m_count - 1) % SIZE];
	}

	const T &get(int index) const
	{
		return m_elems[(m_oldest + index) % SIZE];
	}

	const T &operator[](int index) const
	{
		return get(index);
	}

	const size_t count() const
	{
		return m_count;
	}

	const size_t size_remaining() const
	{
		return SIZE - m_count;
	}

private:
	size_t m_oldest;
	size_t m_count;
	T      m_elems[SIZE];
};

template <typename T, bool ALLOW_OVERWRITE = true>
class dynamic_ring_buffer
{
public:
	dynamic_ring_buffer(size_t size)
	    : m_size(size), m_oldest(0), m_count(0), m_elems(new T[size])
	{
		// Nothing to do.
	}

	void add(const T &item)
	{
		const size_t index = (m_oldest + m_count) % m_size;
		if constexpr (ALLOW_OVERWRITE) {
			if (m_count < m_size) {
				++m_count;
			} else {
				m_oldest = (m_oldest + 1) % m_size;
			}
		} else {
			if (m_count >= m_size) {
				return;
			}
			++m_count;
		}
		m_elems[index] = item;
	}

	const T &get_oldest() const
	{
		return m_elems[m_oldest];
	}

	const T &pop_oldest()
	{
		const T &value = m_elems[m_oldest];
		if (m_count > 0) {
			m_oldest = (m_oldest + 1) % m_size;
			--m_count;
		}
		return value;
	}

	const T &get_newest() const
	{
		return m_elems[(m_oldest + m_count - 1) & m_size];
	}

	const T &get(int index) const
	{
		return m_elems[(m_oldest + index) % m_size];
	}

	const T &operator[](int index) const
	{
		return get(index);
	}

	const size_t count() const
	{
		return m_count;
	}

	const size_t size_remaining() const
	{
		return m_size - m_count;
	}

private:
	const size_t m_size;
	size_t m_oldest;
	size_t m_count;
	T      *m_elems;
};

template <typename T, int SIZE, bool ALLOW_OVERWRITE = true>
class ring_allocator
{
public:
	ring_allocator()
	    : m_oldest(0), m_count(0)
	{
		// Nothing to do.
	}

	T* allocate()
	{
		const size_t index = (m_oldest + m_count) % SIZE;
		if constexpr (ALLOW_OVERWRITE) {
			if (m_count < SIZE) {
				++m_count;
			} else {
				m_oldest = (m_oldest + 1) % SIZE;
			}
		} else {
			if (m_count >= SIZE) {
				return nullptr;
			}
			++m_count;
		}
		return &m_elems[index];
	}

	const T *get_oldest() const
	{
		if (m_count == 0) {
			return nullptr;
		}
		return &m_elems[m_oldest];
	}

	void free_oldest()
	{
		if (m_count > 0) {
			--m_count;
			m_oldest = (m_oldest + 1) % SIZE;
		}
	}

	const T &get(int index) const
	{
		return m_elems[(m_oldest + index) % SIZE];
	}

	const T &operator[](int index) const
	{
		return get(index);
	}

	const size_t count() const
	{
		return m_count;
	}

	const size_t size_remaining() const
	{
		return SIZE - m_count;
	}

private:
	volatile size_t m_oldest;
	volatile size_t m_count;
	T      m_elems[SIZE];
};
