// Shared helpers for the packing searches: L-BFGS, timing and command-line options.
#pragma once

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <map>
#include <string>
#include <vector>

namespace packing {

inline double Now() {
  using namespace std::chrono;
  return duration<double>(steady_clock::now().time_since_epoch()).count();
}

inline double Dot(const double* a, const double* b, int n) {
  double s = 0;
  for (int i = 0; i < n; i++) s += a[i] * b[i];
  return s;
}

// Minimizes fg(x, grad) from x. Stops at fStop, after maxIter iterations, or on
// stagnation. No coordinate moves more than maxStep in one iteration.
template <class F>
double Lbfgs(std::vector<double>& x, F&& fg, int maxIter, double fStop, double maxStep, int* itersOut = nullptr) {
  const int N = static_cast<int>(x.size());
  const int M = 7;
  std::vector<double> g(N), xn(N), gn(N), d(N), sm(M * N), ym(M * N), rho(M), alpha(M);
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

inline std::map<std::string, std::string> ParseArgs(int argc, char** argv, int from) {
  std::map<std::string, std::string> a;
  for (int i = from; i + 1 < argc; i += 2) {
    std::string k = argv[i];
    if (k.rfind("--", 0) == 0) k = k.substr(2);
    a[k] = argv[i + 1];
  }
  return a;
}

inline double Arg(const std::map<std::string, std::string>& a, const std::string& k, double def) {
  auto it = a.find(k);
  return it == a.end() ? def : atof(it->second.c_str());
}

inline std::string ArgS(const std::map<std::string, std::string>& a, const std::string& k, const std::string& def) {
  auto it = a.find(k);
  return it == a.end() ? def : it->second;
}

}  // namespace packing
