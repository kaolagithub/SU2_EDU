/*!
 * \file numerics_direct_turbulent.cpp
 * \brief This file contains all the convective term discretization.
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

#include "../include/numerics_structure.hpp"
#include <limits>

CUpwSca_TurbSA::CUpwSca_TurbSA(unsigned short val_nDim, unsigned short val_nVar,
                               CConfig *config) : CNumerics(val_nDim, val_nVar, config) {
  
  implicit        = (config->GetKind_TimeIntScheme_Turb() == EULER_IMPLICIT);
  grid_movement   = config->GetGrid_Movement();
  
  Velocity_i = new double [nDim];
  Velocity_j = new double [nDim];
  
}

CUpwSca_TurbSA::~CUpwSca_TurbSA(void) {
  
  delete [] Velocity_i;
  delete [] Velocity_j;
  
}

void CUpwSca_TurbSA::ComputeResidual(double *val_residual, double **val_Jacobian_i, double **val_Jacobian_j, CConfig *config) {
  
  q_ij = 0.0;
  
  if (grid_movement) {
    for (iDim = 0; iDim < nDim; iDim++) {
      Velocity_i[iDim] = V_i[iDim+1] - GridVel_i[iDim];
      Velocity_j[iDim] = V_j[iDim+1] - GridVel_j[iDim];
      q_ij += 0.5*(Velocity_i[iDim]+Velocity_j[iDim])*Normal[iDim];
    }
  } else {
    for (iDim = 0; iDim < nDim; iDim++) {
      Velocity_i[iDim] = V_i[iDim+1];
      Velocity_j[iDim] = V_j[iDim+1];
      q_ij += 0.5*(Velocity_i[iDim]+Velocity_j[iDim])*Normal[iDim];
    }
  }
    
  a0 = 0.5*(q_ij+fabs(q_ij));
  a1 = 0.5*(q_ij-fabs(q_ij));
  val_residual[0] = a0*TurbVar_i[0]+a1*TurbVar_j[0];
  
  if (implicit) {
    val_Jacobian_i[0][0] = a0;
    val_Jacobian_j[0][0] = a1;
  }
  
}

CAvgGrad_TurbSA::CAvgGrad_TurbSA(unsigned short val_nDim, unsigned short val_nVar, CConfig *config) : CNumerics(val_nDim, val_nVar, config) {
  unsigned short iVar;
  
  implicit = (config->GetKind_TimeIntScheme_Turb() == EULER_IMPLICIT);
  
  sigma = 2./3.;
  
  Edge_Vector = new double [nDim];
  Proj_Mean_GradTurbVar_Kappa = new double [nVar];
  Proj_Mean_GradTurbVar_Edge = new double [nVar];
  Mean_GradTurbVar = new double* [nVar];
  for (iVar = 0; iVar < nVar; iVar++)
    Mean_GradTurbVar[iVar] = new double [nDim];
  
}

CAvgGrad_TurbSA::~CAvgGrad_TurbSA(void) {
  unsigned short iVar;
  
  delete [] Edge_Vector;
  delete [] Proj_Mean_GradTurbVar_Kappa;
  delete [] Proj_Mean_GradTurbVar_Edge;
  for (iVar = 0; iVar < nVar; iVar++)
    delete [] Mean_GradTurbVar[iVar];
  delete [] Mean_GradTurbVar;
  
}

void CAvgGrad_TurbSA::ComputeResidual(double *val_residual, double **Jacobian_i, double **Jacobian_j, CConfig *config) {
  
    Density_i = V_i[nDim+2];            Density_j = V_j[nDim+2];
    Laminar_Viscosity_i = V_i[nDim+5];  Laminar_Viscosity_j = V_j[nDim+5];
    Eddy_Viscosity_i = V_i[nDim+6];     Eddy_Viscosity_j = V_j[nDim+6];
  
  /*--- Compute mean effective viscosity ---*/
  
  nu_i = Laminar_Viscosity_i/Density_i;
  nu_j = Laminar_Viscosity_j/Density_j;
  nu_e = 0.5*(nu_i+nu_j+TurbVar_i[0]+TurbVar_j[0]);
  
  /*--- Compute vector going from iPoint to jPoint ---*/
  
  dist_ij_2 = 0; proj_vector_ij = 0;
  for (iDim = 0; iDim < nDim; iDim++) {
    Edge_Vector[iDim] = Coord_j[iDim]-Coord_i[iDim];
    dist_ij_2 += Edge_Vector[iDim]*Edge_Vector[iDim];
    proj_vector_ij += Edge_Vector[iDim]*Normal[iDim];
  }
  if (dist_ij_2 == 0.0) proj_vector_ij = 0.0;
  else proj_vector_ij = proj_vector_ij/dist_ij_2;
  
  /*--- Mean gradient approximation ---*/
  
  for (iVar = 0; iVar < nVar; iVar++) {
    Proj_Mean_GradTurbVar_Kappa[iVar] = 0.0;
    Proj_Mean_GradTurbVar_Edge[iVar] = 0.0;
    for (iDim = 0; iDim < nDim; iDim++) {
      Mean_GradTurbVar[iVar][iDim] = 0.5*(TurbVar_Grad_i[iVar][iDim] + TurbVar_Grad_j[iVar][iDim]);
      Proj_Mean_GradTurbVar_Kappa[iVar] += Mean_GradTurbVar[iVar][iDim]*Normal[iDim];
    }
  }
  
  val_residual[0] = nu_e*Proj_Mean_GradTurbVar_Kappa[0]/sigma;
  
  /*--- For Jacobians -> Use of TSL approx. to compute derivatives of the gradients ---*/
  
  if (implicit) {
    Jacobian_i[0][0] = (0.5*Proj_Mean_GradTurbVar_Kappa[0]-nu_e*proj_vector_ij)/sigma;
    Jacobian_j[0][0] = (0.5*Proj_Mean_GradTurbVar_Kappa[0]+nu_e*proj_vector_ij)/sigma;
  }
  
}

CAvgGradCorrected_TurbSA::CAvgGradCorrected_TurbSA(unsigned short val_nDim, unsigned short val_nVar,
                                                   CConfig *config) : CNumerics(val_nDim, val_nVar, config) {
  unsigned short iVar;
  
  implicit        = (config->GetKind_TimeIntScheme_Turb() == EULER_IMPLICIT);
  
  sigma = 2./3.;
  
  Edge_Vector = new double [nDim];
  Proj_Mean_GradTurbVar_Kappa = new double [nVar];
  Proj_Mean_GradTurbVar_Edge = new double [nVar];
  Proj_Mean_GradTurbVar_Corrected = new double [nVar];
  Mean_GradTurbVar = new double* [nVar];
  for (iVar = 0; iVar < nVar; iVar++)
    Mean_GradTurbVar[iVar] = new double [nDim];
  
}

CAvgGradCorrected_TurbSA::~CAvgGradCorrected_TurbSA(void) {
  unsigned short iVar;
  
  delete [] Edge_Vector;
  delete [] Proj_Mean_GradTurbVar_Kappa;
  delete [] Proj_Mean_GradTurbVar_Edge;
  delete [] Proj_Mean_GradTurbVar_Corrected;
  for (iVar = 0; iVar < nVar; iVar++)
    delete [] Mean_GradTurbVar[iVar];
  delete [] Mean_GradTurbVar;
  
}

void CAvgGradCorrected_TurbSA::ComputeResidual(double *val_residual, double **Jacobian_i, double **Jacobian_j, CConfig *config) {
  
    Density_i = V_i[nDim+2];            Density_j = V_j[nDim+2];
    Laminar_Viscosity_i = V_i[nDim+5];  Laminar_Viscosity_j = V_j[nDim+5];
    Eddy_Viscosity_i = V_i[nDim+6];     Eddy_Viscosity_j = V_j[nDim+6];
  
  /*--- Compute mean effective viscosity ---*/
  
  nu_i = Laminar_Viscosity_i/Density_i;
  nu_j = Laminar_Viscosity_j/Density_j;
  nu_e = 0.5*(nu_i+nu_j+TurbVar_i[0]+TurbVar_j[0]);
  
  /*--- Compute vector going from iPoint to jPoint ---*/
  
  dist_ij_2 = 0; proj_vector_ij = 0;
  for (iDim = 0; iDim < nDim; iDim++) {
    Edge_Vector[iDim] = Coord_j[iDim]-Coord_i[iDim];
    dist_ij_2 += Edge_Vector[iDim]*Edge_Vector[iDim];
    proj_vector_ij += Edge_Vector[iDim]*Normal[iDim];
  }
  if (dist_ij_2 == 0.0) proj_vector_ij = 0.0;
  else proj_vector_ij = proj_vector_ij/dist_ij_2;
  
  /*--- Mean gradient approximation. Projection of the mean gradient
   in the direction of the edge ---*/
  
  for (iVar = 0; iVar < nVar; iVar++) {
    Proj_Mean_GradTurbVar_Kappa[iVar] = 0.0;
    Proj_Mean_GradTurbVar_Edge[iVar] = 0.0;
    for (iDim = 0; iDim < nDim; iDim++) {
      Mean_GradTurbVar[iVar][iDim] = 0.5*(TurbVar_Grad_i[iVar][iDim] + TurbVar_Grad_j[iVar][iDim]);
      Proj_Mean_GradTurbVar_Kappa[iVar] += Mean_GradTurbVar[iVar][iDim]*Normal[iDim];
      Proj_Mean_GradTurbVar_Edge[iVar] += Mean_GradTurbVar[iVar][iDim]*Edge_Vector[iDim];
    }
    Proj_Mean_GradTurbVar_Corrected[iVar] = Proj_Mean_GradTurbVar_Kappa[iVar];
    Proj_Mean_GradTurbVar_Corrected[iVar] -= Proj_Mean_GradTurbVar_Edge[iVar]*proj_vector_ij -
    (TurbVar_j[iVar]-TurbVar_i[iVar])*proj_vector_ij;
  }
  
  val_residual[0] = nu_e*Proj_Mean_GradTurbVar_Corrected[0]/sigma;
  
  /*--- For Jacobians -> Use of TSL approx. to compute derivatives of the gradients ---*/
  
  if (implicit) {
    Jacobian_i[0][0] = (0.5*Proj_Mean_GradTurbVar_Corrected[0]-nu_e*proj_vector_ij)/sigma;
    Jacobian_j[0][0] = (0.5*Proj_Mean_GradTurbVar_Corrected[0]+nu_e*proj_vector_ij)/sigma;
  }
  
}

CSourcePieceWise_TurbSA::CSourcePieceWise_TurbSA(unsigned short val_nDim, unsigned short val_nVar,
                                                 CConfig *config) : CNumerics(val_nDim, val_nVar, config) {
  
  transition = false; // Debugging, -AA
  
  /*--- Spalart-Allmaras closure constants ---*/
  cv1_3 = 357.911;
  k2 = 0.1681;
  cb1 = 0.1355;
  cw2 = 0.3;
  cw3_6 = 64.;
  sigma = 2./3.;
  cb2 = 0.622;
  cb2_sigma = cb2/sigma;
  cw1 = cb1/k2+(1+cb2)/sigma;
  
  /*--- LM transition model constants ---*/
  beta = 0.5;
  s1   = 2.0;
  
  nResidual = 4;
  nJacobian = 1;
  testResidual = new double[nResidual];
  testJacobian = new double[nJacobian];
  DUiDXj = new double*[nDim];
  for(int i=0; i < nDim; i++){
    DUiDXj[i] = new double[nDim];
  }
  DNuhatDXj = new double[nDim];
  
}

CSourcePieceWise_TurbSA::~CSourcePieceWise_TurbSA(void) {
  delete testResidual;
  delete testJacobian;
  for (int i=0; i < nDim; i++){
    delete DUiDXj[i];
  }
  delete DUiDXj;
  delete DNuhatDXj;
}

void CSourcePieceWise_TurbSA::ComputeResidual(double *val_residual, double **val_Jacobian_i, double **val_Jacobian_j, CConfig *config) {
  
    Density_i = V_i[nDim+2];
    Laminar_Viscosity_i = V_i[nDim+5];
  
  val_residual[0] = 0.0;
  Production = 0;
  Destruction = 0;
  CrossProduction = 0;
  val_Jacobian_i[0][0] = 0.0;
  
  /*--- Computation of vorticity ---*/
  Vorticity = (PrimVar_Grad_i[2][0]-PrimVar_Grad_i[1][1])*(PrimVar_Grad_i[2][0]-PrimVar_Grad_i[1][1]);
  if (nDim == 3) Vorticity += ( (PrimVar_Grad_i[3][1]-PrimVar_Grad_i[2][2])*(PrimVar_Grad_i[3][1]-PrimVar_Grad_i[2][2]) + (PrimVar_Grad_i[1][2]-PrimVar_Grad_i[3][0])*(PrimVar_Grad_i[1][2]-PrimVar_Grad_i[3][0]) );
  Omega = sqrt(Vorticity);
  
  if (dist_i > 1e-10) {
    
    /*--- Production term ---*/
    dist_i_2 = dist_i*dist_i;
    nu = Laminar_Viscosity_i/Density_i;
    Ji = TurbVar_i[0]/nu;
    Ji_2 = Ji*Ji;
    Ji_3 = Ji_2*Ji;
    fv1 = Ji_3/(Ji_3+cv1_3);
    fv2 = 1.0 - Ji/(1.0+Ji*fv1);
    S = Omega;
    inv_k2_d2 = 1.0/(k2*dist_i_2);
    
    Shat = S + TurbVar_i[0]*fv2*inv_k2_d2;
    inv_Shat = 1.0/max(Shat, 1.0e-10);
    
    /*--- Production term ---*/;
    if (!transition) Production = cb1*Shat*TurbVar_i[0]*Volume;
    else Production = cb1*Shat*TurbVar_i[0]*Volume*intermittency;
    
    /*--- Destruction term ---*/
    
    r = min(TurbVar_i[0]*inv_Shat*inv_k2_d2,10.0);
    g = r + cw2*(r*r*r*r*r*r-r);
    g_6 =	g*g*g*g*g*g;
    glim = pow((1.0+cw3_6)/(g_6+cw3_6),1.0/6.0);
    fw = g*glim;
    
    if (!transition) Destruction = cw1*fw*TurbVar_i[0]*TurbVar_i[0]/dist_i_2*Volume;
    else Destruction = cw1*fw*TurbVar_i[0]*TurbVar_i[0]/dist_i_2*Volume*min(max(intermittency,0.1),1.0);
    
    /*--- Diffusion term ---*/
    
    norm2_Grad = 0.0;
    for (iDim = 0; iDim < nDim; iDim++)
      norm2_Grad += TurbVar_Grad_i[0][iDim]*TurbVar_Grad_i[0][iDim];
    CrossProduction = cb2_sigma*norm2_Grad*Volume;
    
    val_residual[0] = Production - Destruction + CrossProduction;
    
    /*--- Implicit part ---*/
    
    /*--- Production term ---*/
    double Ji_3_cv1_3 = (Ji_3+cv1_3);
    dfv1 = 3.0*Ji_2*cv1_3/(nu*Ji_3_cv1_3*Ji_3_cv1_3);
    double Ji_fv1 = (1.+Ji*fv1);
    dfv2 = -(1/nu-Ji_2*dfv1)/(Ji_fv1*Ji_fv1);
    if ( Shat <= 1.0e-10 ) dShat = 0.0;
    else dShat = (fv2+TurbVar_i[0]*dfv2)*inv_k2_d2;
    val_Jacobian_i[0][0] += cb1*(TurbVar_i[0]*dShat+Shat)*Volume;
    
    /*--- Destruction term ---*/
    dr = (Shat-TurbVar_i[0]*dShat)*inv_Shat*inv_Shat*inv_k2_d2;
    if (r == 10.0) dr = 0.0;
    dg = dr*(1.+cw2*(6.*(r*r*r*r*r)-1.));
    dfw = dg*glim*(1.-g_6/(g_6+cw3_6));
    val_Jacobian_i[0][0] -= cw1*(dfw*TurbVar_i[0] +	2.*fw)*TurbVar_i[0]/dist_i_2*Volume;
  }

}

CUpwSca_TurbSST::CUpwSca_TurbSST(unsigned short val_nDim, unsigned short val_nVar,
                                 CConfig *config) : CNumerics(val_nDim, val_nVar, config) {
  
  implicit        = (config->GetKind_TimeIntScheme_Turb() == EULER_IMPLICIT);
  grid_movement   = config->GetGrid_Movement();
  
  Velocity_i = new double [nDim];
  Velocity_j = new double [nDim];
  
}

CUpwSca_TurbSST::~CUpwSca_TurbSST(void) {
  
  delete [] Velocity_i;
  delete [] Velocity_j;
  
}

void CUpwSca_TurbSST::ComputeResidual(double *val_residual, double **val_Jacobian_i, double **val_Jacobian_j, CConfig *config) {
  
    Density_i = V_i[nDim+2];
    Density_j = V_j[nDim+2];
  
  q_ij = 0.0;
  if (grid_movement) {
    for (iDim = 0; iDim < nDim; iDim++) {
      Velocity_i[iDim] = V_i[iDim+1] - GridVel_i[iDim];
      Velocity_j[iDim] = V_j[iDim+1] - GridVel_j[iDim];
      q_ij += 0.5*(Velocity_i[iDim]+Velocity_j[iDim])*Normal[iDim];
    }
  }
  else {
    for (iDim = 0; iDim < nDim; iDim++) {
      Velocity_i[iDim] = V_i[iDim+1];
      Velocity_j[iDim] = V_j[iDim+1];
      q_ij += 0.5*(Velocity_i[iDim]+Velocity_j[iDim])*Normal[iDim];
    }
  }
  
  a0 = 0.5*(q_ij+fabs(q_ij));
  a1 = 0.5*(q_ij-fabs(q_ij));
  
  val_residual[0] = a0*Density_i*TurbVar_i[0]+a1*Density_j*TurbVar_j[0];
  val_residual[1] = a0*Density_i*TurbVar_i[1]+a1*Density_j*TurbVar_j[1];
  
  if (implicit) {
    val_Jacobian_i[0][0] = a0;		val_Jacobian_i[0][1] = 0.0;
    val_Jacobian_i[1][0] = 0.0;		val_Jacobian_i[1][1] = a0;
    
    val_Jacobian_j[0][0] = a1;		val_Jacobian_j[0][1] = 0.0;
    val_Jacobian_j[1][0] = 0.0;		val_Jacobian_j[1][1] = a1;
  }
  
}

CAvgGrad_TurbSST::CAvgGrad_TurbSST(unsigned short val_nDim, unsigned short val_nVar, double *constants, CConfig *config) : CNumerics(val_nDim, val_nVar, config) {
  
  unsigned short iVar;
  
  implicit = (config->GetKind_TimeIntScheme_Turb() == EULER_IMPLICIT);
  
  sigma_k1  = constants[0];
  sigma_om1 = constants[2];
  sigma_k2  = constants[1];
  sigma_om2 = constants[3];
  
  Edge_Vector = new double [nDim];
  Proj_Mean_GradTurbVar_Normal = new double [nVar];
  Proj_Mean_GradTurbVar_Edge = new double [nVar];
  Proj_Mean_GradTurbVar_Corrected = new double [nVar];
  Mean_GradTurbVar = new double* [nVar];
  for (iVar = 0; iVar < nVar; iVar++)
    Mean_GradTurbVar[iVar] = new double [nDim];
  
}

CAvgGrad_TurbSST::~CAvgGrad_TurbSST(void) {
  
  unsigned short iVar;
  
  delete [] Edge_Vector;
  delete [] Proj_Mean_GradTurbVar_Normal;
  delete [] Proj_Mean_GradTurbVar_Edge;
  delete [] Proj_Mean_GradTurbVar_Corrected;
  for (iVar = 0; iVar < nVar; iVar++)
    delete [] Mean_GradTurbVar[iVar];
  delete [] Mean_GradTurbVar;
  
}

void CAvgGrad_TurbSST::ComputeResidual(double *val_residual, double **Jacobian_i, double **Jacobian_j, CConfig *config) {
  
  double sigma_kine_i, sigma_kine_j, sigma_omega_i, sigma_omega_j;
  double diff_i_kine, diff_i_omega, diff_j_kine, diff_j_omega;
  
    Density_i = V_i[nDim+2];            Density_j = V_j[nDim+2];
    Laminar_Viscosity_i = V_i[nDim+5];  Laminar_Viscosity_j = V_j[nDim+5];
    Eddy_Viscosity_i = V_i[nDim+6];     Eddy_Viscosity_j = V_j[nDim+6];
  
  /*--- Compute the blended constant for the viscous terms ---*/
  sigma_kine_i  = F1_i*sigma_k1 + (1.0 - F1_i)*sigma_k2;
  sigma_kine_j  = F1_j*sigma_k1 + (1.0 - F1_j)*sigma_k2;
  sigma_omega_i = F1_i*sigma_om1 + (1.0 - F1_i)*sigma_om2;
  sigma_omega_j = F1_j*sigma_om1 + (1.0 - F1_j)*sigma_om2;
  
  /*--- Compute mean effective viscosity ---*/
  diff_i_kine  = Laminar_Viscosity_i + sigma_kine_i*Eddy_Viscosity_i;
  diff_j_kine  = Laminar_Viscosity_j + sigma_kine_j*Eddy_Viscosity_j;
  diff_i_omega = Laminar_Viscosity_i + sigma_omega_i*Eddy_Viscosity_i;
  diff_j_omega = Laminar_Viscosity_j + sigma_omega_j*Eddy_Viscosity_j;
  
  diff_kine  = 0.5*(diff_i_kine + diff_j_kine);    // Could instead use weighted average!
  diff_omega = 0.5*(diff_i_omega + diff_j_omega);
  
  /*--- Compute vector going from iPoint to jPoint ---*/
  dist_ij_2 = 0; proj_vector_ij = 0;
  for (iDim = 0; iDim < nDim; iDim++) {
    Edge_Vector[iDim] = Coord_j[iDim]-Coord_i[iDim];
    dist_ij_2 += Edge_Vector[iDim]*Edge_Vector[iDim];
    proj_vector_ij += Edge_Vector[iDim]*Normal[iDim];
  }
  if (dist_ij_2 == 0.0) proj_vector_ij = 0.0;
  else proj_vector_ij = proj_vector_ij/dist_ij_2;
  
  /*--- Mean gradient approximation. Projection of the mean gradient in the direction of the edge ---*/
  for (iVar = 0; iVar < nVar; iVar++) {
    Proj_Mean_GradTurbVar_Normal[iVar] = 0.0;
    Proj_Mean_GradTurbVar_Edge[iVar] = 0.0;
    for (iDim = 0; iDim < nDim; iDim++) {
      Mean_GradTurbVar[iVar][iDim] = 0.5*(TurbVar_Grad_i[iVar][iDim] + TurbVar_Grad_j[iVar][iDim]);
      Proj_Mean_GradTurbVar_Normal[iVar] += Mean_GradTurbVar[iVar][iDim]*Normal[iDim];
    }
    Proj_Mean_GradTurbVar_Corrected[iVar] = Proj_Mean_GradTurbVar_Normal[iVar];
  }
  
  val_residual[0] = diff_kine*Proj_Mean_GradTurbVar_Corrected[0];
  val_residual[1] = diff_omega*Proj_Mean_GradTurbVar_Corrected[1];
  
  /*--- For Jacobians -> Use of TSL approx. to compute derivatives of the gradients ---*/
  if (implicit) {
    Jacobian_i[0][0] = -diff_kine*proj_vector_ij/Density_i;		Jacobian_i[0][1] = 0.0;
    Jacobian_i[1][0] = 0.0;									    Jacobian_i[1][1] = -diff_omega*proj_vector_ij/Density_i;
    
    Jacobian_j[0][0] = diff_kine*proj_vector_ij/Density_j; 		Jacobian_j[0][1] = 0.0;
    Jacobian_j[1][0] = 0.0;									    Jacobian_j[1][1] = diff_omega*proj_vector_ij/Density_j;
  }
  
}


CAvgGradCorrected_TurbSST::CAvgGradCorrected_TurbSST(unsigned short val_nDim, unsigned short val_nVar, double *constants, CConfig *config) : CNumerics(val_nDim, val_nVar, config) {
  
  unsigned short iVar;
  
  implicit = (config->GetKind_TimeIntScheme_Turb() == EULER_IMPLICIT);
  
  sigma_k1  = constants[0];
  sigma_om1 = constants[2];
  sigma_k2  = constants[1];
  sigma_om2 = constants[3];
  
  Edge_Vector = new double [nDim];
  Proj_Mean_GradTurbVar_Normal = new double [nVar];
  Proj_Mean_GradTurbVar_Edge = new double [nVar];
  Proj_Mean_GradTurbVar_Corrected = new double [nVar];
  Mean_GradTurbVar = new double* [nVar];
  for (iVar = 0; iVar < nVar; iVar++)
    Mean_GradTurbVar[iVar] = new double [nDim];
  
}

CAvgGradCorrected_TurbSST::~CAvgGradCorrected_TurbSST(void) {
  
  unsigned short iVar;
  
  delete [] Edge_Vector;
  delete [] Proj_Mean_GradTurbVar_Normal;
  delete [] Proj_Mean_GradTurbVar_Edge;
  delete [] Proj_Mean_GradTurbVar_Corrected;
  for (iVar = 0; iVar < nVar; iVar++)
    delete [] Mean_GradTurbVar[iVar];
  delete [] Mean_GradTurbVar;
  
}

void CAvgGradCorrected_TurbSST::ComputeResidual(double *val_residual, double **Jacobian_i, double **Jacobian_j, CConfig *config) {
  
  double sigma_kine_i, sigma_kine_j, sigma_omega_i, sigma_omega_j;
  double diff_i_kine, diff_i_omega, diff_j_kine, diff_j_omega;
  
    Density_i = V_i[nDim+2];            Density_j = V_j[nDim+2];
    Laminar_Viscosity_i = V_i[nDim+5];  Laminar_Viscosity_j = V_j[nDim+5];
    Eddy_Viscosity_i = V_i[nDim+6];     Eddy_Viscosity_j = V_j[nDim+6];
  
  /*--- Compute the blended constant for the viscous terms ---*/
  sigma_kine_i  = F1_i*sigma_k1 + (1.0 - F1_i)*sigma_k2;
  sigma_kine_j  = F1_j*sigma_k1 + (1.0 - F1_j)*sigma_k2;
  sigma_omega_i = F1_i*sigma_om1 + (1.0 - F1_i)*sigma_om2;
  sigma_omega_j = F1_j*sigma_om1 + (1.0 - F1_j)*sigma_om2;
  
  /*--- Compute mean effective viscosity ---*/
  diff_i_kine  = Laminar_Viscosity_i + sigma_kine_i*Eddy_Viscosity_i;
  diff_j_kine  = Laminar_Viscosity_j + sigma_kine_j*Eddy_Viscosity_j;
  diff_i_omega = Laminar_Viscosity_i + sigma_omega_i*Eddy_Viscosity_i;
  diff_j_omega = Laminar_Viscosity_j + sigma_omega_j*Eddy_Viscosity_j;
  
  diff_kine  = 0.5*(diff_i_kine + diff_j_kine);    // Could instead use weighted average!
  diff_omega = 0.5*(diff_i_omega + diff_j_omega);
  
  /*--- Compute vector going from iPoint to jPoint ---*/
  dist_ij_2 = 0.0; proj_vector_ij = 0.0;
  for (iDim = 0; iDim < nDim; iDim++) {
    Edge_Vector[iDim] = Coord_j[iDim]-Coord_i[iDim];
    dist_ij_2 += Edge_Vector[iDim]*Edge_Vector[iDim];
    proj_vector_ij += Edge_Vector[iDim]*Normal[iDim];
  }
  if (dist_ij_2 == 0.0) proj_vector_ij = 0.0;
  else proj_vector_ij = proj_vector_ij/dist_ij_2;
  
  /*--- Mean gradient approximation. Projection of the mean gradient in the direction of the edge ---*/
  for (iVar = 0; iVar < nVar; iVar++) {
    Proj_Mean_GradTurbVar_Normal[iVar] = 0.0;
    Proj_Mean_GradTurbVar_Edge[iVar] = 0.0;
    for (iDim = 0; iDim < nDim; iDim++) {
      Mean_GradTurbVar[iVar][iDim] = 0.5*(TurbVar_Grad_i[iVar][iDim] + TurbVar_Grad_j[iVar][iDim]);
      Proj_Mean_GradTurbVar_Normal[iVar] += Mean_GradTurbVar[iVar][iDim]*Normal[iDim];
      Proj_Mean_GradTurbVar_Edge[iVar] += Mean_GradTurbVar[iVar][iDim]*Edge_Vector[iDim];
    }
    Proj_Mean_GradTurbVar_Corrected[iVar] = Proj_Mean_GradTurbVar_Normal[iVar];
    Proj_Mean_GradTurbVar_Corrected[iVar] -= Proj_Mean_GradTurbVar_Edge[iVar]*proj_vector_ij -
    (TurbVar_j[iVar]-TurbVar_i[iVar])*proj_vector_ij;
  }
  
  val_residual[0] = diff_kine*Proj_Mean_GradTurbVar_Corrected[0];
  val_residual[1] = diff_omega*Proj_Mean_GradTurbVar_Corrected[1];
  
  /*--- For Jacobians -> Use of TSL approx. to compute derivatives of the gradients ---*/
  if (implicit) {
    Jacobian_i[0][0] = -diff_kine*proj_vector_ij/Density_i;		Jacobian_i[0][1] = 0.0;
    Jacobian_i[1][0] = 0.0;									    Jacobian_i[1][1] = -diff_omega*proj_vector_ij/Density_i;
    
    Jacobian_j[0][0] = diff_kine*proj_vector_ij/Density_j; 		Jacobian_j[0][1] = 0.0;
    Jacobian_j[1][0] = 0.0;									    Jacobian_j[1][1] = diff_omega*proj_vector_ij/Density_j;
  }
  
}

CSourcePieceWise_TurbSST::CSourcePieceWise_TurbSST(unsigned short val_nDim, unsigned short val_nVar, double *constants,
                                                   CConfig *config) : CNumerics(val_nDim, val_nVar, config) {
  
  /*--- Closure constants ---*/
  beta_star     = constants[6];
  sigma_omega_1 = constants[2];
  sigma_omega_2 = constants[3];
  beta_1        = constants[4];
  beta_2        = constants[5];
  alfa_1        = constants[8];
  alfa_2        = constants[9];
  a1            = constants[7];
}

CSourcePieceWise_TurbSST::~CSourcePieceWise_TurbSST(void) { }

void CSourcePieceWise_TurbSST::ComputeResidual(double *val_residual, double **val_Jacobian_i, double **val_Jacobian_j, CConfig *config) {
  
  unsigned short iDim;
  double alfa_blended, beta_blended;
  double diverg, pk, pw, zeta;
  
    Density_i = V_i[nDim+2];
    Laminar_Viscosity_i = V_i[nDim+5];
    Eddy_Viscosity_i = V_i[nDim+6];
  
  val_residual[0] = 0.0;        val_residual[1] = 0.0;
  val_Jacobian_i[0][0] = 0.0;		val_Jacobian_i[0][1] = 0.0;
  val_Jacobian_i[1][0] = 0.0;		val_Jacobian_i[1][1] = 0.0;
  
  /*--- Computation of blended constants for the source terms---*/
  
  alfa_blended = F1_i*alfa_1 + (1.0 - F1_i)*alfa_2;
  beta_blended = F1_i*beta_1 + (1.0 - F1_i)*beta_2;
  
  if (dist_i > 1e-10) {
    
    /*--- Production ---*/
    
    diverg = 0.0;
    for (iDim = 0; iDim < nDim; iDim++)
      diverg += PrimVar_Grad_i[iDim+1][iDim];
    
    pk = Eddy_Viscosity_i*StrainMag*StrainMag - 2.0/3.0*Density_i*TurbVar_i[0]*diverg;
    pk = min(pk,20.0*beta_star*Density_i*TurbVar_i[1]*TurbVar_i[0]);
    pk = max(pk,0.0);
    
    zeta = max(TurbVar_i[1],StrainMag*F2_i/a1);
    pw = StrainMag*StrainMag - 2.0/3.0*zeta*diverg;
    pw = max(pw,0.0);
    
    val_residual[0] += pk*Volume;
    val_residual[1] += alfa_blended*Density_i*pw*Volume;
    
    /*--- Dissipation ---*/
    
    val_residual[0] -= beta_star*Density_i*TurbVar_i[1]*TurbVar_i[0]*Volume;
    val_residual[1] -= beta_blended*Density_i*TurbVar_i[1]*TurbVar_i[1]*Volume;
    
    /*--- Cross diffusion ---*/
    
    val_residual[1] += (1.0 - F1_i)*CDkw*Volume;
    
    /*--- Implicit part ---*/
    
    val_Jacobian_i[0][0] = -beta_star*TurbVar_i[1]*Volume;		val_Jacobian_i[0][1] = 0.0;
    val_Jacobian_i[1][0] = 0.0;                               val_Jacobian_i[1][1] = -2.0*beta_blended*TurbVar_i[1]*Volume;
  }
  
}
