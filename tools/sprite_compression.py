"""Independent-frame LZ4 blocks over lossless palette indices; no runtime dependency."""

def encode_lz(indices: bytes) -> bytes:
    output = bytearray()
    table = {}
    anchor = position = 0
    size = len(indices)

    def length(value):
        while value >= 255:
            output.append(255)
            value -= 255
        output.append(value)

    def emit(end, match=0, distance=0):
        literals = end - anchor
        output.append((min(literals, 15) << 4) | min(max(match - 4, 0), 15))
        if literals >= 15:
            length(literals - 15)
        output.extend(indices[anchor:end])
        if match:
            output.extend((distance & 255, distance >> 8))
            if match >= 19:
                length(match - 19)

    while position + 4 <= size:
        key = indices[position:position + 4]
        previous = table.get(key)
        table[key] = position
        if previous is None or position - previous > 65535:
            position += 1
            continue
        match = 4
        while position + match < size and indices[previous + match] == indices[position + match]:
            match += 1
        emit(position, match, position - previous)
        end = position + match
        for index in range(position + 1, min(end, size - 3)):
            table[indices[index:index + 4]] = index
        anchor = position = end
    emit(size)
    return bytes(output)
