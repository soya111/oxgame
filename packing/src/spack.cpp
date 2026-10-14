// spack: equal spheres in a ball or a cube, in any dimension d >= 2
// (Packomania's "spheres in a sphere" (ssp), "spheres in a cube" (scu) and
// "hyperspheres in a hypersphere" (hsp4..hsp6) tables).
//
// Internally every sphere has radius 1 and the container -- the ball of radius R
// centered at the origin, or the cube [-R, R]^d -- is made as small as possible.
// Packomania fixes the container (unit ball, unit cube [-1/2, 1/2]^d) and
// maximizes the radius: r = 1 / R for the ball, r = 1 / (2R) for the cube.
//
// The optimizer is circpack's in d dimensions: squared-overlap penalty, L-BFGS
// on R + mu * P with mu raised step by step, bisection on R for a strictly
// feasible packing, monotonic basin hopping, and a sweep over a range of n that
// seeds each n from the best packings of n - 1 (add a sphere at the roomiest
// spot) and n + 1 (drop a sphere).
//
// File format: '#' comments, then "d kind n R" (kind 0 = ball, 1 = cube), then n
// lines of d coordinates.

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

struct Container {
  int d = 3;
  bool cube = false;
  // Signed room from point x to the boundary of the container of size R.
  double Inside(const double* x, double R) const {
    if (cube) {
      double m = 0;
      for (int k = 0; k < d; k++) m = std::max(m, std::abs(x[k]));
      return R - m;
    }
    double s = 0;
    for (int k = 0; k < d; k++) s += x[k] * x[k];
    return R - std::sqrt(s);
  }
  // Volume of the container with R = 1 relative to a unit ball's.
  double RelVolume() const {
    if (!cube) return 1.0;
    // unit ball volume pi^(d/2) / Gamma(d/2 + 1); cube [-1,1]^d has volume 2^d
    return std::pow(2.0, d) / (std::pow(kPi, d / 2.0) / std::tgamma(d / 2.0 + 1));
  }
};

struct Packing {
  int n = 0;
  double R = 0;
  vector<double> q;  // n * d coordinates
};

bool ReadPacking(const string& path, const Container& c, Packing& p) {
  FILE* f = fopen(path.c_str(), "r");
  if (!f) return false;
  char line[1 << 14];
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
  if (nums.size() < 4 || static_cast<int>(nums[0]) != c.d || static_cast<int>(nums[1]) != (c.cube ? 1 : 0)) return false;
  p.n = static_cast<int>(nums[2]);
  p.R = nums[3];
  if (static_cast<int>(nums.size()) != 4 + c.d * p.n) return false;
  p.q.assign(nums.begin() + 4, nums.end());
  return true;
}

bool WritePacking(const string& path, const Container& c, const Packing& p, const string& comment) {
  string tmp = path + ".tmp";
  FILE* f = fopen(tmp.c_str(), "w");
  if (!f) return false;
  fprintf(f, "# %s\n%d %d %d %.17g\n", comment.c_str(), c.d, c.cube ? 1 : 0, p.n, p.R);
  for (int i = 0; i < p.n; i++) {
    for (int k = 0; k < c.d; k++) fprintf(f, k ? " %.17g" : "%.17g", p.q[c.d * i + k]);
    fprintf(f, "\n");
  }
  fclose(f);
  return rename(tmp.c_str(), path.c_str()) == 0;
}

void Rescale(Packing& p, double newR) {
  double f = newR / p.R;
  for (double& v : p.q) v *= f;
  p.R = newR;
}

class Energy {
 public:
  Energy(int n, const Container& c) : n_(n), c_(c), order_(n) {}

  // Squared overlaps of unit spheres (clearance 2 + margin between centers and
  // 1 + margin from the boundary); gradient into g (n*d) and gR.
  double Eval(const double* q, double R, double margin, double* g, double* gR) {
    const int d = c_.d;
    std::fill(g, g + d * n_, 0.0);
    double E = 0, gr = 0;
    const double need = 2 + margin, need2 = need * need;
    for (int i = 0; i < n_; i++) order_[i] = i;
    std::sort(order_.begin(), order_.end(), [&](int a, int b) { return q[d * a] < q[d * b]; });
    for (int a = 0; a < n_; a++) {
      int i = order_[a];
      const double* xi = q + d * i;
      for (int b = a + 1; b < n_; b++) {
        int j = order_[b];
        const double* xj = q + d * j;
        if (xj[0] - xi[0] >= need) break;
        double d2 = 0;
        for (int k = 0; k < d && d2 < need2; k++) d2 += (xj[k] - xi[k]) * (xj[k] - xi[k]);
        if (d2 >= need2) continue;
        double dist = std::sqrt(d2);
        if (dist < 1e-12) {  // coincident centers: push apart along the first axis
          E += need * need;
          g[d * i] -= need;
          g[d * j] += need;
          continue;
        }
        double o = need - dist, cf = 2 * o / dist;
        E += o * o;
        for (int k = 0; k < d; k++) {
          double dk = xj[k] - xi[k];
          g[d * i + k] += cf * dk;
          g[d * j + k] -= cf * dk;
        }
      }
    }
    const double lim = R - 1 - margin;
    for (int i = 0; i < n_; i++) {
      const double* x = q + d * i;
      if (c_.cube) {
        for (int k = 0; k < d; k++) {
          double s = std::abs(x[k]) - lim;
          if (s <= 0) continue;
          E += s * s;
          g[d * i + k] += 2 * s * (x[k] > 0 ? 1 : -1);
          gr -= 2 * s;
        }
      } else {
        double r2 = 0;
        for (int k = 0; k < d; k++) r2 += x[k] * x[k];
        double rr = std::sqrt(r2), s = rr - lim;
        if (s <= 0) continue;
        E += s * s;
        if (rr > 0)
          for (int k = 0; k < d; k++) g[d * i + k] += 2 * s * x[k] / rr;
        gr -= 2 * s;
      }
    }
    *gR = gr;
    return E;
  }

  void Clearance(const double* q, double R, double* pairGap, double* wallGap) const {
    const int d = c_.d;
    double pg = 1e300, wg = 1e300;
    for (int i = 0; i < n_; i++) {
      wg = std::min(wg, c_.Inside(q + d * i, R) - 1);
      for (int j = i + 1; j < n_; j++) {
        double d2 = 0;
        for (int k = 0; k < d; k++) d2 += (q[d * i + k] - q[d * j + k]) * (q[d * i + k] - q[d * j + k]);
        pg = std::min(pg, std::sqrt(d2) - 2);
      }
    }
    *pairGap = pg;
    *wallGap = wg;
  }

 private:
  int n_;
  const Container& c_;
  vector<int> order_;
};

class Searcher {
 public:
  Searcher(int n, const Container& c, uint64_t seed) : n_(n), c_(c), en_(n, c), rng_(seed) {}

  double U(double a = 0, double b = 1) { return std::uniform_real_distribution<double>(a, b)(rng_); }
  double G(double s) { return std::normal_distribution<double>(0, s)(rng_); }
  int I(int n) { return static_cast<int>(rng_() % static_cast<uint64_t>(n)); }

  void Pressure(Packing& p, double mu, int iters) {
    const int N = c_.d * n_;
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

  bool Compress(Packing& p, double mu0, double abortAbove = 1e300) {
    double mu = mu0;
    for (; mu <= 1.01e4; mu *= 10) Pressure(p, mu, 300);
    if (p.R > abortAbove) return false;
    for (; mu <= 1.01e10; mu *= 10) Pressure(p, mu, 300);
    return true;
  }

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
    while (lo > 0) {
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

  void RandomPoint(double R, double* x) {
    for (;;) {
      for (int k = 0; k < c_.d; k++) x[k] = U(-R, R);
      if (c_.Inside(x, R) > 0) return;
    }
  }

  Packing RandomStart() {
    // Container with about 30% density so that the start is loose.
    Packing p;
    p.n = n_;
    p.R = std::pow(n_ / (0.3 * c_.RelVolume()), 1.0 / c_.d) + 1;
    p.q.resize(c_.d * n_);
    for (int i = 0; i < n_; i++) RandomPoint(p.R - 1, &p.q[c_.d * i]);
    return p;
  }

  // A cut-out of the D_d lattice (integer points with an even coordinate sum --
  // the face-centred cubic lattice for d = 3), scaled so that neighbours are 2
  // apart, randomly rotated and shifted: the n points deepest inside the
  // container are kept, the container being grown until enough points fit.
  Packing LatticeStart() {
    const int d = c_.d;
    vector<double> Q(d * d);  // random orthonormal basis by Gram-Schmidt
    for (int i = 0; i < d; i++) {
      for (;;) {
        for (int k = 0; k < d; k++) Q[d * i + k] = G(1);
        for (int j = 0; j < i; j++) {
          double dot = 0;
          for (int k = 0; k < d; k++) dot += Q[d * i + k] * Q[d * j + k];
          for (int k = 0; k < d; k++) Q[d * i + k] -= dot * Q[d * j + k];
        }
        double nrm = 0;
        for (int k = 0; k < d; k++) nrm += Q[d * i + k] * Q[d * i + k];
        if (nrm < 1e-6) continue;
        nrm = std::sqrt(nrm);
        for (int k = 0; k < d; k++) Q[d * i + k] /= nrm;
        break;
      }
    }
    vector<double> off(d);
    for (int k = 0; k < d; k++) off[k] = U(0, 2);
    Packing p;
    p.n = n_;
    p.R = std::pow(n_ / (0.7 * c_.RelVolume()), 1.0 / d) + 1;
    const double s2 = std::sqrt(2.0);
    for (;;) {
      int m = static_cast<int>(p.R / s2) + 3;
      vector<std::pair<double, vector<double>>> sites;
      vector<int> z(d, -m);
      vector<double> x(d);
      for (;;) {
        int sum = 0;
        for (int k = 0; k < d; k++) sum += z[k];
        if ((sum & 1) == 0) {
          for (int k = 0; k < d; k++) {
            double v = 0;
            for (int j = 0; j < d; j++) v += Q[d * j + k] * s2 * (z[j] + off[j]);
            x[k] = v;
          }
          double depth = c_.Inside(x.data(), p.R);
          if (depth >= 1) sites.push_back({depth, x});
        }
        int k = 0;
        while (k < d && ++z[k] > m) z[k++] = -m;
        if (k == d) break;
      }
      if (static_cast<int>(sites.size()) >= n_) {
        std::sort(sites.begin(), sites.end(), [](const auto& a, const auto& b) { return a.first > b.first; });
        for (int i = 0; i < n_; i++) p.q.insert(p.q.end(), sites[i].second.begin(), sites[i].second.end());
        return p;
      }
      p.R *= 1.05;
    }
  }

  double Room(const Packing& p, int count, const double* x, int skip) const {
    double r = c_.Inside(x, p.R) - 1;
    for (int i = 0; i < count; i++) {
      if (i == skip) continue;
      double d2 = 0;
      for (int k = 0; k < c_.d; k++) d2 += (x[k] - p.q[c_.d * i + k]) * (x[k] - p.q[c_.d * i + k]);
      r = std::min(r, std::sqrt(d2) - 2);
    }
    return r;
  }

  void Hole(const Packing& p, int count, int skip, double* best) {
    vector<double> x(c_.d);
    double bv = -1e300;
    for (int s = 0; s < 400; s++) {
      RandomPoint(p.R, x.data());
      double v = Room(p, count, x.data(), skip);
      if (v > bv) {
        bv = v;
        std::copy(x.begin(), x.end(), best);
      }
    }
    for (double step = 0.5; step > 1e-3; step *= 0.7) {
      for (int t = 0; t < 8; t++) {
        for (int k = 0; k < c_.d; k++) x[k] = best[k] + G(step);
        double v = Room(p, count, x.data(), skip);
        if (v > bv) {
          bv = v;
          std::copy(x.begin(), x.end(), best);
        }
      }
    }
  }

  Packing FromSmaller(const Packing& s) {
    Packing p = s;
    p.n = n_;
    p.q.resize(c_.d * n_);
    Hole(s, s.n, -1, &p.q[c_.d * (n_ - 1)]);
    Rescale(p, p.R * 1.02);
    return p;
  }

  Packing FromLarger(const Packing& s) {
    Packing p = s;
    int i = I(s.n);
    p.q.erase(p.q.begin() + c_.d * i, p.q.begin() + c_.d * (i + 1));
    p.n = n_;
    return p;
  }

  enum Move { kShake, kLocal, kRelocate, kRotate, kNumMoves };

  void Perturb(Packing& p, int mv) {
    const int d = c_.d;
    switch (mv) {
      case kShake: {
        double s = U(0.05, 0.6);
        for (double& v : p.q) v += U(-s, s);
        break;
      }
      case kLocal: {
        vector<double> cpt(d);
        RandomPoint(p.R, cpt.data());
        double rad = U(2.0, 5.0), s = U(0.3, 1.5);
        for (int i = 0; i < n_; i++) {
          double d2 = 0;
          for (int k = 0; k < d; k++) d2 += (p.q[d * i + k] - cpt[k]) * (p.q[d * i + k] - cpt[k]);
          if (d2 > rad * rad) continue;
          for (int k = 0; k < d; k++) p.q[d * i + k] += G(s);
        }
        break;
      }
      case kRelocate: {
        int m = 1 + I(3);
        for (int t = 0; t < m; t++) {
          int i = I(n_);
          Hole(p, n_, i, &p.q[d * i]);
        }
        break;
      }
      case kRotate: {  // turn a patch about a random point within a random coordinate plane
        vector<double> cpt(d);
        RandomPoint(p.R, cpt.data());
        int a = I(d), b = I(d - 1);
        if (b >= a) b++;
        double rad = U(2.0, 6.0), phi = G(0.25), cs = std::cos(phi), sn = std::sin(phi);
        for (int i = 0; i < n_; i++) {
          double d2 = 0;
          for (int k = 0; k < d; k++) d2 += (p.q[d * i + k] - cpt[k]) * (p.q[d * i + k] - cpt[k]);
          if (d2 > rad * rad) continue;
          double u = p.q[d * i + a] - cpt[a], v = p.q[d * i + b] - cpt[b];
          p.q[d * i + a] = cpt[a] + cs * u - sn * v;
          p.q[d * i + b] = cpt[b] + sn * u + cs * v;
        }
        break;
      }
    }
  }

  Packing BasinHop(Packing start, int trials, double temp, long* tried) {
    Packing cur = start, best = start;
    for (int t = 0; t < trials; t++) {
      Packing x = cur;
      Rescale(x, cur.R * (1 + std::exp(U(std::log(0.001), std::log(0.05)))));
      Perturb(x, I(kNumMoves));
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
  const Container& c_;
  Energy en_;
  std::mt19937_64 rng_;
};

// ---------------------------------------------------------------- commands

string BestPath(const string& dir, int n) { return dir + "/" + std::to_string(n) + ".txt"; }

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

Container ContainerFrom(const std::map<string, string>& a) {
  Container c;
  c.d = static_cast<int>(Arg(a, "d", 3));
  c.cube = ArgS(a, "container", "ball") == "cube";
  return c;
}

// Packomania's radius for a packing of unit spheres in a container of size R.
double PmRadius(const Container& c, double R) { return c.cube ? 0.5 / R : 1.0 / R; }

int CmdCheck(const std::map<string, string>& a) {
  Container c = ContainerFrom(a);
  Packing p;
  if (!ReadPacking(ArgS(a, "in", ""), c, p)) {
    fprintf(stderr, "cannot read --in\n");
    return 1;
  }
  Energy en(p.n, c);
  double pg, wg;
  en.Clearance(p.q.data(), p.R, &pg, &wg);
  printf("d=%d %s n=%d R=%.15f r=%.15f pair gap=%.3e wall gap=%.3e %s\n", c.d, c.cube ? "cube" : "ball", p.n, p.R,
         PmRadius(c, p.R), pg, wg, pg > 0 && wg > 0 ? "VALID" : "INVALID");
  return pg > 0 && wg > 0 ? 0 : 1;
}

int CmdGradCheck(const std::map<string, string>& a) {
  Container c = ContainerFrom(a);
  int n = static_cast<int>(Arg(a, "n", 20));
  Searcher s(n, c, 3);
  Packing p = s.RandomStart();
  Rescale(p, p.R * 0.6);
  Energy en(n, c);
  vector<double> g(c.d * n), g2(c.d * n);
  double gR, gR2, margin = Arg(a, "margin", 0.0);
  en.Eval(p.q.data(), p.R, margin, g.data(), &gR);
  double worst = 0;
  for (int i = 0; i <= c.d * n; i++) {
    const double h = 1e-7;
    Packing pp = p, pm = p;
    if (i < c.d * n) {
      pp.q[i] += h;
      pm.q[i] -= h;
    } else {
      pp.R += h;
      pm.R -= h;
    }
    double ep = en.Eval(pp.q.data(), pp.R, margin, g2.data(), &gR2);
    double em = en.Eval(pm.q.data(), pm.R, margin, g2.data(), &gR2);
    double num = (ep - em) / (2 * h), ana = i < c.d * n ? g[i] : gR;
    worst = std::max(worst, std::abs(num - ana) / std::max(1.0, std::abs(num)));
  }
  printf("worst relative gradient error=%.3e\n", worst);
  return worst < 1e-5 ? 0 : 1;
}

// As circpack's sweep: works through n = nmin..nmax (only those in --only-file if
// given), keeping the best packing of each n in --dir; every visit runs basin
// hopping from the current best and tries seeds from n - 1 and n + 1 (plus a
// random start for small n); an improvement queues both neighbours.
int CmdSweep(const std::map<string, string>& a) {
  Container c = ContainerFrom(a);
  int nmin = static_cast<int>(Arg(a, "nmin", 1)), nmax = static_cast<int>(Arg(a, "nmax", 48));
  string dir = ArgS(a, "dir", ""), tag = ArgS(a, "tag", "spack");
  double timeLimit = Arg(a, "time", 600), temp = Arg(a, "temp", 1e-4);
  int trials = static_cast<int>(Arg(a, "trials", 300));
  if (dir.empty() || c.d < 2) {
    fprintf(stderr, "--dir and --d >= 2 required\n");
    return 1;
  }
  std::map<int, double> rec = ReadRecords(ArgS(a, "records", ""));
  std::mt19937_64 rng(static_cast<uint64_t>(Arg(a, "seed", 1)));
  std::map<int, Packing> best;
  string seedDir = ArgS(a, "seeds", "");
  for (int n = std::max(1, nmin - 1); n <= nmax + 1; n++) {
    Packing p;
    if (ReadPacking(BestPath(dir, n), c, p) && p.n == n) best[n] = p;
    if (!seedDir.empty() && ReadPacking(BestPath(seedDir, n), c, p) && (p.n == n || p.n == n - 1)) {
      if (best.count(n) && best[n].R <= p.R * (1 + 1e-10)) continue;  // cannot beat the stored best
      Searcher s(n, c, 7);
      if (p.n == n - 1) {  // a published file that lost its last sphere: put it at the roomiest spot
        p = s.FromSmaller(p);
        s.Compress(p, 1e3);
      }
      if (s.Polish(p, 1e-11) && (!best.count(n) || p.R < best[n].R * (1 - 1e-12))) {
        best[n] = p;
        if (n >= nmin && n <= nmax) WritePacking(BestPath(dir, n), c, p, "seed from " + seedDir);
      }
    }
  }
  std::set<int> only;
  if (FILE* f = fopen(ArgS(a, "only-file", "").c_str(), "r")) {
    int v;
    while (fscanf(f, "%d", &v) == 1) only.insert(v);
    fclose(f);
  }
  auto visit = [&](int n) { return only.empty() || only.count(n) > 0; };
  auto status = [&](int n) {
    double r = PmRadius(c, best[n].R);
    auto it = rec.find(n);
    if (it == rec.end()) return string("(no record)");
    char buf[96];
    snprintf(buf, sizeof buf, "record %.12f, diff %+.3e", it->second, r - it->second);
    return string(buf);
  };
  vector<int> queue;
  for (int n = nmin; n <= nmax; n++)
    if (visit(n)) queue.push_back(n);
  if (queue.empty()) {
    fprintf(stderr, "no size of %d..%d to visit\n", nmin, nmax);
    return 1;
  }
  std::shuffle(queue.begin(), queue.end(), rng);
  double t0 = Now();
  long visits = 0;
  while (Now() - t0 < timeLimit) {
    if (queue.empty()) {
      for (int n = nmin; n <= nmax; n++)
        if (visit(n)) queue.push_back(n);
      std::shuffle(queue.begin(), queue.end(), rng);
    }
    int n = queue.back();
    queue.pop_back();
    visits++;
    Searcher s(n, c, rng());
    vector<Packing> starts;
    vector<string> kinds;
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
    if (n <= 60 || !best.count(n) || s.U() < 0.2) {
      bool lattice = s.U() < 0.5;
      starts.push_back(lattice ? s.LatticeStart() : s.RandomStart());
      kinds.push_back(lattice ? "lattice" : "random");
    }
    double before = best.count(n) ? best[n].R : 1e300;
    string via;
    for (size_t i = 0; i < starts.size(); i++) {
      Packing p = starts[i];
      if (kinds[i] != "bh") {
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
      Packing disk;
      if (ReadPacking(BestPath(dir, n), c, disk) && disk.n == n && disk.R <= best[n].R) {
        best[n] = disk;
        continue;
      }
      char buf[160];
      snprintf(buf, sizeof buf, "d=%d %s n=%d r=%.15f found by %s", c.d, c.cube ? "cube" : "ball", n,
               PmRadius(c, best[n].R), tag.c_str());
      WritePacking(BestPath(dir, n), c, best[n], buf);
      printf("[%7.0fs] n=%3d r=%.12f  %s  via=%s%s\n", Now() - t0, n, PmRadius(c, best[n].R), status(n).c_str(),
             via.c_str(), before == 1e300 ? "  (first)" : "");
      fflush(stdout);
      for (int m : {n - 1, n + 1})
        if (m >= nmin && m <= nmax && visit(m) && std::find(queue.begin(), queue.end(), m) == queue.end())
          queue.push_back(m);
    }
  }
  printf("sweep done: %ld visits in %.0fs\n", visits, Now() - t0);
  for (int n = nmin; n <= nmax; n++)
    if (best.count(n)) printf("n=%3d r=%.12f  %s\n", n, PmRadius(c, best[n].R), status(n).c_str());
  return 0;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 2) {
    fprintf(stderr,
            "usage: spack <command> [--key value ...]   (--d D --container ball|cube, default 3 ball)\n"
            "  sweep     --dir DIR [--nmin A] [--nmax B] [--time SEC] [--seed N] [--records FILE] [--seeds DIR]\n"
            "            [--trials T] [--temp T] [--tag NAME] [--only-file FILE]\n"
            "  check     --in FILE\n"
            "  gradcheck [--n N] [--margin M]\n");
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
