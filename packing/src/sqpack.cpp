// sqpack: n unit squares in the smallest square (Erich Friedman's "Squares in Squares").
//
// Squares have side 1 and may rotate; the container is the square [-h, h]^2 and
// h is made as small as possible (side s = 2h). The optimizer mirrors circpack:
// squared-penetration penalty, L-BFGS on h + mu * P with mu raised step by step,
// bisection on h for a strictly feasible packing, and basin hopping.
//
// Overlap of two unit squares i, j (centers c, angles t): on each of the four
// separating axes u in {a_i, b_i, a_j, b_j} (a = (cos t, sin t), b = (-sin t, cos t))
// the overlap of the projections is 1/2 + E(t_i - t_j) - |(c_j - c_i) . u| with
// E(D) = (|cos D| + |sin D|) / 2, so the penetration depth is
// 1/2 + E - max_u |d . u| and the squares are disjoint iff it is <= 0.
//
// File format: '#' comments, then "n s", then n lines "x y t" (center, angle in
// radians), the container being [-s/2, s/2]^2.

#include <sys/stat.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <map>
#include <random>
#include <string>
#include <vector>

#include "common.h"

using packing::Arg;
using packing::ArgS;
using packing::Lbfgs;
using packing::Now;
using packing::ParseArgs;
using std::string;
using std::vector;

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kSqrt2 = 1.41421356237309504880;

inline double Sgn(double v) { return v > 0 ? 1.0 : (v < 0 ? -1.0 : 0.0); }

struct Packing {
  int n = 0;
  double h = 0;      // container half-side
  vector<double> q;  // x0 y0 t0 x1 y1 t1 ...
};

bool ReadPacking(const string& path, Packing& p) {
  FILE* f = fopen(path.c_str(), "r");
  if (!f) return false;
  char line[4096];
  vector<double> nums;
  while (fgets(line, sizeof line, f)) {
    if (line[0] == '#') continue;
    char* s = line;
    while (true) {
      char* end;
      double v = strtod(s, &end);
      if (end == s) break;
      nums.push_back(v);
      s = end;
    }
  }
  fclose(f);
  if (nums.size() < 2) return false;
  p.n = static_cast<int>(nums[0]);
  p.h = nums[1] / 2;
  if (static_cast<int>(nums.size()) != 2 + 3 * p.n) return false;
  p.q.assign(nums.begin() + 2, nums.end());
  return true;
}

bool WritePacking(const string& path, const Packing& p, const string& comment) {
  string tmp = path + ".tmp";
  FILE* f = fopen(tmp.c_str(), "w");
  if (!f) return false;
  fprintf(f, "# %s\n%d %.17g\n", comment.c_str(), p.n, 2 * p.h);
  for (int i = 0; i < p.n; i++) {
    double t = std::fmod(p.q[3 * i + 2], kPi / 2);
    if (t < 0) t += kPi / 2;
    fprintf(f, "%.17g %.17g %.17g\n", p.q[3 * i], p.q[3 * i + 1], t);
  }
  fclose(f);
  return rename(tmp.c_str(), path.c_str()) == 0;
}

class Energy {
 public:
  explicit Energy(int n) : n_(n), order_(n) {}

  // Squared penetrations (squares need a gap `margin` between them and from the
  // walls); gradient into g (3n) and gh.
  double Eval(const double* q, double h, double margin, double* g, double* gh) {
    std::fill(g, g + 3 * n_, 0.0);
    double E = 0, gr = 0;
    const double lim = h - margin;
    for (int i = 0; i < n_; i++) {
      double x = q[3 * i], y = q[3 * i + 1], t = q[3 * i + 2];
      double c = std::cos(t), s = std::sin(t);
      double e = 0.5 * (std::abs(c) + std::abs(s)), de = 0.5 * (-Sgn(c) * s + Sgn(s) * c);
      for (int k = 0; k < 2; k++) {
        double v = k ? y : x, viol = std::abs(v) + e - lim;
        if (viol <= 0) continue;
        E += viol * viol;
        g[3 * i + k] += 2 * viol * Sgn(v);
        g[3 * i + 2] += 2 * viol * de;
        gr -= 2 * viol;
      }
    }
    for (int i = 0; i < n_; i++) order_[i] = i;
    std::sort(order_.begin(), order_.end(), [&](int a, int b) { return q[3 * a] < q[3 * b]; });
    const double reach = kSqrt2 + margin, reach2 = reach * reach;
    for (int a = 0; a < n_; a++) {
      int i = order_[a];
      double xi = q[3 * i], yi = q[3 * i + 1], ti = q[3 * i + 2];
      double ci = std::cos(ti), si = std::sin(ti);
      for (int b = a + 1; b < n_; b++) {
        int j = order_[b];
        double dx = q[3 * j] - xi;
        if (dx >= reach) break;
        double dy = q[3 * j + 1] - yi;
        if (dy >= reach || dy <= -reach || dx * dx + dy * dy >= reach2) continue;
        double tj = q[3 * j + 2], cj = std::cos(tj), sj = std::sin(tj);
        double D = ti - tj, cD = std::cos(D), sD = std::sin(D);
        double E2 = 0.5 * (std::abs(cD) + std::abs(sD)), dE2 = 0.5 * (-Sgn(cD) * sD + Sgn(sD) * cD);
        double p[4] = {dx * ci + dy * si, -dx * si + dy * ci, dx * cj + dy * sj, -dx * sj + dy * cj};
        int k = 0;
        for (int m = 1; m < 4; m++)
          if (std::abs(p[m]) > std::abs(p[k])) k = m;
        double M = std::abs(p[k]), delta = 0.5 + E2 + margin - M;
        if (delta <= 0) continue;
        E += delta * delta;
        double f = 2 * delta, sg = Sgn(p[k]);
        g[3 * i + 2] += f * dE2;
        g[3 * j + 2] -= f * dE2;
        double ux, uy;
        switch (k) {
          case 0: ux = ci; uy = si; g[3 * i + 2] -= f * sg * p[1]; break;
          case 1: ux = -si; uy = ci; g[3 * i + 2] += f * sg * p[0]; break;
          case 2: ux = cj; uy = sj; g[3 * j + 2] -= f * sg * p[3]; break;
          default: ux = -sj; uy = cj; g[3 * j + 2] += f * sg * p[2]; break;
        }
        g[3 * j] -= f * sg * ux;
        g[3 * j + 1] -= f * sg * uy;
        g[3 * i] += f * sg * ux;
        g[3 * i + 1] += f * sg * uy;
      }
    }
    *gh = gr;
    return E;
  }

  // Smallest separation between squares (max_u |d.u| - 1/2 - E) and from the walls.
  void Clearance(const double* q, double h, double* pairGap, double* wallGap) const {
    double pg = 1e300, wg = 1e300;
    for (int i = 0; i < n_; i++) {
      double t = q[3 * i + 2], e = 0.5 * (std::abs(std::cos(t)) + std::abs(std::sin(t)));
      wg = std::min(wg, h - e - std::max(std::abs(q[3 * i]), std::abs(q[3 * i + 1])));
      for (int j = i + 1; j < n_; j++) {
        double dx = q[3 * j] - q[3 * i], dy = q[3 * j + 1] - q[3 * i + 1];
        if (dx * dx + dy * dy >= 2.0 + 1e-9) continue;
        double ti = q[3 * i + 2], tj = q[3 * j + 2], D = ti - tj;
        double E2 = 0.5 * (std::abs(std::cos(D)) + std::abs(std::sin(D)));
        double M = 0;
        for (double t : {ti, tj}) {
          M = std::max(M, std::abs(dx * std::cos(t) + dy * std::sin(t)));
          M = std::max(M, std::abs(-dx * std::sin(t) + dy * std::cos(t)));
        }
        pg = std::min(pg, M - 0.5 - E2);
      }
    }
    *pairGap = pg;
    *wallGap = wg;
  }

 private:
  int n_;
  vector<int> order_;
};

void Rescale(Packing& p, double newH) {
  double f = newH / p.h;
  for (int i = 0; i < p.n; i++) {
    p.q[3 * i] *= f;
    p.q[3 * i + 1] *= f;
  }
  p.h = newH;
}

class Searcher {
 public:
  Searcher(int n, uint64_t seed) : n_(n), en_(n), rng_(seed) {}

  double U(double a = 0, double b = 1) { return std::uniform_real_distribution<double>(a, b)(rng_); }
  double G(double s) { return std::normal_distribution<double>(0, s)(rng_); }
  int I(int n) { return static_cast<int>(rng_() % static_cast<uint64_t>(n)); }
  Energy& energy() { return en_; }

  void Pressure(Packing& p, double mu, int iters) {
    const int N = 3 * n_;
    vector<double> x(p.q);
    x.push_back(p.h);
    auto fg = [&](const double* xx, double* g) {
      double gh;
      double P = en_.Eval(xx, xx[N], 0, g, &gh);
      for (int i = 0; i < N; i++) g[i] *= mu;
      g[N] = 1 + mu * gh;
      return xx[N] + mu * P;
    };
    Lbfgs(x, fg, iters, -1e300, 0.2);
    p.h = x[N];
    x.pop_back();
    p.q = x;
  }

  double Relax(Packing& p, double margin, int iters, double fStop) {
    auto fg = [&](const double* xx, double* g) {
      double gh;
      return en_.Eval(xx, p.h, margin, g, &gh);
    };
    return Lbfgs(p.q, fg, iters, fStop, 0.1);
  }

  bool Compress(Packing& p, double mu0, double abortAbove = 1e300) {
    double mu = mu0;
    for (; mu <= 1.01e4; mu *= 10) Pressure(p, mu, 300);
    if (p.h > abortAbove) return false;
    for (; mu <= 1.01e10; mu *= 10) Pressure(p, mu, 300);
    return true;
  }

  // Smallest h (to within tol) for which the arrangement relaxes to a packing with gap `margin`.
  bool Polish(Packing& p, double margin, double tol = 1e-13) {
    const double kFeas = 1e-28;
    const int kIters = 4000;
    Packing hi = p;
    double up = 0;
    for (double step = 1e-10;; step *= 4) {
      Packing t = p;
      Rescale(t, p.h + up);
      if (Relax(t, margin, kIters, kFeas) <= kFeas) {
        hi = t;
        break;
      }
      up = step;
      if (step > 1e-1) return false;
    }
    double gap = std::max(1e-8, 4 * up), lo = hi.h - gap;
    while (true) {
      Packing t = hi;
      Rescale(t, lo);
      if (Relax(t, margin, kIters, kFeas) > kFeas) break;
      hi = t;
      gap *= 2;
      lo = hi.h - gap;
    }
    while (hi.h - lo > tol * hi.h) {
      double mid = 0.5 * (lo + hi.h);
      Packing t = hi;
      Rescale(t, mid);
      if (Relax(t, margin, kIters, kFeas) <= kFeas) {
        hi = t;
      } else {
        lo = mid;
      }
    }
    p = hi;
    return true;
  }

  Packing RandomStart(double density) {
    Packing p;
    p.n = n_;
    p.h = 0.5 * std::sqrt(n_ / density) + 0.5;
    double tiltShare = U();  // fraction of squares that start tilted
    for (int i = 0; i < n_; i++) {
      p.q.push_back(U(-p.h + 0.5, p.h - 0.5));
      p.q.push_back(U(-p.h + 0.5, p.h - 0.5));
      p.q.push_back(U() < tiltShare ? U(0, kPi / 2) : 0.0);
    }
    return p;
  }

  // Free room around (x, y): distance to the walls and to the other centers (a crude proxy).
  double Room(const Packing& p, double x, double y, int skip) const {
    double c = p.h - std::max(std::abs(x), std::abs(y));
    for (int i = 0; i < p.n; i++) {
      if (i == skip) continue;
      c = std::min(c, std::hypot(x - p.q[3 * i], y - p.q[3 * i + 1]) - 1);
    }
    return c;
  }

  void Hole(const Packing& p, int skip, double* bx, double* by) {
    double best = -1e300;
    for (int s = 0; s < 300; s++) {
      double x = U(-p.h, p.h), y = U(-p.h, p.h);
      double c = Room(p, x, y, skip);
      if (c > best) {
        best = c;
        *bx = x;
        *by = y;
      }
    }
  }

  // Seed for n squares from a packing of n + 1: drop one square.
  Packing FromLarger(const Packing& s) {
    Packing p = s;
    int i = I(s.n);
    p.q.erase(p.q.begin() + 3 * i, p.q.begin() + 3 * i + 3);
    p.n = n_;
    return p;
  }

  // Seed for n squares from a packing of n - 1: add an upright square at the roomiest spot.
  Packing FromSmaller(const Packing& s) {
    Packing p = s;
    double x = 0, y = 0;
    Hole(s, -1, &x, &y);
    p.q.push_back(x);
    p.q.push_back(y);
    p.q.push_back(0.0);
    p.n = n_;
    Rescale(p, p.h * 1.03);
    return p;
  }

  enum Move { kShake, kLocal, kRelocate, kRotate, kAlign, kTilt, kNumMoves };

  void Perturb(Packing& p, int mv) {
    switch (mv) {
      case kShake: {
        double s = U(0.02, 0.4), st = U(0, 0.3);
        for (int i = 0; i < n_; i++) {
          p.q[3 * i] += U(-s, s);
          p.q[3 * i + 1] += U(-s, s);
          p.q[3 * i + 2] += U(-st, st);
        }
        break;
      }
      case kLocal: {
        double cx = U(-p.h, p.h), cy = U(-p.h, p.h), rad = U(1.0, 3.5), s = U(0.2, 1.0);
        for (int i = 0; i < n_; i++) {
          if (std::hypot(p.q[3 * i] - cx, p.q[3 * i + 1] - cy) > rad) continue;
          p.q[3 * i] += G(s);
          p.q[3 * i + 1] += G(s);
          p.q[3 * i + 2] += G(0.5);
        }
        break;
      }
      case kRelocate: {
        int m = 1 + I(3);
        for (int t = 0; t < m; t++) {
          int i = I(n_);
          double x = 0, y = 0;
          Hole(p, i, &x, &y);
          p.q[3 * i] = x;
          p.q[3 * i + 1] = y;
          p.q[3 * i + 2] = U() < 0.5 ? 0.0 : U(0, kPi / 2);
        }
        break;
      }
      case kRotate: {  // turn a patch of squares about a random point
        double cx = U(-p.h, p.h), cy = U(-p.h, p.h), rad = U(1.0, 4.0), phi = G(0.3);
        double cs = std::cos(phi), sn = std::sin(phi);
        for (int i = 0; i < n_; i++) {
          double dx = p.q[3 * i] - cx, dy = p.q[3 * i + 1] - cy;
          if (std::hypot(dx, dy) > rad) continue;
          p.q[3 * i] = cx + cs * dx - sn * dy;
          p.q[3 * i + 1] = cy + sn * dx + cs * dy;
          p.q[3 * i + 2] += phi;
        }
        break;
      }
      case kAlign: {  // straighten a few tilted squares
        int m = 1 + I(4);
        for (int t = 0; t < m; t++) p.q[3 * I(n_) + 2] = 0;
        break;
      }
      case kTilt: {  // give a patch one common tilt
        double cx = U(-p.h, p.h), cy = U(-p.h, p.h), rad = U(1.0, 3.0), t0 = U(0, kPi / 2);
        for (int i = 0; i < n_; i++)
          if (std::hypot(p.q[3 * i] - cx, p.q[3 * i + 1] - cy) <= rad) p.q[3 * i + 2] = t0;
        break;
      }
    }
  }

  Packing BasinHop(Packing start, int trials, double temp, double tLimit, long* tried) {
    Packing cur = start, best = start;
    double t0 = Now();
    for (int t = 0; t < trials && Now() - t0 < tLimit; t++) {
      Packing x = cur;
      Rescale(x, cur.h * (1 + std::exp(U(std::log(0.001), std::log(0.05)))));
      Perturb(x, I(kNumMoves));
      double mus[] = {1e1, 1e2, 1e3};
      if (!Compress(x, mus[I(3)], cur.h * (1 + 30 * temp))) continue;
      if (tried) (*tried)++;
      if (x.h < best.h * (1 - 1e-9)) {
        Packing pol = x;
        if (Polish(pol, 1e-11) && pol.h < best.h * (1 - 1e-12)) best = pol;
      }
      if (x.h < cur.h || U() < std::exp(-(x.h - cur.h) / (temp * cur.h))) cur = x;
      if (t % 200 == 199) cur = best;
    }
    return best;
  }

 private:
  int n_;
  Energy en_;
  std::mt19937_64 rng_;
};

// ---------------------------------------------------------------- commands

int CmdCheck(const std::map<string, string>& a) {
  Packing p;
  if (!ReadPacking(ArgS(a, "in", ""), p)) {
    fprintf(stderr, "cannot read --in\n");
    return 1;
  }
  Energy en(p.n);
  double pg, wg;
  en.Clearance(p.q.data(), p.h, &pg, &wg);
  printf("n=%d s=%.15f pair gap=%.3e wall gap=%.3e %s\n", p.n, 2 * p.h, pg, wg, pg >= 0 && wg >= 0 ? "VALID" : "INVALID");
  return pg >= 0 && wg >= 0 ? 0 : 1;
}

int CmdGradCheck(const std::map<string, string>& a) {
  int n = static_cast<int>(Arg(a, "n", 20));
  Searcher s(n, 3);
  Packing p = s.RandomStart(0.5);
  for (int i = 0; i < n; i++) p.q[3 * i + 2] = s.U(0, kPi / 2);
  Rescale(p, p.h * 0.6);
  Energy en(n);
  vector<double> g(3 * n), g2(3 * n);
  double gh, gh2, margin = Arg(a, "margin", 0.0);
  en.Eval(p.q.data(), p.h, margin, g.data(), &gh);
  double worst = 0;
  for (int i = 0; i <= 3 * n; i++) {
    const double hstep = 1e-7;
    Packing pp = p, pm = p;
    if (i < 3 * n) {
      pp.q[i] += hstep;
      pm.q[i] -= hstep;
    } else {
      pp.h += hstep;
      pm.h -= hstep;
    }
    double ep = en.Eval(pp.q.data(), pp.h, margin, g2.data(), &gh2);
    double em = en.Eval(pm.q.data(), pm.h, margin, g2.data(), &gh2);
    double num = (ep - em) / (2 * hstep), ana = i < 3 * n ? g[i] : gh;
    worst = std::max(worst, std::abs(num - ana) / std::max(1.0, std::abs(num)));
  }
  printf("worst relative gradient error=%.3e\n", worst);
  return worst < 1e-5 ? 0 : 1;
}

// Keeps the K best distinct packings found for one n (in memory, mirrored to
// files DIR/i.txt so that the next run continues from the whole pool).
class Pool {
 public:
  Pool(const string& dir, int k) : dir_(dir), k_(k) {}

  void Load(int n) {
    for (int i = 0; i < k_; i++) {
      Packing p;
      if (ReadPacking(Path(i), p) && p.n == n) Add(p);
    }
  }

  // Packings whose sides agree to 1e-7 count as the same arrangement: only the
  // better one is kept. Returns whether the pool changed.
  bool Add(const Packing& p) {
    for (size_t i = 0; i < items_.size(); i++) {
      if (std::abs(items_[i].h - p.h) > 1e-7 * p.h) continue;
      if (p.h >= items_[i].h) return false;
      items_.erase(items_.begin() + i);
      break;
    }
    items_.insert(std::upper_bound(items_.begin(), items_.end(), p,
                                   [](const Packing& x, const Packing& y) { return x.h < y.h; }),
                  p);
    if (static_cast<int>(items_.size()) > k_) items_.pop_back();
    return true;
  }

  void Save() const {
    if (dir_.empty()) return;
    mkdir(dir_.c_str(), 0755);
    for (size_t i = 0; i < items_.size(); i++) WritePacking(Path(static_cast<int>(i)), items_[i], "pool");
  }

  // A member biased towards the best ones.
  const Packing& Pick(double u) const { return items_[static_cast<size_t>(u * u * items_.size())]; }
  bool empty() const { return items_.empty(); }
  size_t size() const { return items_.size(); }

 private:
  string Path(int i) const { return dir_ + "/" + std::to_string(i) + ".txt"; }
  string dir_;
  int k_;
  vector<Packing> items_;
};

// The best packing of size m found in any of the comma-separated directories.
bool BestNeighbour(const string& dirs, int m, Packing& best) {
  bool have = false;
  size_t from = 0;
  while (from <= dirs.size() && !dirs.empty()) {
    size_t to = dirs.find(',', from);
    if (to == string::npos) to = dirs.size();
    Packing p;
    if (ReadPacking(dirs.substr(from, to - from) + "/" + std::to_string(m) + ".txt", p) && p.n == m &&
        (!have || p.h < best.h)) {
      best = p;
      have = true;
    }
    from = to + 1;
  }
  return have;
}

// Basin hopping from --in (or from random starts when --in is absent) for --time
// seconds; the best packing goes to --out whenever it improves. With --pool DIR
// every round starts from one of the --pool-size best distinct packings found so
// far (kept in DIR across runs); with --nbr DIRS some rounds start from the best
// packing of n + 1 minus a square or of n - 1 plus a square.
int CmdSearch(const std::map<string, string>& a) {
  string in = ArgS(a, "in", ""), out = ArgS(a, "out", ""), poolDir = ArgS(a, "pool", ""), nbr = ArgS(a, "nbr", "");
  int n = static_cast<int>(Arg(a, "n", 0));
  double timeLimit = Arg(a, "time", 60), temp = Arg(a, "temp", 1e-4), record = Arg(a, "record", 0);
  double fresh = Arg(a, "fresh", 0.5);  // share of rounds that start from randomness once a best exists
  double nbrShare = nbr.empty() ? 0.0 : Arg(a, "nbr-share", 0.15);
  int trials = static_cast<int>(Arg(a, "trials", 400));
  uint64_t seed = static_cast<uint64_t>(Arg(a, "seed", 1));
  Packing best;
  bool have = false;
  if (!out.empty() && ReadPacking(out, best) && (n == 0 || best.n == n)) have = true;
  Packing start;
  bool fromIn = !in.empty() && ReadPacking(in, start);
  if (fromIn) n = start.n;
  if (n <= 0) {
    fprintf(stderr, "--n or --in required\n");
    return 1;
  }
  Searcher s(n, seed);
  if (fromIn) {
    if (!s.Polish(start, 1e-11)) {
      fprintf(stderr, "cannot polish --in\n");
      return 1;
    }
    printf("start s=%.15f\n", 2 * start.h);
    if (!have || start.h < best.h) {
      best = start;
      have = true;
      if (!out.empty()) WritePacking(out, best, "polished from " + in);
    }
  }
  Pool pool(poolDir, static_cast<int>(Arg(a, "pool-size", 24)));
  pool.Load(n);
  if (have) pool.Add(best);
  double t0 = Now();
  long tried = 0, rounds = 0;
  while (Now() - t0 < timeLimit) {
    rounds++;
    Packing p;
    double u = s.U(), randomShare = fromIn ? 0.0 : (have ? fresh : 1.0);
    string kind = "best";
    if (u < randomShare) {
      p = s.RandomStart(s.U(0.3, 0.6));
      kind = "random";
    } else if (u < randomShare + nbrShare) {
      Packing nb;
      bool larger = s.U() < 0.5;
      if (!BestNeighbour(nbr, larger ? n + 1 : n - 1, nb) && !BestNeighbour(nbr, (larger = !larger) ? n + 1 : n - 1, nb))
        continue;
      p = larger ? s.FromLarger(nb) : s.FromSmaller(nb);
      kind = larger ? "shrink" : "grow";
    } else {
      p = pool.empty() ? best : pool.Pick(s.U());
      kind = "pool";
    }
    if (kind != "best" && kind != "pool") {
      s.Compress(p, 1e1);
      if (!s.Polish(p, 1e-11)) continue;
    }
    Packing b = s.BasinHop(p, trials, temp, timeLimit - (Now() - t0), &tried);
    if (pool.Add(b)) pool.Save();
    if (!have || b.h < best.h * (1 - 1e-13)) {
      best = b;
      have = true;
      double pg, wg;
      s.energy().Clearance(best.q.data(), best.h, &pg, &wg);
      char buf[160];
      snprintf(buf, sizeof buf, "n=%d s=%.15f found by sqpack", n, 2 * best.h);
      if (!out.empty()) WritePacking(out, best, buf);
      printf("[%7.0fs] n=%d s=%.13f%s  gaps %.1e %.1e  via=%s\n", Now() - t0, n, 2 * best.h,
             record > 0 ? (2 * best.h < record - 1e-9 ? "  BELOW RECORD" : "") : "", pg, wg, kind.c_str());
      fflush(stdout);
    }
  }
  printf("done: n=%d best s=%.13f after %ld rounds, %ld trials, %.0fs\n", n, 2 * best.h, rounds, tried, Now() - t0);
  return 0;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 2) {
    fprintf(stderr,
            "usage: sqpack <command> [--key value ...]\n"
            "  search    (--n N | --in FILE) [--out FILE] [--time SEC] [--trials T] [--temp T] [--seed S]\n"
            "            [--record S] [--fresh P] [--pool DIR] [--pool-size K] [--nbr DIR1,DIR2] [--nbr-share P]\n"
            "  check     --in FILE\n"
            "  gradcheck [--n N] [--margin M]\n");
    return 2;
  }
  string cmd = argv[1];
  auto a = ParseArgs(argc, argv, 2);
  if (cmd == "search") return CmdSearch(a);
  if (cmd == "check") return CmdCheck(a);
  if (cmd == "gradcheck") return CmdGradCheck(a);
  fprintf(stderr, "unknown command %s\n", cmd.c_str());
  return 2;
}
