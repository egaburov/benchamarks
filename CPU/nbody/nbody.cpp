#include <cstdlib>
#include <cmath>
#include <cstdio>
#include <algorithm>
#include <vector>
#include <cassert>

typedef double real;

void compute_forces(
    const int     n,
    const restrict real mass[],
    const restrict real posx[],
    const restrict real posy[],
    const restrict real posz[],
    const restrict real velx[],
    const restrict real vely[],
    const restrict real velz[],
    restrict real accx[],
    restrict real accy[],
    restrict real accz[],
    restrict real jrkx[],
    restrict real jrky[],
    restrict real jrkz[],
    restrict real gpot[],
    const real eps2)
{
#pragma omp parallel for
  for (int i = 0; i < n; i++)
  {
    real fx = 0.0;
    real fy = 0.0;
    real fz = 0.0;
    real jx = 0.0;
    real jy = 0.0;
    real jz = 0.0;
    real gp = 0.0;

    const real  xi = posx[i];
    const real  yi = posy[i];
    const real  zi = posz[i];
    const real vxi = velx[i];
    const real vyi = vely[i];
    const real vzi = velz[i];

    for (int j = 0; j < n; j++)
    {
      const real  mj = mass[j];
      const real  xj = posx[j];
      const real  yj = posy[j];
      const real  zj = posz[j];
      const real vxj = velx[j];
      const real vyj = vely[j];
      const real vzj = velz[j];
      const real dx = xj - xi;
      const real dy = yj - yi;
      const real dz = zj - zi;
      const real ds2 = dx*dx + dy*dy + dz*dz + eps2;
      const real  inv_ds  = (real)(1.0f/sqrtf((float)ds2));
      const real  inv_ds2 = inv_ds*inv_ds;
      const real minv_ds  = inv_ds  * mj;
      const real minv_ds3 = inv_ds2 * minv_ds;

      fx += minv_ds3 * dx;
      fy += minv_ds3 * dy;
      fz += minv_ds3 * dz;
      gp -= minv_ds;

      const real dvx = vxj - vxi;
      const real dvy = vyj - vyi;
      const real dvz = vzj - vzi;
      const real rv  = dx*dvx + dy*dvy + dz*dvz;

      const real Jij = real(-3.0) * rv * inv_ds2 * minv_ds3;

      jx += minv_ds3*dvx + Jij*dx;
      jy += minv_ds3*dvy + Jij*dy;
      jz += minv_ds3*dvz + Jij*dz;
    }
    accx[i] = fx;
    accy[i] = fy;
    accz[i] = fz;
    jrkx[i] = jx;
    jrky[i] = jy;
    jrkz[i] = jz;
    gpot[i] = gp;
  }
}

#define FTINY 1e-10
#define FHUGE 1e+10

#include <sys/time.h>
inline double get_time() {
  struct timeval Tvalue;
  struct timezone dummy;

  gettimeofday(&Tvalue,&dummy);
  return ((double) Tvalue.tv_sec +1.e-6*((double) Tvalue.tv_usec));
}


struct real4 {
  real x, y, z, w;
  real4() {};
  real4(const real r) : x(r), y(r), z(r), w(r) {}
  real4(const real &_x, 
      const real &_y,
      const real &_z,
      const real &_w = 0) : x(_x), y(_y), z(_z), w(_w) {};
  real abs2() const {return x*x + y*y + z*z;}
};

inline real sqr(const real &x) {return x*x;}


#define BLOCKDIM 64

void forces(
    const int n,
    const real4 pos[],
    const real4 vel[],
    real4 acc[],
    real4 jrk[],
    const real eps2) 
{
  static std::vector<real> mass(n);
  static std::vector<real> posx(n);
  static std::vector<real> posy(n);
  static std::vector<real> posz(n);
  static std::vector<real> velx(n);
  static std::vector<real> vely(n);
  static std::vector<real> velz(n);
  static std::vector<real> accx(n);
  static std::vector<real> accy(n);
  static std::vector<real> accz(n);
  static std::vector<real> jrkx(n);
  static std::vector<real> jrky(n);
  static std::vector<real> jrkz(n);
  static std::vector<real> gpot(n);
  for (int i =0; i < n; i++)
  {
    mass[i] = pos[i].w;
    posx[i] = pos[i].x;
    posy[i] = pos[i].y;
    posz[i] = pos[i].z;
    velx[i] = vel[i].x;
    vely[i] = vel[i].y;
    velz[i] = vel[i].z;
  }
  compute_forces(
      n,
      &mass[0],
      &posx[0],
      &posy[0],
      &posz[0],
      &velx[0],
      &vely[0],
      &velz[0],
      &accx[0],
      &accy[0],
      &accz[0],
      &jrkx[0],
      &jrky[0],
      &jrkz[0],
      &gpot[0],
      eps2);
  for (int i = 0; i < n; i++)
  {
    acc[i].w = gpot[i];
    acc[i].x = accx[i];
    acc[i].y = accy[i];
    acc[i].z = accz[i];
    jrk[i].x = jrkx[i];
    jrk[i].y = jrky[i];
    jrk[i].z = jrkz[i];
  }
}

real timestep(
    const int n,
    const real4 vel[],
    const real4 acc[],
    const real eta,
    const real eps2) 
{
  real dt_min = FHUGE;
  for (int i = 0; i < n; i++) {
    const real vabs = std::sqrt(vel[i].abs2());
    const real aabs = std::sqrt(acc[i].abs2());

    const real s = std::sqrt(eps2);
    const real idt1 = s/(vabs + FTINY);
    const real idt2 = std::sqrt(s/(aabs + FTINY));
    dt_min = std::min(dt_min, std::min(idt1, idt2));
  }
  dt_min *= eta;

  return dt_min;
}

void energy(
    const int n,
    const real4 pos[],
    const real4 vel[],
    const real4 acc[],
    real &Ekin, real &Epot) {
  Ekin = Epot = 0;
  for (int i = 0; i < n; i++) {
    Ekin += pos[i].w * vel[i].abs2() * 0.5;
    Epot += 0.5*pos[i].w * acc[i].w;
  }
}

real iterate(
    const int n,
    real4 pos[],
    real4 vel[],
    real4 acc[],
    real4 jrk[],
    const real eta,
    const real eps2,
    const real dt) 
{
  std::vector<real4> acc0(n), jrk0(n);
  const real dt2 = dt*(1.0/2.0);
  const real dt3 = dt*(1.0/3.0);

  for (int i = 0; i < n; i++)
  {
    acc0[i] = acc[i];
    jrk0[i] = jrk[i];

    pos[i].x += dt*(vel[i].x + dt2*(acc[i].x + dt3*jrk[i].x));
    pos[i].y += dt*(vel[i].y + dt2*(acc[i].y + dt3*jrk[i].y));
    pos[i].z += dt*(vel[i].z + dt2*(acc[i].z + dt3*jrk[i].z));

    vel[i].x += dt*(acc[i].x + dt2*jrk[i].x);
    vel[i].y += dt*(acc[i].y + dt2*jrk[i].y);
    vel[i].z += dt*(acc[i].z + dt2*jrk[i].z);
  }

  forces(n, pos, vel, acc, jrk, eps2);

  if (dt > 0.0)
  {
    const real h    = dt*0.5;
    const real hinv = 1.0/h;
    const real f1   = 0.5*hinv*hinv;
    const real f2   = 3.0*hinv*f1;

    const real dt2  = dt *dt * (1.0/2.0);
    const real dt3  = dt2*dt * (1.0/3.0);
    const real dt4  = dt3*dt * (1.0/4.0);
    const real dt5  = dt4*dt * (1.0/5.0);

    for (int i = 0; i < n; i++)
    {

      /* compute snp & crk */

      const real4 Am(   acc[i].x - acc0[i].x,     acc[i].y - acc0[i].y,     acc[i].z - acc0[i].z);
      const real4 Jm(h*(jrk[i].x - jrk0[i].x), h*(jrk[i].y - jrk0[i].y), h*(jrk[i].z - jrk0[i].z));
      const real4 Jp(h*(jrk[i].x + jrk0[i].x), h*(jrk[i].y + jrk0[i].y), h*(jrk[i].z + jrk0[i].z));
      real4 snp(f1* Jm.x,         f1* Jm.y,         f1* Jm.z        );
      real4 crk(f2*(Jp.x - Am.x), f2*(Jp.y - Am.y), f2*(Jp.z - Am.z));

      snp.x -= h*crk.x;
      snp.y -= h*crk.y;
      snp.z -= h*crk.z;

      /* correct */

      pos[i].x += dt4*snp.x + dt5*crk.x;
      pos[i].y += dt4*snp.y + dt5*crk.y;
      pos[i].z += dt4*snp.z + dt5*crk.z;

      vel[i].x += dt3*snp.x + dt4*crk.x;
      vel[i].y += dt3*snp.y + dt4*crk.y;
      vel[i].z += dt3*snp.z + dt4*crk.z;
    }
  }

  return timestep(n, vel, acc, eta, eps2);
}


void integrate(
    std::vector<real4> &pos,
    std::vector<real4> &vel,
    const real eta,
    const real eps2,
    const real t_end,
    const int  niter) {

  const int n = pos.size();
  assert(pos.size() == vel.size());
  std::vector<real4> acc(n), jrk(n);


  const double tin = get_time();
  forces(n, &pos[0], &vel[0], &acc[0], &jrk[0], eps2);
  const double fn = n;
  fprintf(stderr, " mean flop rate in %g sec [%g GFLOP/s]\n", get_time() - tin,
      fn*fn*60/(get_time() - tin)/1e9);

  real Epot0, Ekin0;
  energy(n, &pos[0], &vel[0], &acc[0], Ekin0, Epot0);
  const real Etot0 = Epot0 + Ekin0;
  fprintf(stderr, " E: %g %g %g \n", Epot0, Ekin0, Etot0);

  /////////

  real t_global = 0;
  double t0 = 0;
  int iter = 0;
  int ntime = 10;
  real dt = 0;
  real Epot, Ekin, Etot = Etot0;
  while (t_global < t_end) {
    if (iter >= niter) break;
    if (iter % ntime == 0) 
      t0 = get_time();

    dt = iterate(n, &pos[0], &vel[0], &acc[0], &jrk[0], eta, eps2, dt);
    iter++;
    t_global += dt;

    const real Etot_pre = Etot;
    energy(n, &pos[0], &vel[0], &acc[0], Ekin, Epot);
    Etot = Ekin + Epot;

    if (iter % 1 == 0) {
      const real Etot = Ekin + Epot;
      fprintf(stderr, "iter= %d: t= %g  dt= %g Ekin= %g  Epot= %g  Etot= %g , dE = %g d(dE)= %g \n",
          iter, t_global, dt, Ekin, Epot, Etot, (Etot - Etot0)/std::abs(Etot0),
          (Etot - Etot_pre)/std::abs(Etot_pre)   );
    }

    if (iter % ntime == 0) {
      fprintf(stderr, " mean flop rate in %g sec [%g GFLOP/s]\n", get_time() - t0,
          fn*fn*60/(get_time() - t0)/1e9*ntime);
    }

  }
};

int main(int argc, char *argv[]) {
  std::vector<real4> pos, vel;

  int nbodies = 131072;
#if 1
  nbodies = 32768;
#endif
  nbodies = 1024;
  nbodies = 2048;
  nbodies = 4096;
  nbodies = 8192;
  fprintf(stderr, "nbodies= %d\n", nbodies);
  const real R0 = 1;
  const real mp = 1.0/nbodies;
  for (int i = 0; i < nbodies; i++) {
    real xp, yp, zp, s2 = 2*R0;
    real vx, vy, vz;
    while (s2 > R0*R0) {
      xp = (1.0 - 2.0*drand48())*R0;
      yp = (1.0 - 2.0*drand48())*R0;
      zp = (1.0 - 2.0*drand48())*R0;
      s2 = xp*xp + yp*yp + zp*zp;
      vx = drand48() * 0.1;
      vy = drand48() * 0.1;
      vz = drand48() * 0.1;
    } 
    pos.push_back(real4(xp, yp, zp, mp));
    vel.push_back(real4(vx, vy, vz, -1));
  }
  const real eps2 = 4.0f/nbodies;
  real eta  = 0.01f;

  if (argc > 1) eta *= atof(argv[1]);
  fprintf(stderr, " eta= %g \n", eta);
  fprintf(stderr, " stargting ... \n");
  const real tend = 1.0;
  const int  niter = 10;
  integrate(pos, vel, eta, eps2, tend,niter);

}
