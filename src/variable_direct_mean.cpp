/*!
 * \file variable_direct_mean.cpp
 * \brief Definition of the solution fields.
 * \author Aerospace Design Laboratory (Stanford University).
 * \version 1.2.0
 *
 * SU2 EDU, Copyright (C) 2014 Aerospace Design Laboratory (Stanford University).
 *
 * SU2 EDU is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * SU2 EDU is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with SU2 EDU. If not, see <http://www.gnu.org/licenses/>.
 */

#include "../include/variable_structure.hpp"

CEulerVariable::CEulerVariable(void) : CVariable() {
  
  /*--- Array initialization ---*/
	TS_Source = NULL;
	Primitive = NULL;
	Gradient_Primitive = NULL;
	Limiter_Primitive = NULL;
  WindGust = NULL;
  WindGustDer = NULL;
  
}

CEulerVariable::CEulerVariable(double val_density, double *val_velocity, double val_energy, unsigned short val_nDim,
                               unsigned short val_nvar, CConfig *config) : CVariable(val_nDim, val_nvar, config) {
	unsigned short iVar, iDim, iMesh, nMGSmooth = 0;
  
  bool compressible = (config->GetKind_Regime() == COMPRESSIBLE);
  bool incompressible = (config->GetKind_Regime() == INCOMPRESSIBLE);
  bool freesurface = (config->GetKind_Regime() == FREESURFACE);
  bool low_fidelity = config->GetLowFidelitySim();
  bool dual_time = ((config->GetUnsteady_Simulation() == DT_STEPPING_1ST) ||
                    (config->GetUnsteady_Simulation() == DT_STEPPING_2ND));
  bool windgust = config->GetWind_Gust();
  
  /*--- Array initialization ---*/
	TS_Source = NULL;
	Primitive = NULL;
	Gradient_Primitive = NULL;
	Limiter_Primitive = NULL;
  WindGust = NULL;
  WindGustDer = NULL;
  
  /*--- Allocate and initialize the primitive variables and gradients ---*/
  if (incompressible) { nPrimVar = nDim+5; nPrimVarGrad = nDim+3; }
  if (freesurface)    { nPrimVar = nDim+7; nPrimVarGrad = nDim+6; }
  if (compressible)   { nPrimVar = nDim+7; nPrimVarGrad = nDim+4; }
  
	/*--- Allocate residual structures ---*/
	Res_TruncError = new double [nVar];
  
	for (iVar = 0; iVar < nVar; iVar++) {
		Res_TruncError[iVar] = 0.0;
	}
  
	/*--- Only for residual smoothing (multigrid) ---*/
	for (iMesh = 0; iMesh <= config->GetMGLevels(); iMesh++)
		nMGSmooth += config->GetMG_CorrecSmooth(iMesh);
  
	if ((nMGSmooth > 0) || low_fidelity || freesurface) {
		Residual_Sum = new double [nVar];
		Residual_Old = new double [nVar];
	}
  
	/*--- Allocate undivided laplacian (centered) and limiter (upwind)---*/
	if (config->GetKind_ConvNumScheme_Flow() == SPACE_CENTERED) {
		Undivided_Laplacian = new double [nVar];
  }
  
  /*--- Always allocate the slope limiter,
   and the auxiliar variables (check the logic - JST with 2nd order Turb model - ) ---*/
  Limiter_Primitive = new double [nPrimVarGrad];
  for (iVar = 0; iVar < nPrimVarGrad; iVar++)
    Limiter_Primitive[iVar] = 0.0;
  
  Limiter = new double [nVar];
  for (iVar = 0; iVar < nVar; iVar++)
    Limiter[iVar] = 0.0;
  
  Solution_Max = new double [nPrimVarGrad];
  Solution_Min = new double [nPrimVarGrad];
  for (iVar = 0; iVar < nPrimVarGrad; iVar++) {
    Solution_Max[iVar] = 0.0;
    Solution_Min[iVar] = 0.0;
  }

	/*--- Solution and old solution initialization ---*/
	if (compressible) {
		Solution[0] = val_density;
		Solution_Old[0] = val_density;
		for (iDim = 0; iDim < nDim; iDim++) {
			Solution[iDim+1] = val_density*val_velocity[iDim];
			Solution_Old[iDim+1] = val_density*val_velocity[iDim];
		}
		Solution[nVar-1] = val_density*val_energy;
		Solution_Old[nVar-1] = val_density*val_energy;
	}
	if (incompressible || freesurface) {
		Solution[0] = config->GetPressure_FreeStreamND();
		Solution_Old[0] = config->GetPressure_FreeStreamND();
		for (iDim = 0; iDim < nDim; iDim++) {
			Solution[iDim+1] = val_velocity[iDim]*config->GetDensity_FreeStreamND();
			Solution_Old[iDim+1] = val_velocity[iDim]*config->GetDensity_FreeStreamND();
		}
	}
  
	/*--- Allocate and initialize solution for dual time strategy ---*/
	if (dual_time) {
    if (compressible) {
			Solution_time_n[0] = val_density;
			Solution_time_n1[0] = val_density;
			for (iDim = 0; iDim < nDim; iDim++) {
				Solution_time_n[iDim+1] = val_density*val_velocity[iDim];
				Solution_time_n1[iDim+1] = val_density*val_velocity[iDim];
			}
			Solution_time_n[nVar-1] = val_density*val_energy;
			Solution_time_n1[nVar-1] = val_density*val_energy;
		}
    if (incompressible || freesurface) {
			Solution_time_n[0] = config->GetPressure_FreeStreamND();
			Solution_time_n1[0] = config->GetPressure_FreeStreamND();
			for (iDim = 0; iDim < nDim; iDim++) {
				Solution_time_n[iDim+1] = val_velocity[iDim]*config->GetDensity_FreeStreamND();
				Solution_time_n1[iDim+1] = val_velocity[iDim]*config->GetDensity_FreeStreamND();
			}
		}
	}
  
	/*--- Allocate space for the time spectral source terms ---*/
	if (config->GetUnsteady_Simulation() == TIME_SPECTRAL) {
		TS_Source = new double[nVar];
		for (iVar = 0; iVar < nVar; iVar++) TS_Source[iVar] = 0.0;
	}
  
  /*--- Allocate vector for wind gust and wind gust derivative field ---*/
	if (windgust) {
    WindGust = new double [nDim];
    WindGustDer = new double [nDim+1];
  }
  
	/*--- Allocate auxiliar vector for free surface source term ---*/
	if (freesurface) Grad_AuxVar = new double [nDim];
  
  /*--- Incompressible flow, primitive variables nDim+3, (P,vx,vy,vz,rho,beta),
   FreeSurface Incompressible flow, primitive variables nDim+4, (P,vx,vy,vz,rho,beta,dist),
   Compressible flow, primitive variables nDim+5, (T,vx,vy,vz,P,rho,h,c) ---*/
  Primitive = new double [nPrimVar];
  for (iVar = 0; iVar < nPrimVar; iVar++) Primitive[iVar] = 0.0;
  
  /*--- Incompressible flow, gradients primitive variables nDim+2, (P,vx,vy,vz,rho),
   FreeSurface Incompressible flow, primitive variables nDim+3, (P,vx,vy,vz,rho,beta,dist),
   Compressible flow, gradients primitive variables nDim+4, (T,vx,vy,vz,P,rho,h)
   We need P, and rho for running the adjoint problem ---*/
  Gradient_Primitive = new double* [nPrimVarGrad];
  for (iVar = 0; iVar < nPrimVarGrad; iVar++) {
    Gradient_Primitive[iVar] = new double [nDim];
    for (iDim = 0; iDim < nDim; iDim++)
      Gradient_Primitive[iVar][iDim] = 0.0;
  }
  
}

CEulerVariable::CEulerVariable(double *val_solution, unsigned short val_nDim, unsigned short val_nvar, CConfig *config) : CVariable(val_nDim, val_nvar, config) {
	unsigned short iVar, iDim, iMesh, nMGSmooth = 0;
  
  bool compressible = (config->GetKind_Regime() == COMPRESSIBLE);
  bool incompressible = (config->GetKind_Regime() == INCOMPRESSIBLE);
  bool freesurface = (config->GetKind_Regime() == FREESURFACE);
  bool low_fidelity = config->GetLowFidelitySim();
  bool dual_time = ((config->GetUnsteady_Simulation() == DT_STEPPING_1ST) ||
                    (config->GetUnsteady_Simulation() == DT_STEPPING_2ND));
  bool windgust = config->GetWind_Gust();
  
  /*--- Array initialization ---*/
	TS_Source = NULL;
	Primitive = NULL;
	Gradient_Primitive = NULL;
  Limiter_Primitive = NULL;
  WindGust = NULL;
  WindGustDer = NULL;
  
	/*--- Allocate and initialize the primitive variables and gradients ---*/
  if (incompressible) { nPrimVar = nDim+5; nPrimVarGrad = nDim+3; }
  if (freesurface)    { nPrimVar = nDim+7; nPrimVarGrad = nDim+6; }
  if (compressible)   { nPrimVar = nDim+7; nPrimVarGrad = nDim+4; }
  
	/*--- Allocate residual structures ---*/
	Res_TruncError = new double [nVar];
  
	for (iVar = 0; iVar < nVar; iVar++) {
		Res_TruncError[iVar] = 0.0;
	}
  
	/*--- Only for residual smoothing (multigrid) ---*/
	for (iMesh = 0; iMesh <= config->GetMGLevels(); iMesh++)
		nMGSmooth += config->GetMG_CorrecSmooth(iMesh);
  
	if ((nMGSmooth > 0) || low_fidelity || freesurface) {
		Residual_Sum = new double [nVar];
		Residual_Old = new double [nVar];
	}
  
	/*--- Allocate undivided laplacian (centered) and limiter (upwind)---*/
	if (config->GetKind_ConvNumScheme_Flow() == SPACE_CENTERED)
		Undivided_Laplacian = new double [nVar];
  
  /*--- Always allocate the slope limiter,
   and the auxiliar variables (check the logic - JST with 2nd order Turb model - ) ---*/
  Limiter_Primitive = new double [nPrimVarGrad];
  for (iVar = 0; iVar < nPrimVarGrad; iVar++)
    Limiter_Primitive[iVar] = 0.0;
  
  Limiter = new double [nVar];
  for (iVar = 0; iVar < nVar; iVar++)
    Limiter[iVar] = 0.0;
  
  Solution_Max = new double [nPrimVarGrad];
  Solution_Min = new double [nPrimVarGrad];
  for (iVar = 0; iVar < nPrimVarGrad; iVar++) {
    Solution_Max[iVar] = 0.0;
    Solution_Min[iVar] = 0.0;
  }
  
	/*--- Solution initialization ---*/
	for (iVar = 0; iVar < nVar; iVar++) {
		Solution[iVar] = val_solution[iVar];
		Solution_Old[iVar] = val_solution[iVar];
	}
  
	/*--- Allocate and initializate solution for dual time strategy ---*/
	if (dual_time) {
		Solution_time_n = new double [nVar];
		Solution_time_n1 = new double [nVar];
    
		for (iVar = 0; iVar < nVar; iVar++) {
			Solution_time_n[iVar] = val_solution[iVar];
			Solution_time_n1[iVar] = val_solution[iVar];
		}
	}
  
	/*--- Allocate space for the time spectral source terms ---*/
	if (config->GetUnsteady_Simulation() == TIME_SPECTRAL) {
		TS_Source = new double[nVar];
		for (iVar = 0; iVar < nVar; iVar++) TS_Source[iVar] = 0.0;
	}
  
  /*--- Allocate vector for wind gust and wind gust derivative field ---*/
	if (windgust) {
    WindGust = new double [nDim];
    WindGustDer = new double [nDim+1];
  }
  
	/*--- Allocate auxiliar vector for free surface source term ---*/
	if (freesurface) Grad_AuxVar = new double [nDim];
  
  /*--- Incompressible flow, primitive variables nDim+3, (P,vx,vy,vz,rho,beta),
   FreeSurface Incompressible flow, primitive variables nDim+4, (P,vx,vy,vz,rho,beta,dist),
   Compressible flow, primitive variables nDim+5, (T,vx,vy,vz,P,rho,h,c) ---*/
  Primitive = new double [nPrimVar];
  for (iVar = 0; iVar < nPrimVar; iVar++) Primitive[iVar] = 0.0;
  
  /*--- Incompressible flow, gradients primitive variables nDim+2, (P,vx,vy,vz,rho),
   FreeSurface Incompressible flow, primitive variables nDim+4, (P,vx,vy,vz,rho,beta,dist),
   Compressible flow, gradients primitive variables nDim+4, (T,vx,vy,vz,P,rho,h)
   We need P, and rho for running the adjoint problem ---*/
  Gradient_Primitive = new double* [nPrimVarGrad];
  for (iVar = 0; iVar < nPrimVarGrad; iVar++) {
    Gradient_Primitive[iVar] = new double [nDim];
    for (iDim = 0; iDim < nDim; iDim++)
      Gradient_Primitive[iVar][iDim] = 0.0;
  }
  
  /*--- Allocate the limiter for the primitive variables ---*/
  Limiter_Primitive = new double [nPrimVarGrad];
  
}

CEulerVariable::~CEulerVariable(void) {
	unsigned short iVar;
  
	if (TS_Source         != NULL) delete [] TS_Source;
  if (Primitive         != NULL) delete [] Primitive;
  if (Limiter_Primitive != NULL) delete [] Limiter_Primitive;
  if (WindGust          != NULL) delete [] WindGust;
  if (WindGustDer       != NULL) delete [] WindGustDer;
  
  if (Gradient_Primitive != NULL) {
    for (iVar = 0; iVar < nPrimVarGrad; iVar++)
      delete Gradient_Primitive[iVar];
    delete [] Gradient_Primitive;
  }
  
}

void CEulerVariable::SetGradient_PrimitiveZero(unsigned short val_primvar) {
	unsigned short iVar, iDim;
  
	for (iVar = 0; iVar < val_primvar; iVar++)
		for (iDim = 0; iDim < nDim; iDim++)
			Gradient_Primitive[iVar][iDim] = 0.0;
}

double CEulerVariable::GetProjVel(double *val_vector) {
	double ProjVel;
	unsigned short iDim;
  
	ProjVel = 0.0;
	for (iDim = 0; iDim < nDim; iDim++)
		ProjVel += Primitive[iDim+1]*val_vector[iDim];
  
	return ProjVel;
}

bool CEulerVariable::SetPrimVar_Compressible(CConfig *config) {
	unsigned short iVar;
  bool check_dens = false, check_press = false, check_sos = false, check_temp = false, RightVol = true;
  
  double Gas_Constant = config->GetGas_ConstantND();
	double Gamma = config->GetGamma();
  
  SetVelocity();                                // Computes velocity and velocity^2
  check_dens = SetDensity();                    // Check the density
	check_press = SetPressure(Gamma);							// Requires velocity2 computation.
	check_sos = SetSoundSpeed(Gamma);             // Requires pressure computation.
	check_temp = SetTemperature(Gas_Constant);		// Requires pressure computation.
  
  /*--- Check that the solution has a physical meaning ---*/
  
  if (check_dens || check_press || check_sos || check_temp) {
    
    /*--- Copy the old solution ---*/
    
    for (iVar = 0; iVar < nVar; iVar++)
      Solution[iVar] = Solution_Old[iVar];
    
    /*--- Recompute the primitive variables ---*/
    
    SetVelocity();
    check_dens = SetDensity();
    check_press = SetPressure(Gamma);
    check_sos = SetSoundSpeed(Gamma);
    check_temp = SetTemperature(Gas_Constant);
    
    RightVol = false;
    
  }
  
  /*--- Set enthalpy ---*/
  
  SetEnthalpy();                                // Requires pressure computation.
  
  return RightVol;
  
}

CNSVariable::CNSVariable(void) : CEulerVariable() { }

CNSVariable::CNSVariable(double val_density, double *val_velocity, double val_energy,
                         unsigned short val_nDim, unsigned short val_nvar,
                         CConfig *config) : CEulerVariable(val_density, val_velocity, val_energy, val_nDim, val_nvar, config) {
  
	Temperature_Ref = config->GetTemperature_Ref();
	Viscosity_Ref   = config->GetViscosity_Ref();
	Viscosity_Inf   = config->GetViscosity_FreeStreamND();
	Prandtl_Lam     = config->GetPrandtl_Lam();
	Prandtl_Turb    = config->GetPrandtl_Turb();
  
}

CNSVariable::CNSVariable(double *val_solution, unsigned short val_nDim,
                         unsigned short val_nvar, CConfig *config) : CEulerVariable(val_solution, val_nDim, val_nvar, config) {
  
	Temperature_Ref = config->GetTemperature_Ref();
	Viscosity_Ref   = config->GetViscosity_Ref();
	Viscosity_Inf   = config->GetViscosity_FreeStreamND();
	Prandtl_Lam     = config->GetPrandtl_Lam();
	Prandtl_Turb    = config->GetPrandtl_Turb();
}

CNSVariable::~CNSVariable(void) { }

void CNSVariable::SetVorticity(void) {
	double u_y = Gradient_Primitive[1][1];
	double v_x = Gradient_Primitive[2][0];
	double u_z = 0.0;
	double v_z = 0.0;
	double w_x = 0.0;
	double w_y = 0.0;
  
	if (nDim == 3) {
		u_z = Gradient_Primitive[1][2];
		v_z = Gradient_Primitive[2][2];
		w_x = Gradient_Primitive[3][0];
		w_y = Gradient_Primitive[3][1];
	}
  
	Vorticity[0] = w_y-v_z;
	Vorticity[1] = -(w_x-u_z);
	Vorticity[2] = v_x-u_y;
  
}

void CNSVariable::SetStrainMag(void) {
  
  double div13, aux;
  
  if (nDim == 2) {
    
    div13 = 1.0/3.0*(Gradient_Primitive[1][0] + Gradient_Primitive[2][1]);
    StrainMag = 0.0;
    
    aux = (Gradient_Primitive[1][0] - div13); StrainMag += aux*aux;
    aux = (Gradient_Primitive[2][1] - div13); StrainMag += aux*aux;
    
    aux = (Gradient_Primitive[1][1] + Gradient_Primitive[2][0]); StrainMag += 2.0*aux*aux;
    
    StrainMag = sqrt(2.0*StrainMag);
    
  }
  else {
    
    div13 = 1.0/3.0*(Gradient_Primitive[1][0] + Gradient_Primitive[2][1] + Gradient_Primitive[3][2]);
    StrainMag = 0.0;
    
    aux = (Gradient_Primitive[1][0] - div13); StrainMag += aux*aux;
    aux = (Gradient_Primitive[2][1] - div13); StrainMag += aux*aux;
    aux = (Gradient_Primitive[3][2] - div13); StrainMag += aux*aux;
    
    aux = 0.5*(Gradient_Primitive[1][1] + Gradient_Primitive[2][0]); StrainMag += 2.0*aux*aux;
    aux = 0.5*(Gradient_Primitive[1][2] + Gradient_Primitive[3][0]); StrainMag += 2.0*aux*aux;
    aux = 0.5*(Gradient_Primitive[2][2] + Gradient_Primitive[3][1]); StrainMag += 2.0*aux*aux;
    
    StrainMag = sqrt(2.0*StrainMag);
    
  }
  
}

bool CNSVariable::SetPrimVar_Compressible(double eddy_visc, double turb_ke, CConfig *config) {
	unsigned short iVar;
  bool check_dens = false, check_press = false, check_sos = false, check_temp = false, RightVol = true;
  
  double Gas_Constant = config->GetGas_ConstantND();
	double Gamma = config->GetGamma();
  
  SetVelocity();                                  // Computes velocity and velocity^2
  check_dens = SetDensity();                      // Check the density
	check_press = SetPressure(Gamma, turb_ke);      // Requires velocity2 computation.
	check_sos = SetSoundSpeed(Gamma);               // Requires pressure computation.
	check_temp = SetTemperature(Gas_Constant);      // Requires pressure computation.
  
  /*--- Check that the solution has a physical meaning ---*/
  
  if (check_dens || check_press || check_sos || check_temp) {
    
    /*--- Copy the old solution ---*/
    
    for (iVar = 0; iVar < nVar; iVar++)
      Solution[iVar] = Solution_Old[iVar];
    
    /*--- Recompute the primitive variables ---*/
    
    SetVelocity();
    check_dens = SetDensity();
    check_press = SetPressure(Gamma, turb_ke);
    check_sos = SetSoundSpeed(Gamma);
    check_temp = SetTemperature(Gas_Constant);
    
    RightVol = false;
    
  }
  
  /*--- Set enthalpy ---*/
  
	SetEnthalpy();                                  // Requires pressure computation.
  
  /*--- Set laminar viscosity ---*/
  
	SetLaminarViscosity(config);                    // Requires temperature computation.
  
  /*--- Set eddy viscosity ---*/
  
  SetEddyViscosity(eddy_visc);
  
  return RightVol;
  
}
