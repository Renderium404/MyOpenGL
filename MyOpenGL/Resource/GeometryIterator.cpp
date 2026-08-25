#include "GeometryIterator.h"

#include <cassert>

GeometryAttributeIteratorAccessor::GeometryAttributeIteratorAccessor(const void* owner, GLuint location)
    : m_owner(owner),
      m_location(location)
{
}

GeometryAttributeIteratorAccessor::~GeometryAttributeIteratorAccessor()
{
}

const void* GeometryAttributeIteratorAccessor::owner() const
{
    return m_owner;
}

GLuint GeometryAttributeIteratorAccessor::location() const
{
    return m_location;
}

GeometryIndexIteratorAccessor::GeometryIndexIteratorAccessor(const void* owner)
    : m_owner(owner)
{
}

GeometryIndexIteratorAccessor::~GeometryIndexIteratorAccessor()
{
}

const void* GeometryIndexIteratorAccessor::owner() const
{
    return m_owner;
}

AttributeValue::AttributeValue()
    : m_index(0)
{
}

AttributeValue::AttributeValue(const std::shared_ptr<const GeometryAttributeIteratorAccessor>& accessor, std::size_t index)
    : m_accessor(accessor),
      m_index(index)
{
}

bool AttributeValue::valid() const
{
    return m_accessor && m_index < m_accessor->size();
}

int AttributeValue::componentCount() const
{
    return m_accessor ? m_accessor->componentCount() : 0;
}

GLfloat AttributeValue::operator[](int component) const
{
    assert(m_accessor);
    assert(m_index < m_accessor->size());
    assert(component >= 0 && component < m_accessor->componentCount());

    return m_accessor->value(m_index, component);
}

AttributeIterator::AttributeIterator()
    : m_index(0)
{
}

AttributeIterator::AttributeIterator(const std::shared_ptr<const GeometryAttributeIteratorAccessor>& accessor, std::size_t index)
    : m_accessor(accessor),
      m_index(static_cast<difference_type>(index))
{
}

AttributeIterator::reference AttributeIterator::operator*() const
{
    assert(m_accessor);
    assert(m_index >= 0);
    assert(static_cast<std::size_t>(m_index) < m_accessor->size());

    return AttributeValue(m_accessor, static_cast<std::size_t>(m_index));
}

AttributeIterator::reference AttributeIterator::operator[](difference_type offset) const
{
    return *(*this + offset);
}

AttributeIterator& AttributeIterator::operator++()
{
    ++m_index;
    return *this;
}

AttributeIterator AttributeIterator::operator++(int)
{
    AttributeIterator old = *this;
    ++(*this);
    return old;
}

AttributeIterator& AttributeIterator::operator--()
{
    --m_index;
    return *this;
}

AttributeIterator AttributeIterator::operator--(int)
{
    AttributeIterator old = *this;
    --(*this);
    return old;
}

AttributeIterator& AttributeIterator::operator+=(difference_type offset)
{
    m_index += offset;
    return *this;
}

AttributeIterator& AttributeIterator::operator-=(difference_type offset)
{
    m_index -= offset;
    return *this;
}

AttributeIterator AttributeIterator::operator+(difference_type offset) const
{
    AttributeIterator result = *this;
    result += offset;
    return result;
}

AttributeIterator AttributeIterator::operator-(difference_type offset) const
{
    AttributeIterator result = *this;
    result -= offset;
    return result;
}

AttributeIterator::difference_type AttributeIterator::operator-(const AttributeIterator& other) const
{
    assert(sameRange(other));
    return m_index - other.m_index;
}

bool AttributeIterator::operator==(const AttributeIterator& other) const
{
    if (!m_accessor && !other.m_accessor)
        return true;

    return sameRange(other) && m_index == other.m_index;
}

bool AttributeIterator::operator!=(const AttributeIterator& other) const
{
    return !(*this == other);
}

bool AttributeIterator::operator<(const AttributeIterator& other) const
{
    assert(sameRange(other));
    return m_index < other.m_index;
}

bool AttributeIterator::operator<=(const AttributeIterator& other) const
{
    assert(sameRange(other));
    return m_index <= other.m_index;
}

bool AttributeIterator::operator>(const AttributeIterator& other) const
{
    return other < *this;
}

bool AttributeIterator::operator>=(const AttributeIterator& other) const
{
    return other <= *this;
}

bool AttributeIterator::valid() const
{
    return m_accessor && m_index >= 0 && static_cast<std::size_t>(m_index) < m_accessor->size();
}

int AttributeIterator::componentCount() const
{
    return m_accessor ? m_accessor->componentCount() : 0;
}

bool AttributeIterator::sameRange(const AttributeIterator& other) const
{
    if (!m_accessor || !other.m_accessor)
        return m_accessor == other.m_accessor;

    return m_accessor->owner() == other.m_accessor->owner() && m_accessor->location() == other.m_accessor->location();
}

IndexIterator::IndexIterator()
    : m_index(0)
{
}

IndexIterator::IndexIterator(const std::shared_ptr<const GeometryIndexIteratorAccessor>& accessor, std::size_t index)
    : m_accessor(accessor),
      m_index(static_cast<difference_type>(index))
{
}

IndexIterator::reference IndexIterator::operator*() const
{
    assert(m_accessor);
    assert(m_index >= 0);
    assert(static_cast<std::size_t>(m_index) < m_accessor->size());

    return m_accessor->value(static_cast<std::size_t>(m_index));
}

IndexIterator::reference IndexIterator::operator[](difference_type offset) const
{
    assert(m_accessor);

    const difference_type index = m_index + offset;

    assert(index >= 0);
    assert(static_cast<std::size_t>(index) < m_accessor->size());

    return m_accessor->value(static_cast<std::size_t>(index));
}

IndexIterator& IndexIterator::operator++()
{
    ++m_index;
    return *this;
}

IndexIterator IndexIterator::operator++(int)
{
    IndexIterator old = *this;
    ++(*this);
    return old;
}

IndexIterator& IndexIterator::operator--()
{
    --m_index;
    return *this;
}

IndexIterator IndexIterator::operator--(int)
{
    IndexIterator old = *this;
    --(*this);
    return old;
}

IndexIterator& IndexIterator::operator+=(difference_type offset)
{
    m_index += offset;
    return *this;
}

IndexIterator& IndexIterator::operator-=(difference_type offset)
{
    m_index -= offset;
    return *this;
}

IndexIterator IndexIterator::operator+(difference_type offset) const
{
    IndexIterator result = *this;
    result += offset;
    return result;
}

IndexIterator IndexIterator::operator-(difference_type offset) const
{
    IndexIterator result = *this;
    result -= offset;
    return result;
}

IndexIterator::difference_type IndexIterator::operator-(const IndexIterator& other) const
{
    assert(sameRange(other));
    return m_index - other.m_index;
}

bool IndexIterator::operator==(const IndexIterator& other) const
{
    if (!m_accessor && !other.m_accessor)
        return true;

    return sameRange(other) && m_index == other.m_index;
}

bool IndexIterator::operator!=(const IndexIterator& other) const
{
    return !(*this == other);
}

bool IndexIterator::operator<(const IndexIterator& other) const
{
    assert(sameRange(other));
    return m_index < other.m_index;
}

bool IndexIterator::operator<=(const IndexIterator& other) const
{
    assert(sameRange(other));
    return m_index <= other.m_index;
}

bool IndexIterator::operator>(const IndexIterator& other) const
{
    return other < *this;
}

bool IndexIterator::operator>=(const IndexIterator& other) const
{
    return other <= *this;
}

bool IndexIterator::valid() const
{
    return m_accessor && m_index >= 0 && static_cast<std::size_t>(m_index) < m_accessor->size();
}

bool IndexIterator::sameRange(const IndexIterator& other) const
{
    if (!m_accessor || !other.m_accessor)
        return m_accessor == other.m_accessor;

    return m_accessor->owner() == other.m_accessor->owner();
}