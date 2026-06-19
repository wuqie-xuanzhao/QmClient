class LanguageDecodeError(Exception):
    def __init__(self, message, filename, line):
        error = f'File "{filename}", line {line + 1}: {message}'
        super().__init__(error)


def decode(fileobj, elements_per_key):
    data = {}
    current_context = ""
    current_key = None
    index = -1
    for index, line in enumerate(fileobj):
        line = line.encode("utf-8").decode("utf-8-sig")
        line = line[:-1]
        if line and line[-1] == "\r":
            line = line[:-1]
        if not line or line[:1] == "#":
            current_context = ""
            continue

        if line.startswith("[") and not line.startswith("[%"):
            if line[-1] != "]":
                raise LanguageDecodeError("Invalid context string", fileobj.name, index)
            current_context = line[1:-1]
        elif line[:3] == "== ":
            if len(data[current_key]) >= 1 + elements_per_key:
                raise LanguageDecodeError(
                    "Wrong number of elements per key", fileobj.name, index
                )
            if current_key:
                translation = line[3:]
                data[current_key].extend([translation])
            else:
                raise LanguageDecodeError(
                    "Element before key given", fileobj.name, index
                )
        else:
            if current_key:
                if len(data[current_key]) != 1 + elements_per_key:
                    raise LanguageDecodeError(
                        "Wrong number of elements per key", fileobj.name, index
                    )
                data[current_key].append(index - 1 if current_context else index)
            if line in data:
                raise LanguageDecodeError(
                    "Key defined multiple times: " + line, fileobj.name, index
                )
            data[(line, current_context)] = [index - 1 if current_context else index]
            current_key = (line, current_context)
    if current_key is None:
        return {}
    if len(data[current_key]) != 1 + elements_per_key:
        raise LanguageDecodeError(
            "Wrong number of elements per key", fileobj.name, index
        )
    data[current_key].append(index + 1)
    new_data = {}
    for key, value in data.items():
        if key[0]:
            new_data[key] = value
    return new_data


def languages():
    with open("data/languages/index.txt", encoding="utf-8") as f:
        index = decode(f, 3)
    langs = {
        "data/languages/" + key[0] + ".txt": [key[0]] + elements
        for key, elements in index.items()
    }
    return langs


def translations(filename):
    with open(filename, encoding="utf-8") as f:
        return decode(f, 1)


def localizes():
    import source_keys

    return sorted(source_keys.collect_source_key_identities())
