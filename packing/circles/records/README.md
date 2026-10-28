# Packomania records (circles in regular polygons)

Fetched 2026-10-11 from https://www.packomania.com/. Each file lists `N r`, the best known radius
of N equal circles in the regular polygon of circumradius 1 (bottom side horizontal).

The published N=1 entries of the nonagon and decagon tables are computed for circumradius 1/2
(their coordinates for N>=2 fit circumradius 1); they are replaced here by cos(pi/k).

| code | sides | last update | N up to |
|---|---|---|---|
| cpt | 5 | 06-Mar-2023 | 200 |
| chx | 6 | 18-Dec-2020 | 120 (then selected N up to 1261) |
| chp | 7 | 05-Dec-2020 | 48 |
| cno | 9 | 05-Dec-2020 | 48 |
| cdc | 10 | 05-Dec-2020 | 48 |
| chd | 11 | 07-Dec-2020 | 48 |
| cdd | 12 | 07-Dec-2020 | 48 |
| ctd | 13 | 07-Dec-2020 | 48 |
| crd | 14 | 08-Dec-2020 | 48 |
| cpd | 15 | 14-Dec-2020 | 122 |
| cxd | 16 | 07-Mar-2023 | 200 |
| ced | 17 | 10-Dec-2020 | 48 |

Added 2026-10-12 (fetched from the same site the same day; `txt/radius.txt` of each table):

| code | container | last update | N |
|---|---|---|---|
| cci | circle of radius 1 | 25-Dec-2024 | 1-2600 complete, then 135 selected N up to 5000 |
| csq | square of side 1 (circumradius sqrt(2)/2) | 10-Sep-2026 | 951 sizes up to 1000, then selected N up to 9996 (radii printed with 12 digits) |

Added 2026-10-14 (pages fetched 2026-10-14; `txt/radius.txt` of each table):

| code | container | last update | N |
|---|---|---|---|
| ssp | spheres in the unit ball | 13-Jul-2026 | 1153 sizes (selected N up to 2982989) |
| scu | spheres in the unit cube [-1/2, 1/2]^3 | 17-Oct-2026 (see below) | 1134 sizes (selected N up to 1024192) |
| hsp4 | 4-spheres in the unit 4-ball | 06-Jul-2025 | 2-300 |
| hsp5 | 5-spheres in the unit 5-ball | 01-Aug-2025 | 2-300 |
| hsp6 | 6-spheres in the unit 6-ball | 31-Jul-2025 | 2-259 |
| csc | half disk of radius 1 (x^2 + y^2 <= 1, y >= 0) | 09-Sep-2026 | 1-250 |
| ccq | quarter disk of radius 1 (x^2 + y^2 <= 1, x, y >= 0) | 21-Jun-2018 | 1-600 |
| crt | isosceles right triangle with legs 1 (x, y >= 0, x + y <= 1) | 21-Jun-2018 | 1-299 |

Most crt records (251 of 299) come from E. Specht's program of 2009-2010, most ccq records (489 of 600)
from one source of October-December 2010; csc had ten improvements in September 2026.

Rectangles (pages and `txt/radius.txt` fetched 2026-10-14): `crc_100` .. `crc_800` hold N equal circles in
the rectangle 1 x a, a = 0.1 .. 0.8, centered at the origin (|x| <= 1/2, |y| <= a/2); all eight were last
updated 21-Jun-2018 and list every N from 1 to 200 (a = 0.1), 347, 300, 299, 300, 307, 437 and 420 (a = 0.8).
Most of their records come from E. Specht's program crc of 1999-2010.

Added 2026-10-15: `coc`, the regular octagon of circumradius 1 (bottom side horizontal), last updated
23-Sep-2026, N = 1-48 (radii printed with 12 digits); missing from the polygon campaign of 2026-10-11.

Re-fetched 2026-10-28: the tables of the October search (everything above except the twelve regular-polygon
tables of 2026-10-11/12) were fetched again and compared with the copies kept here. Only `scu` had changed (the
page is dated 17-Oct-2026): the radius became larger by more than 1e-7 at 104 of its 1134 sizes (more than 1e-5
at 27, more than 1e-4 at 7, none became smaller), so `scu.txt` is the new table. The other tables (`cci`, `csq`,
`csc`, `ccq`, `crt`, `crc_100`..`crc_800`, `coc`, `ssp`, `hsp4`..`hsp6`) had identical radii.

Packomania's list of pending requests (08-Oct-2026) shows submissions by other people that are not published
yet, for example for `csc` about 200 candidates, `ccq` about 700, `crt` about 260, `crc` about 780 and `csq`
about 810 (a candidate is not necessarily an improvement). The tables here do not contain them.
