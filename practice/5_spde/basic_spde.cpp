// Space time
#include <TMB.hpp>
#include <Eigen/Sparse>
#include <vector>
// Function for detecting NAs
template<class Type>
bool isNA(Type x){
  return R_IsNA(asDouble(x));
}

template<class Type>
Type objective_function<Type>::operator() ()
{
  // Indices
  DATA_INTEGER( n_i );
  DATA_INTEGER( n_x );
  DATA_INTEGER( n_t );
  DATA_INTEGER( n_p );

  // Data
  DATA_IVECTOR( x_s );
  DATA_VECTOR( c_i );
  DATA_VECTOR( Exp_i );
  DATA_IVECTOR( s_i );
  DATA_IVECTOR( t_i );
  DATA_MATRIX( X_xp );

  // SPDE objects
  DATA_SPARSE_MATRIX(G0);
  DATA_SPARSE_MATRIX(G1);
  DATA_SPARSE_MATRIX(G2);

  // Options
  DATA_VECTOR( options )

  // Fixed effects
  PARAMETER_VECTOR(alpha);
  PARAMETER(log_tau_E);
  PARAMETER(log_kappa);
  PARAMETER(rho);

  // Random effects
  PARAMETER_ARRAY(epsilon);

//  PARAMETER_ARRAY(sp);


  // objective function -- joint negative log-likelihood
  using namespace density;
  vector<Type> jnll_comp(3);
  jnll_comp[0] = Type(0);
  jnll_comp[1] = Type(0);
  jnll_comp[2] = Type(0);
  //  parallel_accumulator<Type> jnll_comp[1](this);
  max_parallel_regions = omp_get_max_threads();
  printf("This is thread %d\n", max_parallel_regions);

  // Spatial parameters
  Type kappa2 = exp(2.0*log_kappa);
  Type kappa4 = kappa2*kappa2;
  Type pi = 3.141592;
  Type Range = sqrt(8) / exp( log_kappa );
  Type SigmaE = 1 / sqrt(4*pi*exp(2*log_tau_E)*exp(2*log_kappa));
  Eigen::SparseMatrix<Type> Q = kappa4*G0 + Type(2.0)*kappa2*G1 + G2;
  Type rho_trans = log((1+rho)/(1-rho));


  // Objects for derived values
  vector<Type> linear_x(n_x);
  matrix<Type> Epsilon_xt(n_x, n_t);

  // Priors
  if(options[0] == 1) {
   jnll_comp[2] -= dnorm(log_tau_E, Type(0.0), Type(1.0), true);  // N(0,1) prior for log_tau
   jnll_comp[2] -= dnorm(log_kappa, Type(0.0), Type(1.0), true);  // N(0,1) prior for log_kappa
   jnll_comp[2] -= dnorm(rho_trans, Type(0.0), Type(2.582), true); // N(0, sqrt(1/.15) prior on log((1+rho)/(1-rho))
   for( int i=0; i<alpha.size(); i++){
       printf("This is alpha %d\n", i);
      jnll_comp[2] -= dnorm(alpha(i), Type(0.0), Type(100), true); // N(0, sqrt(1/.0001)) prior for fixed effects.
   }
  }
  // Probability of Gaussian-Markov random fields (GMRFs)
     if(n_t > 1 ){
       PARALLEL_REGION jnll_comp[0] += SCALE(SEPARABLE(AR1(rho),GMRF(Q)),1/exp(log_tau_E))(epsilon);

     } else {
       PARALLEL_REGION jnll_comp[0] += SCALE(GMRF(Q),1/exp(log_tau_E))(epsilon);

     }


  // Transform GMRFs
  for(int x=0; x<n_x; x++){
    for( int t=0; t<n_t; t++){
      Epsilon_xt(x,t) = epsilon(x,t) / exp(log_tau_E);
    }
  }

  // Likelihood contribution from observations
  linear_x = X_xp * alpha.matrix();
  vector<Type> mrprob(n_i);
  for (int i=0; i<n_i; i++){
//THIS LINE CAUSED THE BUG:    Epsilon_xt(x_s(s_i(i)),t_i(i)) = epsilon(x_s(s_i(i)),t_i(i))/exp(log_tau_E);
    mrprob(i) = linear_x(x_s(s_i(i))) + Epsilon_xt(x_s(s_i(i)),t_i(i));
    if( !isNA(c_i(i)) ){
       PARALLEL_REGION jnll_comp[1] -= dbinom( c_i(i), Exp_i(i), invlogit(mrprob(i)), true );
    // TEST   PARALLEL_REGION jnll_comp[1] -= dpois(  c_i(i), invlogit(mrprob(i)) * Exp_i(i), true);
    }
  }
  Type jnll = jnll_comp.sum();


  // Diagnostics
  // REPORT( jnll_comp );
  // REPORT( jnll );
  // REPORT( SigmaE );
//  REPORT( alpha );
  ADREPORT( alpha );
//  REPORT( Epsilon_xt );
  ADREPORT( Epsilon_xt );


  return jnll;
}
