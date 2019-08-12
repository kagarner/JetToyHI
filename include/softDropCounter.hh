#ifndef softDropCounter_h
#define softDropCounter_h

#include <iostream>
#include <vector>
#include <string>
#include <algorithm>
#include <fstream>

#include "fastjet/PseudoJet.hh"
#include "fastjet/ClusterSequenceArea.hh"
#include "fastjet/contrib/SoftDrop.hh"
#include "fastjet/contrib/Recluster.hh"

#include "jetCollection.hh"
#include "jewelMatcher.hh"

//---------------------------------------------------------------
// Description
// This class runs iterative SoftDrop on a set of jets
// Author: Y. Chen
//---------------------------------------------------------------

class softDropCounter
{
private :
  int fRecursiveAlgo_; //clustering algo to be used for the declustering
  double zcut_;        // ZCut used in SD
  double beta_;        // Beta parameter
  double r0_;          // Jet radius
  double rcut_;        // Termination Criteria

  bool doJewelSub_;                                 // do JEWEL 4MomSub
  std::vector<fastjet::PseudoJet> particlesDummy_;  // dummy particles for 4MomSub

  std::vector<fastjet::PseudoJet>  fjInputs_;    // ungroomed jets
  std::vector<std::vector<double>> zgs_;         // all the zg's in the algorithm
  std::vector<std::vector<double>> drs_;         // and the angles in the algorithm
  std::vector<std::vector<double>> pts_;         // pts of each node
  std::vector<std::vector<double>> erads_;       // energy of sum of the two branches at each node
  std::vector<std::vector<double>> log1drs_;     // Log(1/angle) in the algorithm
  std::vector<std::vector<double>> logzdrs_;     // Log(z*angle) in the algorithm
  std::vector<std::vector<double>> logkt_;       // Log(p*angle) in the algorithm
  std::vector<std::vector<double>> tf_;          // tf=2*omega/kt^2 (in fm)

  TH2D* h2DProfileHisto;

public :
  softDropCounter(double z = 0.1, double beta = 0.0, double r0 = 0.4, double rcut = 0.1);
  ~softDropCounter();
  void setZCut(double c);
  void setBeta(double b);
  void setR0(double r) ;
  void setRCut(double r);
  void setInputJets(const std::vector<fastjet::PseudoJet> &v);

  void setRecursiveAlgo(int r);

  void setJewelSubFlag(bool b);
  void doJewelSub(std::vector<fastjet::PseudoJet> dummies);

  std::vector<std::vector<double>> getZgs() const { return zgs_; }
  std::vector<std::vector<double>> getDRs() const { return drs_; }
  std::vector<std::vector<double>> getPts() const { return pts_; }
  std::vector<std::vector<double>> getErads() const { return erads_; }
  std::vector<std::vector<double>> getLog1DRs() const { return log1drs_; }
  std::vector<std::vector<double>> getLogzDRs() const { return logzdrs_; }
  std::vector<std::vector<double>> getLogkt() const { return logkt_; }
  std::vector<std::vector<double>> getTf() const { return tf_; }

  TH2D* getProfileHist(){return h2DProfileHisto;}
  void delProfileHist(){delete h2DProfileHisto; h2DProfileHisto=NULL;}

  void run(const jetCollection &c);
  void run(const std::vector<fastjet::PseudoJet> &v);
  void run();
  std::vector<double> calculateNSD(double Kappa, double AngleKappa = 0);
};

softDropCounter::softDropCounter(double z, double beta, double r0, double rcut)
   : zcut_(z), beta_(beta), r0_(r0), rcut_(rcut)
{
  fRecursiveAlgo_ = 0;
  doJewelSub_     = false;
  h2DProfileHisto = new TH2D("h2DProfileHisto","h2DProfileHisto",500,0,500,20,0,0.5);
}

softDropCounter::~softDropCounter(){
  if(h2DProfileHisto!=NULL)delete h2DProfileHisto;
}

void softDropCounter::setZCut(double c)
{
   zcut_ = c;
}

void softDropCounter::setBeta(double b)
{
   beta_ = b;
}

void softDropCounter::setR0(double r)
{
   r0_   = r;
}

void softDropCounter::setRCut(double r)
{
   rcut_ = r;
}

void softDropCounter::setInputJets(const std::vector<fastjet::PseudoJet> &v)
{
   fjInputs_ = v;
}

void softDropCounter::setRecursiveAlgo(int r)
{
  fRecursiveAlgo_ = r;
}

void softDropCounter::setJewelSubFlag(bool b)
{
  doJewelSub_ = b;
}

void softDropCounter::doJewelSub(std::vector<fastjet::PseudoJet> dummies) {
  setJewelSubFlag(true);
  particlesDummy_ = dummies;
}


void softDropCounter::run(const jetCollection &c)
{
   run(c.getJet());
}

void softDropCounter::run(const std::vector<fastjet::PseudoJet> &v)
{
   setInputJets(v);
   run();
}

void softDropCounter::run()
{
  for(fastjet::PseudoJet &jet: fjInputs_)
   {
      //printf("New Jet *******************************\n");
      if(jet.has_constituents() == false)
      {
         zgs_.push_back(vector<double>());
         drs_.push_back(vector<double>());
         pts_.push_back(vector<double>());
         erads_.push_back(vector<double>());
         log1drs_.push_back(vector<double>());
         logzdrs_.push_back(vector<double>());
         logkt_.push_back(vector<double>());
         tf_.push_back(vector<double>());
         continue;
      }

      std::vector<fastjet::PseudoJet> particles, ghosts;

      fastjet::SelectorIsPureGhost().sift(jet.constituents(), ghosts, particles);
      //need flexibility here on how we order the tree before declustering
      fastjet::JetAlgorithm jetalgo(fastjet::genkt_algorithm);
      float kpval=0;
      if(fRecursiveAlgo_==0){
        //CA
        kpval=0;
      }
      if(fRecursiveAlgo_==1){
        //kT
        kpval=-1;
      } 
      if(fRecursiveAlgo_==2){ 
        //antikt
        kpval=1;
      }
      if(fRecursiveAlgo_==3){ 
        //tform-ordered
        kpval=0.5;
      }

      fastjet::JetDefinition jet_def(jetalgo, fastjet::JetDefinition::max_allowable_R, kpval, static_cast<fastjet::RecombinationScheme>(0));
      fastjet::ClusterSequence cs(particles, jet_def);
      std::vector<fastjet::PseudoJet> tempJets = fastjet::sorted_by_pt(cs.inclusive_jets());

      if(tempJets.size() == 0)
      {
         zgs_.push_back(vector<double>());
         drs_.push_back(vector<double>());
         pts_.push_back(vector<double>());
         erads_.push_back(vector<double>());
         log1drs_.push_back(vector<double>());
         logzdrs_.push_back(vector<double>());
         logkt_.push_back(vector<double>());
         tf_.push_back(vector<double>());
         continue;
      }

      fastjet::PseudoJet CurrentJet = tempJets[0];
      fastjet::PseudoJet Part1, Part2;

      std::vector<double> z;
      std::vector<double> dr;
      std::vector<double> pt;
      std::vector<double> erad;
      std::vector<double> log1dr;
      std::vector<double> logzdr;
      std::vector<double> logkt;
      std::vector<double> tf;


      int iSplit=0;
      while(CurrentJet.has_parents(Part1, Part2))
      {
         if(CurrentJet.pt2() <= 0)
            break;

         fastjet::PseudoJet sj1;
         fastjet::PseudoJet sj2;
         if(doJewelSub_) {
           sj1 = GetCorrectedJet(Part1,particlesDummy_);
           sj2 = GetCorrectedJet(Part2,particlesDummy_);
         } else  {
           sj1 = Part1;
           sj2 = Part2;
         }

         double DeltaR = std::sqrt(sj1.squared_distance(sj2));
         if(DeltaR < rcut_)
            break;

         double PT1 = sj1.pt();
         double PT2 = sj2.pt();
         double zg = -1;

         if(PT1 + PT2 > 0)
            zg = min(PT1, PT2) / (PT1 + PT2);
         else
            break;

         double Threshold = zcut_ * std::pow(DeltaR / r0_, beta_);

         if(zg >= Threshold)   // yay
         {
            if(iSplit==0){
                h2DProfileHisto->Fill(jet.pt(),DeltaR);
            }
            z.push_back(zg);
            dr.push_back(DeltaR);
            pt.push_back(CurrentJet.perp());
            erad.push_back(sj1.e()+sj2.e());
            log1dr.push_back(log(1./DeltaR));
            logzdr.push_back(log(zg*DeltaR));
            logkt.push_back(log(PT2*DeltaR));
            double omega=zg*(sj1.e()+sj2.e());
            tf.push_back(2*omega*0.197327053/((sj2.perp()*DeltaR)*(sj2.perp()*DeltaR)));  //multiplication with hbar c to get right units
            //printf("z=%f, dr=%f, erad=%f, log1dr?%f, logtf=%f, logkt=%f, omega=%f, sj2perp=%f=\n",zg, DeltaR, erad.back(), log1dr.back(),tf.back(), logkt.back(), omega, sj2.perp());
            iSplit++;
         }

         if(PT1 > PT2)
            CurrentJet = Part1;
         else
            CurrentJet = Part2;
      }

      zgs_.push_back(z);
      drs_.push_back(dr);
      pts_.push_back(pt);
      erads_.push_back(erad);
      log1drs_.push_back(log1dr);
      logzdrs_.push_back(logzdr);
      logkt_.push_back(logkt);
      tf_.push_back(tf);
   }
}

std::vector<double> softDropCounter::calculateNSD(double Kappa, double AngleKappa)
{
   std::vector<double> Result;

   for(int i = 0; i < (int)zgs_.size(); i++)
   {
      double Total = 0;
      for(int j = 0; j < (int)zgs_[i].size(); j++)
         Total = Total + pow(zgs_[i][j], Kappa) * pow(drs_[i][j], AngleKappa);
      Result.push_back(Total);
   }

   return Result;
}

#endif
