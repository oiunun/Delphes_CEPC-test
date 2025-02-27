/*
 *  Delphes: a framework for fast simulation of a generic collider experiment
 *  Copyright (C) 2012-2014  Universite catholique de Louvain (UCL), Belgium
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/** \class IdentificationMap
 *
 *  Converts particles with some PDG code into another particle,
 *  according to parametrized probability.
 *
 *  \author M. Selvaggi - UCL, Louvain-la-Neuve
 *
 */

#include "modules/IdentificationMap.h"

#include "classes/DelphesClasses.h"
#include "classes/DelphesFactory.h"
#include "classes/DelphesFormula.h"

#include "ExRootAnalysis/ExRootClassifier.h"
#include "ExRootAnalysis/ExRootFilter.h"
#include "ExRootAnalysis/ExRootResult.h"

//------------------------------Add----------
#include "TrackCovariance/TrkUtil.h"
//-------------------------------------------
#include "TDatabasePDG.h"
#include "TFormula.h"
#include "TLorentzVector.h"
#include "TMath.h"
#include "TObjArray.h"
#include "TRandom3.h"
#include "TString.h"

#include <algorithm>
#include <iostream>
#include <sstream>
#include <stdexcept>

using namespace std;

//------------------------------------------------------------------------------

IdentificationMap::IdentificationMap() :
  fItInputArray(0)
{
}

//------------------------------------------------------------------------------

IdentificationMap::~IdentificationMap()
{
}
//----------------counting effciency fomular
Double_t IdentificationMap::Eff(Double_t bg, Double_t CosTheta){
  Double_t eff;
  Double_t E;
  Double_t Opt = 0;
  E = TrkUtil::Nclusters(bg,Opt)*0.01*(-0.007309)/TMath::Sqrt(1-pow(CosTheta,2)) + 1.245497;
  eff =gRandom->Gaus(E,0.02);
  return eff;
}


//------------------------------------------------------------------------------

void IdentificationMap::Init()
{
  TMisIDMap::iterator itEfficiencyMap;
  ExRootConfParam param;
  DelphesFormula *formula;
  Int_t i, size, pdg;

  // read efficiency formulas
  param = GetParam("EfficiencyFormula");
  size = param.GetSize();

  fEfficiencyMap.clear();
  for(i = 0; i < size / 3; ++i)
  {
    formula = new DelphesFormula;
    formula->Compile(param[i * 3 + 2].GetString());
    pdg = param[i * 3].GetInt();
    fEfficiencyMap.insert(make_pair(pdg, make_pair(param[i * 3 + 1].GetInt(), formula)));
  }

  // set default efficiency formula
  itEfficiencyMap = fEfficiencyMap.find(0);
  if(itEfficiencyMap == fEfficiencyMap.end())
  {
    formula = new DelphesFormula;
    formula->Compile("1.0");

    fEfficiencyMap.insert(make_pair(0, make_pair(0, formula)));
  }

  // import input array

  fInputArray = ImportArray(GetString("InputArray", "ParticlePropagator/stableParticles"));
  fItInputArray = fInputArray->MakeIterator();

  // create output array

  fOutputArray = ExportArray(GetString("OutputArray", "stableParticles"));
}

//------------------------------------------------------------------------------

void IdentificationMap::Finish()
{
  if(fItInputArray) delete fItInputArray;

  TMisIDMap::iterator itEfficiencyMap;
  DelphesFormula *formula;
  for(itEfficiencyMap = fEfficiencyMap.begin(); itEfficiencyMap != fEfficiencyMap.end(); ++itEfficiencyMap)
  {
    formula = (itEfficiencyMap->second).second;
    if(formula) delete formula;
  }
}

//------------------------------------------------------------------------------

void IdentificationMap::Process()
{
  Candidate *candidate,*particle;
  Double_t pt, eta, phi, e ,CosTheta;
  TMisIDMap::iterator itEfficiencyMap;
  pair<TMisIDMap::iterator, TMisIDMap::iterator> range;
  DelphesFormula *formula;
  Int_t pdgCodeIn, pdgCodeOut, charge;

  Double_t p, r, total;

  fItInputArray->Reset();
  while((candidate = static_cast<Candidate *>(fItInputArray->Next())))
  {
    particle = static_cast<Candidate *>(candidate->GetCandidates()->At(0));
    const TLorentzVector &candidatePosition = candidate->Position;
    const TLorentzVector &candidateMomentum = candidate->Momentum;
    const TLorentzVector &particleMomentum = particle->Momentum;
    eta = candidatePosition.Eta();
    phi = candidatePosition.Phi();
    pt = candidateMomentum.Pt();
    e = candidateMomentum.E();
    CosTheta = particleMomentum.CosTheta();

    pdgCodeIn = candidate->PID;
    charge = candidate->Charge;

    // first check that PID of this particle is specified in the map
    // otherwise, look for PID = 0

    itEfficiencyMap = fEfficiencyMap.find(pdgCodeIn);

    range = fEfficiencyMap.equal_range(pdgCodeIn);
    if(range.first == range.second) range = fEfficiencyMap.equal_range(-pdgCodeIn);
    if(range.first == range.second) range = fEfficiencyMap.equal_range(0);

    r = gRandom->Uniform();
    total = 0.0;
    
    //---------------------------------Add---------------  
  //---------------Set parameters
    const Double_t c_light = 2.99792458E8;
    Double_t mass[5]={0.000511, 0.10565, 0.13957, 0.49368, 0.93827};//e u pi k p  GeV
    Int_t PID[5]={11,13,211,321,2212};

  //---------------Set variable
    Double_t chi[2] = {0,0};
    Double_t total_chi2 = 0;
    Double_t probability = 0.0;
    Double_t Prob[5]={0.0,0.0,0.0,0.0,0.0};

  //---------------Get measured value
    Double_t p_meas = particleMomentum.P();
    Double_t dndx_meas = candidate->Nclusters ;
    Int_t charge = candidate->Charge;
    Double_t tof_meas = candidate->TOF; //s

    //-------------------Get parameters
    Double_t l = candidate->L * 1.0E-3;//m
    Double_t L_DC = candidate->L_DC;//m
    //gas proportion 
    int Opt = 0; 
    if(pdgCodeIn==211||pdgCodeIn==-211||pdgCodeIn==321||pdgCodeIn==-321||pdgCodeIn==2212||pdgCodeIn==-2212){
     if(dndx_meas!=0 && l>0 && L_DC>0/* &&  L_DC<10  && p_meas>5*/){
      for(int i=2;i<5;i++){
  
       Double_t bg = p_meas/mass[i]; //TMath::Sqrt(p_meas/TMath::Sqrt(mass[i]*mass[i]+p_meas*p_meas));
       //-------------------Get exp
       Double_t eff = Eff(bg,CosTheta);
       Double_t dndx_exp = TrkUtil::Nclusters(bg,Opt)*L_DC*eff  ;
     
      
       Double_t tof_exp = l*TMath::Sqrt(mass[i]*mass[i]+p_meas*p_meas)/(c_light*p_meas); //s
       if(dndx_exp<=0) break;
    //-------------------sigma
      Double_t counting_sigma = 0.02;
      Double_t tof_sigma = 30E-12;
      Double_t dndx_sigma = TMath::Sqrt(dndx_exp*eff )/*dNdxSigma(dndx_exp, TMath::Sqrt(dndx_exp), counting_sigma, L_DC, CosTheta)*/;/*TMath::Sqrt(dndx_exp/l);*/
    //-------------------chi-square
      chi[0] =  (dndx_meas - dndx_exp)/dndx_sigma ;
      chi[1] =  (tof_meas - tof_exp)/tof_sigma ;//(30E-12);//s
      total_chi2 = chi[0]*chi[0] + chi[1]*chi[1];
      Prob[i] = TMath::Prob(total_chi2,2);   
      if(i==2){
        candidate->Chi_pi = total_chi2;
      }
      if(i==3){
        candidate->Chi_k = total_chi2;
      }
    }
    if(Prob[2] ==0 && Prob[3] == 0 && Prob[4] == 0) {
      candidate->PID_meas = -1;
    }
    else{
      Double_t probability_tot =Prob[2]+Prob[3]+Prob[4];
      candidate->Prob_Pi = Prob[2]/probability_tot;
      candidate->Prob_K = Prob[3]/probability_tot;
      candidate->Prob_P = Prob[4]/probability_tot;

      // if(Prob[0]>Prob[1]&&Prob[0]>Prob[2]&&Prob[0]>Prob[3]&&Prob[0]>Prob[4]   && Prob[0]>0.001){
      //   candidate->PID_meas = charge * PID[0]; 
      //   probability = Prob[0];
      // }
      // if(Prob[1]>Prob[0]&&Prob[1]>Prob[2]&&Prob[1]>Prob[3]&&Prob[1]>Prob[4]   && Prob[1]>0.001){
      //   candidate->PID_meas = charge * PID[1]; 
      //   probability = Prob[1];
      // }
      if(/*Prob[2]>Prob[0]&&Prob[2]>Prob[1]&&*/ Prob[2]>Prob[3]  && Prob[2]>Prob[4]  /* &&  Prob[2]>0.001  */ ){
        candidate->PID_meas = charge * PID[2]; 
      }
      if(/*Prob[3]>Prob[0]&&Prob[3]>Prob[1]&&*/ Prob[3]>Prob[2]  && Prob[3]>Prob[4]  /* &&  Prob[3]>0.001 */  ){
        candidate->PID_meas = charge * PID[3]; 
      }
      if(/*Prob[4]>Prob[4]&&Prob[4]>Prob[1]&&*/ Prob[4]>Prob[2] && Prob[4]>Prob[3]  /* &&  Prob[4]>0.001 */ ){
        candidate->PID_meas = charge * PID[4]; 
      }
     }
    }
    else{
      candidate->PID_meas = -1; 
    }
   }

    // loop over sub-map for this PID
    for(TMisIDMap::iterator it = range.first; it != range.second; ++it)
    {
      formula = (it->second).second;
      pdgCodeOut = (it->second).first;

      p = formula->Eval(pt, eta, phi, e);

      if(total <= r && r < total + p)
      {
        // change PID of particle
        candidate = static_cast<Candidate *>(candidate->Clone());
        if(pdgCodeOut != 0) candidate->PID = charge * pdgCodeOut;
        fOutputArray->Add(candidate);
        break;
      }

      total += p;
    }
  }
}

