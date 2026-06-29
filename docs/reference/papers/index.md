# Reference Papers

This page catalogs the academic papers and technical articles that MRTKLIB's
algorithms are built on — both those **inherited from upstream RTKLIB** (cited
in the original source headers) and those **newly introduced by MRTKLIB** for
features that have no upstream equivalent (e.g. the SPP robust / TDCP work).

!!! warning "The PDFs themselves are **not** in this repository"
    Most papers are copyrighted (journal / conference proceedings) and are
    **kept local only** — `docs/reference/papers/*.pdf` is gitignored and never
    pushed to the remote. This page (the bibliography) **is** tracked. To read a
    paper, follow the **Source** link; if you have a redistributable copy, drop
    it in `docs/reference/papers/` for local reference (open-access papers can
    stay link-only — the tracked citation is the deliverable).

Interface-control documents and standards (RINEX, RTCM, IS-GPS/QZSS, IERS
Conventions, ANTEX/IONEX/SP3, …) are **not** listed here — they live in
[Specifications (ICDs)](../icd/index.md). DOIs marked _(confirm)_ are inferred
from the citation text and should be verified before formal use.

A 📄 `filename.pdf` marker means a local copy is held next to this file
(`author-year-keyword.pdf` convention; gitignored). Entries without a marker are
link-only — follow the Source column.

---

## A. Inherited from upstream RTKLIB

Cited in the `references :` headers of `upstream/RTKLIB/src/*` (and reused
verbatim by MALIB / CLASLIB / MADOCALIB, which add no new academic papers).

### Ambiguity resolution (`lambda.c`)

| Paper | Source | Cited by |
|-------|--------|----------|
| Teunissen, P.J.G. (1995). "The least-squares ambiguity decorrelation adjustment: a method for fast GPS ambiguity estimation." *J. Geodesy* 70:65–82. | DOI [10.1007/BF00863419](https://doi.org/10.1007/BF00863419) | `lambda.c`, `mrtk_lambda.h` |
| Chang, X.-W.; Yang, X.; Zhou, T. (2005). "MLAMBDA: A modified LAMBDA method for integer least-squares estimation." *J. Geodesy* 79:552–565. 📄 `chang-2005-mlambda.pdf` | DOI [10.1007/s00190-005-0004-x](https://doi.org/10.1007/s00190-005-0004-x) | `lambda.c`, `mrtk_lambda.h` |

### Tropospheric mapping functions (`rtkcmn.c`)

| Paper | Source | Cited by |
|-------|--------|----------|
| Boehm, J.; Niell, A.; Tregoning, P.; Schuh, H. (2006). "Global Mapping Function (GMF): A new empirical mapping function based on numerical weather model data." *Geophys. Res. Lett.* 33, L07304. 📄 `boehm-2006-global-mapping-function-gmf.pdf` | DOI [10.1029/2005GL025546](https://doi.org/10.1029/2005GL025546) | `rtkcmn.c` |
| Niell, A.E. (1996). "Global mapping functions for the atmosphere delay at radio wavelengths." *J. Geophys. Res.* 101(B2):3227–3246. 📄 `niell-1996-global-mapping-functions.pdf` | DOI [10.1029/95JB03048](https://doi.org/10.1029/95JB03048) | `rtkcmn.c` |
| MacMillan, D.S. et al. (1997). "Atmospheric gradients and the VLBI terrestrial and celestial reference frames." *Geophys. Res. Lett.* 24(4):453–456. 📄 `macmillan-1997-atmospheric-gradients-vlbi.pdf` | DOI [10.1029/97GL00143](https://doi.org/10.1029/97GL00143) | `ppp.c`, `mrtk_ppp_rtk.c` |

### Satellite attitude / antenna modeling (`ppp.c`)

| Paper | Source | Cited by |
|-------|--------|----------|
| Kouba, J. (2009). "A simplified yaw-attitude model for eclipsing GPS satellites." *GPS Solutions* 13:1–12. 📄 `kouba-2009-yaw-attitude-eclipsing-gps.pdf` | DOI [10.1007/s10291-008-0092-1](https://doi.org/10.1007/s10291-008-0092-1) | `ppp.c` |
| Dilssner, F. (2010). "GPS IIF-1 satellite antenna phase center and attitude modeling." *Inside GNSS*, Sep 2010. | Free article (insidegnss.com) | `ppp.c` |
| Dilssner, F.; Springer, T.; Gienger, G.; Dow, J. (2011). "The GLONASS-M satellite yaw-attitude model." *Advances in Space Research* 47(1):160–171. 📄 `dilssner-2011-glonass-m-yaw-attitude.pdf` | DOI [10.1016/j.asr.2010.09.007](https://doi.org/10.1016/j.asr.2010.09.007) | `ppp.c` |

### Ionosphere (`ionex.c`)

| Paper | Source | Cited by |
|-------|--------|----------|
| Schaer, S.; Markus, R.; Gerhard, B.; Timon, A.S. (1996). "Daily Global Ionosphere Maps based on GPS Carrier Phase Data Routinely Produced by CODE Analysis Center." *Proc. IGS Analysis Center Workshop*. 📄 `schaer-1996-code-global-ionosphere-maps.pdf` | IGS proceedings (no DOI) | `ionex.c` |

### Estimation theory (`rtkcmn.c`, `ppp_ar.c`)

| Paper | Source | Cited by |
|-------|--------|----------|
| Gelb, A. (ed.) (1974). *Applied Optimal Estimation*. MIT Press. | Book (ISBN 0-262-57048-3) | `rtkcmn.c` |
| Okumura, H. (1991). 『C言語による最新アルゴリズム事典』 (Dictionary of algorithms in C). Software Technology. | Book (和書) | `ppp_ar.c` |

### Astrodynamics, geoid & orbit propagation (`ppp.c`, `geoid.c`, `tle.c`)

| Paper | Source | Cited by |
|-------|--------|----------|
| Vallado, D.A. (2004). *Fundamentals of Astrodynamics and Applications*, 2nd ed. Space Technology Library. | Book | `ppp.c`, `tle.c` |
| Lemoine, F.G. et al. — EGM96: The NASA GSFC and NIMA Joint Geopotential Model. NASA/TP-1998-206861. | NASA report | `geoid.c` |
| Pavlis, N.K. et al. (2008/2012). "The development and evaluation of the Earth Gravitational Model 2008 (EGM2008)." *J. Geophys. Res.* 📄 `pavlis-2012-egm2008.pdf` | DOI [10.1029/2011JB008916](https://doi.org/10.1029/2011JB008916) | `geoid.c` |
| Hoots, F.R.; Roehrich, R.L. (1980). "Spacetrack Report No.3: Models for Propagation of NORAD Element Sets." | Spacetrack report (SGP4) | `tle.c` |
| Vallado, D.A.; Crawford, P.; Hujsak, R.; Kelso, T.S. (2006). "Revisiting Spacetrack Report #3." AIAA 2006-6753. | DOI [10.2514/6.2006-6753](https://doi.org/10.2514/6.2006-6753) | `tle.c` |

### PPP processing guide (semi-formal, but a primary reference)

| Paper | Source | Cited by |
|-------|--------|----------|
| Kouba, J. (2009, rev.). "A Guide to using International GNSS Service (IGS) products." | Free PDF (IGS) | `ppp.c`, `mrtk_tides.c`, `cssr2osr.c` |

---

## B. Introduced by MRTKLIB

References for features MRTKLIB added with no upstream RTKLIB equivalent. The
SPP-accuracy set is maintainer-verified against publisher records (May 2026) —
see [`docs/design/spp-accuracy.md`](../../design/spp-accuracy.md) §9.

### SPP robust estimation — IGG-III (#116 P2; `mrtk_spp.c`)

| Paper | Source | Role |
|-------|--------|------|
| Yang, Y.; He, H.; Xu, G. (2001). "Adaptively robust filtering for kinematic geodetic positioning." *J. Geodesy* 75:109–116. 📄 `yang-2001-adaptively-robust-filtering.pdf` | DOI [10.1007/s001900000157](https://doi.org/10.1007/s001900000157) | Primary — IGG-III equivalent-weight scheme implemented in `igg3_weight()` |
| Wang, L.; She, J.; Cui, B. et al. (2025). "Mitigating Integrity Risk in SBAS Positioning Using Enhanced IGG III Robust Estimation." *Remote Sensing* 17(17):3067. 📄 `wang-2025-enhanced-igg3-robust-sbas.pdf` | DOI [10.3390/rs17173067](https://doi.org/10.3390/rs17173067) | Corroborating application |

### Robust scale — MAD (#116 P2)

| Paper | Source | Role |
|-------|--------|------|
| Rousseeuw, P.J.; Croux, C. (1993). "Alternatives to the Median Absolute Deviation." *JASA* 88(424):1273–1283. | DOI [10.1080/01621459.1993.10476408](https://doi.org/10.1080/01621459.1993.10476408) | MADn robust scale for the standardized residual |
| Huber, P.J. (1981). *Robust Statistics*. Wiley. | Book | Foundation for robust scale + IRLS |
| Blewitt, G. et al. (2016). "MIDAS robust trend estimator for accurate GPS station velocities without step detection." *JGR Solid Earth* 121. 📄 `blewitt-2016-midas-robust-trend-estimator.pdf` | DOI [10.1002/2015JB012552](https://doi.org/10.1002/2015JB012552) _(confirm)_ | Operational MAD-scaled robust GNSS |

### TDCP velocity estimation (#116; `mrtk_rtkpos.c`)

| Paper | Source | Role |
|-------|--------|------|
| Van Graas, F.; Soloviev, A. (2004). "Precise Velocity Estimation Using a Stand-Alone GPS Receiver." *NAVIGATION* 51(4):283–292. 📄 `vangraas-2004-precise-velocity-estimation.pdf` | DOI [10.1002/j.2161-4296.2004.tb00359.x](https://doi.org/10.1002/j.2161-4296.2004.tb00359.x) | Primary — TDCP velocity |
| Serrano, L. et al. (2004). "A GPS Velocity Sensor: How Accurate Can It Be? — A First Look." *Proc. ION NTM 2004*, pp. 875–885. | [PDF](http://gauss.gge.unb.ca/papers.pdf/ionntm2004.serrano.pdf) | Primary |
| Freda, P.; Angrisano, A.; Gaglione, S.; Troisi, S. (2015). "Time-differenced carrier phases technique for precise GNSS velocity estimation." *GPS Solutions* 19(2):335–341. 📄 `freda-2015-tdcp-velocity.pdf` | DOI [10.1007/s10291-014-0425-1](https://doi.org/10.1007/s10291-014-0425-1) | Primary |

### Doppler / TDCP fused in an EKF (#116 P6, design)

| Paper | Source | Role |
|-------|--------|------|
| "Doppler measurement integration for kinematic real-time GPS positioning." *Applied Geomatics* 2(4):155–162 (2010). | DOI [10.1007/s12518-010-0031-z](https://doi.org/10.1007/s12518-010-0031-z) | Doppler-in-EKF precedent |
| "The Design a TDCP-Smoothed GNSS/Odometer Integration Scheme … and Robust Regression." *Remote Sensing* 12(16):2550 (2020). | DOI [10.3390/rs12162550](https://doi.org/10.3390/rs12162550) | Template for robust-WLS + TDCP-EKF hybrid |
| "A Doppler enhanced TDCP algorithm based on terrain adaptive and robust Kalman filter using a stand-alone receiver." *J. Navigation* (CUP). | [Article](https://www.cambridge.org/core/journals/journal-of-navigation/article/abs/doppler-enhanced-tdcp-algorithm-based-on-terrain-adaptive-and-robust-kalman-filter-using-a-standalone-receiver/6A77A5FAE63FB107348D85F16FB1E908) _(vol/year confirm)_ | Corroborating |

### Carrier smoothing & clock-jump (#116 P5/P6 lineage)

| Paper | Source | Role |
|-------|--------|------|
| Hatch, R. (1982). "The Synergism of GPS Code and Carrier Measurements." *Proc. 3rd Int. Geodetic Symp. on Satellite Doppler Positioning*, vol. 2, pp. 1213–1231. | Proceedings | Origin of position-domain smoothing TDCP generalises |
| Everett, T.; Taylor, T.; Lee, D.-K.; Akos, D.M. (2022). "Optimizing the Use of RTKLIB for Smartphone-Based GNSS Measurements." *Sensors* 22(10):3825. 📄 `everett-2022-optimizing-rtklib-smartphone.pdf` | DOI [10.3390/s22103825](https://doi.org/10.3390/s22103825) | demo5 clock-jump lineage |

---

## Adding a paper

1. If you have a redistributable copy, place the PDF in `docs/reference/papers/`
   with a stable `author-year-keyword.pdf` filename.
2. Add a row to the relevant section (A = upstream-inherited, B = MRTKLIB-new):
   full citation, DOI/source link, and which module/algorithm cites it.
3. Commit **only** `index.md` — any PDF is gitignored automatically.
4. New algorithm work should cite its references in the source header's
   `references :` block **and** add a row here.
