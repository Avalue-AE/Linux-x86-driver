# Supported boards

This table lists every board this driver supports. It is
generated from each board's own configuration file, supplied with
the board rather than bundled with every copy of this source.
`driver` lists the subsystems this board's own config
file actually builds: a name appears only when its `.conf` sets
both the device and the chipset for that subsystem, the same rule
the build itself uses -- a board never gets credit here for a
module its own build would skip. `verified` and `date` come only
from each board's own `# STATUS:` comment line: `Yes` means that
board has actually been put on a bench, `No` means it has not
(including every board whose file carries no `# STATUS:` line at
all -- a missing marker is not evidence of testing).

| name | driver | verified | date |
|---|---|---|---|
| ACP-BYT2C |  | No |  |
| ADP-226-01 | GPIO | No |  |
| ADP-226 | GPIO | No |  |
| ARC-ADLN | WDT, HWM | No |  |
| ARC-APL | WDT, HWM | No |  |
| ARC-SKLU | WDT, HWM, GPIO | No |  |
| BBM-BSW | WDT, HWM, GPIO | No |  |
| BCX11 | GPIO | No |  |
| BMX-T526-VX11 | WDT, HWM | No |  |
| CVA4 | WDT, HWM, GPIO | No |  |
| EAX-C236KP | WDT, HWM, GPIO | No |  |
| EAX-C246P | WDT, HWM, GPIO | No |  |
| EAX-Q170KP | WDT, HWM, GPIO | No |  |
| EAX-Q170P | WDT, HWM, GPIO | No |  |
| EBM-APL | WDT, HWM, GPIO | No |  |
| EBM-APLV | WDT, HWM | No |  |
| EBM-BYT | WDT, HWM, GPIO | No |  |
| EBM-BYTS | WDT, HWM, GPIO | No |  |
| EBM-BYTV | WDT, HWM, GPIO | No |  |
| EBM-EHLR | WDT, HWM | No |  |
| EBM-EHLS | WDT, HWM | No |  |
| EBM-SKLU | WDT, HWM, GPIO | No |  |
| EBM-SKLUS | WDT, HWM | No |  |
| EBM-TGL | WDT, HWM, GPIO | No |  |
| EBM-TGLS | WDT, HWM, GPIO | No |  |
| ECM-APL | WDT, HWM, GPIO | No |  |
| ECM-APL2 | WDT, HWM, GPIO | No |  |
| ECM-ARL | WDT, HWM, GPIO | No |  |
| ECM-ASL | WDT, HWM, GPIO, MISC | No |  |
| ECM-BYT | WDT, HWM, GPIO | No |  |
| ECM-BYT2 | WDT, HWM, GPIO | No |  |
| ECM-CFS | WDT, HWM, GPIO | No |  |
| ECM-EHL | WDT, HWM, GPIO | No |  |
| ECM-EHL3 | WDT, HWM, GPIO | No |  |
| ECM-MTL | WDT, HWM, GPIO | No |  |
| ECM-RPL | WDT, HWM, GPIO | No |  |
| ECM-SKLH | WDT, HWM, GPIO | No |  |
| ECM-SKLU | WDT, HWM, GPIO | No |  |
| ECM-TGU | WDT, HWM, GPIO | No |  |
| ECM-TGUC | WDT, HWM, GPIO | No |  |
| ECM-TWL | WDT, HWM, GPIO | No |  |
| ECM-TWL3 | WDT, HWM, GPIO | No |  |
| ECM-WHL | WDT, HWM, GPIO | Yes | 2026-08-07 |
| EMS-ARH | WDT, HWM, GPIO, MISC | No |  |
| EMS-MTH | WDT, HWM, GPIO | No |  |
| EMS-MTU | WDT, HWM, GPIO | No |  |
| EMX-APLP | WDT, HWM, GPIO | No |  |
| EMX-ASLP | WDT, HWM, GPIO | No |  |
| EMX-BYT2 | WDT, HWM, GPIO | No |  |
| EMX-BYT3 | WDT, HWM, GPIO | No |  |
| EMX-C246P | WDT, HWM, GPIO | No |  |
| EMX-H110KP | WDT, HWM, GPIO | No |  |
| EMX-H110P | WDT, HWM, GPIO | No |  |
| EMX-H310DP | WDT, HWM, GPIO | No |  |
| EMX-H310P | WDT, HWM, GPIO | No |  |
| EMX-KX60G | WDT, HWM | No |  |
| EMX-MTLP | WDT, HWM, GPIO | No |  |
| EMX-Q170B | WDT, HWM, GPIO | No |  |
| EMX-Q170KP | WDT, HWM, GPIO | No |  |
| EMX-Q170P | WDT, HWM, GPIO | No |  |
| EMX-RPLP | WDT, HWM, GPIO | No |  |
| EMX-SKLGP | WDT, HWM, GPIO | No |  |
| EMX-SKLU | WDT, HWM, GPIO | No |  |
| EMX-SKLUP | WDT, HWM, GPIO | No |  |
| EMX-TGLP | WDT, HWM, GPIO | No |  |
| EMX-VX11 | WDT, HWM, GPIO | No |  |
| EMX-W880P | WDT, HWM, GPIO | No |  |
| EMX-ZXEDP | WDT, HWM, GPIO | No |  |
| EPC-EHL | WDT, HWM, GPIO | No |  |
| EPC-WHL | WDT, HWM, GPIO | Yes | 2026-08-07 |
| EPX-APLP | WDT, HWM, GPIO | No |  |
| EPX-ASLP | WDT, HWM, GPIO | No |  |
| EPX-EHLP | WDT, HWM, GPIO | No |  |
| EQM-APL | WDT, HWM | No |  |
| EQM-BYT | WDT, HWM | No |  |
| EQM-BYT2 | WDT, HWM | No |  |
| EQM-EHL | WDT, HWM | No |  |
| ERX-C236KP | WDT, HWM, GPIO | No |  |
| ERX-H110KP | WDT, HWM, GPIO | No |  |
| ERX-H110P | WDT, HWM, GPIO | No |  |
| ERX-ZXEP | WDT, HWM, GPIO | No |  |
| ESM-APLC | WDT, HWM, GPIO | No |  |
| ESM-APLM | WDT, HWM, GPIO | No |  |
| ESM-ASLC | WDT, HWM, GPIO | No |  |
| ESM-BYT | WDT, HWM, GPIO | No |  |
| ESM-BYT2 | WDT, HWM, GPIO | No |  |
| ESM-CFH | WDT, HWM, GPIO | No |  |
| ESM-EHL | WDT, HWM | No |  |
| ESM-EHLC | WDT, HWM, GPIO | No |  |
| ESM-KX60G | WDT, HWM, GPIO, MISC | Yes | 2026-07-07 |
| ESM-RPL | WDT, HWM, GPIO | No |  |
| ESM-RPLC | WDT, HWM, GPIO | No |  |
| ESM-SKLH | WDT, HWM, GPIO | No |  |
| ESM-SKLU | WDT, HWM, GPIO | No |  |
| ESM-TGH | WDT, HWM, GPIO | No |  |
| ESM-ZXE | WDT, HWM, GPIO | No |  |
| EZX-EHLP | WDT, HWM | No |  |
| HID-2340 | WDT, HWM, GPIO, MISC | No |  |
| HID-2432 | WDT, HWM, GPIO | No |  |
| MX610H | WDT, HWM | No |  |
| NCM-APL | WDT, HWM | No |  |
| NCM-EHL | WDT, HWM, GPIO | No |  |
| NCM-TGU | WDT, HWM, GPIO | No |  |
| NUC-APL-B1 | WDT, HWM | No |  |
| NUC-APL | WDT, HWM | No |  |
| NUC-RPU | WDT, HWM, GPIO, MISC | No |  |
| NUC-TGU | WDT, HWM, GPIO | No |  |
| SLP-SKL | WDT, HWM, GPIO | No |  |
| TPB-04114 | WDT, HWM | No |  |
| VMS-APL | WDT, HWM, GPIO | No |  |
