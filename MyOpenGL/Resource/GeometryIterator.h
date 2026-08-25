#ifndef GEOMETRYITERATOR_H
#define GEOMETRYITERATOR_H

#include <cstddef>
#include <iterator>
#include <memory>

#include <QtGui/qopengl.h>

class GeometryAttributeIteratorAccessor
{
public:
    GeometryAttributeIteratorAccessor(const void* owner, GLuint location);
    virtual ~GeometryAttributeIteratorAccessor();

    const void* owner() const;
    GLuint location() const;

    virtual std::size_t size() const = 0;
    virtual int componentCount() const = 0;
    virtual GLfloat value(std::size_t index, int component) const = 0;

private:
    const void* m_owner;
    GLuint m_location;
};

class GeometryIndexIteratorAccessor
{
public:
    explicit GeometryIndexIteratorAccessor(const void* owner);
    virtual ~GeometryIndexIteratorAccessor();

    const void* owner() const;

    virtual std::size_t size() const = 0;
    virtual GLuint value(std::size_t index) const = 0;

private:
    const void* m_owner;
};

class AttributeValue
{
public:
    AttributeValue();
    AttributeValue(const std::shared_ptr<const GeometryAttributeIteratorAccessor>& accessor, std::size_t index);

    bool valid() const;
    int componentCount() const;
    GLfloat operator[](int component) const;

private:
    std::shared_ptr<const GeometryAttributeIteratorAccessor> m_accessor;
    std::size_t m_index;
};

class AttributeIterator
{
public:
    using iterator_category = std::random_access_iterator_tag;
    using value_type = AttributeValue;
    using difference_type = std::ptrdiff_t;
    using pointer = void;
    using reference = AttributeValue;

    AttributeIterator();
    AttributeIterator(const std::shared_ptr<const GeometryAttributeIteratorAccessor>& accessor, std::size_t index);

    reference operator*() const;
    reference operator[](difference_type offset) const;

    AttributeIterator& operator++();
    AttributeIterator operator++(int);
    AttributeIterator& operator--();
    AttributeIterator operator--(int);

    AttributeIterator& operator+=(difference_type offset);
    AttributeIterator& operator-=(difference_type offset);

    AttributeIterator operator+(difference_type offset) const;
    AttributeIterator operator-(difference_type offset) const;
    difference_type operator-(const AttributeIterator& other) const;

    bool operator==(const AttributeIterator& other) const;
    bool operator!=(const AttributeIterator& other) const;
    bool operator<(const AttributeIterator& other) const;
    bool operator<=(const AttributeIterator& other) const;
    bool operator>(const AttributeIterator& other) const;
    bool operator>=(const AttributeIterator& other) const;

    bool valid() const;
    int componentCount() const;

private:
    bool sameRange(const AttributeIterator& other) const;

private:
    std::shared_ptr<const GeometryAttributeIteratorAccessor> m_accessor;
    difference_type m_index;
};

class IndexIterator
{
public:
    using iterator_category = std::random_access_iterator_tag;
    using value_type = GLuint;
    using difference_type = std::ptrdiff_t;
    using pointer = void;
    using reference = GLuint;

    IndexIterator();
    IndexIterator(const std::shared_ptr<const GeometryIndexIteratorAccessor>& accessor, std::size_t index);

    reference operator*() const;
    reference operator[](difference_type offset) const;

    IndexIterator& operator++();
    IndexIterator operator++(int);
    IndexIterator& operator--();
    IndexIterator operator--(int);

    IndexIterator& operator+=(difference_type offset);
    IndexIterator& operator-=(difference_type offset);

    IndexIterator operator+(difference_type offset) const;
    IndexIterator operator-(difference_type offset) const;
    difference_type operator-(const IndexIterator& other) const;

    bool operator==(const IndexIterator& other) const;
    bool operator!=(const IndexIterator& other) const;
    bool operator<(const IndexIterator& other) const;
    bool operator<=(const IndexIterator& other) const;
    bool operator>(const IndexIterator& other) const;
    bool operator>=(const IndexIterator& other) const;

    bool valid() const;

private:
    bool sameRange(const IndexIterator& other) const;

private:
    std::shared_ptr<const GeometryIndexIteratorAccessor> m_accessor;
    difference_type m_index;
};

#endif // GEOMETRYITERATOR_H