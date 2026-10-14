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
| scu | spheres in the unit cube [-1/2, 1/2]^3 | 08-Oct-2026 | 1134 sizes (selected N up to 1024192) |
| hsp4 | 4-spheres in the unit 4-ball | 06-Jul-2025 | 2-300 |
| hsp5 | 5-spheres in the unit 5-ball | 01-Aug-2025 | 2-300 |
| hsp6 | 6-spheres in the unit 6-ball | 31-Jul-2025 | 2-259 |
| csc | half disk of radius 1 (x^2 + y^2 <= 1, y >= 0) | 09-Sep-2026 | 1-250 |
| ccq | quarter disk of radius 1 (x^2 + y^2 <= 1, x, y >= 0) | 21-Jun-2018 | 1-600 |
| crt | isosceles right triangle with legs 1 (x, y >= 0, x + y <= 1) | 21-Jun-2018 | 1-299 |

Most crt records (251 of 299) come from E. Specht's program of 2009-2010, most ccq records (489 of 600)
from one source of October-December 2010; csc had ten improvements in September 2026.

