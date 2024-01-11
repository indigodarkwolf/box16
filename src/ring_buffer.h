#pragma once

#include <algorithm>
#include <atomic>
#include <functional>
#include <string.h>

template <typename T, size_t SIZE>
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

	const T &get(size_t index) const
	{
		return m_elems[(m_oldest + index) % SIZE];
	}

	T &get(size_t index)
	{
		return m_elems[(m_oldest + index) % SIZE];
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
		return get(m_count - !!m_count);
	}

	T &get_newest()
	{
		return get(m_count - !!m_count);
	}

	T &pop_newest()
	{
		m_count -= !!m_count;
		return get(m_count);
	}

	const T &operator[](size_t index) const
	{
		return get(index);
	}

	T &operator[](size_t index)
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
			for (size_t i = 0; i < m_count; ++i) {
				f(get(i));
			}
		}
	}

	void for_until(std::function<bool(const T &)> f) const
	{
		if (f != nullptr) {
			for (size_t i = 0; i < m_count; ++i) {
				if (!f(get(i))) {
					break;
				}
			}
		}
	}

	void for_each_reverse(std::function<void(const T &)> f) const
	{
		if (f != nullptr) {
			for (size_t i = m_count - 1; i < m_count; --i) {
				f(get(i));
			}
		}
	}

	void for_until_reverse(std::function<bool(const T &)> f) const
	{
		if (f != nullptr) {
			for (size_t i = m_count - 1; i < m_count; --i) {
				if (!f(get(i))) {
					break;
				}
			}
		}
	}

protected:
	size_t m_oldest;
	size_t m_count;
	T      m_elems[SIZE];
};

template <typename T, size_t SIZE>
class lazy_ring_buffer : public ring_buffer<T, SIZE>
{
private:
	using super = ring_buffer<T, SIZE>;

public:
	lazy_ring_buffer()
	    : super(),
	      m_lazy_count(0)
	{
		// Nothing to do.
	}

	T &allocate()
	{
		T &allocated = super::allocate();
		m_lazy_count = super::m_count;
		return allocated;
	}

	void add(const T &item)
	{
		super::add(item);
		m_lazy_count = super::m_count;
	}

	size_t lazy_count() const
	{
		return m_lazy_count;
	}

private:
	size_t m_lazy_count;
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

	const T &get(size_t index) const
	{
		return m_elems[(m_oldest + index) % m_size];
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
		return get(m_count - !!m_count);
	}

	const T &pop_newest()
	{
		m_count -= !!m_count;
		return get(m_count);
	}

	const T &operator[](size_t index) const
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

template <typename T, size_t SIZE, bool ALLOW_OVERWRITE = true>
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

	const T &get(size_t index) const
	{
		return m_elems[(m_oldest + index) % SIZE];
	}

	const T &operator[](size_t index) const
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
