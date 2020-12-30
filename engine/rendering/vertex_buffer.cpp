
#include "vertex_buffer.h"

namespace al::engine
{
    BufferElement::BufferElement() noexcept
        : size{ 0 }
        , offset{ 0 }
        , isNormalized{ false }
        , isInitialized{ false }
    { }

    BufferElement::BufferElement(ShaderDataType type, bool isNormalized) noexcept
        : size          { ShaderDataTypeSize[static_cast<uint32_t>(type)] }
        , offset        { 0 }
        , type          { type }
        , isNormalized  { isNormalized }
        , isInitialized { true }
    { }

    BufferLayout::BufferLayout(const BufferLayout::ElementContainer& elements) noexcept
        : elements{ elements }
    {
        calculate_offset_and_stride();
    }

    const std::size_t                               BufferLayout::get_stride()      const noexcept  { return stride; }
    const BufferLayout::ElementContainer&           BufferLayout::get_elements()    const noexcept  { return elements; }

    BufferLayout::ElementContainer::iterator        BufferLayout::begin()                 noexcept  { return elements.begin(); }
    BufferLayout::ElementContainer::iterator        BufferLayout::end()                   noexcept  { return elements.end(); }
    BufferLayout::ElementContainer::const_iterator  BufferLayout::begin()           const noexcept  { return elements.begin(); }
    BufferLayout::ElementContainer::const_iterator  BufferLayout::end()             const noexcept  { return elements.end(); }

    void BufferLayout::calculate_offset_and_stride() noexcept
    {
        std::size_t offset = 0;
        for (auto& element : elements)
        {
            if (!element.isInitialized) { break; }
            element.offset = offset;
            offset += element.size;
        }
        stride = offset;
    }
}