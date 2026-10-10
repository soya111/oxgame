// tripack: search for dense packings of n unit equilateral triangles inside the
// smallest possible equilateral triangle (Erich Friedman's "Triangles in Triangles").
//
// A piece is (x, y, t): centroid and rotation. Vertex k sits at angle
// t + pi/2 + 2*pi*k/3 at circumradius 1/sqrt(3), so t = 0 points up.
// The container has corners (0,0), (S,0), (S/2, S*sqrt(3)/2).
//
// Overlap is measured with the separating-axis theorem and penalized as a sum of
// squares. L-BFGS minimizes it either at fixed S (feasibility, polishing) or
// together with S under a linear "pressure" term S + mu * P (compression).
// Optionally every piece is a rounded triangle (core triangle dilated by a disk
// of radius r), which smooths the landscape early in a compression.

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <random>
#include <string>
#include <vector>

using std::string;
using std::vector;

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kSqrt3 = 1.73205080756887729353;
constexpr double kH = kSqrt3 / 2;      // height of a unit triangle
constexpr double kCirc = 1 / kSqrt3;   // circumradius of a unit triangle
constexpr double kInr = 0.5 / kSqrt3;  // inradius of a unit triangle
constexpr double kC120 = -0.5;
constexpr double kS120 = kSqrt3 / 2;

// ---------------------------------------------------------------- packing I/O

struct Packing {
  int n = 0;
  double S = 0;
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
  p.S = nums[1];
  if (static_cast<int>(nums.size()) != 2 + 3 * p.n) return false;
  p.q.assign(nums.begin() + 2, nums.end());
  return true;
}

bool WritePacking(const string& path, const Packing& p, const string& comment) {
  string tmp = path + ".tmp";
  FILE* f = fopen(tmp.c_str(), "w");
  if (!f) return false;
  fprintf(f, "# %s\n%d %.15f\n", comment.c_str(), p.n, p.S);
  for (int i = 0; i < p.n; i++) {
    // Equilateral triangles repeat every 120 degrees; report t in [-60, 60).
    double t = p.q[3 * i + 2];
    t -= (2 * kPi / 3) * std::floor(t / (2 * kPi / 3) + 0.5);
    fprintf(f, "%.15f %.15f %.15f\n", p.q[3 * i], p.q[3 * i + 1], t);
  }
  fclose(f);
  return rename(tmp.c_str(), path.c_str()) == 0;
}

// ---------------------------------------------------------------- geometry

struct Piece {
  double cx, cy;
  double vx[3], vy[3];  // vertices
  double ex[3], ey[3];  // unit vectors centroid -> vertex k (also the edge-normal axes)
};

inline void MakePiece(double x, double y, double t, double R, Piece& p) {
  double c = std::cos(t + kPi / 2), s = std::sin(t + kPi / 2);
  p.cx = x;
  p.cy = y;
  p.ex[0] = c;
  p.ey[0] = s;
  p.ex[1] = c * kC120 - s * kS120;
  p.ey[1] = s * kC120 + c * kS120;
  p.ex[2] = c * kC120 + s * kS120;
  p.ey[2] = s * kC120 - c * kS120;
  for (int k = 0; k < 3; k++) {
    p.vx[k] = x + R * p.ex[k];
    p.vy[k] = y + R * p.ey[k];
  }
}

// Penalty P(q, S): squared overlaps between pieces and with the three walls.
// Shapes are rounded triangles of rounding radius r (r = 0: exact unit
// triangles) and are required to keep `margin` clearance.
class Energy {
 public:
  explicit Energy(int n) : n_(n), pc_(n) {}

  double Eval(const double* q, double S, double r, double margin, double* gq, double* gS) {
    const double R = kCirc - 2 * r;  // core circumradius
    for (int i = 0; i < n_; i++) MakePiece(q[3 * i], q[3 * i + 1], q[3 * i + 2], R, pc_[i]);
    std::fill(gq, gq + 3 * n_, 0.0);
    double E = 0, gs = 0;

    const double wall = r + margin;
    for (int i = 0; i < n_; i++) {
      const Piece& p = pc_[i];
      double* g = gq + 3 * i;
      for (int k = 0; k < 3; k++) {
        double vx = p.vx[k], vy = p.vy[k];
        double dvx = -R * p.ey[k], dvy = R * p.ex[k];  // d vertex / d t
        double d = vy - wall;                          // bottom, inward normal (0, 1)
        if (d < 0) {
          E += d * d;
          g[1] += 2 * d;
          g[2] += 2 * d * dvy;
        }
        d = kH * vx - 0.5 * vy - wall;  // left, inward normal (h, -1/2)
        if (d < 0) {
          E += d * d;
          g[0] += 2 * d * kH;
          g[1] -= d;
          g[2] += 2 * d * (kH * dvx - 0.5 * dvy);
        }
        d = kH * (S - vx) - 0.5 * vy - wall;  // right, inward normal (-h, -1/2)
        if (d < 0) {
          E += d * d;
          g[0] -= 2 * d * kH;
          g[1] -= d;
          g[2] += 2 * d * (-kH * dvx - 0.5 * dvy);
          gs += 2 * d * kH;
        }
      }
    }

    const double need = 2 * r + margin;
    const double reach = 2 * R + need;
    const double reach2 = reach * reach;
    for (int i = 0; i < n_; i++) {
      const Piece& A = pc_[i];
      for (int j = i + 1; j < n_; j++) {
        const Piece& B = pc_[j];
        double dx = B.cx - A.cx, dy = B.cy - A.cy;
        if (dx * dx + dy * dy >= reach2) continue;
        Sat sat = SatGap(A, B, need);
        if (sat.gap >= need) continue;
        double* gA = gq + 3 * i;
        double* gB = gq + 3 * j;
        if (r > 0 && sat.gap >= 0) {
          // Disjoint cores: the rounded shapes overlap iff the cores are closer than 2r.
          if (DistancePenalty(A, B, R, need, gA, gB, E)) continue;
        }
        double o = need - sat.gap;
        E += o * o;
        AddSatGradient(A, B, R, sat, -2 * o, gA, gB);
      }
    }
    *gS = gs;
    return E;
  }

  struct Sat {
    double gap;
    int owner, axis, upper, vu, vl;  // axis = owner's edge normal; upper/lower piece vertices
  };

  // Largest separation over the six edge normals (negative = penetration depth).
  // Returns early, with some separation >= stop, once the pieces are known to
  // be at least `stop` apart.
  static Sat SatGap(const Piece& A, const Piece& B, double stop = 1e300) {
    Sat s{-1e300, 0, 0, 0, 0, 0};
    for (int o = 0; o < 2; o++) {
      const Piece& O = o ? B : A;  // axis owner
      const Piece& X = o ? A : B;
      for (int k = 0; k < 3; k++) {
        double ux = O.ex[k], uy = O.ey[k];
        int k1 = k == 2 ? 0 : k + 1;
        double oMax = O.vx[k] * ux + O.vy[k] * uy;    // the owner's vertex k leads along its own axis
        double oMin = O.vx[k1] * ux + O.vy[k1] * uy;  // the opposite edge trails
        double px[3];
        for (int t = 0; t < 3; t++) px[t] = X.vx[t] * ux + X.vy[t] * uy;
        int xMin = 0, xMax = 0;
        for (int t = 1; t < 3; t++) {
          if (px[t] < px[xMin]) xMin = t;
          if (px[t] > px[xMax]) xMax = t;
        }
        double s1 = px[xMin] - oMax;  // X beyond O along u
        if (s1 > s.gap) {
          s = {s1, o, k, o == 0 ? 1 : 0, xMin, k};
          if (s1 >= stop) return s;
        }
        double s2 = oMin - px[xMax];  // O beyond X along u
        if (s2 > s.gap) {
          s = {s2, o, k, o == 0 ? 0 : 1, k1, xMax};
          if (s2 >= stop) return s;
        }
      }
    }
    return s;
  }

  // Smallest separating-axis gap between pieces and smallest vertex-to-wall distance.
  void Clearance(const double* q, double S, double* pairGap, double* wallGap) {
    for (int i = 0; i < n_; i++) MakePiece(q[3 * i], q[3 * i + 1], q[3 * i + 2], kCirc, pc_[i]);
    double pg = 1e300, wg = 1e300;
    for (int i = 0; i < n_; i++) {
      for (int k = 0; k < 3; k++) {
        double vx = pc_[i].vx[k], vy = pc_[i].vy[k];
        wg = std::min({wg, vy, kH * vx - 0.5 * vy, kH * (S - vx) - 0.5 * vy});
      }
      for (int j = i + 1; j < n_; j++) pg = std::min(pg, SatGap(pc_[i], pc_[j]).gap);
    }
    *pairGap = pg;
    *wallGap = wg;
  }

 private:
  // Adds c * d(gap)/d(q) for the separating-axis gap recorded in `s`.
  static void AddSatGradient(const Piece& A, const Piece& B, double R, const Sat& s, double c, double* gA,
                             double* gB) {
    const Piece& U = s.upper ? B : A;
    const Piece& L = s.upper ? A : B;
    double* gU = s.upper ? gB : gA;
    double* gL = s.upper ? gA : gB;
    const Piece& O = s.owner ? B : A;
    double* gO = s.owner ? gB : gA;
    double ux = O.ex[s.axis], uy = O.ey[s.axis];
    gU[0] += c * ux;
    gU[1] += c * uy;
    gL[0] -= c * ux;
    gL[1] -= c * uy;
    gU[2] += c * R * (-ux * U.ey[s.vu] + uy * U.ex[s.vu]);
    gL[2] -= c * R * (-ux * L.ey[s.vl] + uy * L.ex[s.vl]);
    double wx = U.vx[s.vu] - L.vx[s.vl], wy = U.vy[s.vu] - L.vy[s.vl];
    gO[2] += c * (-wx * uy + wy * ux);  // the axis turns with its owner
  }

  // Euclidean distance between disjoint cores; adds the penalty if closer than
  // `need`. Returns false when the distance is too small to give a direction.
  static bool DistancePenalty(const Piece& A, const Piece& B, double R, double need, double* gA, double* gB,
                              double& E) {
    double best = 1e300, bt = 0, bdx = 0, bdy = 0;
    int bp = 0, bv = 0, be = 0;
    for (int a = 0; a < 2; a++) {
      const Piece& P = a ? B : A;
      const Piece& Q = a ? A : B;
      for (int v = 0; v < 3; v++) {
        for (int e = 0; e < 3; e++) {
          int e2 = (e + 1) % 3;
          double ax = Q.vx[e], ay = Q.vy[e];
          double sx = Q.vx[e2] - ax, sy = Q.vy[e2] - ay;
          double t = ((P.vx[v] - ax) * sx + (P.vy[v] - ay) * sy) / (sx * sx + sy * sy);
          t = t < 0 ? 0 : (t > 1 ? 1 : t);
          double ddx = P.vx[v] - (ax + t * sx), ddy = P.vy[v] - (ay + t * sy);
          double d2 = ddx * ddx + ddy * ddy;
          if (d2 < best) {
            best = d2;
            bp = a;
            bv = v;
            be = e;
            bt = t;
            bdx = ddx;
            bdy = ddy;
          }
        }
      }
    }
    double d = std::sqrt(best);
    if (d >= need) return true;
    if (d < 1e-13) return false;
    double o = need - d;
    E += o * o;
    double c = -2 * o;
    double nx = bdx / d, ny = bdy / d;
    const Piece& P = bp ? B : A;
    const Piece& Q = bp ? A : B;
    double* gP = bp ? gB : gA;
    double* gQ = bp ? gA : gB;
    int e2 = (be + 1) % 3;
    gP[0] += c * nx;
    gP[1] += c * ny;
    gP[2] += c * R * (-nx * P.ey[bv] + ny * P.ex[bv]);
    gQ[0] -= c * nx;
    gQ[1] -= c * ny;
    double dqx = -R * ((1 - bt) * Q.ey[be] + bt * Q.ey[e2]);
    double dqy = R * ((1 - bt) * Q.ex[be] + bt * Q.ex[e2]);
    gQ[2] -= c * (nx * dqx + ny * dqy);
    return true;
  }

  int n_;
  vector<Piece> pc_;
};

// ---------------------------------------------------------------- L-BFGS

double Dot(const double* a, const double* b, int n) {
  double s = 0;
  for (int i = 0; i < n; i++) s += a[i] * b[i];
  return s;
}

// Minimizes fg(x, grad) from x. Stops at fStop, after maxIter iterations, or on
// stagnation. No coordinate moves more than maxStep in one iteration.
template <class F>
double Lbfgs(vector<double>& x, F&& fg, int maxIter, double fStop, double maxStep, int* itersOut = nullptr) {
  const int N = static_cast<int>(x.size());
  const int M = 7;
  vector<double> g(N), xn(N), gn(N), d(N), sm(M * N), ym(M * N), rho(M), alpha(M);
  int cnt = 0, head = 0, stall = 0, it = 0;
  double f = fg(x.data(), g.data());
  for (; it < maxIter && f > fStop; it++) {
    for (int i = 0; i < N; i++) d[i] = -g[i];
    for (int l = 0; l < cnt; l++) {
      int idx = (head - 1 - l + M) % M;
      alpha[idx] = rho[idx] * Dot(&sm[idx * N], d.data(), N);
      for (int i = 0; i < N; i++) d[i] -= alpha[idx] * ym[idx * N + i];
    }
    if (cnt > 0) {
      int idx = (head - 1 + M) % M;
      double yy = Dot(&ym[idx * N], &ym[idx * N], N);
      double gamma = 1.0 / (rho[idx] * yy);
      for (int i = 0; i < N; i++) d[i] *= gamma;
    }
    for (int l = cnt - 1; l >= 0; l--) {
      int idx = (head - 1 - l + M) % M;
      double b = rho[idx] * Dot(&ym[idx * N], d.data(), N);
      for (int i = 0; i < N; i++) d[i] += (alpha[idx] - b) * sm[idx * N + i];
    }
    double gd = Dot(g.data(), d.data(), N);
    if (!(gd < 0)) {
      for (int i = 0; i < N; i++) d[i] = -g[i];
      gd = -Dot(g.data(), g.data(), N);
      cnt = 0;
      if (!(gd < 0)) break;
    }
    double dmax = 0;
    for (int i = 0; i < N; i++) dmax = std::max(dmax, std::abs(d[i]));
    double step = std::min(1.0, maxStep / dmax);
    bool ok = false;
    double fn = f;
    for (int ls = 0; ls < 60; ls++) {
      for (int i = 0; i < N; i++) xn[i] = x[i] + step * d[i];
      fn = fg(xn.data(), gn.data());
      if (fn <= f + 1e-4 * step * gd) {
        ok = true;
        break;
      }
      step *= 0.5;
    }
    if (!ok) {
      if (cnt == 0) break;
      cnt = 0;  // drop curvature memory and retry from steepest descent
      continue;
    }
    double sy = 0;
    double* sv = &sm[head * N];
    double* yv = &ym[head * N];
    for (int i = 0; i < N; i++) {
      sv[i] = xn[i] - x[i];
      yv[i] = gn[i] - g[i];
      sy += sv[i] * yv[i];
    }
    if (sy > 1e-300) {
      rho[head] = 1.0 / sy;
      head = (head + 1) % M;
      cnt = std::min(cnt + 1, M);
    }
    double fold = f;
    x.swap(xn);
    g.swap(gn);
    f = fn;
    if (fold - f <= 1e-15 * std::abs(fold)) {
      if (++stall >= 30) break;
    } else {
      stall = 0;
    }
  }
  if (itersOut) *itersOut = it;
  return f;
}

// ---------------------------------------------------------------- search

double Now() {
  using namespace std::chrono;
  return duration<double>(steady_clock::now().time_since_epoch()).count();
}

// Rescales the layout to container side newS, moving centroids about the incenter.
void Rescale(Packing& p, double newS) {
  double f = newS / p.S;
  double ix = p.S / 2, iy = p.S / (2 * kSqrt3);
  double jx = newS / 2, jy = newS / (2 * kSqrt3);
  for (int i = 0; i < p.n; i++) {
    p.q[3 * i] = jx + (p.q[3 * i] - ix) * f;
    p.q[3 * i + 1] = jy + (p.q[3 * i + 1] - iy) * f;
  }
  p.S = newS;
}

enum Move { kJitter, kLocal, kFlip, kRelocate, kRotateCluster, kShear, kSwap, kBlob, kRegion, kNumMoves };
const char* kMoveNames[kNumMoves] = {"jitter", "local", "flip",  "relocate", "rotate",
                                     "shear",  "swap",  "blob", "region"};

class Searcher {
 public:
  Searcher(int n, uint64_t seed) : n_(n), en_(n), rng_(seed) {}

  double U(double a = 0, double b = 1) { return std::uniform_real_distribution<double>(a, b)(rng_); }
  double G(double s) { return std::normal_distribution<double>(0, s)(rng_); }
  int I(int n) { return static_cast<int>(rng_() % static_cast<uint64_t>(n)); }

  // Minimizes S + mu * P(q, S) over q and S.
  void Pressure(Packing& p, double mu, double r, int iters) {
    const int N = 3 * n_;
    vector<double> x(p.q);
    x.push_back(p.S);
    auto fg = [&](const double* xx, double* g) {
      double gS;
      double P = en_.Eval(xx, xx[N], r, 0, g, &gS);
      for (int i = 0; i < N; i++) g[i] *= mu;
      g[N] = 1 + mu * gS;
      return xx[N] + mu * P;
    };
    int used = 0;
    double t0 = Now();
    Lbfgs(x, fg, iters, -1e300, 0.1, &used);
    int slot = std::min(15, std::max(0, static_cast<int>(std::lround(std::log10(mu)))));
    stageIters_[slot] += used;
    stageTime_[slot] += Now() - t0;
    p.S = x[N];
    x.pop_back();
    p.q = x;
  }

  // Minimizes P(q) at fixed S. Returns the final penalty.
  double Relax(Packing& p, double margin, int iters, double fStop) {
    auto fg = [&](const double* xx, double* g) {
      double gS;
      return en_.Eval(xx, p.S, 0, margin, g, &gS);
    };
    return Lbfgs(p.q, fg, iters, fStop, 0.05);
  }

  // Shrinks p.S to (nearly) the smallest container in which the current
  // arrangement relaxes to a packing with clearance `margin`.
  bool Polish(Packing& p, double margin, double tol = 1e-11) {
    const double kFeas = 1e-26;
    const int kIters = 5000;
    Packing hi = p;
    double up = 0;
    for (double step = 1e-9;; step *= 4) {
      Packing t = p;
      Rescale(t, p.S + up);
      if (Relax(t, margin, kIters, kFeas) <= kFeas) {
        hi = t;
        break;
      }
      up = step;
      if (step > 1e-2) return false;
    }
    double gap = std::max(1e-7, 4 * up);
    double lo = hi.S - gap;
    while (true) {
      Packing t = hi;
      Rescale(t, lo);
      if (Relax(t, margin, kIters, kFeas) > kFeas) break;
      hi = t;
      gap *= 2;
      lo = hi.S - gap;
    }
    while (hi.S - lo > tol) {
      double mid = 0.5 * (lo + hi.S);
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

  // Pressure schedule: optional rounded ("blob") phase, then hardening in mu.
  // Soft stages leave small overlaps, so S only grows afterwards: a layout whose
  // S after mu = 1e4 already exceeds abortAbove is abandoned (returns false).
  bool Compress(Packing& p, double mu0, double r0, int blobStages, double abortAbove = 1e300) {
    for (int k = 0; k < blobStages; k++) Pressure(p, 1e3, r0 * (1.0 - static_cast<double>(k) / blobStages), 300);
    double mu = mu0;
    for (; mu <= 1.01e4; mu *= 10) Pressure(p, mu, 0, 300);
    if (p.S > abortAbove) return false;
    for (; mu <= 1.01e9; mu *= 10) Pressure(p, mu, 0, 250);
    return true;
  }

  // Unit triangular lattice of side k = ceil(sqrt(n)) with k^2 - n random holes.
  Packing LatticeStart() {
    int k = static_cast<int>(std::ceil(std::sqrt(static_cast<double>(n_)) - 1e-9));
    vector<std::array<double, 3>> sites;
    for (int row = 0; row < k; row++) {
      for (int j = 0; j < k - row; j++) sites.push_back({row * 0.5 + 0.5 + j, row * kH + kInr, 0});
      for (int j = 0; j < k - 1 - row; j++) sites.push_back({row * 0.5 + 1 + j, row * kH + 2 * kInr, kPi / 3});
    }
    std::shuffle(sites.begin(), sites.end(), rng_);
    Packing p;
    p.n = n_;
    p.S = k;
    for (int i = 0; i < n_; i++) {
      p.q.push_back(sites[i][0] + G(0.01));
      p.q.push_back(sites[i][1] + G(0.01));
      p.q.push_back(sites[i][2] + G(0.03));
    }
    return p;
  }

  // Full lattice of side k with a disordered disk: sites near a random center
  // are dropped and the remaining budget of pieces is scattered inside the disk.
  Packing DefectStart() {
    int k = static_cast<int>(std::ceil(std::sqrt(static_cast<double>(n_)) - 1e-9));
    vector<std::array<double, 3>> sites;
    for (int row = 0; row < k; row++) {
      for (int j = 0; j < k - row; j++) sites.push_back({row * 0.5 + 0.5 + j, row * kH + kInr, 0});
      for (int j = 0; j < k - 1 - row; j++) sites.push_back({row * 0.5 + 1 + j, row * kH + 2 * kInr, kPi / 3});
    }
    int holes = static_cast<int>(sites.size()) - n_;
    double cx, cy;
    RandomPoint(k, &cx, &cy);
    std::sort(sites.begin(), sites.end(), [&](const std::array<double, 3>& a, const std::array<double, 3>& b) {
      return std::hypot(a[0] - cx, a[1] - cy) < std::hypot(b[0] - cx, b[1] - cy);
    });
    // Drop the `holes + extra` sites nearest the center, then put `extra` pieces back at random.
    int extra = I(std::max(1, 2 * holes + 4));
    int drop = std::min(static_cast<int>(sites.size()), holes + extra);
    double rad = std::hypot(sites[drop - 1][0] - cx, sites[drop - 1][1] - cy) + 0.3;
    Packing p;
    p.n = n_;
    p.S = k;
    for (int i = drop; i < static_cast<int>(sites.size()); i++) {
      p.q.push_back(sites[i][0] + G(0.005));
      p.q.push_back(sites[i][1] + G(0.005));
      p.q.push_back(sites[i][2] + G(0.01));
    }
    while (static_cast<int>(p.q.size()) < 3 * n_) {
      p.q.push_back(cx);
      p.q.push_back(cy);
      p.q.push_back(0);
    }
    Scramble(p, cx, cy, rad);
    return p;
  }

  // Uniformly random positions and angles in a roomy container.
  Packing RandomStart() {
    Packing p;
    p.n = n_;
    p.S = std::sqrt(static_cast<double>(n_)) * 1.25 + 1;
    for (int i = 0; i < n_; i++) {
      double a = U(), b = U();
      if (a + b > 1) {
        a = 1 - a;
        b = 1 - b;
      }
      p.q.push_back(p.S * (a + 0.5 * b));
      p.q.push_back(p.S * kH * b);
      p.q.push_back(U(0, 2 * kPi));
    }
    return p;
  }

  int Perturb(Packing& p, int move) {
    const int n = n_;
    switch (move) {
      case kJitter: {
        double sp = U(0.003, 0.04), sa = U(0.005, 0.08);
        for (int i = 0; i < n; i++) {
          p.q[3 * i] += G(sp);
          p.q[3 * i + 1] += G(sp);
          p.q[3 * i + 2] += G(sa);
        }
        break;
      }
      case kLocal: {
        int c = I(n);
        double cx = p.q[3 * c], cy = p.q[3 * c + 1], rad = U(0.7, 2.0);
        double sp = U(0.05, 0.25), sa = U(0.1, 0.8);
        for (int i = 0; i < n; i++) {
          if (std::hypot(p.q[3 * i] - cx, p.q[3 * i + 1] - cy) > rad) continue;
          p.q[3 * i] += G(sp);
          p.q[3 * i + 1] += G(sp);
          p.q[3 * i + 2] += G(sa);
        }
        break;
      }
      case kFlip: {
        int m = 1 + I(2);
        for (int t = 0; t < m; t++) {
          int i = I(n);
          double turns[] = {kPi / 3, -kPi / 3, kPi / 6, -kPi / 6};
          p.q[3 * i + 2] += turns[I(4)];
          p.q[3 * i] += G(0.03);
          p.q[3 * i + 1] += G(0.03);
        }
        break;
      }
      case kRelocate: {
        int m = 1 + I(2);
        for (int t = 0; t < m; t++) {
          int i = I(n);
          double bx = 0, by = 0, bc = -1e300;
          for (int s = 0; s < 300; s++) {
            double a = U(), b = U();
            if (a + b > 1) {
              a = 1 - a;
              b = 1 - b;
            }
            double x = p.S * (a + 0.5 * b), y = p.S * kH * b;
            double c = Clearance(p, x, y, i);
            if (c > bc) {
              bc = c;
              bx = x;
              by = y;
            }
          }
          p.q[3 * i] = bx;
          p.q[3 * i + 1] = by;
          p.q[3 * i + 2] = U(0, 2 * kPi);
        }
        break;
      }
      case kRotateCluster: {
        int c = I(n);
        double cx = p.q[3 * c] + G(0.3), cy = p.q[3 * c + 1] + G(0.3), rad = U(0.8, 3.0), phi = G(0.06);
        double cs = std::cos(phi), sn = std::sin(phi);
        for (int i = 0; i < n; i++) {
          double dx = p.q[3 * i] - cx, dy = p.q[3 * i + 1] - cy;
          if (std::hypot(dx, dy) > rad) continue;
          p.q[3 * i] = cx + cs * dx - sn * dy;
          p.q[3 * i + 1] = cy + sn * dx + cs * dy;
          p.q[3 * i + 2] += phi;
        }
        break;
      }
      case kShear: {
        double ang = I(3) * kPi / 3 + G(0.05);
        double ux = std::cos(ang), uy = std::sin(ang);
        int c = I(n);
        double cx = p.q[3 * c], cy = p.q[3 * c + 1], shift = G(0.15);
        for (int i = 0; i < n; i++) {
          double side = -(p.q[3 * i] - cx) * uy + (p.q[3 * i + 1] - cy) * ux;
          if (side < 0) continue;
          p.q[3 * i] += shift * ux;
          p.q[3 * i + 1] += shift * uy;
        }
        break;
      }
      case kSwap: {
        for (int tries = 0; tries < 50; tries++) {
          int a = I(n), b = I(n);
          if (a == b) continue;
          if (std::hypot(p.q[3 * a] - p.q[3 * b], p.q[3 * a + 1] - p.q[3 * b + 1]) > 2.0) continue;
          double da = std::remainder(p.q[3 * a + 2] - p.q[3 * b + 2], 2 * kPi / 3);
          if (std::abs(da) < 0.15) continue;
          std::swap(p.q[3 * a], p.q[3 * b]);
          std::swap(p.q[3 * a + 1], p.q[3 * b + 1]);
          break;
        }
        break;
      }
      case kBlob: {
        for (int i = 0; i < n; i++) {
          p.q[3 * i] += G(0.01);
          p.q[3 * i + 1] += G(0.01);
          p.q[3 * i + 2] += G(0.02);
        }
        break;
      }
      case kRegion: {
        double cx, cy;
        RandomPoint(p.S, &cx, &cy);
        Scramble(p, cx, cy, U(0.9, 2.0));
        break;
      }
    }
    return move;
  }

  // Uniform random point in the container of side S.
  void RandomPoint(double S, double* x, double* y) {
    double a = U(), b = U();
    if (a + b > 1) {
      a = 1 - a;
      b = 1 - b;
    }
    *x = S * (a + 0.5 * b);
    *y = S * kH * b;
  }

  // Re-places every piece whose centroid lies within `rad` of (cx, cy) at a
  // uniformly random spot of that disk (inside the container), random angle.
  void Scramble(Packing& p, double cx, double cy, double rad) {
    for (int i = 0; i < n_; i++) {
      if (std::hypot(p.q[3 * i] - cx, p.q[3 * i + 1] - cy) > rad) continue;
      for (int tries = 0; tries < 100; tries++) {
        double r = rad * std::sqrt(U()), a = U(0, 2 * kPi);
        double x = cx + r * std::cos(a), y = cy + r * std::sin(a);
        if (std::min({y, kH * x - 0.5 * y, kH * (p.S - x) - 0.5 * y}) < kInr) continue;
        p.q[3 * i] = x;
        p.q[3 * i + 1] = y;
        break;
      }
      p.q[3 * i + 2] = U(0, 2 * kPi);
    }
  }

  // Free room around (x, y): walls and every piece except `skip`.
  double Clearance(const Packing& p, double x, double y, int skip) {
    double c = std::min({y, kH * x - 0.5 * y, kH * (p.S - x) - 0.5 * y});
    for (int i = 0; i < n_; i++) {
      if (i == skip) continue;
      double dx = x - p.q[3 * i], dy = y - p.q[3 * i + 1];
      if (dx * dx + dy * dy > 4) continue;
      Piece pc;
      MakePiece(p.q[3 * i], p.q[3 * i + 1], p.q[3 * i + 2], kCirc, pc);
      double sd = -1e300;  // signed distance lower bound: max over the edge lines
      for (int k = 0; k < 3; k++) sd = std::max(sd, -(dx * pc.ex[k] + dy * pc.ey[k]) - kInr);
      c = std::min(c, sd);
    }
    return c;
  }

  // One basin-hopping trial from `cur`: expand, perturb, compress. Returns a
  // packing with S = +inf when the trial was abandoned early.
  Packing Trial(const Packing& cur, double abortAbove, int* moveOut) {
    Packing t = cur;
    Rescale(t, cur.S * (1 + std::exp(U(std::log(0.002), std::log(0.03)))));
    static const double kWeights[kNumMoves] = {2, 3, 2, 2, 3, 2, 2, 1, 2};
    double tot = 0;
    for (double w : kWeights) tot += w;
    double u = U(0, tot);
    int mv = 0;
    while (mv < kNumMoves - 1 && u > kWeights[mv]) u -= kWeights[mv++];
    Perturb(t, mv);
    double mus[] = {1e1, 1e2, 1e3};
    bool done = mv == kBlob     ? Compress(t, 1e3, U(0.02, 0.15), 8, abortAbove)
                : mv == kRegion ? Compress(t, 1e1, 0, 0, abortAbove)
                                : Compress(t, mus[I(3)], 0, 0, abortAbove);
    if (!done) t.S = 1e300;
    *moveOut = mv;
    return t;
  }

  // Applies one of the six symmetries of the container (0 = identity).
  void Symmetry(Packing& p, int which) {
    double gx = p.S / 2, gy = p.S / (2 * kSqrt3);
    for (int i = 0; i < n_; i++) {
      double& x = p.q[3 * i];
      double& y = p.q[3 * i + 1];
      double& t = p.q[3 * i + 2];
      if (which >= 3) {  // mirror in the vertical axis
        x = p.S - x;
        t = -t;
      }
      for (int r = 0; r < which % 3; r++) {  // rotate 120 degrees about the center
        double dx = x - gx, dy = y - gy;
        x = gx + kC120 * dx - kS120 * dy;
        y = gy + kS120 * dx + kC120 * dy;
      }
    }
  }

  // Child = pieces of A on one side of a random line + pieces of a randomly
  // transformed B on the other; the count is repaired by dropping the most
  // overlapped pieces or adding pieces at the roomiest spots.
  Packing Crossover(const Packing& A, Packing B) {
    Rescale(B, A.S);
    Symmetry(B, I(6));
    double cx, cy;
    RandomPoint(A.S, &cx, &cy);
    double ang = U(0, kPi), ux = std::cos(ang), uy = std::sin(ang);
    Packing c;
    c.n = 0;
    c.S = A.S;
    for (int src = 0; src < 2; src++) {
      const Packing& P = src ? B : A;
      for (int i = 0; i < n_; i++) {
        bool side = (P.q[3 * i] - cx) * -uy + (P.q[3 * i + 1] - cy) * ux >= 0;
        if (side != (src == 0)) continue;
        c.q.insert(c.q.end(), P.q.begin() + 3 * i, P.q.begin() + 3 * i + 3);
        c.n++;
      }
    }
    while (c.n > n_) {
      vector<Piece> pc(c.n);
      for (int i = 0; i < c.n; i++) MakePiece(c.q[3 * i], c.q[3 * i + 1], c.q[3 * i + 2], kCirc, pc[i]);
      vector<double> score(c.n, 0);
      for (int i = 0; i < c.n; i++)
        for (int j = i + 1; j < c.n; j++) {
          double g = Energy::SatGap(pc[i], pc[j], 0).gap;
          if (g < 0) {
            score[i] -= g;
            score[j] -= g;
          }
        }
      int worst = static_cast<int>(std::max_element(score.begin(), score.end()) - score.begin());
      c.q.erase(c.q.begin() + 3 * worst, c.q.begin() + 3 * worst + 3);
      c.n--;
    }
    while (c.n < n_) {
      c.q.insert(c.q.end(), {0.0, 0.0, 0.0});
      c.n++;
      double bx = 0, by = 0, bc = -1e300;
      for (int t = 0; t < 300; t++) {
        double x, y;
        RandomPoint(c.S, &x, &y);
        double cl = ClearanceN(c, x, y, c.n - 1);
        if (cl > bc) {
          bc = cl;
          bx = x;
          by = y;
        }
      }
      c.q[3 * (c.n - 1)] = bx;
      c.q[3 * (c.n - 1) + 1] = by;
      c.q[3 * (c.n - 1) + 2] = U(0, 2 * kPi);
    }
    return c;
  }

  // Like Clearance, but for a packing whose piece count may differ from n_.
  double ClearanceN(const Packing& p, double x, double y, int skip) {
    double c = std::min({y, kH * x - 0.5 * y, kH * (p.S - x) - 0.5 * y});
    for (int i = 0; i < p.n; i++) {
      if (i == skip) continue;
      double dx = x - p.q[3 * i], dy = y - p.q[3 * i + 1];
      if (dx * dx + dy * dy > 4) continue;
      Piece pc;
      MakePiece(p.q[3 * i], p.q[3 * i + 1], p.q[3 * i + 2], kCirc, pc);
      double sd = -1e300;
      for (int k = 0; k < 3; k++) sd = std::max(sd, -(dx * pc.ex[k] + dy * pc.ey[k]) - kInr);
      c = std::min(c, sd);
    }
    return c;
  }

  Energy& energy() { return en_; }

  void PrintStageStats(long trials) const {
    for (int k = 0; k < 16; k++) {
      if (stageIters_[k] == 0) continue;
      printf("  mu=1e%-2d  avg iters/trial=%7.1f  avg ms/trial=%6.2f\n", k, double(stageIters_[k]) / trials,
             1e3 * stageTime_[k] / trials);
    }
  }

 private:
  int n_;
  Energy en_;
  std::mt19937_64 rng_;
  long stageIters_[16] = {};
  double stageTime_[16] = {};
};

// ---------------------------------------------------------------- commands

std::map<string, string> ParseArgs(int argc, char** argv, int from) {
  std::map<string, string> a;
  for (int i = from; i + 1 < argc; i += 2) {
    string k = argv[i];
    if (k.rfind("--", 0) == 0) k = k.substr(2);
    a[k] = argv[i + 1];
  }
  return a;
}

double Arg(const std::map<string, string>& a, const string& k, double def) {
  auto it = a.find(k);
  return it == a.end() ? def : atof(it->second.c_str());
}

string ArgS(const std::map<string, string>& a, const string& k, const string& def) {
  auto it = a.find(k);
  return it == a.end() ? def : it->second;
}

void Report(Energy& en, const Packing& p, const char* label) {
  double pg, wg;
  en.Clearance(p.q.data(), p.S, &pg, &wg);
  printf("%s S=%.10f  min pair gap=%.3e  min wall gap=%.3e  %s\n", label, p.S, pg, wg,
         (pg > 0 && wg > 0) ? "VALID" : "INVALID");
  fflush(stdout);
}

int CmdGradCheck(const std::map<string, string>& a) {
  int n = static_cast<int>(Arg(a, "n", 12));
  double r = Arg(a, "r", 0.05), margin = Arg(a, "margin", 0.0);
  Searcher s(n, static_cast<uint64_t>(Arg(a, "seed", 1)));
  Packing p = s.RandomStart();
  Rescale(p, std::sqrt(static_cast<double>(n)) * 0.95);  // crowded: plenty of overlaps
  Energy en(n);
  vector<double> g(3 * n), g2(3 * n);
  double gS, gS2;
  double E = en.Eval(p.q.data(), p.S, r, margin, g.data(), &gS);
  double worst = 0;
  for (int i = 0; i <= 3 * n; i++) {
    const double h = 1e-7;
    Packing pp = p, pm = p;
    if (i < 3 * n) {
      pp.q[i] += h;
      pm.q[i] -= h;
    } else {
      pp.S += h;
      pm.S -= h;
    }
    double Ep = en.Eval(pp.q.data(), pp.S, r, margin, g2.data(), &gS2);
    double Em = en.Eval(pm.q.data(), pm.S, r, margin, g2.data(), &gS2);
    double num = (Ep - Em) / (2 * h);
    double ana = i < 3 * n ? g[i] : gS;
    worst = std::max(worst, std::abs(num - ana) / std::max(1.0, std::abs(num)));
  }
  printf("E=%.6e  worst relative gradient error=%.3e\n", E, worst);
  return worst < 1e-5 ? 0 : 1;
}

int CmdBench(const std::map<string, string>& a) {
  Packing p;
  if (!ReadPacking(ArgS(a, "in", ""), p)) {
    fprintf(stderr, "cannot read --in\n");
    return 1;
  }
  Energy en(p.n);
  vector<double> g(3 * p.n);
  double gS, sum = 0;
  Rescale(p, p.S * 0.999);  // a few small overlaps, as during compression
  const int reps = static_cast<int>(Arg(a, "reps", 200000));
  double t0 = Now();
  for (int r = 0; r < reps; r++) sum += en.Eval(p.q.data(), p.S, Arg(a, "r", 0), 0, g.data(), &gS);
  printf("%.3f us per Eval (checksum %.6e)\n", 1e6 * (Now() - t0) / reps, sum);
  return 0;
}

int CmdCheck(const std::map<string, string>& a) {
  Packing p;
  if (!ReadPacking(ArgS(a, "in", ""), p)) {
    fprintf(stderr, "cannot read --in\n");
    return 1;
  }
  Energy en(p.n);
  Report(en, p, "check");
  return 0;
}

int CmdPolish(const std::map<string, string>& a) {
  Packing p;
  if (!ReadPacking(ArgS(a, "in", ""), p)) {
    fprintf(stderr, "cannot read --in\n");
    return 1;
  }
  Searcher s(p.n, 1);
  Report(s.energy(), p, "input ");
  double margin = Arg(a, "margin", 1e-10);
  if (!s.Polish(p, margin)) {
    fprintf(stderr, "polish failed\n");
    return 1;
  }
  Report(s.energy(), p, "polish");
  string out = ArgS(a, "out", "");
  if (!out.empty()) WritePacking(out, p, "polished");
  return 0;
}

// Basin hopping from a starting packing; returns the best packing found.
Packing BasinHop(Searcher& s, Packing cur, double timeLimit, double temp, int reset, int maxIters,
                 const string& out, double* globalBest, const string& tag) {
  Packing best = cur;
  double t0 = Now();
  int since = 0;
  long iters = 0;
  vector<int> tries(kNumMoves), wins(kNumMoves);
  while (Now() - t0 < timeLimit && (maxIters <= 0 || iters < maxIters)) {
    iters++;
    int mv;
    Packing t = s.Trial(cur, cur.S + 30 * temp, &mv);
    tries[mv]++;
    if (t.S < best.S - 5e-8) {
      Packing pol = t;
      if (s.Polish(pol, 1e-10) && pol.S < best.S - 1e-9) {
        best = pol;
        wins[mv]++;
        since = 0;
        if (best.S < *globalBest) {
          *globalBest = best.S;
          if (!out.empty()) {
            char buf[256];
            snprintf(buf, sizeof buf, "n=%d S=%.10f found by tripack (%s, move=%s)", best.n, best.S, tag.c_str(),
                     kMoveNames[mv]);
            WritePacking(out, best, buf);
          }
          printf("[%8.1fs] new best S=%.10f  (%s, move=%s, iter=%ld)\n", Now() - t0, best.S, tag.c_str(),
                 kMoveNames[mv], iters);
          fflush(stdout);
        }
      }
    }
    if (t.S < cur.S || s.U() < std::exp(-(t.S - cur.S) / temp)) cur = t;
    if (++since > reset) {
      cur = best;
      since = 0;
    }
  }
  if (maxIters <= 0) {
    printf("basin hopping: %ld trials in %.0fs; best S=%.10f\n", iters, Now() - t0, best.S);
    for (int m = 0; m < kNumMoves; m++) printf("  %-9s tries=%6d wins=%d\n", kMoveNames[m], tries[m], wins[m]);
    s.PrintStageStats(iters);
  }
  return best;
}

int CmdBh(const std::map<string, string>& a) {
  Packing p;
  if (!ReadPacking(ArgS(a, "in", ""), p)) {
    fprintf(stderr, "cannot read --in\n");
    return 1;
  }
  Searcher s(p.n, static_cast<uint64_t>(Arg(a, "seed", 1)));
  s.Polish(p, 1e-10);
  Report(s.energy(), p, "start ");
  double gb = p.S;
  string out = ArgS(a, "out", "");
  Packing best = BasinHop(s, p, Arg(a, "time", 600), Arg(a, "temp", 2e-4), static_cast<int>(Arg(a, "reset", 300)), 0,
                          out, &gb, "bh");
  Report(s.energy(), best, "best  ");
  return 0;
}

int CmdSearch(const std::map<string, string>& a) {
  int n = static_cast<int>(Arg(a, "n", 0));
  if (n <= 0) {
    fprintf(stderr, "--n required\n");
    return 1;
  }
  Searcher s(n, static_cast<uint64_t>(Arg(a, "seed", 1)));
  string init = ArgS(a, "init", "lattice"), out = ArgS(a, "out", "");
  double timeLimit = Arg(a, "time", 600), t0 = Now();
  int bhIters = static_cast<int>(Arg(a, "bh-iters", 300));
  double gb = 1e300;
  Packing best;
  int starts = 0;
  while (Now() - t0 < timeLimit) {
    starts++;
    Packing p = init == "random" ? s.RandomStart() : init == "defect" ? s.DefectStart() : s.LatticeStart();
    if (init == "random") {
      s.Compress(p, 1e2, 0.26, 30);
    } else if (init == "defect" && s.U() < 0.5) {
      s.Compress(p, 1e2, s.U(0.03, 0.15), 10);
    } else {
      s.Compress(p, 1e1, 0, 0);
    }
    if (!s.Polish(p, 1e-10)) continue;
    double s0 = p.S;
    char tag[64];
    snprintf(tag, sizeof tag, "%s start %d", init.c_str(), starts);
    Packing b = BasinHop(s, p, timeLimit - (Now() - t0), Arg(a, "temp", 2e-4), 100, bhIters, out, &gb, tag);
    printf("start %4d: compressed %.6f -> after %d BH trials %.6f   (best so far %.8f)\n", starts, s0, bhIters, b.S,
           gb);
    fflush(stdout);
    if (best.n == 0 || b.S < best.S) best = b;
  }
  if (best.n) Report(s.energy(), best, "best  ");
  return 0;
}

// Memetic search: a pool of locally optimized packings evolved by crossover
// followed by short basin hopping.
int CmdPool(const std::map<string, string>& a) {
  int n = static_cast<int>(Arg(a, "n", 0));
  if (n <= 0) {
    fprintf(stderr, "--n required\n");
    return 1;
  }
  Searcher s(n, static_cast<uint64_t>(Arg(a, "seed", 1)));
  string out = ArgS(a, "out", ""), init = ArgS(a, "init", "mix");
  double timeLimit = Arg(a, "time", 600), t0 = Now(), temp = Arg(a, "temp", 2e-4);
  int K = static_cast<int>(Arg(a, "pool", 10));
  int bhInit = static_cast<int>(Arg(a, "bh-init", 600)), bhChild = static_cast<int>(Arg(a, "bh-child", 200));
  double gb = 1e300, minDiff = Arg(a, "min-diff", 1e-5);
  vector<Packing> pool;
  auto distinct = [&](const Packing& p) {  // crude diversity guard: pool members differ in S
    for (const Packing& q : pool)
      if (std::abs(q.S - p.S) < minDiff) return false;
    return true;
  };
  auto fresh = [&]() {
    bool defect = init == "defect" || (init == "mix" && s.U() < 0.5);
    Packing p = defect ? s.DefectStart() : s.LatticeStart();
    s.Compress(p, 1e1, 0, 0);
    s.Polish(p, 1e-10);
    return BasinHop(s, p, timeLimit, temp, 100, bhInit, out, &gb, defect ? "pool/defect" : "pool/lattice");
  };
  string seeds = ArgS(a, "seeds", "");
  for (size_t pos = 0; !seeds.empty() && pos != string::npos;) {
    size_t comma = seeds.find(',', pos);
    string f = seeds.substr(pos, comma == string::npos ? string::npos : comma - pos);
    pos = comma == string::npos ? string::npos : comma + 1;
    Packing p;
    if (!ReadPacking(f, p) || p.n != n) {
      fprintf(stderr, "skipping seed %s\n", f.c_str());
      continue;
    }
    s.Polish(p, 1e-10);
    gb = std::min(gb, p.S);
    pool.push_back(p);
  }
  while (static_cast<int>(pool.size()) < K && Now() - t0 < timeLimit) {
    Packing p = fresh();
    if (distinct(p)) pool.push_back(p);
    printf("pool init %zu/%d: %.8f\n", pool.size(), K, p.S);
    fflush(stdout);
  }
  long gen = 0;
  while (Now() - t0 < timeLimit) {
    gen++;
    auto pick = [&]() {
      int i = s.I(static_cast<int>(pool.size())), j = s.I(static_cast<int>(pool.size()));
      return pool[i].S < pool[j].S ? i : j;
    };
    Packing child;
    const char* how;
    if (s.U() < 0.1) {
      child = fresh();
      how = "immigrant";
    } else {
      int ia = pick(), ib = pick();
      for (int t = 0; t < 5 && ib == ia; t++) ib = pick();
      child = s.Crossover(pool[ia], pool[ib]);
      Rescale(child, child.S * 1.01);
      s.Compress(child, 1e1, 0, 0);
      if (!s.Polish(child, 1e-10)) continue;
      child = BasinHop(s, child, timeLimit - (Now() - t0), temp, 100, bhChild, out, &gb, "pool/child");
      how = "child";
    }
    // A child close to a member replaces that member if better; otherwise it replaces the worst.
    int worst = 0, near = -1;
    for (int i = 0; i < static_cast<int>(pool.size()); i++) {
      if (pool[i].S > pool[worst].S) worst = i;
      if (std::abs(pool[i].S - child.S) < minDiff) near = i;
    }
    int slot = near >= 0 ? near : worst;
    bool in = child.S < pool[slot].S;
    if (in) pool[slot] = child;
    double bestS = 1e300;
    for (const Packing& q : pool) bestS = std::min(bestS, q.S);
    printf("gen %5ld %-9s S=%.8f %s  pool best=%.8f worst=%.8f\n", gen, how, child.S, in ? "in " : "out", bestS,
           pool[worst].S);
    fflush(stdout);
  }
  return 0;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 2) {
    fprintf(stderr,
            "usage: tripack <command> [--key value ...]\n"
            "  check     --in FILE\n"
            "  polish    --in FILE [--out FILE] [--margin 1e-10]\n"
            "  bh        --in FILE --out FILE [--time SEC] [--seed N] [--temp T] [--reset K]\n"
            "  search    --n N --out FILE [--init lattice|defect|random] [--time SEC] [--seed N] [--bh-iters K]\n"
            "  pool      --n N --out FILE [--pool K] [--init lattice|defect|mix] [--seeds F1,F2]\n"
            "            [--time SEC] [--seed N] [--bh-init K] [--bh-child K] [--min-diff D]\n"
            "  gradcheck [--n N] [--r R] [--margin M] [--seed N]\n");
    return 2;
  }
  string cmd = argv[1];
  auto a = ParseArgs(argc, argv, 2);
  if (cmd == "check") return CmdCheck(a);
  if (cmd == "bench") return CmdBench(a);
  if (cmd == "polish") return CmdPolish(a);
  if (cmd == "bh") return CmdBh(a);
  if (cmd == "search") return CmdSearch(a);
  if (cmd == "pool") return CmdPool(a);
  if (cmd == "gradcheck") return CmdGradCheck(a);
  fprintf(stderr, "unknown command %s\n", cmd.c_str());
  return 2;
}
