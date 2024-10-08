#!/usr/bin/env python3

# Creates HTML for website based on template and markdown.

import argparse
from subprocess import run

from bs4 import BeautifulSoup, element

parser = argparse.ArgumentParser()
parser.add_argument("--template", required=True)
parser.add_argument("--markdown", required=True)
parser.add_argument("--output", default="-")
parser.add_argument("--url")
parser.add_argument("--title")
args = parser.parse_args()

with open(args.template, encoding="utf-8") as template_file:
    template_data = template_file.read()

soup = BeautifulSoup(template_data, "lxml")

# Update page title.
if args.title:
    soup.head.title.string = args.title
    for meta in soup.head.find_all("meta"):
        if meta.get("property", "") == "og:title":
            meta["content"] = args.title

kramdown_data = run(
    ["kramdown", args.markdown], capture_output=True, check=True, encoding="utf-8"
).stdout

kramdown_soup = BeautifulSoup(kramdown_data, "lxml")

# Remove anything between website-skip-start / website-skip-stop comments.
remove = False
for child in kramdown_soup.body.contents:
    if isinstance(child, element.Comment):
        comment = child.string.strip()
        if comment == "website-skip-start":
            remove = True
        elif comment == "website-skip-stop":
            remove = False
        child.extract()
    if remove:
        if isinstance(child, element.Tag):
            child.decompose()
        elif not isinstance(child, element.Comment):
            child.extract()

# Fix links.
if args.url:
    url = args.url
    if url[-1] == "/":
        url = url[:-1]
    for link in kramdown_soup.body.find_all("a"):
        # Retarget markdown links to HTML.
        if link["href"].endswith(".md"):
            link["href"] = link["href"].split("/")[-1][:-3].lower() + ".html"
        # Make external URLs open in separate tabs.
        elif link["href"] != url and not link["href"].startswith(url + "/"):
            link["target"] = "_blank"

replacement_point = soup.find("insert-kramdown-here")
if replacement_point:
    new_contents = kramdown_soup.body
    new_contents.name = "div"
    replacement_point.replace_with(new_contents)
else:
    soup.body.replace_with(kramdown_soup.body)

output_str = soup.prettify()

if args.output != "-":
    with open(args.output, "w", encoding="utf-8") as f:
        f.write(output_str)
else:
    print(output_str)
