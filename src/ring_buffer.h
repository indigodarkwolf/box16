#pragma once

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
		m_oldest = (m_oldest + 1) % SIZE;
		--m_count;
		return value;
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
