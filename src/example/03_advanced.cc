#include <iostream>
#include <APLCON.hpp>
#include <limits>
#include <iomanip>

using namespace std;

int main() {

  // this example illustrates some more advanced usage of the APLCON interface,
  // like setting up (optionally linked) covariances
  // and more complex constraint functions

  // please feel free to modify the code here and see
  // if APLCON will throw you a meaningful exception in
  // case you set up something stupid :)


  // this example uses rather stupid values for covariances,
  // so APLCON needs more iterations to converge
  // have a look at APLCON::Fit_Settings_t to see what can be configured globally

  APLCON::Fit_Settings_t settings = APLCON::Fit_Settings_t::Default;
  settings.MaxIterations = 50;

  APLCON a("Fit A", settings);

  // as an example structure, we use something
  // which looks like a Lorentz vector
  struct Vec {
    double E;
    double px;
    double py;
    double pz;
  };

  // just some particles
  // zeroth component should be large enough
  // for meaningful invariant mass
  Vec vec1a = { 10, 2, 3, 4};
  Vec vec2a = { 50, 6, 7, 8};

  // linked sigmas are shown in 02_linker.cc
  const vector<double> sigma1 = {1};
  const vector<double> sigma2 = {2};

  // you're totally free in
  // how the fields of your data structure are linked:

  // for instance a, we separate E and p
  auto linker_E = [] (Vec& v) -> vector<double*> { return {&v.E}; };
  auto linker_p = [] (Vec& v) -> vector<double*> { return {&v.px, &v.py, &v.pz}; };
  a.LinkVariable("Vec1_E", linker_E(vec1a), sigma1);
  a.LinkVariable("Vec1_p", linker_p(vec1a), sigma1);
  a.LinkVariable("Vec2_E", linker_E(vec2a), sigma2);
  a.LinkVariable("Vec2_p", linker_p(vec2a), sigma2);


  // #######################################
  // SetCovariance was already used for scalar variables in the first example
  // here we show how to setup covariances for vector-valued variables
  // and how to setup linked covariances


  // EXAMPLE (1): covariances between scalar- and vector-valued variable
  // the first  variable defines the number of rows
  // the second variable defines the number of columns
  // the convention for symmetric covariance matrix is: rows<columns,
  // and variable names according to "first row index, then column index")

  const double E_px = 0;
  const double E_py = 0;
  const double E_pz = 0;
  a.SetCovariance("Vec1_E", "Vec1_p", // variables give 1 row and 3 columns
                  vector<double>{
                    E_px, E_py, E_pz
                  });
  // in this case, swapping the variable names would setup the same covariances,
  // which is not true in general. so always check if you actually made it right :)


  // EXAMPLE (2): covariances for vector-valued variable Vec1_p,
  // which---interpreted as momentum---can have covariances pypx, pzpx, pzpy

  const double pypx = 0;
  const double pzpx = 0;
  const double pzpy = 0;
  // the corresponding covariance matrix is a simple 1-dimensional vector,
  // but interpreted as the part below the diagonal
  // (since the diagonal of the covariance matrix corresponds to sigmas of px, py and pz, which are specified above)
  const vector<double> covariance_matrix_p = {
    // the /**/ indicate the position of the diagonal elements
    /**/             // first  row -> x component
    pypx, /**/       // second row -> y component
    pzpx, pzpy /**/  // third  row -> z component
  };
  a.SetCovariance("Vec1_p", "Vec1_p", covariance_matrix_p);


  // EXAMPLE (3): covariances for two vector-valued variables Vec1_p and Vec2_p
  // since there are no diagonal elements of the full covariance matrix involved,
  // the given vector of covariances represents a 3x3 matrix in this case

  // variables indicate 3 rows and 3 columns
  const vector<double> covariance_matrix_pp = {
    0,  0,  0,
   0, 0, 0,
   0, 0, 0
  };
  a.SetCovariance("Vec1_p", "Vec2_p",covariance_matrix_pp);

  // #######################################
  // now some more advanced constraint business
  // remember that the arguments correspond to the specified
  // in general, there are four different constraint types
  // which are supported by the interface:
  // (1) arguments are all scalars and returns a scalar
  // (2) arguments are all vectors and returns a scalar
  // (3) arguments are all scalars and returns a vector
  // (4) arguments are all vectors and returns a vector
  // in case of (3), (4) the provided constraint aggregates several scalar constraints into one function

  // example for case (2)
  auto invariant_mass = [] (const vector<double>& E, const vector<double>& p) -> double {
    // note that, although E is a scalar variable,
    // it is provided as a vector with one element
    // (mixing scalar/vector arguments are not supported at the moment)
    // M^2 = E^2 - vec(p)^2
    const double M2 = pow(E[0],2) - pow(p[0],2) - pow(p[1],2) - pow(p[2],2);
    return M2 - pow(60,2); // require it to be some value
  };
  a.AddConstraint("invariant_mass", {"Vec1_E", "Vec1_p"}, invariant_mass);

  // example for case (4)
  auto equal_momentum_3 = [] (const vector<double>& a, const vector<double>& b) -> vector<double> {
    // one should check that the vectors a, b have the appropiate lengths...
    return {
      a[0] - b[0],
      a[1] - b[1],
      a[2] - b[2]
    }; // returns 3 scalar constraints
  };
  a.AddConstraint("equal_momentum",  {"Vec1_p", "Vec2_p"}, equal_momentum_3);

  // don't execute DoFit yet because we copy the initial values in Vec1/Vec2 below


  // #######################################
  // we setup instance b exactly as instance a,
  // but this time with 4-vectors
  // this hopefully shows how powerful the interface is :)

  APLCON b("Fit B");
  Vec vec1b = vec1a;
  Vec vec2b = vec2a;

  // for instance b, we link all 4 components at once
  auto linker4   = [] (Vec& v) -> vector<double*> { return {&v.E, &v.px, &v.py, &v.pz}; };
  b.LinkVariable("Vec1", linker4(vec1b), sigma1);
  b.LinkVariable("Vec2", linker4(vec2b), sigma2);


  // covariances can contain NaN to indicate that they should be kept 0
  // can also use APLCON::NaN, which is the identical expression but easier to remember
  const double NaN = numeric_limits<double>::quiet_NaN();

  // we show here how to link covariances.
  // In case you want to keep those linked covariances 0, provide nullptr



  auto equal_vector = [] (const vector<double>& a, const vector<double>& b) -> vector<double> {
    // TODO: check check if sizes of a and b are equal
    vector<double> r(a.size());
    for(size_t i=0;i<a.size();i++)
      r[i] = a[i]-b[i];
    return r;
  };

  // finally, do the fit
  // note that many setup exceptions are only thrown here,
  // because only with a fully setup instance it's possible to check many things

  cout.precision(3); // set precision globally, which makes output nicer
  cout << a.DoFit() << endl;

}