from pathlib import Path

page = Path("dist/index.html")
html = page.read_text(encoding="utf-8")
guard = Path(".github/site-access-guard.js").read_text(encoding="utf-8")

if "</head>" not in html or "</body>" not in html:
    raise SystemExit("Live index.html is missing closing head/body tags")

# The access guard still checks permissions, but does not hide the public timetable
# while Firebase initializes. If access is denied, the guard replaces the page.
script = '<script id="tobiservices-access-guard" type="module">\n' + guard + '\n</script>\n'
html = html.replace("</body>", script + "</body>", 1)
page.write_text(html, encoding="utf-8")
