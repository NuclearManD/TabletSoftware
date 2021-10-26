
def splitlines(text, chr, num_chars_per_line):
    li = []
    chars_in_line = 0
    last_space_index = -1
    this_line_start = 0
    for i in range(len(text)):
        if text[i] == chr:
            last_space_index = i
        chars_in_line += 1

        # More than instead of more than or equal to, because one of
        # these characters (the separator) will be deleted.  So the number
        # of chars in the line is really one less than chars_in_line.
        if chars_in_line > num_chars_per_line:
            # Try to split
            if last_space_index != -1:
                # Split
                li.append(text[this_line_start:last_space_index])

                # Start a new line
                this_line_start = last_space_index + 1
                chars_in_line = i - this_line_start
                last_space_index = -1

    li.append(text[this_line_start:])
    return li
