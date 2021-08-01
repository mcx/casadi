/*
 *    This file is part of CasADi.
 *
 *    CasADi -- A symbolic framework for dynamic optimization.
 *    Copyright (C) 2010-2014 Joel Andersson, Joris Gillis, Moritz Diehl,
 *                            K.U. Leuven. All rights reserved.
 *    Copyright (C) 2011-2014 Greg Horn
 *
 *    CasADi is free software; you can redistribute it and/or
 *    modify it under the terms of the GNU Lesser General Public
 *    License as published by the Free Software Foundation; either
 *    version 3 of the License, or (at your option) any later version.
 *
 *    CasADi is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *    Lesser General Public License for more details.
 *
 *    You should have received a copy of the GNU Lesser General Public
 *    License along with CasADi; if not, write to the Free Software
 *    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */


#ifndef CASADI_DAE_BUILDER_INTERNAL_HPP
#define CASADI_DAE_BUILDER_INTERNAL_HPP

#include <unordered_map>

#include "dae_builder.hpp"
#include "shared_object_internal.hpp"
#include "importer.hpp"

#ifdef WITH_FMU
//#include <fmi2FunctionTypes.h>
#include <fmi2Functions.h>
//#include <fmi2TypesPlatform.h>
#endif  // WITH_FMU

namespace casadi {

// Forward declarations
class XmlNode;
class Fmu;

/** \brief Holds expressions and meta-data corresponding to a physical quantity evolving in time
    \date 2012-2021
    \author Joel Andersson
 */
struct CASADI_EXPORT Variable {
  /// Variable type
  enum Type {REAL, INTEGER, BOOLEAN, STRING, ENUM, N_TYPE};

  /// Causality: FMI 2.0 specification, section 2.2.7
  enum Causality {PARAMETER, CALCULATED_PARAMETER, INPUT, OUTPUT, LOCAL, INDEPENDENT, N_CAUSALITY};

  /// Variability: FMI 2.0 specification, section 2.2.7
  enum Variability {CONSTANT, FIXED, TUNABLE, DISCRETE, CONTINUOUS, N_VARIABILITY};

  /// Initial: FMI 2.0 specification, section 2.2.7
  enum Initial {EXACT, APPROX, CALCULATED, INITIAL_NA, N_INITIAL};

  // Attributes
  enum Attribute {MIN, MAX, NOMINAL, START, N_ATTRIBUTE};

  /// Constructor
  Variable(const std::string& name = "");

  /** Attributes common to all types of variables, cf. FMI specification */
  ///@{
  std::string name;
  casadi_int value_reference;
  std::string description;
  Type type;
  Causality causality;
  Variability variability;
  Initial initial;
  ///@}

  /** Attributes specific to Real, cf. FMI specification */
  ///@{
  // std::string declared_type;
  // std::string quantity;
  std::string unit;
  std::string display_unit;
  // bool relative_quantity;
  MX min;
  MX max;
  MX nominal;
  // bool unbounded;
  MX start;
  casadi_int derivative;
  casadi_int antiderivative;
  // bool reinit;
  ///@}

  /// Do other expressions depend on this variable
  bool dependency;

  /// Dependencies
  std::vector<casadi_int> dependencies;

  /// Variable expression
  MX v;

  /// Binding equation
  MX beq;

  // Default initial attribute, per specification
  static Initial default_initial(Causality causality, Variability variability);

  // Get attribute by enum
  MX attribute(Attribute att) const;
};

/// \cond INTERNAL
/// Internal class for DaeBuilder, see comments on the public class.
class CASADI_EXPORT DaeBuilderInternal : public SharedObjectInternal {
  friend class DaeBuilder;
  friend class FmuFunction;
  friend class Fmu;
  friend class FmuFunctionJac;
  friend class FmuFunctionAdj;

 public:

  /// Constructor
  explicit DaeBuilderInternal(const std::string& name, const std::string& path);

  /// Destructor
  ~DaeBuilderInternal() override;

  /// Readable name of the internal class
  std::string class_name() const override {return "DaeBuilderInternal";}

  /// Check if dimensions match
  void sanity_check() const;

  /** @name Manipulation
   *  Reformulate the dynamic optimization problem.
   */
  ///@{

  /// Eliminate all dependent variables
  void eliminate_w();

  /// Lift problem formulation by extracting shared subexpressions
  void lift(bool lift_shared, bool lift_calls);

  /// Eliminate quadrature states and turn them into ODE states
  void eliminate_quad();

  /// Sort dependent parameters
  void sort_d();

  /// Sort dependent variables
  void sort_w();

  /// Sort algebraic variables
  void sort_z(const std::vector<std::string>& z_order);

  /// Clear input variable
  void clear_in(const std::string& v);

  /// Clear output variable
  void clear_out(const std::string& v);

  /// Prune unused controls
  void prune(bool prune_p, bool prune_u);

  /// Identify free variables and residual equations
  void tearing_variables(std::vector<std::string>* res, std::vector<std::string>* iv,
    std::vector<std::string>* iv_on_hold) const;

  /// Identify free variables and residual equations
  void tear();
  ///@}

  /** @name Import and export
   */
  ///@{
  /// Import existing problem from FMI/XML
  void load_fmi_description(const std::string& filename);

  /// Import FMI functions
  void load_fmi_functions(const std::string& path);

  // Input convension in codegen
  enum DaeBuilderInternalIn {
    DAE_BUILDER_T,
    DAE_BUILDER_C,
    DAE_BUILDER_P,
    DAE_BUILDER_D,
    DAE_BUILDER_W,
    DAE_BUILDER_U,
    DAE_BUILDER_X,
    DAE_BUILDER_Z,
    DAE_BUILDER_Q,
    DAE_BUILDER_Y,
    DAE_BUILDER_NUM_IN
  };

  // Output convension in codegen
  enum DaeBuilderInternalOut {
    DAE_BUILDER_ODE,
    DAE_BUILDER_ALG,
    DAE_BUILDER_QUAD,
    DAE_BUILDER_DDEF,
    DAE_BUILDER_WDEF,
    DAE_BUILDER_YDEF,
    DAE_BUILDER_NUM_OUT
  };

  // Get input expression, given enum
  std::vector<MX> input(DaeBuilderInternalIn ind) const;

  // Get output expression, given enum
  std::vector<MX> output(DaeBuilderInternalOut ind) const;

  // Get input expression, given enum
  std::vector<MX> input(const std::vector<DaeBuilderInternalIn>& ind) const;

  // Get output expression, given enum
  std::vector<MX> output(const std::vector<DaeBuilderInternalOut>& ind) const;

  /// Add a named linear combination of output expressions
  void add_lc(const std::string& name, const std::vector<std::string>& f_out);

  /// Construct a function object
  Function create(const std::string& fname,
      const std::vector<std::string>& s_in,
      const std::vector<std::string>& s_out, bool sx = false, bool lifted_calls = false) const;

  /// Construct a function object for evaluating attributes
  Function attribute_fun(const std::string& fname,
      const std::vector<std::string>& s_in,
      const std::vector<std::string>& s_out) const;

  /// Construct a function for evaluating dependent parameters
  Function dependent_fun(const std::string& fname,
      const std::vector<std::string>& s_in,
      const std::vector<std::string>& s_out) const;

  /// Construct a function from an FMU DLL
  Function fmu_fun(const std::string& name,
      const std::vector<std::vector<size_t>>& id_in,
      const std::vector<std::vector<size_t>>& id_out,
      const std::vector<std::string>& name_in,
      const std::vector<std::string>& name_out,
      const Dict& opts) const;
  ///@}

  /// Function corresponding to all equations
  Function gather_eq() const;

  /// Get variable expression by name
  const MX& var(const std::string& name) const;

  /// Get a derivative expression by name
  MX der(const std::string& name) const;

  /// Get a derivative expression by non-differentiated expression
  MX der(const MX& var) const;

  /// Readable name of the class
  std::string type_name() const {return "DaeBuilderInternal";}

  /// Print description
  void disp(std::ostream& stream, bool more) const override;

  /// Get string representation
  std::string get_str(bool more=false) const {
    std::stringstream ss;
    disp(ss, more);
    return ss.str();
  }

  /// Add a variable
  size_t add_variable(const std::string& name, const Variable& var);

  /// Check if a particular variable exists
  bool has_variable(const std::string& name) const;

  ///@{
  /// Access a variable by index
  Variable& variable(size_t ind) {return variables_.at(ind);}
  const Variable& variable(size_t ind) const {return variables_.at(ind);}
  ///@}

  ///@{
  /// Access a variable by name
  Variable& variable(const std::string& name) {return variable(find(name));}
  const Variable& variable(const std::string& name) const {return variable(find(name));}
  ///@}

  /// Get variable expression by index
  const MX& var(size_t ind) const;

  /// Get variable expressions by index
  std::vector<MX> var(const std::vector<size_t>& ind) const;

  /// Get index of variable
  size_t find(const std::string& name) const;

    /// Get the (cached) oracle, SX or MX
  const Function& oracle(bool sx = false, bool elim_w = false, bool lifted_calls = false) const;

  // Internal methods
protected:

  /// Get the qualified name
  static std::string qualified_name(const XmlNode& nn);

  // FMI attributes
  std::string fmi_version_;
  std::string model_name_;
  std::string guid_;
  std::string description_;
  std::string author_;
  std::string copyright_;
  std::string license_;
  std::string generation_tool_;
  std::string generation_date_and_time_;
  std::string variable_naming_convention_;
  casadi_int number_of_event_indicators_;

  // Model Exchange
  std::string model_identifier_;
  bool provides_directional_derivative_;
  std::vector<std::string> source_files_;

  /// Name of instance
  std::string name_;

  // Path to FMU, if any
  std::string path_;

  /// All variables
  std::vector<Variable> variables_;

  // Model structure
  std::vector<size_t> outputs_, derivatives_, initial_unknowns_;

  /// Find of variable by name
  std::unordered_map<std::string, size_t> varind_;

  /// Ordered variables
  std::vector<size_t> t_, p_, u_, x_, z_, q_, c_, d_, w_, y_, ode_, alg_, quad_;

  ///@{
  /// Ordered variables and equations
  std::vector<MX> aux_;
  std::vector<MX> init_lhs_, init_rhs_;
  std::vector<MX> when_cond_, when_lhs_, when_rhs_;
  ///@}

  /** \brief Definitions of dependent constants */
  std::vector<MX> cdef() const;

  /** \brief Definitions of dependent parameters */
  std::vector<MX> ddef() const;

  /** \brief Definitions of dependent variables */
  std::vector<MX> wdef() const;

  /** \brief Definitions of output variables */
  std::vector<MX> ydef() const;

  /** \brief ODE right hand sides */
  std::vector<MX> ode() const;

  /** \brief Algebraic right hand sides */
  std::vector<MX> alg() const;

  /** \brief Quadrature right hand sides */
  std::vector<MX> quad() const;

  ///@{
  /// Add a new variable
  MX add_t(const std::string& name);
  MX add_p(const std::string& name, casadi_int n);
  MX add_u(const std::string& name, casadi_int n);
  MX add_x(const std::string& name, casadi_int n);
  MX add_z(const std::string& name, casadi_int n);
  MX add_q(const std::string& name, casadi_int n);
  MX add_c(const std::string& name, const MX& new_cdef);
  MX add_d(const std::string& name, const MX& new_ddef);
  MX add_w(const std::string& name, const MX& new_wdef);
  MX add_y(const std::string& name, const MX& new_ydef);
  MX add_ode(const std::string& name, const MX& new_ode);
  MX add_alg(const std::string& name, const MX& new_alg);
  MX add_quad(const std::string& name, const MX& new_quad);
  ///@}

  /// Linear combinations of output expressions
  Function::AuxOut lc_;

  /** \brief Functions */
  std::vector<Function> fun_;

  /** \brief Function oracles (cached) */
  mutable Function oracle_[2][2][2];

  /// Should the cache be cleared?
  mutable bool clear_cache_;

  /// FMU binary interface (cached)
  mutable Fmu* fmu_;

  /// Initialize FMU binary interface
  void init_fmu() const;

  // Name of system, per the FMI specification
  static std::string system_infix();

  // DLL suffix, per the FMI specification
  static std::string dll_suffix();

  /// Read an equation
  MX read_expr(const XmlNode& node);

  /// Read a variable
  Variable& read_variable(const XmlNode& node);

  // Read ModelExchange
  void import_model_exchange(const XmlNode& n);

  // Read ModelVariables
  void import_model_variables(const XmlNode& modvars);

  // Read ModelStructure
  void import_model_structure(const XmlNode& n);

  /// Problem structure has changed: Clear cache
  void clear_cache() const;

  /// Add a function from loaded expressions
  Function add_fun(const std::string& name,
                   const std::vector<std::string>& arg,
                   const std::vector<std::string>& res, const Dict& opts=Dict());

  /// Add an already existing function
  Function add_fun(const Function& f);

  /// Does a particular function already exist?
  bool has_fun(const std::string& name) const;

  /// Get function by name
  Function fun(const std::string& name) const;

  /// Helper class, represents inputs and outputs for a function call node
  struct CallIO {
    // Function instances
    Function f, adj1_f, J, H;
    // Index in v and vdef
    std::vector<size_t> v, vdef;
    // Nondifferentiated inputs
    std::vector<MX> arg;
    // Nondifferentiated inputs
    std::vector<MX> res;
    // Jacobian outputs
    std::vector<MX> jac_res;
    // Adjoint seeds
    std::vector<MX> adj1_arg;
    // Adjoint sensitivities
    std::vector<MX> adj1_res;
    // Hessian outputs
    std::vector<MX> hess_res;
    // Calculate Jacobian blocks
    void calc_jac();
    // Calculate gradient of Lagrangian
    void calc_grad();
    // Calculate Hessian of Lagrangian
    void calc_hess();
    // Access a specific Jacobian block
    const MX& jac(casadi_int oind, casadi_int iind) const;
    // Access a specific Hessian block
    const MX& hess(casadi_int iind1, casadi_int iind2) const;
  };

  /// Calculate contribution to jac_vdef_v from lifted calls
  MX jac_vdef_v_from_calls(std::map<MXNode*, CallIO>& call_nodes,
    const std::vector<casadi_int>& h_offsets) const;

  /// Calculate contribution to hess_?_v_v from lifted calls
  MX hess_v_v_from_calls(std::map<MXNode*, CallIO>& call_nodes,
    const std::vector<casadi_int>& h_offsets) const;

  // Sort dependent variables/parameters
  static void sort_dependent(std::vector<MX>& v, std::vector<MX>& vdef);
};

/// Helper class: Specify number of entries in an enum
template<typename T>
struct enum_traits {};

// Helper function: Convert string to enum
template<typename T>
T to_enum(const std::string& s) {
  // Linear search over permitted values
  for (size_t i = 0; i < enum_traits<T>::n_enum; ++i) {
    if (s == to_string(static_cast<T>(i))) return static_cast<T>(i);
  }
  // Informative error message
  std::stringstream ss;
  ss << "No such enum: '" << s << "'. Permitted values: ";
  for (size_t i = 0; i < enum_traits<T>::n_enum; ++i) {
    // Separate strings
    if (i > 0) ss << ", ";
    // Print enum name
    ss << "'" << to_string(static_cast<T>(i)) << "'";
  }
  casadi_error(ss.str());
  return enum_traits<T>::n_enum;  // never reached
}

///@{
/// Number of entries in enums
template<> struct enum_traits<Variable::Type> {
  static const Variable::Type n_enum = Variable::N_TYPE;
};
template<> struct enum_traits<Variable::Causality> {
  static const Variable::Causality n_enum = Variable::N_CAUSALITY;
};
template<> struct enum_traits<Variable::Variability> {
  static const Variable::Variability n_enum = Variable::N_VARIABILITY;
};
template<> struct enum_traits<Variable::Initial> {
  static const Variable::Initial n_enum = Variable::N_INITIAL;
};
template<> struct enum_traits<Variable::Attribute> {
  static const Variable::Attribute n_enum = Variable::N_ATTRIBUTE;
};
template<> struct enum_traits<DaeBuilderInternal::DaeBuilderInternalIn> {
  static const DaeBuilderInternal::DaeBuilderInternalIn n_enum
    = DaeBuilderInternal::DAE_BUILDER_NUM_IN;
};
template<> struct enum_traits<DaeBuilderInternal::DaeBuilderInternalOut> {
  static const DaeBuilderInternal::DaeBuilderInternalOut n_enum
    = DaeBuilderInternal::DAE_BUILDER_NUM_OUT;
};
///@}

///@{
/// Convert to string
CASADI_EXPORT std::string to_string(Variable::Type v);
CASADI_EXPORT std::string to_string(Variable::Causality v);
CASADI_EXPORT std::string to_string(Variable::Variability v);
CASADI_EXPORT std::string to_string(Variable::Initial v);
CASADI_EXPORT std::string to_string(Variable::Attribute v);
CASADI_EXPORT std::string to_string(DaeBuilderInternal::DaeBuilderInternalIn v);
CASADI_EXPORT std::string to_string(DaeBuilderInternal::DaeBuilderInternalOut v);
///@}


#ifdef WITH_FMU
struct CASADI_EXPORT Fmu {
  // Constructor
  Fmu(const DaeBuilderInternal& self);

  // Destructor
  ~Fmu();

  // Initialize
  void init();

  // Load an FMI function
  signal_t get_function(const std::string& symname);

  // Process message
  static void logger(fmi2ComponentEnvironment componentEnvironment,
    fmi2String instanceName,
    fmi2Status status,
    fmi2String category,
    fmi2String message, ...);

  // New memory object
  int checkout();

  // Free memory object
  void release(int mem);

  // New memory object
  fmi2Component instantiate();

  // Setup experiment
  int setup_experiment(int mem);

  // Reset solver
  int reset(int mem);

  // Enter initialization mode
  int enter_initialization_mode(int mem);

  // Exit initialization mode
  int exit_initialization_mode(int mem);

  // Set value
  void set(int mem, size_t id, double value);

  // Request the calculation of a variable
  void request(int mem, size_t id);

  // Calculate all requested variables
  int eval(int mem);

  // Get a calculated variable
  void get(int mem, size_t id, double* value);

  // Set seed
  void set_seed(int mem, size_t id, double value);

  // Calculate directional derivatives
  int eval_derivative(int mem, bool enable_ad, bool validate_ad);

  // Get a derivative
  void get_sens(int mem, size_t id, double* value);

  // Evaluate, non-differentated
  int eval(int mem, const double** arg, double** res,
    const std::vector<std::vector<size_t>>& id_in,
    const std::vector<std::vector<size_t>>& id_out);

  // Evaluate Jacobian numerically
  int eval_jac(int mem, const double** arg, double** res,
    const std::vector<std::vector<size_t>>& id_in,
    const std::vector<std::vector<size_t>>& id_out,
    bool enable_ad, bool validate_ad,
    const std::vector<Sparsity>& sp_jac);

  // Evaluate adjoint numerically
  int eval_adj(int mem, const double** arg, double** res,
    const std::vector<std::vector<size_t>>& id_in,
    const std::vector<std::vector<size_t>>& id_out,
    bool enable_ad, bool validate_ad);

  // Get memory object
  fmi2Component memory(int mem);

  // Get memory object and remove from pool
  fmi2Component pop_memory(int mem);

  // DaeBuilder instance
  const DaeBuilderInternal& self_;

  // DLL
  Importer li_;

  // FMU C API function prototypes. Cf. FMI specification 2.0.2
  fmi2InstantiateTYPE* instantiate_;
  fmi2FreeInstanceTYPE* free_instance_;
  fmi2ResetTYPE* reset_;
  fmi2SetupExperimentTYPE* setup_experiment_;
  fmi2EnterInitializationModeTYPE* enter_initialization_mode_;
  fmi2ExitInitializationModeTYPE* exit_initialization_mode_;
  fmi2EnterContinuousTimeModeTYPE* enter_continuous_time_mode_;
  fmi2SetRealTYPE* set_real_;
  fmi2SetBooleanTYPE* set_boolean_;
  fmi2GetRealTYPE* get_real_;
  fmi2GetDirectionalDerivativeTYPE* get_directional_derivative_;

  // Callback functions
  fmi2CallbackFunctions functions_;

  // Path to the FMU resource directory
  std::string resource_loc_;

  // Memory object
  struct Memory {
    // Component memory
    fmi2Component c;
    // Currently in use
    bool in_use;
    // Value buffer
    std::vector<double> buffer_;
    // Sensitivities
    std::vector<double> sens_;
    // Which entries have been changed
    std::vector<bool> changed_;
    // Which entries are being requested
    std::vector<bool> requested_;
    // Work vector (reals)
    std::vector<fmi2Real> work_, dwork_;
    // Work vector (value references)
    std::vector<fmi2ValueReference> vr_work_;
    // Constructor
    explicit Memory() : c(0), in_use(false) {}
  };

  // Memory objects
  std::vector<Memory> mem_;
};
#endif  // WITH_FMU


} // namespace casadi

#endif // CASADI_DAE_BUILDER_INTERNAL_HPP
