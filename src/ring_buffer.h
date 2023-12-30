#pragma once

#include <algorithm>
#include <atomic>
#include <functional>
#include <string.h>

template <typename T, int SIZE>
class ring_buffer
{
public:
	ring_buffer()
	    : m_oldest(0), m_count(0)
	{
		// Nothing to do.
	}

	void clear()
	{
		m_oldest = 0;
		m_count  = 0;
	}

	T &allocate()
	{
		const size_t index = (m_oldest + m_count) % SIZE;
		if (m_count < SIZE) {
			++m_count;
		} else {
			m_oldest = (m_oldest + 1) % SIZE;
		}
		return m_elems[index];
	}

	void add(const T &item)
	{
		auto &elem = allocate();
		elem       = item;
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

	const T &pop_newest()
	{
		m_count -= !!m_count;
		return get(static_cast<int>(m_count));
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

	void for_each(std::function<void(const T &)> f) const
	{
		if (f != nullptr) {
			for (int i = 0; i < m_count; ++i) {
				f(get(i));
			}
		}
	}

	void for_until(std::function<bool(const T &)> f) const
	{
		if (f != nullptr) {
			for (int i = 0; i < m_count; ++i) {
				if (!f(get(i))) {
					break;
				}
			}
		}
	}

	void for_each_reverse(std::function<void(const T &)> f) const
	{
		if (f != nullptr) {
			for (int i = (int)m_count - 1; i > 0; --i) {
				f(get(i));
			}
		}
	}

	void for_until_reverse(std::function<bool(const T &)> f) const
	{
		if (f != nullptr) {
			for (int i = (int)m_count - 1; i > 0; --i) {
				if (!f(get(i))) {
					break;
				}
			}
		}
	}

private:
	size_t m_oldest;
	size_t m_count;
	T      m_elems[SIZE];
};

template <typename T>
class dynamic_ring_buffer
{
public:
	dynamic_ring_buffer(size_t size)
	    : m_size(size), m_oldest(0), m_count(0), m_elems(new T[size])
	{
		// Nothing to do.
	}

	T &allocate()
	{
		const size_t index = (m_oldest + m_count) % m_size;
		if (m_count < m_size) {
			++m_count;
		} else {
			m_oldest = (m_oldest + 1) % m_size;
		}
		return m_elems[index];
	}

	void add(const T &item)
	{
		auto &elem = allocate();
		elem       = item;
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
		return m_elems[(m_oldest + m_count - 1) % m_size];
	}

	const T &pop_newest()
	{
		m_count -= !!m_count;
		return get(static_cast<int>(m_count));
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
	size_t       m_oldest;
	size_t       m_count;
	T           *m_elems;
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

	T *allocate()
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
	std::atomic<size_t> m_oldest;
	std::atomic<size_t> m_count;
	T                   m_elems[SIZE];
};
