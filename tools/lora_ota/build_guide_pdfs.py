#!/usr/bin/env python3
"""Build the French and English operator PDFs (requires Pandoc and Chromium)."""
from pathlib import Path
import subprocess
import tempfile
import re
from urllib.parse import urljoin

DOCS = Path(__file__).resolve().parents[2] / 'docs'
STYLE = '''<style>
@page { size: A4; margin: 15mm 14mm 17mm;
  @bottom-right { content: counter(page) " / " counter(pages); font: 9pt Arial; color: #666; }
}
html { font: 10.5pt Arial, sans-serif; color: #222; }
body { margin: 0; max-width: none; line-height: 1.3; }
h1 { font-size: 21pt; } h2 { font-size: 15pt; margin-top: 18pt; }
h3 { font-size: 12pt; } h1,h2,h3 { break-after: avoid; }
p { orphans: 3; widows: 3; }
a { color: #124c82; overflow-wrap: anywhere; }
table { border-collapse: collapse; width: 100%; font-size: 9pt; }
th,td { border: 1px solid #bbb; padding: 5pt; vertical-align: top; }
th { background: #eee; } tr { break-inside: avoid; }
thead { display: table-header-group; }
code { font: 8pt "DejaVu Sans Mono", monospace; overflow-wrap: anywhere; }
pre { padding: 7pt; border: 1px solid #ddd; background: #f6f6f6;
      white-space: pre-wrap; overflow-wrap: anywhere; break-inside: avoid; }
pre code { white-space: pre-wrap; font-size: 7.4pt; }
</style>'''


def main():
    for lang, stem in [('fr', 'LORA_OTA_GUIDE_fr'), ('en', 'LORA_OTA_GUIDE')]:
        source = DOCS / (stem + '.md')
        title = source.read_text().splitlines()[0].removeprefix('# ')
        with tempfile.TemporaryDirectory(prefix='ota-guide-') as folder:
            tmp = Path(folder)
            style = tmp / 'style.html'
            style.write_text(STYLE)
            html = tmp / 'guide.html'
            subprocess.run(['pandoc', str(source), '--standalone', '--section-divs',
                            '--metadata', 'lang=' + lang,
                            '--metadata', 'pagetitle=' + title,
                            '--include-in-header', str(style),
                            '--output', str(html)], check=True)
            # Resolve document links against the public branch, not the temporary HTML.
            base = 'https://github.com/moricef/LoRa_APRS_iGate/blob/experiment/lora-ota/docs/'
            html.write_text(re.sub(r'href="([^"]+)"',
                lambda m: 'href="' + (m[1] if m[1].startswith('#') else urljoin(base, m[1])) + '"',
                html.read_text()))
            result = subprocess.run([
                'chromium', '--headless', '--no-sandbox', '--disable-gpu',
                '--disable-dev-shm-usage', '--no-pdf-header-footer',
                '--user-data-dir=' + str(tmp / 'browser'),
                '--print-to-pdf=' + str(tmp / 'guide.pdf'),
                '--virtual-time-budget=1500', html.as_uri()
            ], capture_output=True, text=True, check=True)
            pdf = tmp / 'guide.pdf'
            if not pdf.exists() or pdf.stat().st_size < 10000:
                raise RuntimeError('Missing or incomplete PDF: ' + result.stderr)
            output = DOCS / (stem + '.pdf')
            output.write_bytes(pdf.read_bytes())
            print(output.name, output.stat().st_size, 'bytes', flush=True)


if __name__ == '__main__':
    main()
