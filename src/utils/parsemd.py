from typing import List
import re

MD_IMAGE_PATTERN = re.compile('[!][[]([a-zA-Z0-9 .,:;\'"|{}@#$%^&*()_\\/\\\\]*)[]][(]([a-zA-Z0-9 .,_\\/]+)[)]')


class MarkdownElement:
    pass


class MarkdownHeaderElement(MarkdownElement):
    def __init__(self, tier: int, text: str):
        self.tier = tier
        self.text = text


class MarkdownParagraphElement(MarkdownElement):
    def __init__(self, text: str):
        self.text = text


class MarkdownImageElement(MarkdownElement):
    def __init__(self, path: str, desc: str):
        self.path = path
        self.desc = desc


def parseMarkdown(text: str) -> List[MarkdownElement]:
    sections = text.strip().replace('\r', '\n').split('\n\n')

    output_li = []
    for i in sections:
        if i.startswith('#'):
            pounds, s = i.split(' ', 1)
            output_li.append(MarkdownHeaderElement(pounds.count('#'), s.strip()))

        elif MD_IMAGE_PATTERN.match(i) is not None:
            desc, path = MD_IMAGE_PATTERN.match(i).groups()
            output_li.append(MarkdownImageElement(path, desc.strip()))

        else:
            output_li.append(MarkdownParagraphElement(i.strip().replace('\n', ' ')))

    return output_li
