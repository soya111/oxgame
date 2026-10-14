// circpack: equal circles in a regular k-gon (Packomania's "circles in a regular polygon"),
// in a circle for k = 0, and in a half disk (k = -1), a quarter disk (k = -2) or an
// isosceles right triangle (k = -3).
//
// Internally every circle has radius 1 and the container -- a regular k-gon of
// circumradius R, centered at the origin, with a horizontal bottom side, the
// circle of radius R, or one of the other shapes scaled by R (see Polygon) -- is
// made as small as possible. Packomania fixes the container (circumradius Rc;
// radius 1 or legs 1 for the other shapes) and maximizes the radius; the two
// views are related by r = Rc / R.
//
// The optimizer mirrors tripack: squared-overlap penalty, L-BFGS on R + mu * P
// with mu raised step by step, bisection on R for a strictly feasible packing,
// and monotonic basin hopping. The sweep command works through a range of N and
// seeds each N from the best packings of N - 1 (add a circle at the roomiest
// spot) and N + 1 (drop a circle), revisiting neighbours whenever one improves.

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <map>
#include <random>
#include <set>
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

struct Packing {
  int n = 0;
  double R = 0;      // container circumradius, circles have radius 1
  vector<double> q;  // x0 y0 x1 y1 ...
};

bool ReadPacking(const string& path, int k, Packing& p) {
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
  if (nums.size() < 3 || static_cast<int>(nums[0]) != k) return false;
  p.n = static_cast<int>(nums[1]);
  p.R = nums[2];
  if (static_cast<int>(nums.size()) != 3 + 2 * p.n) return false;
  p.q.assign(nums.begin() + 3, nums.end());
  return true;
}

bool WritePacking(const string& path, int k, const Packing& p, const string& comment) {
  string tmp = path + ".tmp";
  FILE* f = fopen(tmp.c_str(), "w");
  if (!f) return false;
  fprintf(f, "# %s\n%d %d %.17g\n", comment.c_str(), k, p.n, p.R);
  for (int i = 0; i < p.n; i++) fprintf(f, "%.17g %.17g\n", p.q[2 * i], p.q[2 * i + 1]);
  fclose(f);
  return rename(tmp.c_str(), path.c_str()) == 0;
}

// The container for R = 1; it is scaled by R about the origin.
//   k >= 3: the regular k-gon of circumradius 1 centered at the origin with a
//           horizontal bottom side: side j has outward normal at angle
//           -pi/2 + 2*pi*j/k and lies at distance cos(pi/k) (the apothem) from the center;
//   k = 0:  the unit circle;
//   k = -1: the half disk x^2 + y^2 <= 1, y >= 0 (Packomania's csc);
//   k = -2: the quarter disk x^2 + y^2 <= 1, x >= 0, y >= 0 (ccq);
//   k = -3: the isosceles right triangle x >= 0, y >= 0, x + y <= 1 (crt).
// Each is the intersection of half-planes u . x <= h and, for k = 0, -1, -2, the unit disk.
struct Polygon {
  int k;
  bool disk;
  double area, cx, cy;  // area and centroid
  vector<double> ux, uy, h;
  explicit Polygon(int k_) : k(k_), disk(k_ <= 0 && k_ >= -2), area(kPi), cx(0), cy(0) {
    auto plane = [&](double x, double y, double off) {
      ux.push_back(x);
      uy.push_back(y);
      h.push_back(off);
    };
    if (k >= 3) {
      for (int j = 0; j < k; j++) {
        double a = -kPi / 2 + 2 * kPi * j / k;
        plane(std::cos(a), std::sin(a), std::cos(kPi / k));
      }
      area = 0.5 * k * std::sin(2 * kPi / k);
    } else if (k == -1) {
      plane(0, -1, 0);
      area = kPi / 2;
      cy = 4 / (3 * kPi);
    } else if (k == -2) {
      plane(-1, 0, 0);
      plane(0, -1, 0);
      area = kPi / 4;
      cx = cy = 4 / (3 * kPi);
    } else if (k == -3) {
      plane(-1, 0, 0);
      plane(0, -1, 0);
      plane(std::sqrt(0.5), std::sqrt(0.5), std::sqrt(0.5));
      area = 0.5;
      cx = cy = 1.0 / 3;
    }
  }
  static bool Valid(int k) { return k >= 3 || (k <= 0 && k >= -3); }
  bool circle() const { return k == 0; }
  double Area() const { return area; }
  // Signed distance from (x, y) to the boundary (positive inside).
  double Inside(double x, double y, double R) const {
    double d = disk ? R - std::hypot(x, y) : 1e300;
    for (size_t j = 0; j < h.size(); j++) d = std::min(d, R * h[j] - (x * ux[j] + y * uy[j]));
    return d;
  }
  // The container's mirror symmetry: x -> -x, or x <-> y for the quarter disk and the triangle.
  void Mirror(double& x, double& y) const {
    if (k == -2 || k == -3) {
      std::swap(x, y);
    } else {
      x = -x;
    }
  }
};

class Energy {
 public:
  Energy(int n, const Polygon& poly) : n_(n), poly_(poly), order_(n) {}

  // Squared overlaps of unit circles (needing clearance 2 + margin between
  // centers and 1 + margin from the walls); gradient into g (2n) and gR.
  double Eval(const double* q, double R, double margin, double* g, double* gR) {
    std::fill(g, g + 2 * n_, 0.0);
    double E = 0, gr = 0;
    const double need = 2 + margin, need2 = need * need;
    for (int i = 0; i < n_; i++) order_[i] = i;
    std::sort(order_.begin(), order_.end(), [&](int a, int b) { return q[2 * a] < q[2 * b]; });
    for (int a = 0; a < n_; a++) {
      int i = order_[a];
      double xi = q[2 * i], yi = q[2 * i + 1];
      for (int b = a + 1; b < n_; b++) {
        int j = order_[b];
        double dx = q[2 * j] - xi;
        if (dx >= need) break;
        double dy = q[2 * j + 1] - yi;
        if (dy >= need || dy <= -need) continue;
        double d2 = dx * dx + dy * dy;
        if (d2 >= need2) continue;
        double d = std::sqrt(d2);
        if (d < 1e-12) {  // coincident centers: push apart along a fixed direction
          dx = 1e-12;
          dy = 0;
          d = 1e-12;
        }
        double o = need - d, c = 2 * o / d;
        E += o * o;
        g[2 * i] += c * dx;
        g[2 * i + 1] += c * dy;
        g[2 * j] -= c * dx;
        g[2 * j + 1] -= c * dy;
      }
    }
    for (int i = 0; i < n_; i++) {
      double x = q[2 * i], y = q[2 * i + 1];
      if (poly_.disk) {
        double d = std::hypot(x, y), s = d - (R - 1 - margin);
        if (s > 0) {
          E += s * s;
          if (d > 0) {  // at the very center the direction is undefined, but the container still counts
            g[2 * i] += 2 * s * x / d;
            g[2 * i + 1] += 2 * s * y / d;
          }
          gr -= 2 * s;
        }
      }
      for (size_t j = 0; j < poly_.h.size(); j++) {
        double s = x * poly_.ux[j] + y * poly_.uy[j] - (R * poly_.h[j] - 1 - margin);
        if (s <= 0) continue;
        E += s * s;
        g[2 * i] += 2 * s * poly_.ux[j];
        g[2 * i + 1] += 2 * s * poly_.uy[j];
        gr -= 2 * s * poly_.h[j];
      }
    }
    *gR = gr;
    return E;
  }

  // Smallest center distance minus 2 and smallest wall clearance minus 1.
  void Clearance(const double* q, double R, double* pairGap, double* wallGap) const {
    double pg = 1e300, wg = 1e300;
    for (int i = 0; i < n_; i++) {
      wg = std::min(wg, poly_.Inside(q[2 * i], q[2 * i + 1], R) - 1);
      for (int j = i + 1; j < n_; j++)
        pg = std::min(pg, std::hypot(q[2 * i] - q[2 * j], q[2 * i + 1] - q[2 * j + 1]) - 2);
    }
    *pairGap = pg;
    *wallGap = wg;
  }

 private:
  int n_;
  const Polygon& poly_;
  vector<int> order_;
};

void Rescale(Packing& p, double newR) {
  double f = newR / p.R;
  for (double& v : p.q) v *= f;
  p.R = newR;
}

class Searcher {
 public:
  Searcher(int n, const Polygon& poly, uint64_t seed) : n_(n), poly_(poly), en_(n, poly), rng_(seed) {}

  double U(double a = 0, double b = 1) { return std::uniform_real_distribution<double>(a, b)(rng_); }
  double G(double s) { return std::normal_distribution<double>(0, s)(rng_); }
  int I(int n) { return static_cast<int>(rng_() % static_cast<uint64_t>(n)); }
  Energy& energy() { return en_; }

  // Minimizes R + mu * P(q, R) over q and R.
  void Pressure(Packing& p, double mu, int iters) {
    const int N = 2 * n_;
    vector<double> x(p.q);
    x.push_back(p.R);
    auto fg = [&](const double* xx, double* g) {
      double gR;
      double P = en_.Eval(xx, xx[N], 0, g, &gR);
      for (int i = 0; i < N; i++) g[i] *= mu;
      g[N] = 1 + mu * gR;
      return xx[N] + mu * P;
    };
    Lbfgs(x, fg, iters, -1e300, 0.5);
    p.R = x[N];
    x.pop_back();
    p.q = x;
  }

  double Relax(Packing& p, double margin, int iters, double fStop) {
    auto fg = [&](const double* xx, double* g) {
      double gR;
      return en_.Eval(xx, p.R, margin, g, &gR);
    };
    return Lbfgs(p.q, fg, iters, fStop, 0.2);
  }

  // Pressure schedule from soft to hard. Gives up (returns false) when R after
  // the soft stages already exceeds abortAbove.
  bool Compress(Packing& p, double mu0, double abortAbove = 1e300) {
    double mu = mu0;
    for (; mu <= 1.01e4; mu *= 10) Pressure(p, mu, 300);
    if (p.R > abortAbove) return false;
    for (; mu <= 1.01e10; mu *= 10) Pressure(p, mu, 300);
    return true;
  }

  // Smallest R (to within tol) for which the arrangement relaxes to a packing
  // with clearance `margin`.
  bool Polish(Packing& p, double margin, double tol = 1e-12) {
    const double kFeas = 1e-28;
    const int kIters = 4000;
    Packing hi = p;
    double up = 0;
    for (double step = 1e-10;; step *= 4) {
      Packing t = p;
      Rescale(t, p.R + up);
      if (Relax(t, margin, kIters, kFeas) <= kFeas) {
        hi = t;
        break;
      }
      up = step;
      if (step > 1e-1) return false;
    }
    double gap = std::max(1e-8, 4 * up), lo = hi.R - gap;
    while (true) {
      Packing t = hi;
      Rescale(t, lo);
      if (Relax(t, margin, kIters, kFeas) > kFeas) break;
      hi = t;
      gap *= 2;
      lo = hi.R - gap;
    }
    while (hi.R - lo > tol * hi.R) {
      double mid = 0.5 * (lo + hi.R);
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

  void RandomPoint(double R, double* x, double* y) {
    for (;;) {
      double a = U(-R, R), b = U(-R, R);
      if (poly_.Inside(a, b, R) > 0) {
        *x = a;
        *y = b;
        return;
      }
    }
  }

  Packing RandomStart() {
    // Container with about 55% density so that the start is loose.
    double area1 = poly_.Area();
    Packing p;
    p.n = n_;
    p.R = std::sqrt(n_ * kPi / (0.55 * area1)) + 1;
    for (int i = 0; i < n_; i++) {
      double x, y;
      RandomPoint(p.R - 1, &x, &y);
      p.q.push_back(x);
      p.q.push_back(y);
    }
    return p;
  }

  // Hexagonal patch: a lattice of spacing 2 with random orientation and offset;
  // the n sites lying deepest inside the container are kept.
  Packing LatticeStart() {
    double area1 = poly_.Area();
    Packing p;
    p.n = n_;
    p.R = std::sqrt(n_ * kPi / (0.85 * area1)) + 1;
    double phi = U(0, kPi / 3), ox = U(0, 2), oy = U(0, 2);
    double ax = 2 * std::cos(phi), ay = 2 * std::sin(phi);
    double bx = 2 * std::cos(phi + kPi / 3), by = 2 * std::sin(phi + kPi / 3);
    for (;;) {
      vector<std::pair<double, std::pair<double, double>>> sites;
      int m = static_cast<int>(p.R) + 2;
      for (int i = -m; i <= m; i++) {
        for (int j = -m; j <= m; j++) {
          double x = ox + i * ax + j * bx, y = oy + i * ay + j * by;
          double depth = poly_.Inside(x, y, p.R);
          if (depth >= 1) sites.push_back({depth, {x, y}});
        }
      }
      if (static_cast<int>(sites.size()) >= n_) {
        std::sort(sites.begin(), sites.end(), [](const auto& a, const auto& b) { return a.first > b.first; });
        for (int i = 0; i < n_; i++) {
          p.q.push_back(sites[i].second.first);
          p.q.push_back(sites[i].second.second);
        }
        return p;
      }
      p.R *= 1.03;
    }
  }

  // Free room around (x, y): distance to the walls and to every other circle, minus 1.
  double Room(const Packing& p, int count, double x, double y, int skip) const {
    double c = poly_.Inside(x, y, p.R) - 1;
    for (int i = 0; i < count; i++) {
      if (i == skip) continue;
      c = std::min(c, std::hypot(x - p.q[2 * i], y - p.q[2 * i + 1]) - 2);
    }
    return c;
  }

  // Roomiest spot found among random samples refined by a little hill climbing.
  void Hole(const Packing& p, int count, int skip, double* bx, double* by) {
    double best = -1e300;
    for (int s = 0; s < 400; s++) {
      double x, y;
      RandomPoint(p.R, &x, &y);
      double c = Room(p, count, x, y, skip);
      if (c > best) {
        best = c;
        *bx = x;
        *by = y;
      }
    }
    for (double step = 0.5; step > 1e-3; step *= 0.7) {
      for (int t = 0; t < 8; t++) {
        double x = *bx + G(step), y = *by + G(step);
        double c = Room(p, count, x, y, skip);
        if (c > best) {
          best = c;
          *bx = x;
          *by = y;
        }
      }
    }
  }

  // Seed for n circles from a packing of n - 1: add one at the roomiest spot.
  Packing FromSmaller(const Packing& s) {
    Packing p = s;
    p.n = n_;
    double x = 0, y = 0;
    Hole(s, s.n, -1, &x, &y);
    p.q.push_back(x);
    p.q.push_back(y);
    Rescale(p, p.R * 1.02);
    return p;
  }

  // Seed for n circles from a packing of n + 1: drop one circle.
  Packing FromLarger(const Packing& s) {
    Packing p = s;
    int i = I(s.n);
    p.q.erase(p.q.begin() + 2 * i, p.q.begin() + 2 * i + 2);
    p.n = n_;
    return p;
  }

  // A symmetry of the container applied to a packing: rotation by m * 2pi/k
  // (any angle for the circle), optionally preceded by the mirror in the
  // vertical axis. The shapes with k < 0 have their mirror only.
  void Symmetry(Packing& p, int m, bool mirror) {
    if (poly_.k < 0) {
      if (mirror)
        for (int i = 0; i < p.n; i++) poly_.Mirror(p.q[2 * i], p.q[2 * i + 1]);
      return;
    }
    double a = poly_.circle() ? U(0, 2 * kPi) : 2 * kPi * m / poly_.k, c = std::cos(a), s = std::sin(a);
    for (int i = 0; i < p.n; i++) {
      double x = p.q[2 * i], y = p.q[2 * i + 1];
      if (mirror) x = -x;
      p.q[2 * i] = c * x - s * y;
      p.q[2 * i + 1] = s * x + c * y;
    }
  }

  // Child of A and B: A's circles on one side of a random line through the
  // container, B's on the other; the count is repaired by dropping the most
  // overlapped circles or adding circles at the roomiest spots.
  Packing Crossover(const Packing& A, Packing B) {
    Rescale(B, A.R);
    // The cut passes near the centroid; its offset scales with the container's size.
    double ang = U(0, kPi), ux = std::cos(ang), uy = std::sin(ang),
           off = U(-0.3, 0.3) * A.R * (poly_.k < 0 ? std::sqrt(poly_.Area() / kPi) : 1.0);
    double cx = poly_.cx * A.R, cy = poly_.cy * A.R;
    Packing c;
    c.n = 0;
    c.R = A.R;
    for (int src = 0; src < 2; src++) {
      const Packing& P = src ? B : A;
      for (int i = 0; i < P.n; i++) {
        bool side = (P.q[2 * i] - cx) * -uy + (P.q[2 * i + 1] - cy) * ux >= off;
        if (side != (src == 0)) continue;
        c.q.push_back(P.q[2 * i]);
        c.q.push_back(P.q[2 * i + 1]);
        c.n++;
      }
    }
    while (c.n > n_) {  // drop the circle with the largest total overlap
      int worst = 0;
      double wv = -1;
      for (int i = 0; i < c.n; i++) {
        double v = 0;
        for (int j = 0; j < c.n; j++)
          if (j != i) v += std::max(0.0, 2 - std::hypot(c.q[2 * i] - c.q[2 * j], c.q[2 * i + 1] - c.q[2 * j + 1]));
        if (v > wv) {
          wv = v;
          worst = i;
        }
      }
      c.q.erase(c.q.begin() + 2 * worst, c.q.begin() + 2 * worst + 2);
      c.n--;
    }
    while (c.n < n_) {
      double x = 0, y = 0;
      Hole(c, c.n, -1, &x, &y);
      c.q.push_back(x);
      c.q.push_back(y);
      c.n++;
    }
    return c;
  }

  enum Move { kShake, kLocal, kRelocate, kRotate, kNumMoves };

  void Perturb(Packing& p, int mv) {
    switch (mv) {
      case kShake: {  // monotonic basin hopping: every coordinate moved a little
        double s = U(0.05, 0.6);
        for (double& v : p.q) v += U(-s, s);
        break;
      }
      case kLocal: {
        double cx, cy;
        RandomPoint(p.R, &cx, &cy);
        double rad = U(2.0, 5.0), s = U(0.3, 1.5);
        for (int i = 0; i < n_; i++) {
          if (std::hypot(p.q[2 * i] - cx, p.q[2 * i + 1] - cy) > rad) continue;
          p.q[2 * i] += G(s);
          p.q[2 * i + 1] += G(s);
        }
        break;
      }
      case kRelocate: {
        int m = 1 + I(3);
        for (int t = 0; t < m; t++) {
          int i = I(n_);
          double x = 0, y = 0;
          Hole(p, n_, i, &x, &y);
          p.q[2 * i] = x;
          p.q[2 * i + 1] = y;
        }
        break;
      }
      case kRotate: {  // turn a patch of circles about a random point
        double cx, cy;
        RandomPoint(p.R, &cx, &cy);
        double rad = U(2.0, 6.0), phi = G(0.25), cs = std::cos(phi), sn = std::sin(phi);
        for (int i = 0; i < n_; i++) {
          double dx = p.q[2 * i] - cx, dy = p.q[2 * i + 1] - cy;
          if (std::hypot(dx, dy) > rad) continue;
          p.q[2 * i] = cx + cs * dx - sn * dy;
          p.q[2 * i + 1] = cy + sn * dx + cs * dy;
        }
        break;
      }
    }
  }

  // Basin hopping from `start`; returns the best polished packing found.
  Packing BasinHop(Packing start, int trials, double temp, long* tried) {
    Packing cur = start, best = start;
    for (int t = 0; t < trials; t++) {
      Packing x = cur;
      Rescale(x, cur.R * (1 + std::exp(U(std::log(0.001), std::log(0.05)))));
      int mv = I(kNumMoves);
      Perturb(x, mv);
      double mus[] = {1e1, 1e2, 1e3};
      if (!Compress(x, mus[I(3)], cur.R * (1 + 30 * temp))) continue;
      if (tried) (*tried)++;
      if (x.R < best.R * (1 - 1e-9)) {
        Packing pol = x;
        if (Polish(pol, 1e-11) && pol.R < best.R * (1 - 1e-11)) best = pol;
      }
      if (x.R < cur.R || U() < std::exp(-(x.R - cur.R) / (temp * cur.R))) cur = x;
      if (t % 200 == 199) cur = best;
    }
    return best;
  }

 private:
  int n_;
  const Polygon& poly_;
  Energy en_;
  std::mt19937_64 rng_;
};

// ---------------------------------------------------------------- commands

string BestPath(const string& dir, int n) { return dir + "/" + std::to_string(n) + ".txt"; }

// Reads "N r" lines (Packomania radii for a container of circumradius rc).
std::map<int, double> ReadRecords(const string& path) {
  std::map<int, double> r;
  FILE* f = fopen(path.c_str(), "r");
  if (!f) return r;
  int n;
  double v;
  while (fscanf(f, "%d %lf", &n, &v) == 2) r[n] = v;
  fclose(f);
  return r;
}

int CmdCheck(const std::map<string, string>& a) {
  int k = static_cast<int>(Arg(a, "k", 0));
  Polygon poly(k);
  Packing p;
  if (!ReadPacking(ArgS(a, "in", ""), k, p)) {
    fprintf(stderr, "cannot read --in for k=%d\n", k);
    return 1;
  }
  Energy en(p.n, poly);
  double pg, wg;
  en.Clearance(p.q.data(), p.R, &pg, &wg);
  double rc = Arg(a, "rc", 1.0);
  printf("k=%d n=%d R=%.15f r=%.15f pair gap=%.3e wall gap=%.3e %s\n", k, p.n, p.R, rc / p.R, pg, wg,
         pg > 0 && wg > 0 ? "VALID" : "INVALID");
  return pg > 0 && wg > 0 ? 0 : 1;
}

int CmdGradCheck(const std::map<string, string>& a) {
  int k = static_cast<int>(Arg(a, "k", 7)), n = static_cast<int>(Arg(a, "n", 20));
  Polygon poly(k);
  Searcher s(n, poly, 3);
  Packing p = s.RandomStart();
  Rescale(p, p.R * 0.6);
  Energy en(n, poly);
  vector<double> g(2 * n), g2(2 * n);
  double gR, gR2, margin = Arg(a, "margin", 0.0);
  en.Eval(p.q.data(), p.R, margin, g.data(), &gR);
  double worst = 0;
  for (int i = 0; i <= 2 * n; i++) {
    const double h = 1e-7;
    Packing pp = p, pm = p;
    if (i < 2 * n) {
      pp.q[i] += h;
      pm.q[i] -= h;
    } else {
      pp.R += h;
      pm.R -= h;
    }
    double ep = en.Eval(pp.q.data(), pp.R, margin, g2.data(), &gR2);
    double em = en.Eval(pm.q.data(), pm.R, margin, g2.data(), &gR2);
    double num = (ep - em) / (2 * h), ana = i < 2 * n ? g[i] : gR;
    worst = std::max(worst, std::abs(num - ana) / std::max(1.0, std::abs(num)));
  }
  printf("worst relative gradient error=%.3e\n", worst);
  return worst < 1e-5 ? 0 : 1;
}

// Works through n = nmin..nmax, keeping the best packing of each n in --dir.
// Every visit runs basin hopping from the current best and tries seeds built
// from the neighbours n - 1 and n + 1 plus a fresh random start; an improvement
// queues both neighbours for another visit.
int CmdSweep(const std::map<string, string>& a) {
  int k = static_cast<int>(Arg(a, "k", 0)), nmin = static_cast<int>(Arg(a, "nmin", 1)),
      nmax = static_cast<int>(Arg(a, "nmax", 48));
  string dir = ArgS(a, "dir", ""), tag = ArgS(a, "tag", "circpack");
  double timeLimit = Arg(a, "time", 600), rc = Arg(a, "rc", 1.0), temp = Arg(a, "temp", 1e-4);
  int trials = static_cast<int>(Arg(a, "trials", 300));
  if (!Polygon::Valid(k) || dir.empty()) {
    fprintf(stderr, "--k (>= 3; 0 circle, -1 half disk, -2 quarter disk, -3 right triangle) and --dir required\n");
    return 1;
  }
  Polygon poly(k);
  std::map<int, double> rec = ReadRecords(ArgS(a, "records", ""));
  std::mt19937_64 rng(static_cast<uint64_t>(Arg(a, "seed", 1)));
  std::map<int, Packing> best;
  string seedDir = ArgS(a, "seeds", "");
  for (int n = std::max(1, nmin - 1); n <= nmax + 1; n++) {
    Packing p;
    if (ReadPacking(BestPath(dir, n), k, p) && p.n == n) best[n] = p;
    // Outside packings (e.g. the published records) are polished, since their
    // printed digits can leave tiny overlaps, and adopted when they are better.
    if (!seedDir.empty() && ReadPacking(BestPath(seedDir, n), k, p) && p.n == n) {
      // Polishing costs seconds for large n: skip seeds that cannot beat the stored best.
      if (best.count(n) && best[n].R <= p.R * (1 + 1e-10)) continue;
      Searcher s(n, poly, 7);
      if (s.Polish(p, 1e-11) && (!best.count(n) || p.R < best[n].R * (1 - 1e-12))) {
        best[n] = p;
        if (n >= nmin && n <= nmax) WritePacking(BestPath(dir, n), k, p, "seed from " + seedDir);
      }
    }
  }
  auto status = [&](int n) {
    double r = rc / best[n].R;
    auto it = rec.find(n);
    if (it == rec.end()) return string("(no record)");
    char buf[96];
    snprintf(buf, sizeof buf, "record %.12f, diff %+.3e", it->second, r - it->second);
    return string(buf);
  };
  // --only-file restricts the visits to the sizes it lists (neighbours still seed them).
  std::set<int> only;
  if (FILE* f = fopen(ArgS(a, "only-file", "").c_str(), "r")) {
    int v;
    while (fscanf(f, "%d", &v) == 1) only.insert(v);
    fclose(f);
  }
  auto visit = [&](int n) { return only.empty() || only.count(n) > 0; };
  vector<int> queue;
  for (int n = nmin; n <= nmax; n++)
    if (visit(n)) queue.push_back(n);
  if (queue.empty()) {
    fprintf(stderr, "no size of %d..%d is listed in --only-file\n", nmin, nmax);
    return 1;
  }
  std::shuffle(queue.begin(), queue.end(), rng);
  double t0 = Now();
  long visits = 0;
  while (Now() - t0 < timeLimit) {
    if (queue.empty()) {  // start another pass over the whole range
      for (int n = nmin; n <= nmax; n++)
        if (visit(n)) queue.push_back(n);
      std::shuffle(queue.begin(), queue.end(), rng);
    }
    int n = queue.back();
    queue.pop_back();
    visits++;
    Searcher s(n, poly, rng());
    vector<Packing> starts;
    vector<string> kinds;  // how each start was made, reported with improvements
    if (best.count(n)) {
      starts.push_back(best[n]);
      kinds.push_back("bh");
    }
    if (best.count(n - 1)) {
      starts.push_back(s.FromSmaller(best[n - 1]));
      kinds.push_back("grow");
    }
    if (best.count(n + 1)) {
      starts.push_back(s.FromLarger(best[n + 1]));
      kinds.push_back("shrink");
    }
    if (best.count(n) && n > 20 && s.U() < 0.5) {  // recombine the best with a neighbour seed or itself
      Packing other = s.U() < 0.5 && best.count(n - 1)   ? s.FromSmaller(best[n - 1])
                      : s.U() < 0.5 && best.count(n + 1) ? s.FromLarger(best[n + 1])
                                                         : best[n];
      s.Symmetry(other, s.I(std::max(poly.k, 1)), s.U() < 0.5);
      starts.push_back(s.Crossover(best[n], other));
      kinds.push_back("cross");
    }
    // Fresh lattice/random starts pay off for small n only (campaign statistics:
    // above n ~ 60 nearly every record improvement came from the other starts).
    if (n <= 60 || !best.count(n) || s.U() < 0.2) {
      bool lattice = s.U() < 0.5;
      starts.push_back(lattice ? s.LatticeStart() : s.RandomStart());
      kinds.push_back(lattice ? "lattice" : "random");
    }
    double before = best.count(n) ? best[n].R : 1e300;
    string via;
    for (size_t i = 0; i < starts.size(); i++) {
      Packing p = starts[i];
      if (kinds[i] != "bh") {  // fresh seeds are compressed and polished first
        s.Compress(p, 1e1);
        if (!s.Polish(p, 1e-11)) continue;
      }
      long tried = 0;
      int budget = kinds[i] == "bh" ? (n > 60 ? trials * 3 / 2 : trials) : trials / 2;
      Packing b = s.BasinHop(p, budget, temp, &tried);
      if (!best.count(n) || b.R < best[n].R * (1 - 1e-12)) {
        best[n] = b;
        via = kinds[i];
      }
    }
    if (best[n].R < before * (1 - 1e-12)) {
      // Another process may have stored something better meanwhile: never overwrite it.
      Packing disk;
      if (ReadPacking(BestPath(dir, n), k, disk) && disk.n == n && disk.R <= best[n].R) {
        best[n] = disk;
        continue;
      }
      char buf[160];
      snprintf(buf, sizeof buf, "k=%d n=%d r=%.15f found by %s", k, n, rc / best[n].R, tag.c_str());
      WritePacking(BestPath(dir, n), k, best[n], buf);
      printf("[%7.0fs] n=%3d r=%.12f  %s  via=%s%s\n", Now() - t0, n, rc / best[n].R, status(n).c_str(),
             via.c_str(), before == 1e300 ? "  (first)" : "");
      fflush(stdout);
      for (int m : {n - 1, n + 1})
        if (m >= nmin && m <= nmax && visit(m) && std::find(queue.begin(), queue.end(), m) == queue.end())
          queue.push_back(m);
    }
  }
  printf("sweep done: %ld visits in %.0fs\n", visits, Now() - t0);
  for (int n = nmin; n <= nmax; n++)
    if (best.count(n)) printf("n=%3d r=%.12f  %s\n", n, rc / best[n].R, status(n).c_str());
  return 0;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 2) {
    fprintf(stderr,
            "usage: circpack <command> [--key value ...]\n"
            "  sweep     --k K (0 circle, -1 half disk, -2 quarter disk, -3 right triangle) --dir DIR [--nmin A] [--nmax B] [--time SEC] [--seed N] [--rc RC]\n"
            "            [--records FILE] [--seeds DIR] [--trials T] [--temp T] [--tag NAME] [--only-file FILE]\n"
            "  check     --k K --in FILE [--rc RC]\n"
            "  gradcheck [--k K] [--n N] [--margin M]\n");
    return 2;
  }
  string cmd = argv[1];
  auto a = ParseArgs(argc, argv, 2);
  if (cmd == "sweep") return CmdSweep(a);
  if (cmd == "check") return CmdCheck(a);
  if (cmd == "gradcheck") return CmdGradCheck(a);
  fprintf(stderr, "unknown command %s\n", cmd.c_str());
  return 2;
}
