#!/usr/bin/env python3
"""Regenerate both OTA presentation PDFs from their adjacent Markdown sources."""
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parent
STYLE = """<style>
@page { size:A4; margin:16mm 16mm 18mm;
  @bottom-right { content:counter(page) " / " counter(pages); font:9pt Arial; color:#666; }
}
html { font-family:Arial,sans-serif; font-size:12pt; color:#222; }
html[lang="en"] { font-size:11.5pt; }
body { margin:0; padding:0; max-width:none; font-size:inherit; line-height:1.22; }
h1 { font-size:20pt; line-height:1.14; margin:0 0 12pt; }
h2 { font-size:14pt; line-height:1.2; margin:17pt 0 7pt; }
h3 { font-size:12pt; margin:12pt 0 6pt; }
h1,h2,h3 { break-after:avoid; }
p { margin:0 0 7pt; orphans:3; widows:3; }
section.level2:last-child { break-inside:avoid; }
table { border-collapse:collapse; width:100%; margin:9pt 0 11pt;
        font-size:10.5pt; line-height:1.2; break-inside:avoid; }
th,td { border:1px solid #bbb; padding:6pt 7pt; vertical-align:top; }
th { background:#eee; font-weight:bold; }
th:first-child,td:first-child { width:37%; text-align:left; }
thead { display:table-header-group; }
tr { break-inside:avoid; }
code { font-family:"DejaVu Sans Mono",monospace; font-size:9pt;
       overflow-wrap:anywhere; white-space:normal; }
a { color:inherit; text-decoration:underline; }
</style>"""


def main():
    for lang, stem in [('fr', 'LoRa_APRS_Multihop_OTA_for_Jon_fr'),
                       ('en', 'LoRa_APRS_Multihop_OTA_for_Jon')]:
        source = ROOT / (stem + '.md')
        output = ROOT / (stem + '.pdf')
        title = source.read_text().splitlines()[0].removeprefix('# ')
        with tempfile.TemporaryDirectory(prefix='ota-pdf-') as folder:
            tmp = Path(folder)
            style = tmp / 'style.html'
            style.write_text(STYLE)
            html = tmp / 'document.html'
            subprocess.run(['pandoc', str(source), '--standalone', '--section-divs',
                            '--metadata', 'lang=' + lang,
                            '--metadata', 'pagetitle=' + title,
                            '--include-in-header', str(style),
                            '--output', str(html)], check=True)
            result = subprocess.run([
                'chromium', '--headless', '--no-sandbox', '--disable-gpu',
                '--disable-dev-shm-usage', '--no-pdf-header-footer',
                '--user-data-dir=' + str(tmp / 'browser'),
                '--print-to-pdf=' + str(tmp / 'document.pdf'),
                '--virtual-time-budget=1500', html.as_uri()
            ], capture_output=True, text=True)
            result.check_returncode()
            pdf = tmp / 'document.pdf'
            if not pdf.exists() or pdf.stat().st_size < 10000:
                raise RuntimeError('Missing or incomplete PDF: ' + result.stderr)
            output.write_bytes(pdf.read_bytes())
        print(output.name, output.stat().st_size, 'bytes', flush=True)


if __name__ == '__main__':
    main()
