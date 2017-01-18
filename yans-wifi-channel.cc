/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2006,2007 INRIA
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Author: Mathieu Lacage, <mathieu.lacage@sophia.inria.fr>
 */


/*
*  This file is based on the file with the same name in the ns-3.26 Wi-Fi module
*  and was modified to include adjacent channel interference.
*
*  Copyright (C) 2017 Institute for Networked Systems, RWTH Aachen University 
*  (for modifications of ns-3.26 files)
*
*  This program is free software; you can redistribute it and/or modify
*  it under the terms of the GNU General Public License as published by
*  the Free Software Foundation; either version 2 of the License, or
*  (at your option) any later version.
*
*  This program is distributed in the hope that it will be useful,
*  but WITHOUT ANY WARRANTY; without even the implied warranty of
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*  GNU General Public License for more details.
*
*  You should have received a copy of the GNU General Public License along
*  with this program; if not, write to the Free Software Foundation, Inc.,
*  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*
*  Contact information:
*  Andra Voicu
*  avo@inets.rwth-aachen.de
*  Institute for Networked Systems
*  RWTH Aachen University
*  Kackertstr. 9 
*  52072 Aachen, Germany
*  www.inets.rwth-aachen.de
*/

#include "ns3/packet.h"
#include "ns3/simulator.h"
#include "ns3/mobility-model.h"
#include "ns3/net-device.h"
#include "ns3/node.h"
#include "ns3/log.h"
#include "ns3/pointer.h"
#include "ns3/object-factory.h"
#include "yans-wifi-channel.h"
#include "ns3/propagation-loss-model.h"
#include "ns3/propagation-delay-model.h"
#include <cmath>
#include <algorithm>
#include <vector>


namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("YansWifiChannel");

NS_OBJECT_ENSURE_REGISTERED (YansWifiChannel);

TypeId
YansWifiChannel::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::YansWifiChannel")
    .SetParent<WifiChannel> ()
    .SetGroupName ("Wifi")
    .AddConstructor<YansWifiChannel> ()
    .AddAttribute ("PropagationLossModel", "A pointer to the propagation loss model attached to this channel.",
                   PointerValue (),
                   MakePointerAccessor (&YansWifiChannel::m_loss),
                   MakePointerChecker<PropagationLossModel> ())
    .AddAttribute ("PropagationDelayModel", "A pointer to the propagation delay model attached to this channel.",
                   PointerValue (),
                   MakePointerAccessor (&YansWifiChannel::m_delay),
                   MakePointerChecker<PropagationDelayModel> ())
  ;
  return tid;
}

YansWifiChannel::YansWifiChannel ()
{
}

YansWifiChannel::~YansWifiChannel ()
{
  NS_LOG_FUNCTION_NOARGS ();
  m_phyList.clear ();
}

///////////////////////////////////////////////////////////////////////////////
//////////////////////////////////  Added code ////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

// Andra, 10.11.2016


// calculate area between y and x by dividing into trapezoids
double trapz(std::vector<double> fo, std::vector<double> psdo)
{
  double area=0.0;
  
  for (std::vector<int>::size_type k = 0; k < fo.size()-1; k++) 
        {
         area=area+((psdo[k]+psdo[k+1])*(fo[k+1]-fo[k])/2.0);
        }

  return area;
}

double overlapFactorToBeDecoded(uint32_t senderChannelWidth, uint32_t receiverChannelWidth, uint32_t senderCentralFrequency,  uint32_t receiverCentralFrequency, double transmitterPower, double receiverPower)
{
  // WORKS ONLY WITH CONSECUTIVE CHANNELS OF SAME WIDTH!!!
  
  // convert power in mW
  double transmitterPowermW = pow(10.0, transmitterPower/10.0);
  double receiverPowermW = pow(10.0, receiverPower/10.0);
 
  // spectrum mask given by standard
  double psd_db[9] = {-40, -28, -20, 0, 0, 0, -20, -28, -40};
  double f_20MHz[9] = {-30, -20, -11, -9, 0, 9, 11, 20, 30};
  double f_40MHz[9] = {-60, -40, -21, -19, 0, 19, 21, 40, 60};
  double f_80MHz[9] = {-120, -80, -40, -39, 0, 39, 41, 80, 120};
  double f_160MHz[9] = {-240, -160, -81, -79, 0, 79, 81, 160, 240};

  // spectrum mask for given transmitter and receiver
  double f1[9];
  double f2[9];
  double psd_max1=0;
  double psd1[9]; 
  double psd_max2=0;
  double psd2[9]; 

  switch (senderChannelWidth)
        {
         case 20:
                psd_max1=transmitterPowermW/20.1414;
                for(uint32_t i=0; i<9; i++)
                        {
                         f1[i]=senderCentralFrequency+f_20MHz[i];
                         psd1[i]=psd_max1*pow(10, psd_db[i]/10.0);
                        }
                break;
         case 40:
                psd_max1=transmitterPowermW/40.2744;
                for(uint32_t i=0; i<9; i++)
                        {
                         f1[i]=senderCentralFrequency+f_40MHz[i];
                         psd1[i]=psd_max1*pow(10, psd_db[i]/10.0);
                        }
                break;
         case 80:
                psd_max1=transmitterPowermW/80.5404;
                for(uint32_t i=0; i<9; i++)
                        {
                         f1[i]=senderCentralFrequency+f_80MHz[i];
                         psd1[i]=psd_max1*pow(10, psd_db[i]/10.0);
                        }
                break;
         case 160:
                psd_max1=transmitterPowermW/161.0724;
                for(uint32_t i=0; i<9; i++)
                        {
                         f1[i]=senderCentralFrequency+f_160MHz[i];
                         psd1[i]=psd_max1*pow(10, psd_db[i]/10.0);
                        }
                break;
          }

  switch (receiverChannelWidth)
        {
         case 20:
                psd_max2=receiverPowermW/20.1414;
                for(uint32_t i=0; i<9; i++)
                        {
                         f2[i]=receiverCentralFrequency+f_20MHz[i];
                         psd2[i]=psd_max2*pow(10, psd_db[i]/10.0);
                        }
                break;
         case 40:
                psd_max2=receiverPowermW/40.2744;
                for(uint32_t i=0; i<9; i++)
                        {
                         f2[i]=receiverCentralFrequency+f_40MHz[i];
                         psd2[i]=psd_max2*pow(10, psd_db[i]/10.0);
                        }
                break;
         case 80:
                psd_max2=receiverPowermW/80.5404;
                for(uint32_t i=0; i<9; i++)
                        {
                         f2[i]=receiverCentralFrequency+f_80MHz[i];
                         psd2[i]=psd_max2*pow(10, psd_db[i]/10.0);
                        }
                break;
         case 160:
                psd_max2=receiverPowermW/161.0724;
                for(uint32_t i=0; i<9; i++)
                        {
                         f2[i]=receiverCentralFrequency+f_160MHz[i];
                         psd2[i]=psd_max2*pow(10, psd_db[i]/10.0);
                        }
                break;
          }


  // calculate overlapping area
  std::vector<double> psdo;
  std::vector<double> fo;
  double f_min=std::max(f1[0],f2[0]);
  double f_max=std::min(f1[8],f2[8]);

  for (uint32_t i=0; i<9; i++)
        if (f_min<=f1[i] && f1[i]<=f_max)
                fo.push_back(f1[i]);

  for (uint32_t i=0; i<9; i++)
        {
         bool aux=0;
         for (uint32_t j=0; j<9; j++)
                {
                 if (fo[j]==f2[i])
                        aux=1;
                }
         if(aux==0 && f_min<=f2[i] && f2[i]<=f_max)
                fo.push_back(f2[i]);
        }
  std::sort(fo.begin(), fo.end());


  std::vector<double> fo_aux;
  std::vector<double> psdo_aux;
  
  // fill in values for psdo and add extra intersection points
  uint32_t aux1;
  uint32_t aux2;
  for (std::vector<int>::size_type i=0; i<fo.size(); i++)
        {
          aux1=1000; // just any number not valid as index in this context
          aux2=1000;
          for (uint32_t j=0; j<9; j++)
                if (f1[j]==fo[i]) aux1=j;
          for (uint32_t j=0; j<9; j++)
                if (f2[j]==fo[i]) aux2=j;
          if (aux1!=1000 && aux2!=1000)
                {
                 psdo.push_back(std::min(psd1[aux1], psd2[aux2]));
                 // add additional intersection points
                 if ((psd1[aux1]<psd2[aux2] && psd1[aux1+1]>psd2[aux2+1]) || (psd1[aux1]>psd2[aux2] && psd1[aux1+1]<psd2[aux2+1]))
                        {
                         double ax1=f1[aux1];
                         double bx1=f1[aux1+1];
                         double ay1=psd1[aux1];
                         double by1=psd1[aux1+1];
                         double ax2=f2[aux2];
                         double bx2=f2[aux2+1];
                         double ay2=psd2[aux2];
                         double by2=psd2[aux2+1];
                         double wx=(((ax2*(by2-ay2))/(bx2-ax2))-ay2-((ax1*(by1-ay1))/(bx1-ax1))+ay1)/(((by2-ay2)/(bx2-ax2))-((by1-ay1)/(bx1-ax1)));   
                         double wy=(wx*(by2-ay2)/(bx2-ax2))-(ax2*(by2-ay2)/(bx2-ax2))+ay2;
                         fo_aux.push_back(wx);
                         psdo_aux.push_back(wy); 
                        }
                }
           else
                if (aux1!=1000 && aux2==1000)
                        {
                         for (uint32_t j=0; j<9-1; j++)
                                {
                                 if ((f2[j]<f1[aux1]) && (f1[aux1]<f2[j+1]))
                                        {
					if (psd1[aux1]<=psd2[j] && psd1[aux1]<=psd2[j+1])
                                                psdo.push_back(psd1[aux1]);
                                        else
                                                {
                                                 double ax=f2[j];
                                                 double bx=f2[j+1];
                                                 double ay=psd2[j];
                                                 double by=psd2[j+1];
                                                 double wx=f1[aux1];
                                                 double wy=((by-ay)*(wx-ax)/(bx-ax))+ay;
                                                 psdo.push_back(std::min(wy,psd1[aux1]));              
                                                }
					}
                                }
                        }
                else
                    if (aux1==1000 && aux2!=1000)
                            {
                             for (uint32_t j=0; j<9-1; j++)
                                {
                                 if ((f1[j]<f2[aux2]) && (f2[aux2]<f1[j+1]))
                                        {
					if (psd2[aux2]<=psd1[j] && psd2[aux2]<=psd1[j+1])
                                                psdo.push_back(psd2[aux2]);
                                        else
                                                {
                                                 double ax=f1[j];
                                                 double bx=f1[j+1];
                                                 double ay=psd1[j];
                                                 double by=psd1[j+1];
                                                 double wx=f2[aux2];
                                                 double wy=((by-ay)*(wx-ax)/(bx-ax))+ay;
                                                 psdo.push_back(std::min(wy,psd2[aux2]));              
                                                }
					}
                                }
                        }  
          
        }
   

  // merge original frequency points with additional intersection points
  for (uint32_t i=0; i<fo_aux.size(); i++)
        {
	 std::vector<double>::iterator itp=psdo.begin();	
	 for (std::vector<double>::iterator it=fo.begin(); it<fo.end()-1; it++)
                {
		 itp++;
		 if (fo_aux[i]>*it && fo_aux[i]<*(it+1))
                        {
			 fo.insert(it+1,fo_aux[i]);
         		 psdo.insert(itp,psdo_aux[i]);
			 break; 
                        }
		}
        }

  /*
  std::cout<<"fo=[ ";
  for (uint32_t i=0; i<fo.size(); i++)
  	std::cout<<fo[i]<<" ";
  std::cout<<"\n";
  std::cout<<"psdo=[ ";
  for (uint32_t i=0; i<psdo.size(); i++)
  	std::cout<<psdo[i]<<" ";
  std::cout<<"\n";
  */

  std::vector<double> psd1_vector;
  std::vector<double> f1_vector;
  
  for (uint32_t i=0; i<9; i++)
        {
        psd1_vector.push_back(psd1[i]);
        f1_vector.push_back(f1[i]);
        }

  double alpha=0.0;  
  if (fo.size()>1)
  	{
  	 alpha = trapz(fo, psdo) / trapz(f1_vector, psd1_vector);
  	}
  
  return alpha;
}



///////////////////////////////////////////////////////////////////////////////
///////////////////////////////  End of Added code ////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void
YansWifiChannel::SetPropagationLossModel (Ptr<PropagationLossModel> loss)
{
  m_loss = loss;
}

void
YansWifiChannel::SetPropagationDelayModel (Ptr<PropagationDelayModel> delay)
{
  m_delay = delay;
}

void
YansWifiChannel::Send (Ptr<YansWifiPhy> sender, Ptr<const Packet> packet, double txPowerDbm,
                       WifiTxVector txVector, WifiPreamble preamble, enum mpduType mpdutype, Time duration) const
{
  Ptr<MobilityModel> senderMobility = sender->GetMobility ()->GetObject<MobilityModel> ();
  NS_ASSERT (senderMobility != 0);
  uint32_t j = 0;
  double overlappingFactorDb = 0.0;

  ////////////////////////////////////
  // ADJACENT CHANNEL INTERFERENCE //  for 802.11ac
  ////////////////////////////////////


  // The term co-channel interference is used when all the power coming from the interfer
  // has to be taken into account. Therefore overlapingFactor is 0.0.
  //
  // On the contrary, the term adjacent channel interference is used when only a part of  
  // the interferer's power should be taken into account. 

 
  for (PhyList::const_iterator i = m_phyList.begin (); i != m_phyList.end (); i++, j++)
    { 
      //uint32_t senderChannelNo = sender->GetChannelNumber();
      uint32_t senderChannelWidth = sender->GetChannelWidth(); 
      uint32_t senderCentralFrequency = sender->GetFrequency();
      
      //uint32_t receiverChannelNo = (*i)->GetChannelNumber();
      uint32_t receiverChannelWidth = (*i)->GetChannelWidth(); 
      uint32_t receiverCentralFrequency = (*i)->GetFrequency(); 
      double receiverPower = (*i)->GetTxPowerEnd(); //in dBm


      bool setModifs = true;
      
      if (sender != (*i))
        {   

          if((*i)->GetChannelNumber () != sender->GetChannelNumber () && !setModifs)
                {
                  continue;
                }

          if ((senderCentralFrequency-senderChannelWidth/2<=receiverCentralFrequency-receiverChannelWidth/2) && (senderCentralFrequency+senderChannelWidth/2>=receiverCentralFrequency+receiverChannelWidth/2))
                {
                 // Sender's bandwidth included in receiver's bandwidth
                 // Co channel interference
                 overlappingFactorDb = 0.0;
                }
          else 
                {
                 if((senderCentralFrequency-senderChannelWidth/2>=20+receiverCentralFrequency+receiverChannelWidth/2) || (20+senderCentralFrequency+senderChannelWidth/2<=receiverCentralFrequency-receiverChannelWidth/2))
                        {
                         // 20 MHz difference between the edges of the two used channels
                         // no overlapping, no ACI
                         continue;
                        }
                  else
                        {
                         // ACI
                         double alpha = overlapFactorToBeDecoded(senderChannelWidth, receiverChannelWidth, senderCentralFrequency,  receiverCentralFrequency, txPowerDbm, receiverPower);
                         overlappingFactorDb = 10*log10(alpha);
                        }
                 }

              Ptr<MobilityModel> receiverMobility = (*i)->GetMobility ()->GetObject<MobilityModel> ();
              Time delay = m_delay->GetDelay (senderMobility, receiverMobility);
              double rxPowerDbm = m_loss->CalcRxPower (txPowerDbm, senderMobility, receiverMobility);
              rxPowerDbm += overlappingFactorDb;
              NS_LOG_DEBUG ("propagation: txPower=" << txPowerDbm << "dbm, rxPower=" << rxPowerDbm << "dbm, " <<
                        "distance=" << senderMobility->GetDistanceFrom (receiverMobility) << "m, delay=" << delay);
              Ptr<Packet> copy = packet->Copy ();
              Ptr<Object> dstNetDevice = m_phyList[j]->GetDevice ();
              uint32_t dstNode;

          //---------End of modification


          if (dstNetDevice == 0)
            {
              dstNode = 0xffffffff;
            }
          else
            {
              dstNode = dstNetDevice->GetObject<NetDevice> ()->GetNode ()->GetId ();
            }

          struct Parameters parameters;
          parameters.rxPowerDbm = rxPowerDbm;
          parameters.type = mpdutype;
          parameters.duration = duration;
          parameters.txVector = txVector;
          parameters.preamble = preamble;
          // change
          parameters.channelFrequency = sender->GetFrequency ();
          parameters.channelWidth = sender->GetChannelWidth ();
          // end change

          Simulator::ScheduleWithContext (dstNode,
                                          delay, &YansWifiChannel::Receive, this,
                                          j, copy, parameters);
        }
    }
}

void
YansWifiChannel::Receive (uint32_t i, Ptr<Packet> packet, struct Parameters parameters) const
{
  m_phyList[i]->StartReceivePreambleAndHeader (packet, parameters.rxPowerDbm, parameters.txVector, parameters.preamble, parameters.type, parameters.duration, parameters.channelFrequency, parameters.channelWidth); //--------change here
}

uint32_t
YansWifiChannel::GetNDevices (void) const
{
  return m_phyList.size ();
}

Ptr<NetDevice>
YansWifiChannel::GetDevice (uint32_t i) const
{
  return m_phyList[i]->GetDevice ()->GetObject<NetDevice> ();
}

void
YansWifiChannel::Add (Ptr<YansWifiPhy> phy)
{
  m_phyList.push_back (phy);
}

int64_t
YansWifiChannel::AssignStreams (int64_t stream)
{
  int64_t currentStream = stream;
  currentStream += m_loss->AssignStreams (stream);
  return (currentStream - stream);
}

} //namespace ns3
